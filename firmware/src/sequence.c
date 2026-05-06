/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#include "sequence.h"

#include <avr/eeprom.h>
#include <stdbool.h>
#include <stdint.h>

#include "animations.h"
#include "button.h"
#include "charlieplex.h"

#define SEQUENCE_EEPROM_ADDR_FLAG      16U
#define SEQUENCE_EEPROM_ADDR_MESSAGE   17U
#define SEQUENCE_EEPROM_FLAG           0x56U

#define SEQUENCE_MESSAGE_LEN           17U
#define SEQUENCE_MESSAGE_KEY           0x2BU

#define SEQUENCE_GROUP_COUNT           6U
#define SEQUENCE_SYMBOL_COUNT          3U

#define SEQUENCE_DOUBLE_PRESS_MS       350UL
#define SEQUENCE_CURSOR_BLINK_MS       300UL
#define SEQUENCE_MULTIPLEX_STEP_MS     2UL
#define SEQUENCE_TIMEOUT_MS            30000UL

#define SEQUENCE_ACCEPT_BLINK_COUNT    3U
#define SEQUENCE_ACCEPT_ON_MS          180UL
#define SEQUENCE_ACCEPT_OFF_MS         140UL

static const uint8_t group_leds[SEQUENCE_GROUP_COUNT][4] = {
    {  1U, 11U, 12U, 13U },
    { 14U, 15U, 16U,  0U },
    { 17U, 18U, 19U,  0U },
    { 20U, 10U,  9U,  8U },
    {  7U,  6U,  5U,  0U },
    {  4U,  3U,  2U,  0U }
};

static const uint8_t group_len[SEQUENCE_GROUP_COUNT] = {
    4U, 3U, 3U, 4U, 3U, 3U
};

static const uint8_t required_symbols[SEQUENCE_SYMBOL_COUNT] = {
    (uint8_t)((1U << 5) | (1U << 4)),
    (uint8_t)((1U << 4) | (1U << 3) | (1U << 2) | (1U << 1)),
    (uint8_t)((1U << 0) | (1U << 1) | (1U << 2))
};

static bool active = false;

static uint8_t cursor_group = 0;
static uint8_t current_symbol = 0;
static uint8_t symbol_index = 0;

static bool pending_single_press = false;
static uint32_t pending_press_ms = 0;
static uint32_t last_activity_ms = 0;

static uint32_t last_multiplex_ms = 0;
static uint8_t scan_group = 0;
static uint8_t scan_led = 0;

static bool accept_feedback = false;
static bool accept_leds_on = false;
static uint8_t accepted_symbol = 0;
static uint8_t accept_blink_count = 0;
static uint32_t accept_phase_ms = 0;

static void sequence_check_current_symbol(uint32_t now_ms);

static void sequence_write_secret(void)
{
    static const uint8_t message[SEQUENCE_MESSAGE_LEN] = {
        0x4EU, 0x46U, 0x49U, 0x59U, 0x4AU, 0x48U, 0x4EU, 0x0BU,
        0x5FU, 0x43U, 0x4EU, 0x0BU, 0x5DU, 0x44U, 0x42U, 0x4FU,
        0x2BU
    };

    if (eeprom_read_byte((const uint8_t *)SEQUENCE_EEPROM_ADDR_FLAG) ==
        SEQUENCE_EEPROM_FLAG) {
        return;
    }

    for (uint8_t i = 0U; i < SEQUENCE_MESSAGE_LEN; i++) {
        eeprom_update_byte((uint8_t *)(SEQUENCE_EEPROM_ADDR_MESSAGE + i),
                           (uint8_t)(message[i] ^ SEQUENCE_MESSAGE_KEY));
    }

    eeprom_update_byte((uint8_t *)SEQUENCE_EEPROM_ADDR_FLAG, SEQUENCE_EEPROM_FLAG);
}

static void sequence_reset_scan(void)
{
    scan_group = 0U;
    scan_led = 0U;
}

static void sequence_reset_input(void)
{
    cursor_group = 0U;
    current_symbol = 0U;
    symbol_index = 0U;

    pending_single_press = false;
    pending_press_ms = 0U;

    accept_feedback = false;
    accept_leds_on = false;
    accepted_symbol = 0U;
    accept_blink_count = 0U;
    accept_phase_ms = 0U;

    sequence_reset_scan();
}

static void sequence_exit(void)
{
    active = false;
    pending_single_press = false;
    accept_feedback = false;
    charlieplex_all_off();
}

static void sequence_advance_cursor(void)
{
    cursor_group++;

    if (cursor_group >= SEQUENCE_GROUP_COUNT) {
        cursor_group = 0U;
    }

    sequence_reset_scan();
}

static void sequence_toggle_current_group(uint32_t now_ms)
{
    current_symbol ^= (uint8_t)(1U << cursor_group);
    sequence_reset_scan();

    sequence_check_current_symbol(now_ms);
}

static void sequence_advance_scan(void)
{
    scan_led++;

    if (scan_led >= group_len[scan_group]) {
        scan_led = 0U;
        scan_group++;

        if (scan_group >= SEQUENCE_GROUP_COUNT) {
            scan_group = 0U;
        }
    }
}

