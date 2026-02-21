/*
 * SleepyTime — watch_time.c
 *
 * Pure time domain logic. No Zephyr, no hardware dependencies.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "watch_time.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* ── Internal helpers ───────────────────────────────────────────────────────
 */

static bool is_leap_year(uint16_t year) {
  return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

static uint8_t days_in_month(uint8_t month, uint16_t year) {
  static const uint8_t dom[13] = {0,  31, 28, 31, 30, 31, 30,
                                  31, 31, 30, 31, 30, 31};
  if (month == 2 && is_leap_year(year)) {
    return 29;
  }
  if (month < 1 || month > 12) {
    return 0;
  }
  return dom[month];
}

/* ── Validation ─────────────────────────────────────────────────────────────
 */

bool watch_time_is_valid(const watch_time_t *t) {
  if (t == NULL) {
    return false;
  }
  if (t->year < 2000 || t->year > 2099) {
    return false;
  }
  if (t->month < 1 || t->month > 12) {
    return false;
  }
  if (t->day < 1 || t->day > days_in_month(t->month, t->year)) {
    return false;
  }
  if (t->hour > 23) {
    return false;
  }
  if (t->minute > 59) {
    return false;
  }
  if (t->second > 59) {
    return false;
  }
  if (t->weekday > 6) {
    return false;
  }
  return true;
}

/* ── Arithmetic ─────────────────────────────────────────────────────────────
 */

void watch_time_add_seconds(watch_time_t *t, uint32_t seconds) {
  if (t == NULL) {
    return;
  }

  uint32_t rem = seconds;

  /* Seconds */
  rem += t->second;
  t->second = (uint8_t)(rem % 60);
  rem /= 60;

  /* Minutes */
  rem += t->minute;
  t->minute = (uint8_t)(rem % 60);
  rem /= 60;

  /* Hours */
  rem += t->hour;
  t->hour = (uint8_t)(rem % 24);
  rem /= 24;

  /* Days — roll over months and years */
  while (rem > 0) {
    uint8_t dim = days_in_month(t->month, t->year);
    uint32_t days_left_in_month = (uint32_t)(dim - t->day);

    if (rem <= days_left_in_month) {
      t->day += (uint8_t)rem;
      rem = 0;
    } else {
      rem -= (days_left_in_month + 1U);
      t->day = 1;
      t->month++;
      if (t->month > 12) {
        t->month = 1;
        t->year++;
      }
    }
  }

  watch_time_compute_weekday(t);
}

/*
 * Convert a watch_time_t to an approximate epoch-style day count + seconds
 * for difference arithmetic. Accurate for dates 2000–2099.
 */
static uint32_t to_epoch_seconds(const watch_time_t *t) {
  /* Days from 2000-01-01 to start of t->year */
  uint32_t days = 0;
  for (uint16_t y = 2000; y < t->year; y++) {
    days += is_leap_year(y) ? 366U : 365U;
  }
  /* Days in completed months this year */
  for (uint8_t m = 1; m < t->month; m++) {
    days += days_in_month(m, t->year);
  }
  days += (uint32_t)(t->day - 1U);

  return days * 86400U + (uint32_t)t->hour * 3600U + (uint32_t)t->minute * 60U +
         (uint32_t)t->second;
}

uint32_t watch_time_diff_seconds(const watch_time_t *a, const watch_time_t *b) {
  if (a == NULL || b == NULL) {
    return 0;
  }
  uint32_t ea = to_epoch_seconds(a);
  uint32_t eb = to_epoch_seconds(b);
  return (ea >= eb) ? (ea - eb) : (eb - ea);
}

bool watch_time_is_after(const watch_time_t *a, const watch_time_t *b) {
  if (a == NULL || b == NULL) {
    return false;
  }
  return to_epoch_seconds(a) > to_epoch_seconds(b);
}

watch_sync_result_t watch_time_apply_sync(watch_time_t *current,
                                          const watch_time_t *incoming) {
  if (current == NULL || incoming == NULL) {
    return WATCH_SYNC_INVALID;
  }
  if (!watch_time_is_valid(incoming)) {
    return WATCH_SYNC_INVALID;
  }
  /*
   * Reject if incoming is more than WATCH_TIME_MAX_BACKWARDS_JUMP_S behind
   * current. A small backwards jump (e.g. NTP correction) is allowed.
   */
  if (watch_time_is_after(current, incoming)) {
    uint32_t delta = watch_time_diff_seconds(current, incoming);
    if (delta > WATCH_TIME_MAX_BACKWARDS_JUMP_S) {
      return WATCH_SYNC_STALE;
    }
  }
  *current = *incoming;
  return WATCH_SYNC_OK;
}

/* ── Weekday ────────────────────────────────────────────────────────────────
 */

void watch_time_compute_weekday(watch_time_t *t) {
  // 1. Safety check for NULL and valid month range
  if (t == NULL || t->month < 1 || t->month > 12) {
    return;
  }

  /*
   * Tomohiko Sakamoto's algorithm.
   * Returns 0 = Sunday, 1 = Monday, … 6 = Saturday.
   */
  static const int adj[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  int y = (int)t->year;
  int m = (int)t->month;
  int d = (int)t->day;

  if (m < 3) {
    y--;
  }

  // 2. The index 'm - 1' is now guaranteed to be 0-11
  int wd = (y + y / 4 - y / 100 + y / 400 + adj[m - 1] + d) % 7;
  t->weekday = (uint8_t)((wd + 7) % 7); /* ensure non-negative */
}

const char *watch_time_weekday_abbr(uint8_t weekday) {
  static const char *abbr[7] = {"Sun", "Mon", "Tue", "Wed",
                                "Thu", "Fri", "Sat"};
  if (weekday > 6) {
    return "???";
  }
  return abbr[weekday];
}

const char *watch_time_month_abbr(uint8_t month) {
  static const char *abbr[13] = {"???", "Jan", "Feb", "Mar", "Apr",
                                 "May", "Jun", "Jul", "Aug", "Sep",
                                 "Oct", "Nov", "Dec"};
  if (month < 1 || month > 12) {
    return abbr[0];
  }
  return abbr[month];
}

/* ── Formatting ─────────────────────────────────────────────────────────────
 */

void watch_time_fmt_hhmm(const watch_time_t *t, char *buf, uint8_t len) {
  if (t == NULL || buf == NULL || len < 6) {
    return;
  }
  snprintf(buf, len, "%02u:%02u", t->hour, t->minute);
}

void watch_time_fmt_hhmmss(const watch_time_t *t, char *buf, uint8_t len) {
  if (t == NULL || buf == NULL || len < 9) {
    return;
  }
  snprintf(buf, len, "%02u:%02u:%02u", t->hour, t->minute, t->second);
}

void watch_time_fmt_date(const watch_time_t *t, char *buf, uint8_t len) {
  if (t == NULL || buf == NULL || len < 12) {
    return;
  }
  snprintf(buf, len, "%s %02u %s", watch_time_weekday_abbr(t->weekday), t->day,
           watch_time_month_abbr(t->month));
}

/* ── Alarm ──────────────────────────────────────────────────────────────────
 */

bool watch_alarm_should_fire(const watch_alarm_t *alarm,
                             const watch_time_t *t) {
  if (alarm == NULL || t == NULL) {
    return false;
  }
  if (!alarm->enabled) {
    return false;
  }
  if (alarm->dow_mask == 0) {
    return false;
  }
  if (!(alarm->dow_mask & (1U << t->weekday))) {
    return false;
  }
  return (alarm->hour == t->hour) && (alarm->minute == t->minute) &&
         (t->second == 0);
}

uint32_t watch_alarm_seconds_until(const watch_alarm_t *alarm,
                                   const watch_time_t *now) {
  if (alarm == NULL || now == NULL) {
    return UINT32_MAX;
  }
  if (!alarm->enabled || alarm->dow_mask == 0) {
    return UINT32_MAX;
  }

  /* Current time as seconds-since-midnight */
  uint32_t now_secs = (uint32_t)now->hour * 3600U +
                      (uint32_t)now->minute * 60U + (uint32_t)now->second;
  uint32_t alarm_secs =
      (uint32_t)alarm->hour * 3600U + (uint32_t)alarm->minute * 60U;

  /* Search forward up to 7 days (inclusive) to handle "already passed today" */
  for (uint8_t days_ahead = 0; days_ahead <= 7; days_ahead++) {
    uint8_t candidate_dow = (uint8_t)((now->weekday + days_ahead) % 7);
    if (!(alarm->dow_mask & (1U << candidate_dow))) {
      continue;
    }
    uint32_t candidate_offset = (uint32_t)days_ahead * 86400U + alarm_secs;
    if (candidate_offset > now_secs) {
      return candidate_offset - now_secs;
    }
  }

  return UINT32_MAX;
}
