/*
 * SleepyTime — epd_canvas.c
 *
 * Software framebuffer and layout engine for the 1.54" 200 × 200 e-paper
 * display. No hardware dependencies — pure C, host-testable.
 *
 * Font: 5 × 7 pixel bitmap font (printable ASCII 0x20–0x7E).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "epd_canvas.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* ── 5×7 bitmap font ────────────────────────────────────────────────────────
 */
/*
 * Each glyph is 5 bytes, one byte per column, bits 0–6 are rows top→bottom.
 * Index 0 = ASCII 0x20 (space).
 */
static const uint8_t font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, /* 0x20   */
    {0x00, 0x00, 0x5F, 0x00, 0x00}, /* 0x21 ! */
    {0x00, 0x07, 0x00, 0x07, 0x00}, /* 0x22 " */
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, /* 0x23 # */
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, /* 0x24 $ */
    {0x23, 0x13, 0x08, 0x64, 0x62}, /* 0x25 % */
    {0x36, 0x49, 0x55, 0x22, 0x50}, /* 0x26 & */
    {0x00, 0x05, 0x03, 0x00, 0x00}, /* 0x27 ' */
    {0x00, 0x1C, 0x22, 0x41, 0x00}, /* 0x28 ( */
    {0x00, 0x41, 0x22, 0x1C, 0x00}, /* 0x29 ) */
    {0x08, 0x2A, 0x1C, 0x2A, 0x08}, /* 0x2A * */
    {0x08, 0x08, 0x3E, 0x08, 0x08}, /* 0x2B + */
    {0x00, 0x50, 0x30, 0x00, 0x00}, /* 0x2C , */
    {0x08, 0x08, 0x08, 0x08, 0x08}, /* 0x2D - */
    {0x00, 0x60, 0x60, 0x00, 0x00}, /* 0x2E . */
    {0x20, 0x10, 0x08, 0x04, 0x02}, /* 0x2F / */
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, /* 0x30 0 */
    {0x00, 0x42, 0x7F, 0x40, 0x00}, /* 0x31 1 */
    {0x42, 0x61, 0x51, 0x49, 0x46}, /* 0x32 2 */
    {0x21, 0x41, 0x45, 0x4B, 0x31}, /* 0x33 3 */
    {0x18, 0x14, 0x12, 0x7F, 0x10}, /* 0x34 4 */
    {0x27, 0x45, 0x45, 0x45, 0x39}, /* 0x35 5 */
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, /* 0x36 6 */
    {0x01, 0x71, 0x09, 0x05, 0x03}, /* 0x37 7 */
    {0x36, 0x49, 0x49, 0x49, 0x36}, /* 0x38 8 */
    {0x06, 0x49, 0x49, 0x29, 0x1E}, /* 0x39 9 */
    {0x00, 0x36, 0x36, 0x00, 0x00}, /* 0x3A : */
    {0x00, 0x56, 0x36, 0x00, 0x00}, /* 0x3B ; */
    {0x00, 0x08, 0x14, 0x22, 0x41}, /* 0x3C < */
    {0x14, 0x14, 0x14, 0x14, 0x14}, /* 0x3D = */
    {0x41, 0x22, 0x14, 0x08, 0x00}, /* 0x3E > */
    {0x02, 0x01, 0x51, 0x09, 0x06}, /* 0x3F ? */
    {0x32, 0x49, 0x79, 0x41, 0x3E}, /* 0x40 @ */
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, /* 0x41 A */
    {0x7F, 0x49, 0x49, 0x49, 0x36}, /* 0x42 B */
    {0x3E, 0x41, 0x41, 0x41, 0x22}, /* 0x43 C */
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, /* 0x44 D */
    {0x7F, 0x49, 0x49, 0x49, 0x41}, /* 0x45 E */
    {0x7F, 0x09, 0x09, 0x01, 0x01}, /* 0x46 F */
    {0x3E, 0x41, 0x41, 0x51, 0x32}, /* 0x47 G */
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, /* 0x48 H */
    {0x00, 0x41, 0x7F, 0x41, 0x00}, /* 0x49 I */
    {0x20, 0x40, 0x41, 0x3F, 0x01}, /* 0x4A J */
    {0x7F, 0x08, 0x14, 0x22, 0x41}, /* 0x4B K */
    {0x7F, 0x40, 0x40, 0x40, 0x40}, /* 0x4C L */
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, /* 0x4D M */
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, /* 0x4E N */
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, /* 0x4F O */
    {0x7F, 0x09, 0x09, 0x09, 0x06}, /* 0x50 P */
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, /* 0x51 Q */
    {0x7F, 0x09, 0x19, 0x29, 0x46}, /* 0x52 R */
    {0x46, 0x49, 0x49, 0x49, 0x31}, /* 0x53 S */
    {0x01, 0x01, 0x7F, 0x01, 0x01}, /* 0x54 T */
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, /* 0x55 U */
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, /* 0x56 V */
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, /* 0x57 W */
    {0x63, 0x14, 0x08, 0x14, 0x63}, /* 0x58 X */
    {0x03, 0x04, 0x78, 0x04, 0x03}, /* 0x59 Y */
    {0x61, 0x51, 0x49, 0x45, 0x43}, /* 0x5A Z */
    {0x00, 0x00, 0x7F, 0x41, 0x41}, /* 0x5B [ */
    {0x02, 0x04, 0x08, 0x10, 0x20}, /* 0x5C \ */
    {0x41, 0x41, 0x7F, 0x00, 0x00}, /* 0x5D ] */
    {0x04, 0x02, 0x01, 0x02, 0x04}, /* 0x5E ^ */
    {0x40, 0x40, 0x40, 0x40, 0x40}, /* 0x5F _ */
    {0x00, 0x01, 0x02, 0x04, 0x00}, /* 0x60 ` */
    {0x20, 0x54, 0x54, 0x54, 0x78}, /* 0x61 a */
    {0x7F, 0x48, 0x44, 0x44, 0x38}, /* 0x62 b */
    {0x38, 0x44, 0x44, 0x44, 0x20}, /* 0x63 c */
    {0x38, 0x44, 0x44, 0x48, 0x7F}, /* 0x64 d */
    {0x38, 0x54, 0x54, 0x54, 0x18}, /* 0x65 e */
    {0x08, 0x7E, 0x09, 0x01, 0x02}, /* 0x66 f */
    {0x08, 0x14, 0x54, 0x54, 0x3C}, /* 0x67 g */
    {0x7F, 0x08, 0x04, 0x04, 0x78}, /* 0x68 h */
    {0x00, 0x44, 0x7D, 0x40, 0x00}, /* 0x69 i */
    {0x20, 0x40, 0x44, 0x3D, 0x00}, /* 0x6A j */
    {0x00, 0x7F, 0x10, 0x28, 0x44}, /* 0x6B k */
    {0x00, 0x41, 0x7F, 0x40, 0x00}, /* 0x6C l */
    {0x7C, 0x04, 0x18, 0x04, 0x78}, /* 0x6D m */
    {0x7C, 0x08, 0x04, 0x04, 0x78}, /* 0x6E n */
    {0x38, 0x44, 0x44, 0x44, 0x38}, /* 0x6F o */
    {0x7C, 0x14, 0x14, 0x14, 0x08}, /* 0x70 p */
    {0x08, 0x14, 0x14, 0x18, 0x7C}, /* 0x71 q */
    {0x7C, 0x08, 0x04, 0x04, 0x08}, /* 0x72 r */
    {0x48, 0x54, 0x54, 0x54, 0x20}, /* 0x73 s */
    {0x04, 0x3F, 0x44, 0x40, 0x20}, /* 0x74 t */
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, /* 0x75 u */
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, /* 0x76 v */
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, /* 0x77 w */
    {0x44, 0x28, 0x10, 0x28, 0x44}, /* 0x78 x */
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, /* 0x79 y */
    {0x44, 0x64, 0x54, 0x4C, 0x44}, /* 0x7A z */
    {0x00, 0x08, 0x36, 0x41, 0x00}, /* 0x7B { */
    {0x00, 0x00, 0x7F, 0x00, 0x00}, /* 0x7C | */
    {0x00, 0x41, 0x36, 0x08, 0x00}, /* 0x7D } */
    {0x08, 0x08, 0x2A, 0x1C, 0x08}, /* 0x7E ~ */
};

