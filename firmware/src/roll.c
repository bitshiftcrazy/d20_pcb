/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#include "roll.h"

#include <limits.h>

#include "config.h"

/* ------------------------------------------------------------------------- */
/* Roll tuning                                                               */
/* ------------------------------------------------------------------------- */

#define GODMODE_TAP_COUNT      11U
#define GODMODE_TAP_WINDOW_MS  500UL

/*
 * The dice is an instrument of fate. And as such,
 * it aims to work in its favour...which may or may not
 * coincide with yours.
 * After enough nat1s / nat20s, we apply a slight bias.
 * And as so often irl, with enough bad luck...you're really f*cked.
 * We do not fully force the range every time.
 *
 * With the values below:
 * - 20% chance to reroll low after enough nat1s
 * - 20% chance to reroll high after enough nat20s
 */
#define BIAS_REROLL_DENOMINATOR 10U
#define BIAS_REROLL_THRESHOLD   2U

/* ------------------------------------------------------------------------- */
/* Internal state                                                            */
/* ------------------------------------------------------------------------- */

#define RNG_INITIAL_SEED 0x0B11U
static uint16_t rng_state = RNG_INITIAL_SEED;

static uint8_t nat1_count = 0U;
static uint8_t nat20_count = 0U;

static bool godmode_enabled = false;
static uint8_t godmode_tap_count = 0U;
static uint32_t last_button_press_time_ms = 0UL;

/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

static uint16_t roll_rng_next(void)
{
    uint16_t x = rng_state;

    /* Avoid the all-zero trap state */
    if (x == 0U) {
        x = RNG_INITIAL_SEED;
    }

    /*
     * Small xorshift-style PRNG
     * Good enough for a slightly suspicious magical dice
     * We never tried to be fair anyways.
     * Not suitable for cryptography, prophecy, or dealings with gods
     */
    x ^= (uint16_t)(x << 7);
    x ^= (uint16_t)(x >> 9);
    x ^= (uint16_t)(x << 8);

    if (x == 0U) {
        x = RNG_INITIAL_SEED;
    }

    rng_state = x;
    return x;
}

static uint8_t roll_random_range(uint8_t min_value, uint8_t max_value)
{
    uint8_t span;
    uint16_t limit;
    uint16_t r;

    if (max_value <= min_value) {
        return min_value;
    }

    span = (uint8_t)(max_value - min_value + 1U);

    /* Rejection sampling avoids obvious modulo bias */
    limit = (uint16_t)(USHRT_MAX - (USHRT_MAX % span));

    do {
        r = roll_rng_next();
    } while (r >= limit);

    return (uint8_t)(min_value + (r % span));
}

static void roll_increment_counter(uint8_t *counter)
{
    if (*counter < UINT8_MAX) {
        (*counter)++;
    }
}

static bool roll_random_chance(uint8_t denominator, uint8_t threshold)
{
    if ((denominator == 0U) || (threshold == 0U)) {
        return false;
    }

    return (roll_random_range(0U, (uint8_t)(denominator - 1U)) < threshold);
}

static bool roll_should_apply_bias(void)
{
    return roll_random_chance(BIAS_REROLL_DENOMINATOR,
                              BIAS_REROLL_THRESHOLD);
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

void roll_init(uint16_t seed)
{
    nat1_count = 0U;
    nat20_count = 0U;

    godmode_enabled = false;
    godmode_tap_count = 0U;
    last_button_press_time_ms = 0UL;

    rng_state = seed;
    if (rng_state == 0U) {
        rng_state = RNG_INITIAL_SEED;
    }
}

/* Feed a little more chaos into the dice's current mood */
void roll_seed_entropy(uint16_t entropy)
{
    rng_state ^= entropy;
    rng_state ^= (uint16_t)(entropy << 7);

    if (rng_state == 0U) {
        rng_state = RNG_INITIAL_SEED;
    }
}

void roll_note_button_press(uint32_t now_ms)
{
    if ((now_ms - last_button_press_time_ms) < GODMODE_TAP_WINDOW_MS) {
        if (godmode_tap_count < UINT8_MAX) {
            godmode_tap_count++;
        }
    } else {
        godmode_tap_count = 1U;
    }

    last_button_press_time_ms = now_ms;

    if (godmode_tap_count >= GODMODE_TAP_COUNT) {
        godmode_enabled = true;
        godmode_tap_count = 0U;
    }
}

bool roll_godmode_active(void)
{
    return godmode_enabled;
}

void roll_disable_godmode(void)
{
    godmode_enabled = false;
    godmode_tap_count = 0U;
}

roll_result_t roll_perform(void)
{
    roll_result_t result;
    uint8_t raw_roll;

    if (godmode_enabled) {
        raw_roll = roll_random_range(
            GODMODE_MIN_ROLL,
            GODMODE_MAX_ROLL
        );
    } else {
        /*
         * Internal roll range is 0..19
         * User-facing result becomes 1..20 later
         */
        raw_roll = roll_random_range(0U, 19U);

        /*
         * Slight cursed bias after repeated nat1s:
         * the dice remembers repeated failure and may lean low again
         * Ever felt as if your dice hates you? Well...
         */
        if ((nat1_count >= NAT1_BIAS_TRIGGER) &&
            roll_should_apply_bias()) {
            raw_roll = roll_random_range(
                LOW_BIAS_MIN_ROLL,
                LOW_BIAS_MAX_ROLL
            );
        }

        /*
         * Slight blessed bias after repeated nat20s:
         * repeated glory should leave a residue
         * This intentionally happens after the nat1 check
         * If both counters are high, the later check may override
         * the earlier one. That is deliberate, not an accident
         */
        if ((nat20_count >= NAT20_BIAS_TRIGGER) &&
            roll_should_apply_bias()) {
            raw_roll = roll_random_range(
                HIGH_BIAS_MIN_ROLL,
                HIGH_BIAS_MAX_ROLL
            );
        }
    }

    result.value = (uint8_t)(raw_roll + 1U);
    result.is_nat1 = (raw_roll == 0U);
    result.is_nat20 = (raw_roll == 19U);
    result.used_godmode = godmode_enabled;

    if (result.is_nat1) {
        roll_increment_counter(&nat1_count);
    }

    if (result.is_nat20) {
        roll_increment_counter(&nat20_count);
    }

    result.nat1_count = nat1_count;
    result.nat20_count = nat20_count;

    return result;
}

bool roll_upset_active(void)
{
    return (nat1_count >= UPSET_NAT1_TRIGGER);
}

bool roll_upset_should_ignore_input(void)
{
    if (!roll_upset_active()) {
        return false;
    }

    return roll_random_chance(UPSET_INPUT_DENOMINATOR,
                              UPSET_INPUT_THRESHOLD);
}

bool roll_upset_should_glitch_animation(void)
{
    if (!roll_upset_active()) {
        return false;
    }

    return roll_random_chance(UPSET_GLITCH_DENOMINATOR,
                              UPSET_GLITCH_THRESHOLD);
}
