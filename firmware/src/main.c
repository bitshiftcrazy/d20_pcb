/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#include <stdbool.h>
#include <stdint.h>
#include <avr/interrupt.h>

#include "adc.h"
#include "animations.h"
#include "attunement.h"
#include "button.h"
#include "charlieplex.h"
#include "pins.h"
#include "roll.h"
#include "timer.h"
#include "touch.h"
#include "sequence.h"

/*
 * Application entry point and top-level state machine.
 *
 * This is the part where the artifact wakes up.
 *
 * The firmware behaves like a small ritual with four phases:
 *
 *   WAIT_FOR_START
 *     - the dice is dormant, it listens, but does not respond
 *     - only an adventurer's decisive button press will wake it
 *
 *   IDLE
 *     - the dice is awake and bound to its bearer
 *     - touch or button may invoke a roll
 *     - a long press re-forges the attunement bond
 *     - if certain conditions are met, it may begin to... misbehave
 *
 *   COOLDOWN
 *     - the dice settles after a roll
 *     - touch is ignored to prevent accidental retriggering
 *     - button input may still provoke it into acting again
 *
 *   SEQUENCE
 *
 * Input model
 * -----------
 * - Button:
 *     - short press → invoke roll / contribute to hidden sequences
 *     - long press  → break and re-establish attunement
 *
 * - Touch:
 *     - event-based trigger
 *     - ignored during cooldown
 *
 * Notes
 * -----
 * - hardware initialization happens here
 * - most animations require concentration from the dice (i.e. blocking rituals)
 * - this file coordinates the threads of the weave, but does not decide fate
 */

/* ------------------------------------------------------------------------- */
/* App state                                                                 */
/* ------------------------------------------------------------------------- */
/*
 * Brief settling period after invoking fate.
 * Button presses bypass this and can reroll immediately.
 */
#define TOUCH_REARM_MS 350UL

typedef enum {
    APP_STATE_WAIT_FOR_START = 0,
    APP_STATE_IDLE,
    APP_STATE_COOLDOWN,
    APP_STATE_SEQUENCE
} app_state_t;

static app_state_t app_state = APP_STATE_WAIT_FOR_START;
static uint32_t cooldown_start_ms = 0UL;

/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

/* Cheap (non-cryptographic ;)) entropy mixing from touch ADC, time, and attunement state */
static uint16_t app_collect_entropy(uint32_t now_ms)
{
    uint16_t entropy = 0U;

    entropy ^= adc_read(TOUCH_ADC_CHANNEL);
    entropy ^= (uint16_t)now_ms;
    entropy ^= (uint16_t)(now_ms >> 16);
    entropy ^= (uint16_t)((uint16_t)attunement_get_value() << 8);
    entropy ^= 0xA55AU;

    return entropy;
}

static void app_enter_cooldown(uint32_t now_ms)
{
    cooldown_start_ms = now_ms;
    app_state = APP_STATE_COOLDOWN;
    charlieplex_all_off();
}

static void app_run_attunement_sequence(void)
{
    /*
     * Re-attunement is a dedicated blocking ritual and requires concentration.
     * During this time, we intentionally ignore button/touch input.
     */
    roll_seed_entropy(app_collect_entropy(millis()));

    (void)attunement_measure_and_store();

    /*
     * Animations derive timing/variation from attunement, so re-init them
     * after a newly measured value is available.
     */
    animations_init();

    /* Feed the newly forged attunement back into the weave. */
    roll_seed_entropy((uint16_t)attunement_get_value() << 8);

    /*
     * Reset touch detector state after the ritual so stale lock state or
     * half-built sampling history does not leak into normal operation.
     */
    touch_init();

    animations_play_attunement();

    button_init();
    touch_init();

    charlieplex_all_off();
    app_enter_cooldown(millis());
}

static void app_start_roll(uint32_t now_ms)
{
    /* reroll until committed, unless long-press triggers into re-attunement */
    while (1) {
        roll_result_t result;
        anim_roll_status_t anim_status;

        roll_seed_entropy(app_collect_entropy(now_ms));
        result = roll_perform();

        if (roll_upset_should_glitch_animation()) {
            animations_flash_all(1U, 25U, 40U);
        }

        anim_status = animations_play_roll(result.value);

        if (anim_status == ANIM_ROLL_COMPLETED) {
            if (result.is_nat20) {
                animations_play_nat20();
            }

            charlieplex_all_off();
            app_enter_cooldown(millis());
            return;
        }

        if (anim_status == ANIM_ROLL_ABORT_LONG) {
            app_run_attunement_sequence();
            return;
        }

        /*
         * Short press interrupted the roll animation.
         * Every short button press contributes to a hidden sequence.
         * Eleven in a row may... change things.
         * Count it toward the "hidden" (sooo hidden...) 11-press sequence,
         * then immediately reroll.
         */
        now_ms = millis();
        roll_note_button_press(now_ms);
    }
}

