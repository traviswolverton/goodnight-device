// Minimal HTTP GET into a caller-supplied buffer, for the broker-settings
// fetch (device_config.cpp). Taken from desk-dashboard-esp32, where it
// also serves the CAMERA screen's snapshot fetch.

#include "http_get.h"

#include <WiFi.h>
#include <strings.h>  // strncasecmp, for the manual HTTP header parse below

namespace {
constexpr size_t kHttpLineBufSize = 128;
constexpr size_t kHttpMaxHeaderLines = 40;
}  // namespace

// Minimal hand-rolled HTTP/1.1 GET, deliberately not ESP32's HTTPClient
// class. HTTPClient.h unconditionally pulls in WiFiClientSecure (full
// mbedTLS plus the root cert bundle) whether or not it's ever used for
// https:// -- measured at ~45KB of flash here, more than this board's
// existing OTA partition had headroom for (linking with HTTPClient in
// the mix overflowed it by ~26KB). The server's endpoints this is used
// for (/device/config) are plain HTTP on the LAN, no TLS, so
// none of that is needed. This parses exactly what they return -- a
// status line and a Content-Length header, then exactly that many body
// bytes -- and nothing more (no chunked encoding, no redirects). Bounds
// the whole operation (connect + headers + body) with an explicit
// millis() deadline, not just per-call socket timeouts, so a
// connection that dribbles in a byte at a time can't run past it.
bool httpGetToBuffer(const char* url, const char* bearerToken, uint8_t* buffer, size_t bufferSize,
                     unsigned long deadlineMs, size_t* outLen) {
  const char* kPrefix = "http://";
  if (strncmp(url, kPrefix, strlen(kPrefix)) != 0) {
    Serial.println("[http] URL is not http://");
    return false;
  }
  const char* hostStart = url + strlen(kPrefix);
  const char* pathStart = strchr(hostStart, '/');
  const char* path = pathStart ? pathStart : "/";

  char host[64];
  size_t hostLen = pathStart ? (size_t)(pathStart - hostStart) : strlen(hostStart);
  if (hostLen >= sizeof(host)) hostLen = sizeof(host) - 1;
  memcpy(host, hostStart, hostLen);
  host[hostLen] = '\0';

  uint16_t port = 80;
  char* colon = strchr(host, ':');
  if (colon) {
    port = (uint16_t)atoi(colon + 1);
    *colon = '\0';
  }

  int32_t connectTimeoutMs = (int32_t)(deadlineMs - millis());
  WiFiClient client;
  if (connectTimeoutMs <= 0 || !client.connect(host, port, connectTimeoutMs)) {
    Serial.printf("[http] connect to %s:%u failed\n", host, port);
    return false;
  }
  // WiFiClient::setTimeout() takes *seconds*, unlike the base Stream
  // class it overrides (confirmed against this core's WiFiClient.cpp,
  // not assumed) -- rounds the remaining budget up so a 1-999ms
  // remainder doesn't round down to 0.
  uint32_t remainingMs = (millis() < deadlineMs) ? (deadlineMs - millis()) : 0;
  client.setTimeout((remainingMs + 999) / 1000);

  client.printf("GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n", path, host);
  if (bearerToken != nullptr) {
    client.printf("Authorization: Bearer %s\r\n", bearerToken);
  }
  client.print("\r\n");

  char line[kHttpLineBufSize];

  // Status line, e.g. "HTTP/1.1 200 OK".
  size_t n = client.readBytesUntil('\n', line, sizeof(line) - 1);
  if (n == 0 || millis() > deadlineMs) {
    Serial.println("[http] no response");
    client.stop();
    return false;
  }
  line[n] = '\0';
  char* statusSpace = strchr(line, ' ');
  int statusCode = statusSpace ? atoi(statusSpace + 1) : -1;
  if (statusCode != 200) {
    Serial.printf("[http] GET failed: %s\n", line);
    client.stop();
    return false;
  }

  long contentLength = -1;
  for (size_t i = 0; i < kHttpMaxHeaderLines; i++) {
    n = client.readBytesUntil('\n', line, sizeof(line) - 1);
    if (millis() > deadlineMs) {
      Serial.println("[http] header read timed out");
      client.stop();
      return false;
    }
    line[n] = '\0';
    if (n > 0 && line[n - 1] == '\r') line[n - 1] = '\0';  // readBytesUntil
                                                            // stops at '\n',
                                                            // leaving '\r'
    if (line[0] == '\0') break;  // blank line -- end of headers
    if (strncasecmp(line, "content-length:", 15) == 0) {
      contentLength = atol(line + 15);
    }
  }

  if (contentLength <= 0 || (size_t)contentLength > bufferSize) {
    Serial.printf("[http] Content-Length out of range: %ld\n", contentLength);
    client.stop();
    return false;
  }

  size_t received = 0;
  while (received < (size_t)contentLength) {
    if (millis() > deadlineMs) {
      Serial.printf("[http] body read timed out at %u/%ld\n", (unsigned)received,
                    contentLength);
      client.stop();
      return false;
    }
    int avail = client.available();
    if (avail <= 0) {
      if (!client.connected()) break;
      delay(1);  // brief yield while waiting for more bytes -- this
                 // whole function is already a documented blocking call
                 // bounded by deadlineMs above, not the main loop
      continue;
    }
    int n2 = client.read(buffer + received, (size_t)contentLength - received);
    if (n2 <= 0) break;
    received += (size_t)n2;
  }
  client.stop();

  if (received != (size_t)contentLength) {
    Serial.printf("[http] incomplete: got %u of %ld bytes\n", (unsigned)received,
                  contentLength);
    return false;
  }

  *outLen = received;
  return true;
}
