#!/usr/bin/env python3
"""Generate include/secrets.h from OpenBao (KV v2).

Run it before a build; it is deliberately not a PlatformIO pre: script, so
a build never needs the network and never rewrites secrets.h on its own.

    BAO_ADDR=https://<openbao> BAO_TOKEN=<token> scripts/gen_secrets.py

Never prints a value, only field and define NAMES. Neither the generated
file nor any value is meant to be committed (include/secrets.h is
gitignored; this script refuses to write it if that stops being true).

Environment:
  BAO_ADDR         OpenBao API address (required; not kept in this repo)
  BAO_TOKEN        token; else the file `bao login` writes (~/.vault-token)
  BAO_CACERT       CA certificate to trust, for a self-signed server
  BAO_SKIP_VERIFY  1 = don't verify the server certificate (self-signed
                   and no CA file at hand; the token still goes over TLS)
  BAO_MOUNT        KV v2 mount (default: homelab)
  BAO_PATH         path prefix inside it (default: bedside)

Modes:
  (default)        fetch, compare with the existing file, write
  --check          fetch and compare, write nothing
  --lint           offline: secrets.h.example vs. what this script produces
  --ota-password   print the OTA password for command substitution only:
      OTA_PASSWORD="$(scripts/gen_secrets.py --ota-password)" \\
          pio run -t upload -e esp32dev-ota
"""
import argparse
import json
import os
import re
import ssl
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXAMPLE = os.path.join(PROJECT_DIR, "include", "secrets.h.example")
DEFAULT_OUT = os.path.join(PROJECT_DIR, "include", "secrets.h")

# Defines in secrets.h.example that are deliberately never generated.
SKIPPED = {}


DEFAULT_MQTT_USERNAME = "bedside-esp32"

# define -> (entry, field, kind, required). Order here is the order in the
# generated file. kind: str = quoted C string, int = bare integer literal.
SPEC = [
    ("WIFI_SSID",     "secrets", "wifi_ssid",     "str",  True),
    ("WIFI_PASSWORD", "secrets", "wifi_password", "str",  True),
    ("SERVER_URL",    "config",  "server_url",    "url",  True),
    ("DEVICE_TOKEN",  "secrets", "device_token",  "str",  True),
    ("MQTT_HOST",     "config",  "mqtt_host",     "host", True),
    ("MQTT_PORT",     "config",  "mqtt_port",     "port", True),
    # No field for this in the OpenBao entries (it is not secret); optional
    # `mqtt_username` in config overrides the default.
    ("MQTT_USERNAME", "config",  "mqtt_username", "str",  False),
    ("MQTT_PASSWORD", "secrets", "mqtt_password", "str",  True),
    ("OTA_PASSWORD",  "secrets", "ota_password",  "str",  True),
]
PRODUCED = [s[0] for s in SPEC]

DEFINE_RE = re.compile(r"^\s*#\s*define\s+([A-Za-z_]\w*)", re.M)


class Fail(Exception):
    pass


def define_names(text):
    return set(DEFINE_RE.findall(text))


# ---- example <-> script consistency (offline) -----------------------------

def lint_example():
    try:
        with open(EXAMPLE) as f:
            example = define_names(f.read())
    except OSError as e:
        raise Fail("cannot read %s: %s" % (EXAMPLE, e.strerror))
    produced, skipped = set(PRODUCED), set(SKIPPED)
    problems = []
    for n in sorted(example - produced - skipped):
        problems.append("%s is in secrets.h.example but this script neither "
                        "produces nor knowingly skips it" % n)
    for n in sorted(produced - example):
        problems.append("%s is produced by this script but is not in "
                        "secrets.h.example" % n)
    for n in sorted(skipped - example):
        problems.append("%s is listed as skipped but is not in "
                        "secrets.h.example" % n)
    if problems:
        raise Fail("secrets.h.example and scripts/gen_secrets.py disagree:\n  "
                   + "\n  ".join(problems))


# ---- OpenBao ---------------------------------------------------------------

def bao_token():
    tok = os.environ.get("BAO_TOKEN", "").strip()
    if tok:
        return tok
    try:
        with open(os.path.expanduser("~/.vault-token")) as f:
            tok = f.read().strip()
    except OSError:
        tok = ""
    if not tok:
        raise Fail("no token: set BAO_TOKEN (e.g. from `bao login`)")
    return tok


