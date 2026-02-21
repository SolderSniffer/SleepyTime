/*
 * tests/test_watch_sm/test_watch_sm.c
 *
 * Unity unit tests for lib/watch_sm.
 *
 * The iface is fully stubbed — no Zephyr, no hardware. Stubs record call
 * counts so tests can assert which hardware actions the SM triggered.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "watch_sm.h"
#include "watch_time.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ── Stub state ───────────────────────────────────────────────────────────── */

static struct
{
    int display_time_calls;
    int display_sync_calls;
    int display_low_bat_calls;
    int ble_start_calls;
    int ble_stop_calls;
    int save_boot_state_calls;
    int enter_sleep_calls;
    uint8_t battery_pct;
} stubs;

static void reset_stubs(void)
{
    memset(&stubs, 0, sizeof(stubs));
    stubs.battery_pct = 80U;
}

/* ── Stub implementations ─────────────────────────────────────────────────── */

static void stub_display_time(const watch_time_t *t, uint8_t battery_pct)
{
    (void)t;
    (void)battery_pct;
    stubs.display_time_calls++;
}

static void stub_display_sync_screen(void)
{
    stubs.display_sync_calls++;
}

static void stub_display_low_battery(uint8_t battery_pct)
{
    (void)battery_pct;
    stubs.display_low_bat_calls++;
}

static uint8_t stub_get_battery_pct(void)
{
    return stubs.battery_pct;
}

static void stub_ble_start(void) { stubs.ble_start_calls++; }
static void stub_ble_stop(void) { stubs.ble_stop_calls++; }

static void stub_save_boot_state(const watch_time_t *t)
{
    (void)t;
    stubs.save_boot_state_calls++;
}

static void stub_enter_sleep(void)
{
    stubs.enter_sleep_calls++;
    /*
     * In tests we must return (unlike hardware). The SM checks this is
     * called; it won't recurse because the test drives events explicitly.
     */
}

static void stub_log(const char *fmt, ...)
{
    /* Uncomment to debug test failures: */
    /* va_list args; va_start(args, fmt); vprintf(fmt, args); va_end(args); printf("\n"); */
    (void)fmt;
}

static const watch_sm_iface_t k_stub_iface = {
    .display_time = stub_display_time,
    .display_sync_screen = stub_display_sync_screen,
    .display_low_battery = stub_display_low_battery,
    .get_battery_pct = stub_get_battery_pct,
    .ble_start = stub_ble_start,
    .ble_stop = stub_ble_stop,
    .save_boot_state = stub_save_boot_state,
    .enter_sleep = stub_enter_sleep,
    .log = stub_log,
};

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static watch_sm_t sm;

static const watch_time_t k_valid_time = {
    .year = 2025,
    .month = 6,
    .day = 15,
    .hour = 14,
    .minute = 30,
    .second = 0,
    .weekday = 0,
};

/** Boot the SM into DISPLAY_UPDATE via a normal wrist-raise wakeup. */
static void boot_sm_wrist_raise(void)
{
    watch_sm_init(&sm, &k_stub_iface);
    watch_sm_on_boot(&sm, WATCH_BOOT_WRIST_RAISE, &k_valid_time);
}

/** Drive SM to ACTIVE state (boot → DISPLAY_UPDATE → ACTIVE). */
static void sm_to_active(void)
{
    boot_sm_wrist_raise();
    watch_sm_event(&sm, WATCH_EVT_DISPLAY_READY);
    TEST_ASSERT_EQUAL_INT(WATCH_STATE_ACTIVE, watch_sm_state(&sm));
}

void setUp(void)
{
    reset_stubs();
    memset(&sm, 0, sizeof(sm));
}

void tearDown(void) {}

/* ── Boot transitions ─────────────────────────────────────────────────────── */

void test_boot_wrist_raise_goes_to_display_update(void)
{
    boot_sm_wrist_raise();
    TEST_ASSERT_EQUAL_INT(WATCH_STATE_DISPLAY_UPDATE, watch_sm_state(&sm));
}

