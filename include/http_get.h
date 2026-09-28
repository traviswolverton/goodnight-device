#pragma once

#include <stddef.h>
#include <stdint.h>

// Blocking HTTP/1.1 GET of a plain http:// URL into `buffer`, bounded as a
// whole by `deadlineMs` (an absolute millis() value). Succeeds only on a
// 200 with a Content-Length that fits; no chunked encoding, no redirects,
// no TLS -- see http_get.cpp for why this isn't ESP32's HTTPClient.
// `bearerToken`, if not null, is sent as `Authorization: Bearer <token>`.
bool httpGetToBuffer(const char* url, const char* bearerToken, uint8_t* buffer, size_t bufferSize,
                     unsigned long deadlineMs, size_t* outLen);
