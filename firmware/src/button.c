/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#include "button.h"
#include "pins.h"
#include "config.h"

#include <avr/io.h>

/* ------------------------------------------------------------------------- */
/* Internal state                                                            */
/* ------------------------------------------------------------------------- */

static bool raw_state = false;
static bool stable_state = false;
static bool last_stable_state = false;

static bool pressed_event = false;
static bool released_event = false;
static bool long_press_event = false;

static uint32_t last_change_time = 0U;
static uint32_t press_start_time = 0U;
static bool long_press_already_fired = false;

/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

static bool button_read_hw(void)
{
    /* Button pressed = LOW because of pull-up */
    return (BUTTON_PINREG & (1U << BUTTON_BIT)) == 0U;
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

void button_init(void)
{
    bool hw;

    /* Configure as input */
    BUTTON_DDR &= (uint8_t)~(1U << BUTTON_BIT);

    /* Enable pull-up */
    BUTTON_PORT |= (uint8_t)(1U << BUTTON_BIT);

    hw = button_read_hw();

    raw_state = hw;
    stable_state = hw;
    last_stable_state = hw;

    pressed_event = false;
    released_event = false;
    long_press_event = false;

    press_start_time = 0U;
    last_change_time = 0U;

    /*
     * If init happens while the button is already held, do not allow that
     * existing hold to become a new long-press event.
     */
    long_press_already_fired = hw;
}

/*
 * Update debounce state and edge/long-press events.
 * Must be called regularly from the main loop with a monotonic timestamp.
 */
void button_update(uint32_t now_ms)
{
    bool hw = button_read_hw();

    pressed_event = false;
    released_event = false;

    /* Detect raw change */
    if (hw != raw_state) {
        raw_state = hw;
        last_change_time = now_ms;
    }

    /* Debounce filter */
    if ((now_ms - last_change_time) >= BUTTON_DEBOUNCE_MS) {
        stable_state = raw_state;
    }

    /* Edge detection */
    if (stable_state && !last_stable_state) {
        pressed_event = true;
        press_start_time = now_ms;
        long_press_already_fired = false;
    }

    if (!stable_state && last_stable_state) {
        released_event = true;
    }

    /* Long press */
    if (stable_state && !long_press_already_fired) {
        if ((now_ms - press_start_time) >= BUTTON_HOLD_TIME_MS) {
            long_press_event = true;
            long_press_already_fired = true;
        }
    }

    last_stable_state = stable_state;
}

bool button_down(void)
{
    return stable_state;
}

bool button_pressed(void)
{
    return pressed_event;
}

bool button_released(void)
{
    return released_event;
}

bool button_long(void)
{
    bool fired = long_press_event;
    long_press_event = false;
    return fired;
}