#define FONT_FIRST_CHAR 0x20
#define FONT_LAST_CHAR 0x7E
#define FONT_NUM_GLYPHS (FONT_LAST_CHAR - FONT_FIRST_CHAR + 1)

/* ── Internal helpers ───────────────────────────────────────────────────────
 */

static void set_pixel_raw(epd_canvas_t *canvas, uint16_t x, uint16_t y, uint8_t colour) {
  /* Caller guarantees in-bounds. */
  uint32_t byte_idx = (uint32_t)y * EPD_FB_STRIDE + (x / 8U);
  uint8_t bit_mask = (uint8_t)(0x80U >> (x % 8U));

  if (colour == EPD_BLACK) {
    canvas->fb[byte_idx] &= (uint8_t)~bit_mask;
  } else {
    canvas->fb[byte_idx] |= bit_mask;
  }
}

static uint8_t get_pixel_raw(const epd_canvas_t *canvas, uint16_t x, uint16_t y) {
  uint32_t byte_idx = (uint32_t)y * EPD_FB_STRIDE + (x / 8U);
  uint8_t bit_mask = (uint8_t)(0x80U >> (x % 8U));
  return (canvas->fb[byte_idx] & bit_mask) ? EPD_WHITE : EPD_BLACK;
}

/* ── Initialisation ─────────────────────────────────────────────────────────
 */

