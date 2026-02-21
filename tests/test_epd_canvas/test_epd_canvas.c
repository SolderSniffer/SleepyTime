/*
 * tests/test_epd_canvas/test_epd_canvas.c
 *
 * Unity unit tests for lib/epd_canvas.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "epd_canvas.h"

#include <string.h>

static epd_canvas_t canvas;

void setUp(void)
{
    epd_canvas_init(&canvas, EPD_WHITE);
}

void tearDown(void) {}

/* ── Init / clear ─────────────────────────────────────────────────────────── */

void test_init_fills_white(void)
{
    /* After init with EPD_WHITE every pixel should be white */
    for (uint16_t y = 0; y < EPD_HEIGHT_PX; y++)
    {
        for (uint16_t x = 0; x < EPD_WIDTH_PX; x++)
        {
            TEST_ASSERT_EQUAL_UINT8(EPD_WHITE,
                                    epd_canvas_get_pixel(&canvas, x, y));
        }
    }
}

void test_clear_black_fills_black(void)
{
    epd_canvas_clear(&canvas, EPD_BLACK);
    TEST_ASSERT_EQUAL_UINT8(EPD_BLACK, epd_canvas_get_pixel(&canvas, 0, 0));
    TEST_ASSERT_EQUAL_UINT8(EPD_BLACK,
                            epd_canvas_get_pixel(&canvas,
                                                 EPD_WIDTH_PX - 1U,
                                                 EPD_HEIGHT_PX - 1U));
}

/* ── Pixel set / get ──────────────────────────────────────────────────────── */

void test_set_and_get_pixel(void)
{
    epd_canvas_set_pixel(&canvas, 10, 20, EPD_BLACK);
    TEST_ASSERT_EQUAL_UINT8(EPD_BLACK, epd_canvas_get_pixel(&canvas, 10, 20));
    /* Neighbours untouched */
    TEST_ASSERT_EQUAL_UINT8(EPD_WHITE, epd_canvas_get_pixel(&canvas, 11, 20));
    TEST_ASSERT_EQUAL_UINT8(EPD_WHITE, epd_canvas_get_pixel(&canvas, 9, 20));
}

void test_out_of_bounds_set_does_not_crash(void)
{
    /* Should silently clip */
    epd_canvas_set_pixel(&canvas, EPD_WIDTH_PX, 0, EPD_BLACK);
    epd_canvas_set_pixel(&canvas, 0, EPD_HEIGHT_PX, EPD_BLACK);
    epd_canvas_set_pixel(&canvas, 0xFFFF, 0xFFFF, EPD_BLACK);
}

void test_out_of_bounds_get_returns_white(void)
{
    TEST_ASSERT_EQUAL_UINT8(EPD_WHITE,
                            epd_canvas_get_pixel(&canvas, EPD_WIDTH_PX, 0));
}

/* ── Framebuffer encoding ─────────────────────────────────────────────────── */

void test_fb_msb_first_encoding(void)
{
    /*
     * Pixel (0,0) maps to bit 7 of byte 0.
     * Set it black and verify the byte directly.
     */
    epd_canvas_set_pixel(&canvas, 0, 0, EPD_BLACK);
    const uint8_t *fb = epd_canvas_fb(&canvas);
    TEST_ASSERT_EQUAL_UINT8(0x00U, fb[0] & 0x80U); /* bit 7 cleared = black */

    epd_canvas_set_pixel(&canvas, 0, 0, EPD_WHITE);
    fb = epd_canvas_fb(&canvas);
    TEST_ASSERT_EQUAL_UINT8(0x80U, fb[0] & 0x80U); /* bit 7 set = white */
}

void test_stride_is_correct(void)
{
    /* Pixel (0,1) should be in byte EPD_FB_STRIDE, not byte 1 */
    epd_canvas_set_pixel(&canvas, 0, 1, EPD_BLACK);
    const uint8_t *fb = epd_canvas_fb(&canvas);
    /* Row 0 byte 0 should still be white */
    TEST_ASSERT_EQUAL_UINT8(0x80U, fb[0] & 0x80U);
    /* Row 1 byte 0 bit 7 should be black */
    TEST_ASSERT_EQUAL_UINT8(0x00U, fb[EPD_FB_STRIDE] & 0x80U);
}

/* ── Shapes ───────────────────────────────────────────────────────────────── */