static void sequence_show_symbol_step(uint8_t symbol)
{
    for (uint8_t tries = 0U; tries < 20U; tries++) {
        uint8_t led;

        if ((symbol & (uint8_t)(1U << scan_group)) == 0U) {
            sequence_advance_scan();
            continue;
        }

        led = group_leds[scan_group][scan_led];
        sequence_advance_scan();

        if (led > 0U) {
            charlieplex_led_on((uint8_t)(led - 1U));
            return;
        }
    }

    charlieplex_all_off();
}

static bool sequence_group_visible(uint8_t group, uint32_t now_ms)
{
    bool cursor_visible;
    uint8_t group_mask = (uint8_t)(1U << group);

    if (current_symbol & group_mask) {
        return true;
    }

    cursor_visible = ((now_ms / SEQUENCE_CURSOR_BLINK_MS) & 1U) == 0U;

    return (group == cursor_group) && cursor_visible;
}

static void sequence_display_step(uint32_t now_ms)
{
    for (uint8_t tries = 0U; tries < 20U; tries++) {
        uint8_t led;

        if (!sequence_group_visible(scan_group, now_ms)) {
            sequence_advance_scan();
            continue;
        }

        led = group_leds[scan_group][scan_led];
        sequence_advance_scan();

        if (led > 0U) {
            charlieplex_led_on((uint8_t)(led - 1U));
            return;
        }
    }

    charlieplex_all_off();
}

static void sequence_start_accept_feedback(uint8_t symbol, uint32_t now_ms)
{
    accepted_symbol = symbol;
    accept_feedback = true;
    accept_leds_on = true;
    accept_blink_count = 0U;
    accept_phase_ms = now_ms;

    pending_single_press = false;
    current_symbol = 0U;
    cursor_group = 0U;

    sequence_reset_scan();
}

static void sequence_update_accept_feedback(uint32_t now_ms)
{
    uint32_t phase_len;

    phase_len = accept_leds_on ? SEQUENCE_ACCEPT_ON_MS : SEQUENCE_ACCEPT_OFF_MS;

    if ((now_ms - accept_phase_ms) >= phase_len) {
        accept_phase_ms = now_ms;

        if (accept_leds_on) {
            accept_leds_on = false;
            accept_blink_count++;

            if (accept_blink_count >= SEQUENCE_ACCEPT_BLINK_COUNT) {
                accept_feedback = false;
                accepted_symbol = 0U;
                cursor_group = 0U;
                current_symbol = 0U;
                sequence_reset_scan();
                charlieplex_all_off();
                return;
            }
        } else {
            accept_leds_on = true;
        }
    }

    if (accept_leds_on) {
        sequence_show_symbol_step(accepted_symbol);
    } else {
        charlieplex_all_off();
    }
}

static void sequence_check_current_symbol(uint32_t now_ms)
{
    if (current_symbol != required_symbols[symbol_index]) {
        return;
    }

    symbol_index++;

    if (symbol_index >= SEQUENCE_SYMBOL_COUNT) {
        sequence_write_secret();

        animations_play_nat20();

        sequence_exit();
        return;
    }

    sequence_start_accept_feedback(current_symbol, now_ms);
}

void sequence_init(void)
{
    active = false;
    sequence_reset_input();
}

void sequence_enter(void)
{
    active = true;
    sequence_reset_input();

    last_activity_ms = 0U;
    last_multiplex_ms = 0U;

    charlieplex_all_off();
}

void sequence_update(uint32_t now_ms)
{
    if (!active) {
        return;
    }

    if (last_activity_ms == 0U) {
        last_activity_ms = now_ms;
    }

    if (accept_feedback) {
        if ((now_ms - last_multiplex_ms) >= SEQUENCE_MULTIPLEX_STEP_MS) {
            last_multiplex_ms = now_ms;
            sequence_update_accept_feedback(now_ms);
        }

        return;
    }

    if ((now_ms - last_activity_ms) >= SEQUENCE_TIMEOUT_MS) {
        sequence_exit();
        return;
    }

    if ((now_ms - last_multiplex_ms) >= SEQUENCE_MULTIPLEX_STEP_MS) {
        last_multiplex_ms = now_ms;
        sequence_display_step(now_ms);
    }

    if (button_long()) {
        sequence_exit();
        return;
    }

    if (button_pressed()) {
        last_activity_ms = now_ms;

        if (pending_single_press &&
            ((now_ms - pending_press_ms) <= SEQUENCE_DOUBLE_PRESS_MS)) {
            pending_single_press = false;
            sequence_toggle_current_group(now_ms);
            return;
        }

        pending_single_press = true;
        pending_press_ms = now_ms;
    }

    if (pending_single_press &&
        ((now_ms - pending_press_ms) > SEQUENCE_DOUBLE_PRESS_MS)) {
        pending_single_press = false;
        sequence_advance_cursor();
    }
}

bool sequence_active(void)
{
    return active;
}