def ssl_context():
    cafile = os.environ.get("BAO_CACERT")
    if os.environ.get("BAO_SKIP_VERIFY") == "1":
        ctx = ssl.create_default_context()
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE
        return ctx
    return ssl.create_default_context(cafile=cafile) if cafile else None


def read_entry(addr, token, mount, name):
    url = "%s/v1/%s/data/%s" % (addr.rstrip("/"), mount, name)
    req = urllib.request.Request(url, headers={"X-Vault-Token": token})
    where = "%s/%s" % (mount, name)
    try:
        with urllib.request.urlopen(req, timeout=10, context=ssl_context()) as r:
            body = json.load(r)
    except urllib.error.HTTPError as e:
        msgs = ""
        try:
            msgs = "; ".join(json.load(e).get("errors") or [])
        except Exception:
            pass
        hint = {
            403: " (token lacks read on it: is the bedside-build-read policy "
                 "applied and attached to this token?)",
            404: " (entry does not exist, or the mount/path is wrong)",
        }.get(e.code, "")
        raise Fail("reading %s: HTTP %d%s%s"
                   % (where, e.code, hint, (" - " + msgs) if msgs else ""))
    except ssl.SSLError as e:
        raise Fail("TLS error reading %s: %s. For a self-signed server set "
                   "BAO_CACERT=<ca file> (or BAO_SKIP_VERIFY=1)"
                   % (where, e.reason or e))
    except urllib.error.URLError as e:
        reason = e.reason
        if isinstance(reason, ssl.SSLError):
            raise Fail("TLS error reading %s: %s. For a self-signed server "
                       "set BAO_CACERT=<ca file> (or BAO_SKIP_VERIFY=1)"
                       % (where, reason.reason or reason))
        raise Fail("cannot reach OpenBao for %s: %s" % (where, reason))
    except (ValueError, OSError) as e:
        raise Fail("reading %s: unusable response (%s)" % (where, type(e).__name__))
    data = (body.get("data") or {}).get("data")
    if not isinstance(data, dict):
        raise Fail("%s: response has no KV v2 data (is the mount KV v2?)" % where)
    return data


# ---- values -> C -----------------------------------------------------------

def c_string(value, where):
    if not isinstance(value, str) or not value:
        raise Fail("%s is missing or empty" % where)
    if any(ord(c) < 0x20 or ord(c) == 0x7F for c in value):
        raise Fail("%s contains a control character" % where)
    return '"%s"' % value.replace("\\", "\\\\").replace('"', '\\"')


def render(define, kind, value, where):
    if kind == "port":
        if isinstance(value, bool) or not re.fullmatch(r"\d{1,5}", str(value).strip()):
            raise Fail("%s is not a port number" % where)
        port = int(str(value).strip())
        if not 1 <= port <= 65535:
            raise Fail("%s is out of range" % where)
        return str(port)
    if kind == "host":
        s = value.strip() if isinstance(value, str) else value
        c_string(s, where)
        if re.search(r"[:/\s]", s):
            raise Fail("%s must be the bare host (no scheme, port or path); "
                       "the port is the separate mqtt_port field" % where)
        return c_string(s, where)
    if kind == "url":
        s = value.strip().rstrip("/") if isinstance(value, str) else value
        c_string(s, where)
        if not s.startswith("http://"):
            raise Fail("%s must be a plain http:// address" % where)
        return c_string(s, where)
    return c_string(value, where)


def build(entries, show):
    """-> {define: C literal}. Reports field NAMES only."""
    out, missing = {}, []
    for define, entry, field, kind, required in SPEC:
        value = entries[entry].get(field)
        where = "%s.%s" % (entry, field)
        if value in (None, ""):
            if required:
                missing.append(where)
                continue
            value, note = DEFAULT_MQTT_USERNAME, " (default; no %s field)" % where
        else:
            note = ""
        out[define] = render(define, kind, value, where)
        show("  %-14s <- %s%s" % (define, where, note))
    if missing:
        raise Fail("missing field(s) in OpenBao: " + ", ".join(missing))
    return out


def file_text(defines):
    lines = [
        "// Generated by scripts/gen_secrets.py from OpenBao. Do not edit and",
        "// never commit (gitignored). Re-run the script to refresh it.",
        "",
        "#pragma once",
        "",
    ]
    for name in PRODUCED:
        if name not in defines:
            continue
        lines.append("#define %-14s %s" % (name, defines[name]))
    return "\n".join(lines) + "\n"