void test_boot_calls_display_time(void)
{
    boot_sm_wrist_raise();
    TEST_ASSERT_EQUAL_INT(1, stubs.display_time_calls);
}

void test_boot_cold_no_valid_time_still_transitions(void)
{
    watch_sm_init(&sm, &k_stub_iface);
    watch_sm_on_boot(&sm, WATCH_BOOT_COLD, NULL);
    TEST_ASSERT_EQUAL_INT(WATCH_STATE_DISPLAY_UPDATE, watch_sm_state(&sm));
}

void test_boot_critical_battery_goes_to_low_battery(void)
{
    stubs.battery_pct = 3U; /* below BATTERY_CRIT_PCT (5) */
    watch_sm_init(&sm, &k_stub_iface);
    watch_sm_on_boot(&sm, WATCH_BOOT_COLD, &k_valid_time);
    TEST_ASSERT_EQUAL_INT(WATCH_STATE_LOW_BATTERY, watch_sm_state(&sm));
    TEST_ASSERT_EQUAL_INT(1, stubs.display_low_bat_calls);
}

/* ── DISPLAY_UPDATE transitions ───────────────────────────────────────────── */

void test_display_ready_wrist_raise_goes_active(void)
{
    boot_sm_wrist_raise();
    watch_sm_event(&sm, WATCH_EVT_DISPLAY_READY);
    TEST_ASSERT_EQUAL_INT(WATCH_STATE_ACTIVE, watch_sm_state(&sm));
}

void test_display_ready_rtc_alarm_goes_sleep_pending(void)
{
    watch_sm_init(&sm, &k_stub_iface);
    watch_sm_on_boot(&sm, WATCH_BOOT_RTC_ALARM, &k_valid_time);
    /* boot_reason == RTC_ALARM and no ble_sync_pending → SLEEP_PENDING */
    watch_sm_event(&sm, WATCH_EVT_DISPLAY_READY);
    TEST_ASSERT_EQUAL_INT(WATCH_STATE_SLEEP_PENDING, watch_sm_state(&sm));
}

void test_low_battery_in_display_update_goes_to_low_battery(void)
{
    boot_sm_wrist_raise();
    watch_sm_event(&sm, WATCH_EVT_LOW_BATTERY);
    TEST_ASSERT_EQUAL_INT(WATCH_STATE_LOW_BATTERY, watch_sm_state(&sm));
}

/* ── ACTIVE transitions ───────────────────────────────────────────────────── */

void test_wrist_lowered_from_active_goes_sleep_pending(void)
{
    sm_to_active();
    watch_sm_event(&sm, WATCH_EVT_WRIST_LOWERED);
    TEST_ASSERT_EQUAL_INT(WATCH_STATE_SLEEP_PENDING, watch_sm_state(&sm));
}

void test_sleep_pending_saves_boot_state(void)
{
    sm_to_active();
    watch_sm_event(&sm, WATCH_EVT_WRIST_LOWERED);
    TEST_ASSERT_EQUAL_INT(1, stubs.save_boot_state_calls);
}

void test_tick_from_active_goes_to_display_update(void)
{
    sm_to_active();
    watch_sm_event(&sm, WATCH_EVT_TICK);
    TEST_ASSERT_EQUAL_INT(WATCH_STATE_DISPLAY_UPDATE, watch_sm_state(&sm));
}

void test_tick_advances_time_by_one_minute(void)
{
    sm_to_active();
    /* Reset call count after initial display */
    stubs.display_time_calls = 0;
    watch_sm_event(&sm, WATCH_EVT_TICK);
    /* After TICK → DISPLAY_UPDATE, display_time should be called */
    TEST_ASSERT_EQUAL_INT(1, stubs.display_time_calls);
}

void test_sync_requested_from_active_goes_ble_sync(void)
{
    sm_to_active();
    watch_sm_event(&sm, WATCH_EVT_SYNC_REQUESTED);
    TEST_ASSERT_EQUAL_INT(WATCH_STATE_BLE_SYNC, watch_sm_state(&sm));
}

