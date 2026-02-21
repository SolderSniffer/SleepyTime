/*
 * SleepyTime — epd_canvas.h
 *
 * Software framebuffer and layout engine for the 1.54" e-paper display
 * (200 × 200 px, 1-bit monochrome).
 *
 * This module has zero hardware dependencies — it operates purely on an
 * in-memory pixel buffer and is fully unit-testable on the host.
 *
 * Coordinate system: origin (0, 0) at top-left, x increases right, y down.
 * Pixel value: 0 = black, 1 = white (matches EPD convention).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EPD_CANVAS_H
#define EPD_CANVAS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Display geometry ───────────────────────────────────────────────────────
 */

#define EPD_WIDTH_PX 200U
#define EPD_HEIGHT_PX 200U

/**
 * Framebuffer size in bytes.
 * 1 bit per pixel, packed MSB-first per byte, rows padded to byte boundary.
 * 200 px wide → 25 bytes per row → 200 rows → 5000 bytes total.
 */
#define EPD_FB_STRIDE ((EPD_WIDTH_PX + 7U) / 8U)    /* bytes per row = 25  */
#define EPD_FB_SIZE (EPD_FB_STRIDE * EPD_HEIGHT_PX) /* total bytes = 5000 */

/* ── Canvas context ─────────────────────────────────────────────────────────
 */

/** Opaque canvas state. Embed or allocate by the caller. */
typedef struct {
  uint8_t fb[EPD_FB_SIZE]; /**< Raw 1-bpp framebuffer, MSB-first.         */
} epd_canvas_t;

/* ── Pixel colours ──────────────────────────────────────────────────────────
 */

#define EPD_BLACK 0U
#define EPD_WHITE 1U

/* ── Initialisation ─────────────────────────────────────────────────────────
 */

/**
 * @brief Initialise canvas and fill with @p fill_colour.
 *        Call once before any draw operations.
 */
void epd_canvas_init(epd_canvas_t *canvas, uint8_t fill_colour);

/**
 * @brief Fill entire canvas with @p colour.
 */
void epd_canvas_clear(epd_canvas_t *canvas, uint8_t colour);

/* ── Raw framebuffer access ─────────────────────────────────────────────────
 */

/**
 * @brief Return a const pointer to the raw framebuffer.
 *        Use this to hand the buffer to the SPI EPD driver.
 */
const uint8_t *epd_canvas_fb(const epd_canvas_t *canvas);

/* ── Pixel primitives ───────────────────────────────────────────────────────
 */

/**
 * @brief Set a single pixel. Silently clips out-of-bounds coordinates.
 */
void epd_canvas_set_pixel(epd_canvas_t *canvas, uint16_t x, uint16_t y,
                          uint8_t colour);

/**
 * @brief Read a single pixel. Returns EPD_WHITE for out-of-bounds.
 */
uint8_t epd_canvas_get_pixel(const epd_canvas_t *canvas, uint16_t x,
                             uint16_t y);

/* ── Shape primitives ───────────────────────────────────────────────────────
 */

/**
 * @brief Draw a horizontal line from (x0, y) to (x1, y).
 */
void epd_canvas_hline(epd_canvas_t *canvas, uint16_t x0, uint16_t x1,
                      uint16_t y, uint8_t colour);

/**
 * @brief Draw a vertical line from (x, y0) to (x, y1).
 */
void epd_canvas_vline(epd_canvas_t *canvas, uint16_t x, uint16_t y0,
                      uint16_t y1, uint8_t colour);

/**
 * @brief Draw a filled rectangle.
 */
void epd_canvas_fill_rect(epd_canvas_t *canvas, uint16_t x, uint16_t y,
                          uint16_t w, uint16_t h, uint8_t colour);

/**
 * @brief Draw an unfilled rectangle (1 px border).
 */
void epd_canvas_rect(epd_canvas_t *canvas, uint16_t x, uint16_t y, uint16_t w,
                     uint16_t h, uint8_t colour);

/* ── Font / text ────────────────────────────────────────────────────────────
 */

/**
 * Font scale factor — text is rendered at FONT_SCALE × base glyph size.
 * Supported values: 1 (small), 2 (medium), 3 (large).
 */
typedef uint8_t epd_font_scale_t;

#define EPD_FONT_SMALL ((epd_font_scale_t)1U)
#define EPD_FONT_MEDIUM ((epd_font_scale_t)2U)
#define EPD_FONT_LARGE ((epd_font_scale_t)3U)

/** Base glyph dimensions (5 × 7 pixels, 1 px spacing) */
#define FONT_BASE_W 5U
#define FONT_BASE_H 7U

/**
 * @brief Return the pixel width of a string at a given scale,
 *        including inter-character spacing. Useful for centring.
 */
uint16_t epd_canvas_text_width(const char *str, epd_font_scale_t scale);

/**
 * @brief Draw a null-terminated ASCII string at (x, y).
 *        Non-printable / non-ASCII characters are rendered as a space.
 *        Clips at canvas boundary — does not wrap.
 *
 * @param fg  Foreground (glyph) colour.
 * @param bg  Background colour (fills glyph bounding box).
 */
void epd_canvas_text(epd_canvas_t *canvas, uint16_t x, uint16_t y,
                     const char *str, epd_font_scale_t scale, uint8_t fg,
                     uint8_t bg);

/* ── Watch face layouts ─────────────────────────────────────────────────────
 */

/**
 * @brief Render the primary watch face.
 *
 * Layout (200 × 200 px, white background):
 *   - Large time string "HH:MM"  centred, y ≈ 70
 *   - Medium date string         centred, y ≈ 130
 *   - Small battery % string     bottom-right corner
 *
 * @param time_str   Null-terminated "HH:MM" string.
 * @param date_str   Null-terminated "Www DD Mmm" string.
 * @param battery_pct  0–100. Values > 100 are clamped.
 */
void epd_canvas_draw_watch_face(epd_canvas_t *canvas, const char *time_str,
                                const char *date_str, uint8_t battery_pct);

/**
 * @brief Render a minimal "pairing / BLE sync" screen.
 *
 * Draws a centred BLE icon placeholder and "Syncing…" text.
 */
void epd_canvas_draw_sync_screen(epd_canvas_t *canvas);

/**
 * @brief Render a low-battery warning screen.
 */
void epd_canvas_draw_low_battery(epd_canvas_t *canvas, uint8_t battery_pct);

#ifdef __cplusplus
}
#endif

#endif /* EPD_CANVAS_H */
