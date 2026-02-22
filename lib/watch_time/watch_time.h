/*
 * SleepyTime — watch_time.h
 *
 * Pure time domain logic: types, arithmetic, formatting, and alarm scheduling.
 * No Zephyr dependencies — compiles cleanly on host for unit testing.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WATCH_TIME_H
#define WATCH_TIME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Types ──────────────────────────────────────────────────────────────────
 */

/** Calendar time representation. Seconds-resolution, no sub-second state. */
typedef struct {
  uint16_t year;   /**< Full year, e.g. 2025                  */
  uint8_t month;   /**< 1–12                                   */
  uint8_t day;     /**< 1–31                                   */
  uint8_t hour;    /**< 0–23                                   */
  uint8_t minute;  /**< 0–59                                   */
  uint8_t second;  /**< 0–59                                   */
  uint8_t weekday; /**< 0 = Sunday … 6 = Saturday             */
} watch_time_t;

/** A simple HH:MM alarm with day-of-week mask. */
typedef struct {
  uint8_t hour;     /**< 0–23                               */
  uint8_t minute;   /**< 0–59                               */
  uint8_t dow_mask; /**< Bit N set → alarm fires on weekday N (0=Sun) */
  bool enabled;
} watch_alarm_t;

/** Result of a BLE / CTS time-sync attempt. */
typedef enum {
  WATCH_SYNC_OK = 0,  /**< Time accepted and applied          */
  WATCH_SYNC_STALE,   /**< Incoming time older than current   */
  WATCH_SYNC_INVALID, /**< Field out of range                 */
} watch_sync_result_t;

/* ── Validation ─────────────────────────────────────────────────────────────
 */

/**
 * @brief Validate all fields of a watch_time_t.
 * @return true if the struct represents a plausible calendar datetime.
 */
bool watch_time_is_valid(const watch_time_t *t);

/* ── Arithmetic ─────────────────────────────────────────────────────────────
 */

/**
 * @brief Add @p seconds to @p t, rolling over all fields correctly.
 *        Handles month-length and leap-year boundaries.
 */
void watch_time_add_seconds(watch_time_t *t, uint32_t seconds);

/**
 * @brief Compute the absolute difference in seconds between two times.
 *        Returns 0 if @p a and @p b are equal.
 *        Assumes both times are within the same century (no overflow).
 */
uint32_t watch_time_diff_seconds(const watch_time_t *a, const watch_time_t *b);

/**
 * @brief Return true if @p a is strictly later than @p b.
 */
bool watch_time_is_after(const watch_time_t *a, const watch_time_t *b);

/**
 * @brief Attempt to apply an incoming sync time.
 *
 * Copies @p incoming into @p current only if the incoming time passes
 * validation and is not more than WATCH_TIME_MAX_BACKWARDS_JUMP_S seconds
 * behind the current time (guards against stale CTS packets).
 *
 * @param current   Existing time (updated on WATCH_SYNC_OK).
 * @param incoming  Candidate time from BLE / RTC read.
 * @return          Sync result code.
 */
watch_sync_result_t watch_time_apply_sync(watch_time_t *current, const watch_time_t *incoming);

/** Maximum seconds the incoming sync time may lag current before rejection. */
#define WATCH_TIME_MAX_BACKWARDS_JUMP_S (60U)

/* ── Weekday ────────────────────────────────────────────────────────────────
 */

/**
 * @brief Compute the day-of-week for a given date using Tomohiko Sakamoto's
 *        algorithm. Sets t->weekday in place.
 */
void watch_time_compute_weekday(watch_time_t *t);

/**
 * @brief Return a three-character abbreviation for a weekday index (0=Sun).
 *        Returns "???" for out-of-range values.
 */
const char *watch_time_weekday_abbr(uint8_t weekday);

/**
 * @brief Return a three-character month abbreviation (month 1–12).
 *        Returns "???" for out-of-range values.
 */
const char *watch_time_month_abbr(uint8_t month);

/* ── Formatting ─────────────────────────────────────────────────────────────
 */

/**
 * @brief Format time as "HH:MM" into @p buf (needs ≥ 6 bytes).
 */
void watch_time_fmt_hhmm(const watch_time_t *t, char *buf, uint8_t len);

/**
 * @brief Format time as "HH:MM:SS" into @p buf (needs ≥ 9 bytes).
 */
void watch_time_fmt_hhmmss(const watch_time_t *t, char *buf, uint8_t len);

/**
 * @brief Format date as "Www DD Mmm" into @p buf (needs ≥ 12 bytes).
 *        e.g. "Tue 14 Jan"
 */
void watch_time_fmt_date(const watch_time_t *t, char *buf, uint8_t len);

/* ── Alarm ──────────────────────────────────────────────────────────────────
 */

/**
 * @brief Return true if @p alarm should fire at the given time.
 *        Checks enabled flag, hour, minute (second is ignored), and dow_mask.
 */
bool watch_alarm_should_fire(const watch_alarm_t *alarm, const watch_time_t *t);

/**
 * @brief Compute seconds until @p alarm next fires relative to @p now.
 *        Returns UINT32_MAX if alarm is disabled or dow_mask is zero.
 */
uint32_t watch_alarm_seconds_until(const watch_alarm_t *alarm, const watch_time_t *now);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_TIME_H */
