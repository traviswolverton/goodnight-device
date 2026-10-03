#include "local_time.h"

#include <Arduino.h>
#include <time.h>

namespace {

// POSIX rule for US Central: CST (UTC-6), CDT (UTC-5) from the second
// Sunday in March to the first Sunday in November.
constexpr char kTimezone[] = "CST6CDT,M3.2.0,M11.1.0";

// Anything before this year means the clock was never set (it starts at
// 1970).
constexpr int kFirstValidYear = 2024;

}  // namespace

void localTimeBegin() {
  configTzTime(kTimezone, "pool.ntp.org", "time.google.com");
}

bool localTimeNow12h(uint8_t& hour12, uint8_t& minute, bool& pm) {
  struct tm t;
  if (!getLocalTime(&t, 0) || t.tm_year + 1900 < kFirstValidYear) {
    return false;
  }
  hour12 = t.tm_hour % 12 == 0 ? 12 : t.tm_hour % 12;
  minute = t.tm_min;
  pm = t.tm_hour >= 12;
  return true;
}