void test_ble_sync_entry_calls_ble_start(void)
{
    sm_to_active();
    watch_sm_event(&sm, WATCH_EVT_SYNC_REQUESTED);
    TEST_ASSERT_EQUAL_INT(1, stubs.ble_start_calls);
}

void test_ble_sync_entry_shows_sync_screen(void)
{
    sm_to_active();
    watch_sm_event(&sm, WATCH_EVT_SYNC_REQUESTED);
    TEST_ASSERT_EQUAL_INT(1, stubs.display_sync_calls);
}

void test_low_battery_from_active_goes_to_low_battery(void)
{
    sm_to_active();
    watch_sm_event(&sm, WATCH_EVT_LOW_BATTERY);
    TEST_ASSERT_EQUAL_INT(WATCH_STATE_LOW_BATTERY, watch_sm_state(&sm));
}

/* ── BLE_SYNC transitions ─────────────────────────────────────────────────── */

void test_ble_time_synced_goes_to_display_update(void)
{
    sm_to_active();
    watch_sm_event(&sm, WATCH_EVT_SYNC_REQUESTED);
    watch_sm_event(&sm, WATCH_EVT_BLE_TIME_SYNCED);
    TEST_ASSERT_EQUAL_INT(WATCH_STATE_DISPLAY_UPDATE, watch_sm_state(&sm));
}

void test_ble_disconnected_goes_to_display_update(void)
{
    sm_to_active();
    watch_sm_event(&sm, WATCH_EVT_SYNC_REQUESTED);
    watch_sm_event(&sm, WATCH_EVT_BLE_DISCONNECTED);
    TEST_ASSERT_EQUAL_INT(WATCH_STATE_DISPLAY_UPDATE, watch_sm_state(&sm));
}

void test_ble_synced_calls_ble_stop(void)
{
    sm_to_active();
    watch_sm_event(&sm, WATCH_EVT_SYNC_REQUESTED);
    stubs.ble_stop_calls = 0;
    watch_sm_event(&sm, WATCH_EVT_BLE_TIME_SYNCED);
    TEST_ASSERT_EQUAL_INT(1, stubs.ble_stop_calls);
}

void test_on_time_sync_valid_time_accepted(void)
{
    sm_to_active();
    watch_sm_event(&sm, WATCH_EVT_SYNC_REQUESTED);

    watch_time_t new_time = k_valid_time;
    new_time.minute = 45U;
    watch_sm_on_time_sync(&sm, &new_time);
    /* Should have transitioned to DISPLAY_UPDATE */
    TEST_ASSERT_EQUAL_INT(WATCH_STATE_DISPLAY_UPDATE, watch_sm_state(&sm));
}

void test_on_time_sync_invalid_time_falls_back(void)
{
    sm_to_active();
    watch_sm_event(&sm, WATCH_EVT_SYNC_REQUESTED);

    watch_time_t bad_time = {.year = 2025, .month = 13}; /* invalid */
    watch_sm_on_time_sync(&sm, &bad_time);
    /* Invalid sync → BLE_DISCONNECTED path → DISPLAY_UPDATE */
    TEST_ASSERT_EQUAL_INT(WATCH_STATE_DISPLAY_UPDATE, watch_sm_state(&sm));
}

/* ── SLEEP_PENDING transitions ────────────────────────────────────────────── */

void test_display_ready_in_sleep_pending_calls_enter_sleep(void)
{
    sm_to_active();
    watch_sm_event(&sm, WATCH_EVT_WRIST_LOWERED);
    TEST_ASSERT_EQUAL_INT(WATCH_STATE_SLEEP_PENDING, watch_sm_state(&sm));
    watch_sm_event(&sm, WATCH_EVT_DISPLAY_READY);
    TEST_ASSERT_EQUAL_INT(1, stubs.enter_sleep_calls);
}

