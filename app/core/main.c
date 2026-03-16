/*
 * main.c — nRF54L15 BLE CTS Timekeeping with E-Paper Display
 *
 * Flow:
 *  1. Boot → check bond storage
 *  2. No bond → advertise → pair → save bond
 *  3. Connect → read CTS → store anchor (unix_anchor_s + grtc_anchor_ticks)
 *  4. Disconnect → race to System On sleep (all SRAM retained)
 *  5. Wake every 60 s via RTC alarm → update e-paper display → sleep again
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/settings/settings.h>
#include <zephyr/pm/pm.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/services/cts_client.h>
#include <bluetooth/gatt_dm.h>
#include <nrfx_grtc.h>
#include <lvgl.h>

/* ─────────────────────────────────────────────
 *  GRTC helpers
 * ───────────────────────────────────────────── */
/** Return raw 64-bit GRTC ticks (1 MHz on nRF54L15). */
static inline uint64_t grtc_ticks_get(void)
{
    return nrfx_grtc_syscounter_get();
}

#define GRTC_HZ  1000000ULL   /* 1 MHz GRTC clock on nRF54L15 */

/* ─────────────────────────────────────────────
 *  Time anchor  (retained across System On sleep because it is in SRAM)
 * ───────────────────────────────────────────── */
typedef struct {
    uint64_t unix_s;        /* UTC seconds at the moment of CTS read  */
    uint64_t grtc_ticks;    /* GRTC ticks at the same moment           */
    bool     valid;
} time_anchor_t;

static time_anchor_t g_anchor;   /* zero-init on first POR; kept in SRAM */

/** Return current UTC epoch seconds derived from anchor + elapsed GRTC ticks. */
static uint64_t current_unix_s(void)
{
    if (!g_anchor.valid) {
        return 0;
    }
    uint64_t now_ticks = grtc_ticks_get();
    uint64_t elapsed_s = (now_ticks - g_anchor.grtc_ticks) / GRTC_HZ;
    return g_anchor.unix_s + elapsed_s;
}

/* ─────────────────────────────────────────────
 *  Calendar helpers
 * ───────────────────────────────────────────── */
static const char *const DAY_NAMES[] = {
    "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"
};
static const uint8_t DAYS_IN_MONTH[] = {
    31,28,31,30,31,30,31,31,30,31,30,31
};

static bool is_leap(uint32_t y)
{
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

typedef struct {
    uint16_t year;
    uint8_t  month;   /* 1-12  */
    uint8_t  day;     /* 1-31  */
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint8_t  dow;     /* 0=Sun */
} datetime_t;

static void unix_to_datetime(uint64_t unix_s, datetime_t *dt)
{
    uint32_t sec  = (uint32_t)(unix_s % 60);
    uint32_t min  = (uint32_t)((unix_s / 60) % 60);
    uint32_t hour = (uint32_t)((unix_s / 3600) % 24);
    uint32_t days = (uint32_t)(unix_s / 86400);

    /* Day of week: 1970-01-01 was a Thursday (dow=4) */
    dt->dow    = (uint8_t)((days + 4) % 7);
    dt->hour   = (uint8_t)hour;
    dt->minute = (uint8_t)min;
    dt->second = (uint8_t)sec;

    uint32_t y = 1970;
    while (1) {
        uint32_t days_in_year = is_leap(y) ? 366 : 365;
        if (days < days_in_year) break;
        days -= days_in_year;
        y++;
    }
    dt->year = (uint16_t)y;

    uint8_t m = 1;
    while (1) {
        uint32_t dim = DAYS_IN_MONTH[m - 1];
        if (m == 2 && is_leap(y)) dim = 29;
        if (days < dim) break;
        days -= dim;
        m++;
    }
    dt->month = m;
    dt->day   = (uint8_t)(days + 1);
}

/* ─────────────────────────────────────────────
 *  Display / LVGL
 * ───────────────────────────────────────────── */
static lv_obj_t *time_label;

static void display_init(void)
{
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display_dev)) {
        printk("[DISPLAY] Device not ready — halting.\n");
        return;
    }
    //lv_init();
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    time_label = lv_label_create(scr);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
    lv_obj_center(time_label);
    lv_label_set_text(time_label, "--:--");
    lv_task_handler();   /* initial render */
}

