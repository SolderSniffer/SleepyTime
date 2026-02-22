/*
 * SleepyTime — bmi270_service.h
 *
 * BMI270 accelerometer backend implementing motion_iface_t.
 *
 * This is a placeholder backend used to validate compile-time backend
 * selection and wiring. Replace with real BMI270 interrupt/wakeup logic
 * when implementing sensor support.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BMI270_SERVICE_H
#define BMI270_SERVICE_H

#include "lis3dh_service.h"

void bmi270_service_init(void);
const motion_iface_t *bmi270_service_iface(void);

#endif /* BMI270_SERVICE_H */