void test_hline_sets_pixels(void)
{
    epd_canvas_hline(&canvas, 5, 10, 3, EPD_BLACK);
    for (uint16_t x = 5; x <= 10; x++)
    {
        TEST_ASSERT_EQUAL_UINT8(EPD_BLACK, epd_canvas_get_pixel(&canvas, x, 3));
    }
    TEST_ASSERT_EQUAL_UINT8(EPD_WHITE, epd_canvas_get_pixel(&canvas, 4, 3));
    TEST_ASSERT_EQUAL_UINT8(EPD_WHITE, epd_canvas_get_pixel(&canvas, 11, 3));
}

void test_fill_rect_dimensions(void)
{
    epd_canvas_fill_rect(&canvas, 10, 10, 5, 5, EPD_BLACK);
    /* Inside */
    TEST_ASSERT_EQUAL_UINT8(EPD_BLACK, epd_canvas_get_pixel(&canvas, 10, 10));
    TEST_ASSERT_EQUAL_UINT8(EPD_BLACK, epd_canvas_get_pixel(&canvas, 14, 14));
    /* Outside */
    TEST_ASSERT_EQUAL_UINT8(EPD_WHITE, epd_canvas_get_pixel(&canvas, 15, 10));
    TEST_ASSERT_EQUAL_UINT8(EPD_WHITE, epd_canvas_get_pixel(&canvas, 10, 15));
}

void test_rect_border_only(void)
{
    epd_canvas_rect(&canvas, 20, 20, 10, 10, EPD_BLACK);
    /* Corners */
    TEST_ASSERT_EQUAL_UINT8(EPD_BLACK, epd_canvas_get_pixel(&canvas, 20, 20));
    TEST_ASSERT_EQUAL_UINT8(EPD_BLACK, epd_canvas_get_pixel(&canvas, 29, 29));
    /* Interior should still be white */
    TEST_ASSERT_EQUAL_UINT8(EPD_WHITE, epd_canvas_get_pixel(&canvas, 25, 25));
}

/* ── Text width ───────────────────────────────────────────────────────────── */

void test_text_width_empty_string(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, epd_canvas_text_width("", EPD_FONT_SMALL));
}

void test_text_width_single_char(void)
{
    /* Single char: FONT_BASE_W pixels wide, no trailing space */
    TEST_ASSERT_EQUAL_UINT16(FONT_BASE_W * EPD_FONT_SMALL,
                             epd_canvas_text_width("A", EPD_FONT_SMALL));
}

void test_text_width_scales(void)
{
    uint16_t w1 = epd_canvas_text_width("HI", EPD_FONT_SMALL);
    uint16_t w2 = epd_canvas_text_width("HI", EPD_FONT_MEDIUM);
    TEST_ASSERT_EQUAL_UINT16(w1 * 2U, w2);
}

/* ── Watch face smoke tests ───────────────────────────────────────────────── */

void test_draw_watch_face_does_not_crash(void)
{
    epd_canvas_draw_watch_face(&canvas, "12:34", "Wed 15 Jan", 87U);
    /* Verify canvas is not all-white (some pixels were drawn) */
    bool found_black = false;
    for (uint16_t i = 0; i < EPD_FB_SIZE && !found_black; i++)
    {
        if (epd_canvas_fb(&canvas)[i] != 0xFFU)
        {
            found_black = true;
        }
    }
    TEST_ASSERT_TRUE(found_black);
}

void test_draw_sync_screen_does_not_crash(void)
{
    epd_canvas_draw_sync_screen(&canvas);
}

void test_draw_low_battery_does_not_crash(void)
{
    epd_canvas_draw_low_battery(&canvas, 5U);
}

void test_battery_pct_clamped(void)
{
    /* Should not crash or corrupt memory with out-of-range value */
    epd_canvas_draw_watch_face(&canvas, "00:00", "Sun 01 Jan", 255U);
}

/* ── Entry point ──────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_fills_white);
    RUN_TEST(test_clear_black_fills_black);

    RUN_TEST(test_set_and_get_pixel);
    RUN_TEST(test_out_of_bounds_set_does_not_crash);
    RUN_TEST(test_out_of_bounds_get_returns_white);

    RUN_TEST(test_fb_msb_first_encoding);
    RUN_TEST(test_stride_is_correct);

    RUN_TEST(test_hline_sets_pixels);
    RUN_TEST(test_fill_rect_dimensions);
    RUN_TEST(test_rect_border_only);

    RUN_TEST(test_text_width_empty_string);
    RUN_TEST(test_text_width_single_char);
    RUN_TEST(test_text_width_scales);

    RUN_TEST(test_draw_watch_face_does_not_crash);
    RUN_TEST(test_draw_sync_screen_does_not_crash);
    RUN_TEST(test_draw_low_battery_does_not_crash);
    RUN_TEST(test_battery_pct_clamped);

    return UNITY_END();
}