void epd_canvas_init(epd_canvas_t *canvas, uint8_t fill_colour) {
  if (canvas == NULL) {
    return;
  }
  epd_canvas_clear(canvas, fill_colour);
}

void epd_canvas_clear(epd_canvas_t *canvas, uint8_t colour) {
  if (canvas == NULL) {
    return;
  }
  memset(canvas->fb, (colour == EPD_WHITE) ? 0xFFU : 0x00U, EPD_FB_SIZE);
}

/* ── Raw framebuffer access ─────────────────────────────────────────────────
 */

const uint8_t *epd_canvas_fb(const epd_canvas_t *canvas) {
  if (canvas == NULL) {
    return NULL;
  }
  return canvas->fb;
}

/* ── Pixel primitives ───────────────────────────────────────────────────────
 */

void epd_canvas_set_pixel(epd_canvas_t *canvas, uint16_t x, uint16_t y, uint8_t colour) {
  if (canvas == NULL) {
    return;
  }
  if (x >= EPD_WIDTH_PX || y >= EPD_HEIGHT_PX) {
    return; /* clip */
  }
  set_pixel_raw(canvas, x, y, colour);
}

uint8_t epd_canvas_get_pixel(const epd_canvas_t *canvas, uint16_t x, uint16_t y) {
  if (canvas == NULL) {
    return EPD_WHITE;
  }
  if (x >= EPD_WIDTH_PX || y >= EPD_HEIGHT_PX) {
    return EPD_WHITE; /* out-of-bounds → white */
  }
  return get_pixel_raw(canvas, x, y);
}

/* ── Shape primitives ───────────────────────────────────────────────────────
 */

void epd_canvas_hline(epd_canvas_t *canvas, uint16_t x0, uint16_t x1, uint16_t y, uint8_t colour) {
  if (canvas == NULL) {
    return;
  }
  if (y >= EPD_HEIGHT_PX) {
    return;
  }
  if (x0 > x1) {
    uint16_t tmp = x0;
    x0 = x1;
    x1 = tmp;
  }
  if (x1 >= EPD_WIDTH_PX) {
    x1 = EPD_WIDTH_PX - 1U;
  }
  for (uint16_t x = x0; x <= x1; x++) {
    set_pixel_raw(canvas, x, y, colour);
  }
}

void epd_canvas_vline(epd_canvas_t *canvas, uint16_t x, uint16_t y0, uint16_t y1, uint8_t colour) {
  if (canvas == NULL) {
    return;
  }
  if (x >= EPD_WIDTH_PX) {
    return;
  }
  if (y0 > y1) {
    uint16_t tmp = y0;
    y0 = y1;
    y1 = tmp;
  }
  if (y1 >= EPD_HEIGHT_PX) {
    y1 = EPD_HEIGHT_PX - 1U;
  }
  for (uint16_t y = y0; y <= y1; y++) {
    set_pixel_raw(canvas, x, y, colour);
  }
}