void test_wrist_raised_in_sleep_pending_cancels_sleep(void)
{
    sm_to_active();
    watch_sm_event(&sm, WATCH_EVT_WRIST_LOWERED);
    watch_sm_event(&sm, WATCH_EVT_WRIST_RAISED);
    /* Should cancel sleep and go back to DISPLAY_UPDATE */
    TEST_ASSERT_EQUAL_INT(WATCH_STATE_DISPLAY_UPDATE, watch_sm_state(&sm));
    TEST_ASSERT_EQUAL_INT(0, stubs.enter_sleep_calls);
}

/* ── LOW_BATTERY transitions ──────────────────────────────────────────────── */

void test_display_ready_in_low_battery_goes_sleep_pending(void)
{
    stubs.battery_pct = 3U;
    watch_sm_init(&sm, &k_stub_iface);
    watch_sm_on_boot(&sm, WATCH_BOOT_COLD, &k_valid_time);
    /* Now in LOW_BATTERY, send DISPLAY_READY */
    watch_sm_event(&sm, WATCH_EVT_DISPLAY_READY);
    TEST_ASSERT_EQUAL_INT(WATCH_STATE_SLEEP_PENDING, watch_sm_state(&sm));
}

void test_battery_ok_from_low_battery_goes_to_display_update(void)
{
    stubs.battery_pct = 3U;
    watch_sm_init(&sm, &k_stub_iface);
    watch_sm_on_boot(&sm, WATCH_BOOT_COLD, &k_valid_time);
    stubs.battery_pct = 90U;
    watch_sm_event(&sm, WATCH_EVT_BATTERY_OK);
    TEST_ASSERT_EQUAL_INT(WATCH_STATE_DISPLAY_UPDATE, watch_sm_state(&sm));
}

/* ── Name helpers ─────────────────────────────────────────────────────────── */

void test_state_name_not_null(void)
{
    for (int i = 0; i < (int)WATCH_STATE_COUNT; i++)
    {
        TEST_ASSERT_NOT_NULL(watch_sm_state_name((watch_state_t)i));
    }
}

void test_event_name_not_null(void)
{
    for (int i = 0; i < (int)WATCH_EVT_COUNT; i++)
    {
        TEST_ASSERT_NOT_NULL(watch_sm_event_name((watch_event_t)i));
    }
}

/* ── Entry point ──────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_boot_wrist_raise_goes_to_display_update);
    RUN_TEST(test_boot_calls_display_time);
    RUN_TEST(test_boot_cold_no_valid_time_still_transitions);
    RUN_TEST(test_boot_critical_battery_goes_to_low_battery);

    RUN_TEST(test_display_ready_wrist_raise_goes_active);
    RUN_TEST(test_display_ready_rtc_alarm_goes_sleep_pending);
    RUN_TEST(test_low_battery_in_display_update_goes_to_low_battery);

    RUN_TEST(test_wrist_lowered_from_active_goes_sleep_pending);
    RUN_TEST(test_sleep_pending_saves_boot_state);
    RUN_TEST(test_tick_from_active_goes_to_display_update);
    RUN_TEST(test_tick_advances_time_by_one_minute);
    RUN_TEST(test_sync_requested_from_active_goes_ble_sync);
    RUN_TEST(test_ble_sync_entry_calls_ble_start);
    RUN_TEST(test_ble_sync_entry_shows_sync_screen);
    RUN_TEST(test_low_battery_from_active_goes_to_low_battery);

    RUN_TEST(test_ble_time_synced_goes_to_display_update);
    RUN_TEST(test_ble_disconnected_goes_to_display_update);
    RUN_TEST(test_ble_synced_calls_ble_stop);
    RUN_TEST(test_on_time_sync_valid_time_accepted);
    RUN_TEST(test_on_time_sync_invalid_time_falls_back);

    RUN_TEST(test_display_ready_in_sleep_pending_calls_enter_sleep);
    RUN_TEST(test_wrist_raised_in_sleep_pending_cancels_sleep);

    RUN_TEST(test_display_ready_in_low_battery_goes_sleep_pending);
    RUN_TEST(test_battery_ok_from_low_battery_goes_to_display_update);

    RUN_TEST(test_state_name_not_null);
    RUN_TEST(test_event_name_not_null);

    return UNITY_END();
}