# PlatformIO pre-build script: writes include/version_generated.h with the
# git version (e.g. "c62c675", or "c62c675-dirty" with uncommitted changes),
# so the diagnostics page can show which firmware is running. The header is
# only rewritten when the version changes, so only the files that include it
# recompile.
Import("env")
import os
import subprocess

project_dir = env["PROJECT_DIR"]


def git(*args):
    try:
        return subprocess.check_output(
            ["git", *args], cwd=project_dir, stderr=subprocess.DEVNULL
        ).decode().strip()
    except Exception:
        return ""


version = git("describe", "--always", "--dirty", "--abbrev=7") or "unknown"
path = os.path.join(project_dir, "include", "version_generated.h")
content = '#pragma once\n#define FIRMWARE_VERSION "%s"\n' % version
if not os.path.exists(path) or open(path).read() != content:
    with open(path, "w") as f:
        f.write(content)
