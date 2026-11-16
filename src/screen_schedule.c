// SPDX-License-Identifier: GPL-3.0-or-later
#include "screen_schedule.h"

#include <math.h>

static bool is_daylight(const struct tm *utc, double latitude, double longitude)
{
    // NOAA solar position approximation, using UTC and east-positive longitude.
    const double pi = 3.14159265358979323846;
    const double rad = pi / 180.0;
    const int year = utc->tm_year + 1900;
    const bool leap_year = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    const double utc_minutes = utc->tm_hour * 60 + utc->tm_min + utc->tm_sec / 60.0;
    const double gamma = 2.0 * pi / (leap_year ? 366 : 365)
                       * (utc->tm_yday + (utc_minutes - 720.0) / 1440.0);
    const double sin_gamma = sin(gamma);
    const double cos_gamma = cos(gamma);
    const double sin_2gamma = sin(2.0 * gamma);
    const double cos_2gamma = cos(2.0 * gamma);
    const double equation_of_time = 229.18 * (0.000075 + 0.001868 * cos_gamma
        - 0.032077 * sin_gamma - 0.014615 * cos_2gamma - 0.040849 * sin_2gamma);
    const double declination = 0.006918 - 0.399912 * cos_gamma + 0.070257 * sin_gamma
        - 0.006758 * cos_2gamma + 0.000907 * sin_2gamma
        - 0.002697 * cos(3.0 * gamma) + 0.00148 * sin(3.0 * gamma);
    const double hour_angle = (utc_minutes + equation_of_time + 4.0 * longitude - 720.0) * rad / 4.0;
    const double cos_zenith = sin(latitude * rad) * sin(declination)
        + cos(latitude * rad) * cos(declination) * cos(hour_angle);

    // This threshold accounts for atmospheric refraction and the sun's apparent radius.
    return cos_zenith >= cos(90.833 * rad);
}

bool screen_schedule_is_on(const screen_schedule_t *schedule, time_t now)
{
    struct tm clock;

    switch (schedule->mode) {
        case SCREEN_SCHEDULE_ALWAYS:
            return true;
        case SCREEN_SCHEDULE_FIXED: {
            if (now == (time_t)-1 || localtime_r(&now, &clock) == NULL)
                return false;
            const int minute = clock.tm_hour * 60 + clock.tm_min;
            const int on = schedule->on_minute;
            const int off = schedule->off_minute;
            return on < off ? minute >= on && minute < off : minute >= on || minute < off;
        }
        case SCREEN_SCHEDULE_SUN:
            return now != (time_t)-1 && gmtime_r(&now, &clock) != NULL
                && is_daylight(&clock, schedule->latitude, schedule->longitude);
        case SCREEN_SCHEDULE_INVALID:
            return false;
    }
    return false;
}