static void display_update(uint8_t hour, uint8_t minute)
{
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);
    lv_label_set_text(time_label, buf);
    lv_task_handler();   /* blocks until e-paper refresh completes */
}

/**
 * Snapshot the current time from the GRTC anchor BEFORE calling into LVGL.
 * The blocking e-paper refresh (up to ~2 s) therefore has no effect on the
 * displayed time — it reflects the instant of wake, not the instant refresh
 * completes.
 */
static void display_update_from_anchor(void)
{
    if (!g_anchor.valid) {
        printk("[TIME] Anchor not yet set — waiting for CTS.\n");
        return;
    }

    /* Snapshot time before the blocking e-paper refresh */
    uint64_t unix_s = current_unix_s();
    datetime_t dt;
    unix_to_datetime(unix_s, &dt);

    printk("[TIME] %s  %02u/%02u/%04u  %02u:%02u:%02u\n",
           DAY_NAMES[dt.dow],
           dt.day, dt.month, dt.year,
           dt.hour, dt.minute, dt.second);

    display_update(dt.hour, dt.minute);   /* blocking refresh after snapshot */
}

/* ─────────────────────────────────────────────
 *  BLE / Bond state
 * ───────────────────────────────────────────── */
static bool g_bonded           = false;
static bool g_cts_read_done    = false;
static struct bt_conn *g_conn  = NULL;

/* Semaphore: main thread waits here while BLE work happens */
static K_SEM_DEFINE(sem_ble_done, 0, 1);

/* ─────────────────────────────────────────────
 *  Bond check via settings callback
 * ───────────────────────────────────────────── */
static uint8_t bond_count;

static void bond_count_cb(const struct bt_bond_info *info, void *user_data)
{
    bond_count++;
}

static bool has_bonds(void)
{
    bond_count = 0;
    bt_foreach_bond(BT_ID_DEFAULT, bond_count_cb, NULL);
    return bond_count > 0;
}

/* ─────────────────────────────────────────────
 *  CTS client
 * ───────────────────────────────────────────── */
static struct bt_cts_client g_cts_c;