static bool app_handle_long_press(void)
{
    if (!button_long()) {
        return false;
    }

    app_run_attunement_sequence();
    return true;
}

/* ------------------------------------------------------------------------- */
/* Main loop                                                                 */
/* ------------------------------------------------------------------------- */

int main(void)
{
    uint32_t now_ms;
    uint16_t seed;

    charlieplex_init();
    adc_init();
    button_init();
    touch_init();
    timer_init();
    /* Enable global interrupts after timer setup */
    sei();
    attunement_init();
    sequence_init();

    seed = app_collect_entropy(millis());
    roll_init(seed);

    if (attunement_has_value()) {
        animations_init();
        roll_seed_entropy((uint16_t)attunement_get_value() << 8);
    }

    app_state = APP_STATE_WAIT_FOR_START;
    charlieplex_all_off();

    while (1) {
        now_ms = millis();

        button_update(now_ms);
        touch_update();

        switch (app_state) {
        case APP_STATE_WAIT_FOR_START:
            /*
             * The artifact is powered, but not yet awake.
             * Ignore touch and wait for a deliberate button press.
             * The start press does NOT count as a roll trigger or godmode tap.
             */
            (void)touch_triggered();
			(void)touch_long();
            charlieplex_all_off();

            if (button_pressed()) {
                if (attunement_has_value()) {
                    app_state = APP_STATE_IDLE;
                    charlieplex_all_off();
                } else {
                    app_run_attunement_sequence();
                }
            }
            break;

        case APP_STATE_IDLE:
            if (app_handle_long_press()) {
                break;
            }

            if (button_pressed()) {
                /*
                 * Every short button press counts toward the "hidden" (yeah...)
                 * 11-tap "super secret" god mode sequence (hi, I'm an easter egg).
                 */
                roll_note_button_press(now_ms);

                if (roll_upset_should_ignore_input()) {
                    animations_flash_all(1U, 15U, 30U);
                    break;
                }

                app_start_roll(now_ms);
                break;
            }

            if (touch_long()) {
                sequence_enter();
                app_state = APP_STATE_SEQUENCE;
                break;
            }

            if (touch_triggered()) {
                if (roll_upset_should_ignore_input()) {
                    animations_flash_all(1U, 15U, 30U);
                    break;
                }

                app_start_roll(now_ms);
                break;
            }

            if (roll_godmode_active()) {
                animations_godmode_pulse_step(now_ms);
            } else {
                charlieplex_all_off();
            }
            break;

        case APP_STATE_COOLDOWN:
            if (app_handle_long_press()) {
                break;
            }

            /*
             * The button has the power to force a response, even when touch is ignored.
             * This gives immediate reroll from button during the post-roll dead time.
             */
            if (button_pressed()) {
                roll_note_button_press(now_ms);

                if (roll_upset_should_ignore_input()) {
                    animations_flash_all(1U, 15U, 30U);
                    break;
                }

                app_start_roll(now_ms);
                break;
            }

            /*
             * Touch is intentionally ignored during cooldown, but we still
             * consume any one-shot trigger so it does not fire stale later.
             */
            (void)touch_triggered();
            (void)touch_long();

            if ((uint32_t)(now_ms - cooldown_start_ms) >= TOUCH_REARM_MS) {
                app_state = APP_STATE_IDLE;
                charlieplex_all_off();
            } else if (roll_godmode_active()) {
                animations_godmode_pulse_step(now_ms);
            } else {
                charlieplex_all_off();
            }
            break;

        case APP_STATE_SEQUENCE:
            sequence_update(now_ms);

            if (!sequence_active()) {
                button_init();
                touch_init();
                app_enter_cooldown(millis());
            }
            break;

        default:
            app_state = APP_STATE_WAIT_FOR_START;
            charlieplex_all_off();
            break;
        }
    }
}
