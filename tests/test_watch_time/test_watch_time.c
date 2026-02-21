/*
 * tests/test_watch_time/test_watch_time.c
 *
 * Unity unit tests for lib/watch_time.
 * Updated to include leap year, alarm, and safety edge cases.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "watch_time.h"
#include <stddef.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── Validation ───────────────────────────────────────────────────────────── */

void test_valid_time_passes(void)
{
    watch_time_t t = {
        .year = 2025, .month = 6, .day = 15, .hour = 14, .minute = 30, .second = 0, .weekday = 0};
    TEST_ASSERT_TRUE(watch_time_is_valid(&t));
}

void test_null_is_invalid(void)
{
    TEST_ASSERT_FALSE(watch_time_is_valid(NULL));
}

void test_month_zero_is_invalid(void)
{
    watch_time_t t = {.year = 2025, .month = 0, .day = 1, .hour = 0, .minute = 0, .second = 0};
    TEST_ASSERT_FALSE(watch_time_is_valid(&t));
}

void test_feb_29_leap_year_valid(void)
{
    watch_time_t t = {.year = 2024, .month = 2, .day = 29, .hour = 0, .minute = 0, .second = 0};
    TEST_ASSERT_TRUE(watch_time_is_valid(&t));
}

void test_feb_29_non_leap_year_invalid(void)
{
    watch_time_t t = {.year = 2025, .month = 2, .day = 29, .hour = 0, .minute = 0, .second = 0};
    TEST_ASSERT_FALSE(watch_time_is_valid(&t));
}

/* ── Arithmetic & Rollovers ───────────────────────────────────────────────── */

void test_add_seconds_basic(void)
{
    watch_time_t t = {.year = 2025, .month = 1, .day = 1, .hour = 0, .minute = 0, .second = 0};
    watch_time_add_seconds(&t, 90);
    TEST_ASSERT_EQUAL_UINT8(1, t.minute);
    TEST_ASSERT_EQUAL_UINT8(30, t.second);
}

void test_add_seconds_leap_day_rollover(void)
{
    // 2024 is a leap year; Feb 28 + 1 day should be Feb 29
    watch_time_t t = {.year = 2024, .month = 2, .day = 28, .hour = 23, .minute = 59, .second = 59};
    watch_time_add_seconds(&t, 1);
    TEST_ASSERT_EQUAL_UINT8(2, t.month);
    TEST_ASSERT_EQUAL_UINT8(29, t.day);
}

void test_add_seconds_year_rollover(void)
{
    watch_time_t t = {.year = 2025, .month = 12, .day = 31, .hour = 23, .minute = 59, .second = 59};
    watch_time_add_seconds(&t, 1);
    TEST_ASSERT_EQUAL_UINT16(2026, t.year);
    TEST_ASSERT_EQUAL_UINT8(1, t.month);
    TEST_ASSERT_EQUAL_UINT8(1, t.day);
}

/* ── Weekday ──────────────────────────────────────────────────────────────── */

void test_weekday_known_date(void)
{
    /* 2025-01-01 is a Wednesday = 3 */
    watch_time_t t = {.year = 2025, .month = 1, .day = 1};
    watch_time_compute_weekday(&t);
    TEST_ASSERT_EQUAL_UINT8(3, t.weekday);
}

void test_compute_weekday_invalid_month_safety(void)
{
    watch_time_t t = {.year = 2025, .month = 0, .day = 1};
    // Should return early due to our new guard and not crash/OOB
    watch_time_compute_weekday(&t);
    TEST_PASS();
}

/* ── Formatting ───────────────────────────────────────────────────────────── */

void test_fmt_hhmm(void)
{
    watch_time_t t = {.hour = 9, .minute = 5};
    char buf[8];
    watch_time_fmt_hhmm(&t, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("09:05", buf);
}

void test_fmt_hhmm_small_buffer_safety(void)
{
    watch_time_t t = {.hour = 12, .minute = 34};
    char buf[4] = "ABC";
    // Function requires len >= 6. It should return without writing.
    watch_time_fmt_hhmm(&t, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("ABC", buf);
}

/* ── Alarms ───────────────────────────────────────────────────────────────── */

void test_alarm_fires_on_matching_time(void)
{
    watch_alarm_t alarm = {.hour = 7, .minute = 30, .dow_mask = 0x7F, .enabled = true};
    watch_time_t t = {.hour = 7, .minute = 30, .second = 0, .weekday = 1};
    TEST_ASSERT_TRUE(watch_alarm_should_fire(&alarm, &t));
}

void test_alarm_seconds_until_later_today(void)
{
    watch_alarm_t alarm = {.hour = 10, .minute = 0, .dow_mask = 0x7F, .enabled = true};
    watch_time_t now = {.hour = 9, .minute = 0, .second = 0, .weekday = 1};
    // 1 hour = 3600 seconds
    TEST_ASSERT_EQUAL_UINT32(3600, watch_alarm_seconds_until(&alarm, &now));
}

void test_alarm_seconds_until_next_week(void)
{
    // Current time is 09:00:00. Alarm was 08:00:00 today.
    // Since it's passed, it should find next week's occurrence.
    watch_alarm_t alarm = {.hour = 8, .minute = 0, .dow_mask = (1U << 1), .enabled = true};
    watch_time_t now = {.hour = 9, .minute = 0, .second = 0, .weekday = 1};
    // 7 days - 1 hour = 601,200 seconds
    TEST_ASSERT_EQUAL_UINT32(601200, watch_alarm_seconds_until(&alarm, &now));
}

/* ── Entry point ──────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_valid_time_passes);
    RUN_TEST(test_null_is_invalid);
    RUN_TEST(test_month_zero_is_invalid);
    RUN_TEST(test_feb_29_leap_year_valid);
    RUN_TEST(test_feb_29_non_leap_year_invalid);

    RUN_TEST(test_add_seconds_basic);
    RUN_TEST(test_add_seconds_leap_day_rollover);
    RUN_TEST(test_add_seconds_year_rollover);

    RUN_TEST(test_weekday_known_date);
    RUN_TEST(test_compute_weekday_invalid_month_safety);

    RUN_TEST(test_fmt_hhmm);
    RUN_TEST(test_fmt_hhmm_small_buffer_safety);

    RUN_TEST(test_alarm_fires_on_matching_time);
    RUN_TEST(test_alarm_seconds_until_later_today);
    RUN_TEST(test_alarm_seconds_until_next_week);

    return UNITY_END();
}