void epd_canvas_fill_rect(epd_canvas_t *canvas, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                          uint8_t colour) {
  if (canvas == NULL || w == 0 || h == 0) {
    return;
  }
  uint16_t x_end = (uint16_t)(x + w - 1U);
  uint16_t y_end = (uint16_t)(y + h - 1U);
  if (x >= EPD_WIDTH_PX || y >= EPD_HEIGHT_PX) {
    return;
  }
  if (x_end >= EPD_WIDTH_PX) {
    x_end = EPD_WIDTH_PX - 1U;
  }
  if (y_end >= EPD_HEIGHT_PX) {
    y_end = EPD_HEIGHT_PX - 1U;
  }

  for (uint16_t row = y; row <= y_end; row++) {
    epd_canvas_hline(canvas, x, x_end, row, colour);
  }
}

void epd_canvas_rect(epd_canvas_t *canvas, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                     uint8_t colour) {
  if (canvas == NULL || w == 0 || h == 0) {
    return;
  }
  epd_canvas_hline(canvas, x, (uint16_t)(x + w - 1U), y, colour);
  epd_canvas_hline(canvas, x, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U), colour);
  epd_canvas_vline(canvas, x, y, (uint16_t)(y + h - 1U), colour);
  epd_canvas_vline(canvas, (uint16_t)(x + w - 1U), y, (uint16_t)(y + h - 1U), colour);
}

/* ── Font / text ────────────────────────────────────────────────────────────
 */

static const uint8_t *glyph_for(char c) {
  unsigned char uc = (unsigned char)c;
  if (uc < FONT_FIRST_CHAR || uc > FONT_LAST_CHAR) {
    uc = (unsigned char)' ';
  }
  return font5x7[uc - FONT_FIRST_CHAR];
}

uint16_t epd_canvas_text_width(const char *str, epd_font_scale_t scale) {
  if (str == NULL || scale == 0) {
    return 0;
  }
  uint16_t len = 0;
  for (const char *p = str; *p != '\0'; p++) {
    len++;
  }
  /* Each glyph is FONT_BASE_W wide + 1 px spacing, minus trailing space */
  if (len == 0) {
    return 0;
  }
  return (uint16_t)(((uint32_t)len * (FONT_BASE_W + 1U) - 1U) * scale);
}

void epd_canvas_text(epd_canvas_t *canvas, uint16_t x, uint16_t y, const char *str,
                     epd_font_scale_t scale, uint8_t fg, uint8_t bg) {
  if (canvas == NULL || str == NULL || scale == 0) {
    return;
  }

  uint16_t cx = x;

  for (const char *p = str; *p != '\0'; p++) {
    const uint8_t *glyph = glyph_for(*p);

    /* Draw scaled glyph column by column */
    for (uint8_t col = 0; col < FONT_BASE_W; col++) {
      uint8_t col_data = glyph[col];
      for (uint8_t row = 0; row < FONT_BASE_H; row++) {
        uint8_t pixel = (col_data & (1U << row)) ? fg : bg;
        for (uint8_t sy = 0; sy < scale; sy++) {
          for (uint8_t sx = 0; sx < scale; sx++) {
            uint16_t px = (uint16_t)(cx + col * scale + sx);
            uint16_t py = (uint16_t)(y + row * scale + sy);
            epd_canvas_set_pixel(canvas, px, py, pixel);
          }
        }
      }
    }
    cx += (uint16_t)((FONT_BASE_W + 1U) * scale);

    if (cx >= EPD_WIDTH_PX) {
      break; /* clip — no wrapping */
    }
  }
}

/* ── Watch face layouts ─────────────────────────────────────────────────────
 */

