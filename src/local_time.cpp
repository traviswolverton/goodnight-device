#include "local_time.h"

#include <Arduino.h>
#include <time.h>

namespace {

// POSIX rule for US Central: CST (UTC-6), CDT (UTC-5) from the second
// Sunday in March to the first Sunday in November.
constexpr char kTimezone[] = "CST6CDT,M3.2.0,M11.1.0";

// Seconds since 1970 for 2024-01-01. Anything earlier means the clock was
// never set (it starts at 1970).
constexpr time_t kFirstValidEpoch = 1704067200;

}  // namespace

void localTimeBegin() {
  configTzTime(kTimezone, "pool.ntp.org", "time.google.com");
}

bool localTimeNow12h(uint8_t& hour12, uint8_t& minute, bool& pm) {
  // Not getLocalTime(&t, 0): with a zero timeout it checks only once, and
  // if the millisecond counter ticks over first it reports "not set" even
  // when it is, which made the clock flip to "--:--" at random.
  time_t now = time(nullptr);
  if (now < kFirstValidEpoch) {
    return false;
  }
  struct tm t;
  localtime_r(&now, &t);
  hour12 = t.tm_hour % 12 == 0 ? 12 : t.tm_hour % 12;
  minute = t.tm_min;
  pm = t.tm_hour >= 12;
  return true;
}
