/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#include "touch.h"
#include "adc.h"
#include "pins.h"
#include "timer.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * Capacitive touch detection (threshold-based)
 * --------------------------------------------
 *
 * Functions test_adc_touch_range_calibration or test_adc_band_finder from
 * the tests module can be used to find a proper touch threshold.
 * For testing hold the board in your hand, touch the pad front and back,
 * best between index finger and thumb, briefly, release.
 * (if you think it's kinda obvious how to touch a touchpad, think again, there
 *  has been some confusion ;))
 * Currently the board is not too sensitive, due to the nature of the board,
 * we don't want it to fire at random grabs, only when we truly mean to.
 *
 * Current values with good results:
 *
 * Signal difference between idle and touch:
 *
 *   idle  ~ 50–100
 *   touch ~ 500+
 *
 * Detection:
 *   if (value > TOUCH_THRESHOLD) -> touch
 *
 * Release:
 *   if (value < TOUCH_RELEASE_THRESHOLD) -> unlock
 *
 * Hysteresis is supposed to avoid chatter around the threshold.
 */

/* ------------------------------------------------------------------------- */
/* Touch tuning                                                              */
/* ------------------------------------------------------------------------- */

#define TOUCH_THRESHOLD           300U
#define TOUCH_RELEASE_THRESHOLD   200U

#define TOUCH_HOLD_TIME_MS        6000UL

/* number of samples for simple averaging */
#define SAMPLE_COUNT              4U

/* ------------------------------------------------------------------------- */
/* Internal state                                                            */
/* ------------------------------------------------------------------------- */

static bool triggered = false;
static bool locked = false;

static bool touching = false;
static bool long_triggered = false;
static bool long_fired = false;
static uint32_t touch_start_ms = 0;

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

void touch_init(void)
{
    triggered = false;
    locked = false;
    touching = false;
    long_triggered = false;
    long_fired = false;
    touch_start_ms = 0U;
}

void touch_update(void)
{
    uint16_t v = adc_read_avg(TOUCH_ADC_CHANNEL, SAMPLE_COUNT);
    uint32_t now_ms = millis();

    if (!locked && v > TOUCH_THRESHOLD) {
        locked = true;

        touching = true;
        long_triggered = false;
        long_fired = false;
        touch_start_ms = now_ms;
    }

    if (touching && !long_fired &&
        ((now_ms - touch_start_ms) >= TOUCH_HOLD_TIME_MS)) {
        long_triggered = true;
        long_fired = true;
    }

    /*
     * Release once value drops sufficiently below threshold.
     * A short touch is only emitted on release, otherwise it would steal
     * the gesture before it can ever become a long touch.
     */
    if (locked && v < TOUCH_RELEASE_THRESHOLD) {
        if (!long_fired) {
            triggered = true;
        }

        locked = false;

        touching = false;
        long_triggered = false;
        long_fired = false;
        touch_start_ms = 0U;
    }
}

bool touch_triggered(void)
{
    if (triggered) {
        triggered = false;
        return true;
    }

    return false;
}

bool touch_long(void)
{
    if (long_triggered) {
        long_triggered = false;
        return true;
    }

    return false;
}