void epd_canvas_draw_watch_face(epd_canvas_t *canvas, const char *time_str, const char *date_str,
                                uint8_t battery_pct) {
  if (canvas == NULL) {
    return;
  }

  epd_canvas_clear(canvas, EPD_WHITE);

  /* ── Time — large, vertically centred slightly above mid ── */
  uint16_t tw = epd_canvas_text_width(time_str, EPD_FONT_LARGE);
  uint16_t tx = (EPD_WIDTH_PX > tw) ? (EPD_WIDTH_PX - tw) / 2U : 0U;
  epd_canvas_text(canvas, tx, 72U, time_str, EPD_FONT_LARGE, EPD_BLACK, EPD_WHITE);

  /* ── Divider line under time ── */
  epd_canvas_hline(canvas, 20U, 179U, 100U, EPD_BLACK);

  /* ── Date — medium, below divider ── */
  uint16_t dw = epd_canvas_text_width(date_str, EPD_FONT_MEDIUM);
  uint16_t dx = (EPD_WIDTH_PX > dw) ? (EPD_WIDTH_PX - dw) / 2U : 0U;
  epd_canvas_text(canvas, dx, 110U, date_str, EPD_FONT_MEDIUM, EPD_BLACK, EPD_WHITE);

  /* ── Battery percentage — small, bottom-right ── */
  char bat_str[8];
  uint8_t pct = (battery_pct > 100U) ? 100U : battery_pct;
  snprintf(bat_str, sizeof(bat_str), "%u%%", pct);
  uint16_t bw = epd_canvas_text_width(bat_str, EPD_FONT_SMALL);
  uint16_t bx = (EPD_WIDTH_PX > bw + 4U) ? (EPD_WIDTH_PX - bw - 4U) : 0U;
  epd_canvas_text(canvas, bx, 188U, bat_str, EPD_FONT_SMALL, EPD_BLACK, EPD_WHITE);
}

void epd_canvas_draw_sync_screen(epd_canvas_t *canvas) {
  if (canvas == NULL) {
    return;
  }

  epd_canvas_clear(canvas, EPD_WHITE);

  /* Simple BLE icon placeholder: three concentric arcs approximated by rects */
  epd_canvas_fill_rect(canvas, 93U, 93U, 14U, 14U, EPD_BLACK); /* centre dot */
  epd_canvas_rect(canvas, 83U, 83U, 34U, 34U, EPD_BLACK);      /* inner ring */
  epd_canvas_rect(canvas, 73U, 73U, 54U, 54U, EPD_BLACK);      /* outer ring */

  const char *msg = "Syncing...";
  uint16_t mw = epd_canvas_text_width(msg, EPD_FONT_MEDIUM);
  uint16_t mx = (EPD_WIDTH_PX > mw) ? (EPD_WIDTH_PX - mw) / 2U : 0U;
  epd_canvas_text(canvas, mx, 140U, msg, EPD_FONT_MEDIUM, EPD_BLACK, EPD_WHITE);
}

void epd_canvas_draw_low_battery(epd_canvas_t *canvas, uint8_t battery_pct) {
  if (canvas == NULL) {
    return;
  }

  epd_canvas_clear(canvas, EPD_WHITE);

  /* Battery outline */
  epd_canvas_rect(canvas, 70U, 80U, 60U, 30U, EPD_BLACK);
  epd_canvas_fill_rect(canvas, 130U, 88U, 6U, 14U, EPD_BLACK); /* nub */

  /* Fill level — proportional to pct */
  uint8_t pct = (battery_pct > 100U) ? 100U : battery_pct;
  uint16_t fill_w = (uint16_t)((uint32_t)56U * pct / 100U);
  if (fill_w > 0U) {
    epd_canvas_fill_rect(canvas, 72U, 82U, fill_w, 26U, EPD_BLACK);
  }

  const char *msg = "Low Battery";
  uint16_t mw = epd_canvas_text_width(msg, EPD_FONT_MEDIUM);
  uint16_t mx = (EPD_WIDTH_PX > mw) ? (EPD_WIDTH_PX - mw) / 2U : 0U;
  epd_canvas_text(canvas, mx, 125U, msg, EPD_FONT_MEDIUM, EPD_BLACK, EPD_WHITE);

  char pct_str[8];
  snprintf(pct_str, sizeof(pct_str), "%u%%", pct);
  uint16_t pw = epd_canvas_text_width(pct_str, EPD_FONT_SMALL);
  uint16_t px = (EPD_WIDTH_PX > pw) ? (EPD_WIDTH_PX - pw) / 2U : 0U;
  epd_canvas_text(canvas, px, 148U, pct_str, EPD_FONT_SMALL, EPD_BLACK, EPD_WHITE);
}