static void cts_read_cb(struct bt_cts_client *cts_c,
                        struct bt_cts_current_time *current_time,
                        int err)
{
    if (err) {
        printk("[CTS] Read error: %d\n", err);
        return;
    }

    struct bt_cts_exact_time_256 *t = &current_time->exact_time_256;

    uint32_t days = 0;
    for (uint16_t y = 1970; y < t->year; y++) {
        days += is_leap(y) ? 366 : 365;
    }
    for (uint8_t m = 1; m < t->month; m++) {
        uint8_t dim = DAYS_IN_MONTH[m - 1];
        if (m == 2 && is_leap(t->year)) dim = 29;
        days += dim;
    }
    days += t->day - 1;

    uint64_t unix_s = (uint64_t)days * 86400
                    + (uint64_t)t->hours * 3600
                    + (uint64_t)t->minutes * 60
                    + t->seconds;

    /* Capture GRTC at (approximately) the same instant */
    uint64_t grtc_now = grtc_ticks_get();

    g_anchor.unix_s      = unix_s;
    g_anchor.grtc_ticks  = grtc_now;
    g_anchor.valid       = true;

    printk("[CTS] Time anchored: unix=%llu  grtc=%llu\n",
           (unsigned long long)unix_s,
           (unsigned long long)grtc_now);

    g_cts_read_done = true;

    if (g_conn) {
        bt_conn_disconnect(g_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
}

/* ─────────────────────────────────────────────
 *  GATT Discovery Manager callbacks
 * ───────────────────────────────────────────── */
static void discovery_completed_cb(struct bt_gatt_dm *dm, void *ctx)
{
    ARG_UNUSED(ctx);
    int err = bt_cts_handles_assign(dm, &g_cts_c);
    if (err) {
        printk("[CTS] handles_assign failed: %d\n", err);
        bt_gatt_dm_data_release(dm);
        return;
    }
    printk("[CTS] Handles assigned, reading time...\n");
    bt_cts_read_current_time(&g_cts_c, cts_read_cb);
    bt_gatt_dm_data_release(dm);
}

static void discovery_service_not_found_cb(struct bt_conn *conn, void *ctx)
{
    ARG_UNUSED(ctx);
    printk("[CTS] CTS service not found on peer.\n");
}

static void discovery_error_cb(struct bt_conn *conn, int err, void *ctx)
{
    ARG_UNUSED(ctx);
    printk("[CTS] GATT discovery error: %d\n", err);
}

static const struct bt_gatt_dm_cb dm_cb = {
    .completed         = discovery_completed_cb,
    .service_not_found = discovery_service_not_found_cb,
    .error_found       = discovery_error_cb,
};

/* ─────────────────────────────────────────────
 *  BLE connection callbacks
 * ───────────────────────────────────────────── */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        printk("[BLE] Connection failed: %u\n", err);
        return;
    }
    g_conn = bt_conn_ref(conn);
    printk("[BLE] Connected.\n");

    bt_cts_client_init(&g_cts_c);

    int dm_err = bt_gatt_dm_start(conn, BT_UUID_CTS, &dm_cb, NULL);
    if (dm_err) {
        printk("[CTS] bt_gatt_dm_start failed: %d\n", dm_err);
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    printk("[BLE] Disconnected (reason 0x%02x).\n", reason);
    if (g_conn) {
        bt_conn_unref(g_conn);
        g_conn = NULL;
    }
    k_sem_give(&sem_ble_done);
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err)
{
    if (err) {
        printk("[BLE] Security change failed: %d\n", err);
        return;
    }
    printk("[BLE] Security level: %u\n", (unsigned)level);
}

static void identity_resolved(struct bt_conn *conn,
                              const bt_addr_le_t *rpa,
                              const bt_addr_le_t *identity)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(identity, addr, sizeof(addr));
    printk("[BLE] Identity resolved: %s\n", addr);
}

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
    printk("[BLE] Pairing complete, bonded=%d\n", (int)bonded);
    g_bonded = bonded;
}

static void pairing_failed(struct bt_conn *conn,
                           enum bt_security_err reason)
{
    printk("[BLE] Pairing failed: %d\n", reason);
}

static struct bt_conn_cb conn_cb = {
    .connected          = connected,
    .disconnected       = disconnected,
    .security_changed   = security_changed,
    .identity_resolved  = identity_resolved,
};

static struct bt_conn_auth_info_cb auth_info_cb = {
    .pairing_complete = pairing_complete,
    .pairing_failed   = pairing_failed,
};

/* ─────────────────────────────────────────────
 *  Advertising
 * ───────────────────────────────────────────── */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL,
                  BT_UUID_16_ENCODE(BT_UUID_CTS_VAL)),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void start_advertising(void)
{
    int err = bt_le_adv_start(
                  BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN,
                                  BT_GAP_ADV_FAST_INT_MIN_2,
                                  BT_GAP_ADV_FAST_INT_MAX_2,
                                  NULL),
                  ad, ARRAY_SIZE(ad),
                  sd, ARRAY_SIZE(sd));
    if (err) {
        printk("[BLE] Advertising start failed: %d\n", err);
    } else {
        printk("[BLE] Advertising started.\n");
    }
}

/* ─────────────────────────────────────────────
 *  Periodic wake timer (60 s)
 * ───────────────────────────────────────────── */
#define WAKE_INTERVAL_MS  (60 * 1000)

