// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef XGP_SCREEN_SCHEDULE_H
#define XGP_SCREEN_SCHEDULE_H

#include <stdbool.h>
#include <time.h>

typedef enum {
    SCREEN_SCHEDULE_ALWAYS,
    SCREEN_SCHEDULE_FIXED,
    SCREEN_SCHEDULE_SUN,
    SCREEN_SCHEDULE_INVALID
} screen_schedule_mode_t;

typedef struct {
    screen_schedule_mode_t mode;
    int on_minute;
    int off_minute;
    double latitude;
    double longitude;
} screen_schedule_t;

bool screen_schedule_is_on(const screen_schedule_t *schedule, time_t now);

#endif
