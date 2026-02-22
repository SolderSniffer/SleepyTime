/*
 * SleepyTime — wakeup_pcf8563.h
 *
 * PCF8563 implementation of wakeup_timer_iface_t.
 *
 * This is a placeholder backend used to validate compile-time backend
 * selection and wiring. Replace with real I2C alarm programming and
 * /INT wake integration when implementing PCF8563 support.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WAKEUP_PCF8563_H
#define WAKEUP_PCF8563_H

#include "power_service.h"

const wakeup_timer_iface_t *wakeup_pcf8563_iface(void);

#endif /* WAKEUP_PCF8563_H */