# ---- writing ---------------------------------------------------------------

def git(*args):
    return subprocess.run(["git", *args], cwd=PROJECT_DIR,
                          capture_output=True).returncode


def check_not_committable(path):
    rel = os.path.relpath(path, PROJECT_DIR)
    if rel.startswith(".."):
        return  # outside the repo (e.g. a test output)
    if git("ls-files", "--error-unmatch", rel) == 0:
        raise Fail("%s is tracked by git; refusing to write secrets into it" % rel)
    if git("check-ignore", "-q", rel) != 0:
        raise Fail("%s is not gitignored; add it to .gitignore first" % rel)


def compare(path, new_names, say):
    """Compare define NAMES with the existing file. -> names that would be lost."""
    if not os.path.exists(path):
        say("no existing %s; nothing to compare" % os.path.basename(path))
        return set()
    with open(path) as f:
        old = define_names(f.read())
    lost = old - new_names - set(SKIPPED)
    added = new_names - old
    say("compared with existing %s (names only):" % os.path.basename(path))
    say("  same names:        %s" % (", ".join(sorted(old & new_names)) or "-"))
    say("  new in generated:  %s" % (", ".join(sorted(added)) or "-"))
    say("  would be LOST:     %s" % (", ".join(sorted(lost)) or "-"))
    return lost


def write_secret_file(path, text):
    d = os.path.dirname(path)
    fd, tmp = tempfile.mkstemp(dir=d, prefix=".secrets.", suffix=".tmp")
    try:
        os.fchmod(fd, 0o600)
        with os.fdopen(fd, "w") as f:
            f.write(text)
        if os.path.exists(path):
            os.replace(path, path + ".old")  # gitignored; delete when happy
            os.chmod(path + ".old", 0o600)
        os.replace(tmp, path)
    except BaseException:
        if os.path.exists(tmp):
            os.unlink(tmp)
        raise


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--check", action="store_true", help="fetch and compare, write nothing")
    ap.add_argument("--lint", action="store_true", help="offline example check only")
    ap.add_argument("--ota-password", action="store_true",
                    help="print only the OTA password (for $(...))")
    ap.add_argument("--force", action="store_true",
                    help="replace even if defines in the existing file would be lost")
    ap.add_argument("--output", default=DEFAULT_OUT, help=argparse.SUPPRESS)
    args = ap.parse_args()

    def say(msg):
        print(msg, file=sys.stderr)

    lint_example()
    if args.lint:
        say("ok: secrets.h.example matches what the script produces or skips")
        return

    addr = os.environ.get("BAO_ADDR", "").strip()
    if not addr:
        raise Fail("BAO_ADDR is not set")
    token = bao_token()
    mount = os.environ.get("BAO_MOUNT", "homelab")
    prefix = os.environ.get("BAO_PATH", "bedside")

    if args.ota_password:
        if sys.stdout.isatty():
            raise Fail("--ota-password prints a secret; use it inside $(...), "
                       "not on a terminal")
        data = read_entry(addr, token, mount, prefix + "/secrets")
        value = data.get("ota_password")
        if not isinstance(value, str) or not value:
            raise Fail("missing field in OpenBao: secrets.ota_password")
        sys.stdout.write(value)
        return

    if not args.check:
        check_not_committable(args.output)

    entries = {}
    for entry in ("secrets", "config"):
        entries[entry] = read_entry(addr, token, mount, "%s/%s" % (prefix, entry))
        say("%s/%s fields: %s" % (prefix, entry, ", ".join(sorted(entries[entry])) or "-"))

    say("mapping:")
    defines = build(entries, say)
    lost = compare(args.output, set(defines), say)

    if args.check:
        say("--check: nothing written")
        return
    if lost and not args.force:
        raise Fail("not replacing %s: it defines names this script does not "
                   "produce (above). Review them, or re-run with --force"
                   % os.path.basename(args.output))
    write_secret_file(args.output, file_text(defines))
    say("wrote %s (%d defines, mode 600)" % (os.path.relpath(args.output, PROJECT_DIR), len(defines)))


if __name__ == "__main__":
    try:
        main()
    except Fail as e:
        print("gen_secrets: " + str(e), file=sys.stderr)
        sys.exit(1)
