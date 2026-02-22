/*
 * SleepyTime — wakeup_grtc.h
 *
 * GRTC implementation of wakeup_timer_iface_t.
 *
 * The nRF54L15 Global Real-Time Counter (GRTC) stays active in System Off
 * and can wake the SoC after a programmed interval. This is the default
 * wakeup timer for SleepyTime — a periodic 60-second tick that silently
 * refreshes the display.
 *
 * Alternative wakeup timer implementations (e.g. PCF8563 via /INT GPIO)
 * implement the same wakeup_timer_iface_t and are swapped in main.c
 * without any change to power_service.c.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WAKEUP_GRTC_H
#define WAKEUP_GRTC_H

#include "power_service.h"

/**
 * wakeup_grtc_iface() - return the GRTC wakeup_timer_iface_t implementation.
 *
 * main.c passes this to power_service_init() during wiring.
 */
const wakeup_timer_iface_t *wakeup_grtc_iface(void);

#endif /* WAKEUP_GRTC_H */