static void wake_timer_cb(struct k_timer *timer_id);
K_TIMER_DEFINE(wake_timer, wake_timer_cb, NULL);
static K_SEM_DEFINE(sem_wake, 0, 1);

static void wake_timer_cb(struct k_timer *timer_id)
{
    k_sem_give(&sem_wake);
}

/* ─────────────────────────────────────────────
 *  main
 * ───────────────────────────────────────────── */
int main(void)
{
    int err;

    printk("=== nRF54L15 CTS Timekeeping ===\n");

    /* 1. Initialise BT + load persistent settings (bonds) */
    err = bt_enable(NULL);
    if (err) {
        printk("bt_enable failed: %d\n", err);
        return err;
    }

    err = settings_load();
    if (err) {
        printk("settings_load failed: %d\n", err);
        /* non-fatal */
    }

    bt_conn_cb_register(&conn_cb);
    bt_conn_auth_info_cb_register(&auth_info_cb);

    /* 2. Advertise and get CTS anchor, unless SRAM already holds a valid one
     *    (warm wake from System On sleep).
     */
    if (!g_anchor.valid) {
        if (has_bonds()) {
            printk("[BOND] Bond found — re-advertising for CTS refresh.\n");
        } else {
            printk("[BOND] No bonds — advertising for CTS (pairing optional).\n");
        }

        start_advertising();

        /* Block until cts_read_cb disconnects us, or peer disconnects */
        k_sem_take(&sem_ble_done, K_FOREVER);
        bt_le_adv_stop();

        if (!g_cts_read_done) {
            printk("[CTS] CTS read did not complete — rebooting.\n");
            sys_reboot(SYS_REBOOT_COLD);
        }

        if (g_bonded) {
            printk("[BOND] Bond saved.\n");
        }
    } else {
        printk("[TIME] Anchor valid from previous session — skipping BLE.\n");
    }

    /* 3. Initialise display now that we have (or retained) a valid anchor.
     *    Done after BLE sync to avoid any interaction between the e-paper
     *    SPI bus and BLE radio activity during the initial pairing window.
     */
    display_init();

    /* 4. Update display immediately with the anchored time */
    display_update_from_anchor();

    /* 5. Start periodic timer aligned to the next whole minute boundary.
     *
     *    Without this the timer fires at e.g. :12 past every minute because
     *    the anchor happened to be set at :12.  By using the remaining seconds
     *    in the current minute as the one-shot first delay, every subsequent
     *    expiry lands on :00.
     */
    uint64_t unix_s_now       = current_unix_s();
    uint32_t secs_past_minute = (uint32_t)(unix_s_now % 60);
    uint32_t ms_to_next_min   = (60 - secs_past_minute) * 1000;

    /* If we are already within the last second of a minute, skip ahead a
     * full minute so the timer does not fire almost immediately.          */
    if (ms_to_next_min < 1000) {
        ms_to_next_min += 60000;
    }

    printk("[TIME] First update in %u ms (aligning to minute boundary).\n",
           ms_to_next_min);

    k_timer_start(&wake_timer,
                  K_MSEC(ms_to_next_min),   /* one-shot delay to :00 */
                  K_MSEC(WAKE_INTERVAL_MS)); /* then every 60 s       */

    while (1) {
        /*
         * Race to System On sleep.
         *
         * k_sem_take() with K_FOREVER lets Zephyr's PM subsystem enter the
         * deepest idle state while all SRAM is retained (System On).
         * The GRTC keeps running during System On sleep on the nRF54L15, so
         * our tick-based anchor arithmetic remains accurate across sleep/wake
         * cycles without any additional calibration.
         *
         * The 60 s k_timer expiry posts sem_wake from its ISR, waking this
         * thread. We snapshot time and push to the display before yielding
         * back so PM can return the SoC to sleep.
         */
        k_sem_take(&sem_wake, K_FOREVER);
        display_update_from_anchor();
        k_yield();
    }

    /* Unreachable */
    return 0;
}