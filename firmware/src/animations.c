/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#include "animations.h"

#include <stdint.h>
#include <util/delay.h>

#include "attunement.h"
#include "button.h"
#include "charlieplex.h"
#include "config.h"
#include "timer.h"

#define ARRAY_LEN(x) ((uint8_t)(sizeof(x) / sizeof((x)[0])))

/* ------------------------------------------------------------------------------------------- */
/* General animation timing                                                                    */
/* ------------------------------------------------------------------------------------------- */

#define ROLL_STARTUP_ROUNDS        2U
#define ROLL_STARTUP_STEP_MS      40U
#define ROLL_UP_STEP_MS           50U

#define RESULT_HOLD_NORMAL_MS   5000U
#define RESULT_HOLD_NAT20_MS    2000U

#define ATTUNEMENT_ROUNDS         3U
#define ATTUNEMENT_STEP_MS       50U
#define ATTUNEMENT_PAUSE_MS     300U

#define NAT20_END_PAUSE_MS      600U

#define GODMODE_UPDATE_MS        20UL
#define GODMODE_PULSE_STEP        5U
#define GODMODE_PWM_LOW_US      300U
#define GODMODE_PWM_HIGH_US     300U

/* ------------------------------------------------------------------------------------------- */
/* Internal state                                                                              */
/* ------------------------------------------------------------------------------------------- */

static uint8_t  godmode_brightness  = 0U;
static int8_t   godmode_direction   = GODMODE_PULSE_STEP;
static uint32_t godmode_last_update = 0UL;

#define RNG_INITIAL_SEED 0x0B11U
static uint16_t rng_state = RNG_INITIAL_SEED; /* themed around the recurring "11" motif */

/* ------------------------------------------------------------------------------------------- */
/* Core rendering primitives                                                                   */
/* ------------------------------------------------------------------------------------------- */

/* Blocking millisecond delay helper for animation timing. */
static void animation_delay_ms(uint16_t ms)
{
    while (ms-- != 0U) {
        _delay_ms(1);
    }
}

static void show_led_for_ms(uint8_t led_index, uint16_t ms)
{
    while (ms-- != 0U) {
        charlieplex_led_on(led_index);
        _delay_ms(1);
    }

    charlieplex_all_off();
}

static void play_all_leds(uint16_t hold_ms)
{
    uint32_t start = millis();
    uint8_t i;

    while ((uint32_t)(millis() - start) < hold_ms) {
        for (i = 0U; i < LED_COUNT; i++) {
            charlieplex_led_on(i);
            _delay_us(300);
            charlieplex_all_off();

            if ((uint32_t)(millis() - start) >= hold_ms) {
                break;
            }
        }
    }

    charlieplex_all_off();
}

/*
 * Play one persistent multi-LED frame for a given duration.
 * This is the same for all character animations:
 * rapidly cycle through the LEDs in the frame so the whole set appears bright.
 */
static void play_led_group_ms(const uint8_t *leds,
                              uint8_t count,
                              uint16_t hold_ms,
                              uint8_t per_led_ms)
{
    uint32_t start = millis();
    uint8_t i;

    while ((uint32_t)(millis() - start) < hold_ms) {
        for (i = 0U; i < count; i++) {
            charlieplex_led_on(leds[i]);
            animation_delay_ms(per_led_ms);

            if ((uint32_t)(millis() - start) >= hold_ms) {
                break;
            }
        }
    }

    charlieplex_all_off();
}

/* ------------------------------------------------------------------------------------------ */
/* Character-specific rendering helpers                                                       */
/* ------------------------------------------------------------------------------------------ */

#if (CHARACTER_ID == CHARACTER_ASHTAN) || (CHARACTER_ID == CHARACTER_SALEM)
/* Faster multi-LED frame helper for denser/brighter visual effects. */
static void play_led_group_us(const uint8_t *leds,
                              uint8_t count,
                              uint16_t hold_ms,
                              uint16_t per_led_us)
{
    uint32_t start = millis();
    uint8_t i;

    while ((uint32_t)(millis() - start) < hold_ms) {
        for (i = 0U; i < count; i++) {
            charlieplex_led_on(leds[i]);
            _delay_us(per_led_us);
            charlieplex_all_off();

            if ((uint32_t)(millis() - start) >= hold_ms) {
                break;
            }
        }
    }

    charlieplex_all_off();
}
#endif

/* ------------------------------------------------------------------------------------------ */
/* Ring & traversal helpers                                                                   */
/* ------------------------------------------------------------------------------------------ */

/* Clockwise LED order around the d20 board. */
static const uint8_t ring_order[LED_COUNT] = {
    0U, 10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U,
    9U,  8U,  7U,  6U,  5U,  4U,  3U,  2U,  1U
};

static uint8_t result_to_ring_step(uint8_t led_index)
{
    uint8_t i;

    for (i = 0U; i < LED_COUNT; i++) {
        if (ring_order[i] == led_index) {
            return i;
        }
    }

    return 0U;
}

static void play_ring_once(uint16_t step_ms)
{
    uint8_t i;

    for (i = 0U; i < LED_COUNT; i++) {
        show_led_for_ms(ring_order[i], step_ms);
    }
}

/* ------------------------------------------------------------------------------------------ */
/* Roll animation helpers (abortable)                                                         */
/* ------------------------------------------------------------------------------------------ */

/*
 * Poll button state during abortable roll animation phases.
 * Normal roll animations must be interruptible by a button press at any time.
 */
static anim_roll_status_t poll_roll_abort(void)
{
    uint32_t now_ms = millis();

    button_update(now_ms);

    if (button_long()) {
        charlieplex_all_off();
        return ANIM_ROLL_ABORT_LONG;
    }

    if (button_pressed()) {
        charlieplex_all_off();
        return ANIM_ROLL_ABORT_SHORT;
    }

    return ANIM_ROLL_COMPLETED;
}

static anim_roll_status_t show_led_for_ms_abortable(uint8_t led_index, uint16_t ms)
{
    anim_roll_status_t status;

    while (ms-- != 0U) {
        charlieplex_led_on(led_index);
        _delay_ms(1);

        status = poll_roll_abort();
        if (status != ANIM_ROLL_COMPLETED) {
            charlieplex_all_off();
            return status;
        }
    }

    charlieplex_all_off();
    return ANIM_ROLL_COMPLETED;
}

static anim_roll_status_t play_ring_once_abortable(uint16_t step_ms)
{
    uint8_t i;
    anim_roll_status_t status;

    for (i = 0U; i < LED_COUNT; i++) {
        status = show_led_for_ms_abortable(ring_order[i], step_ms);
        if (status != ANIM_ROLL_COMPLETED) {
            return status;
        }
    }

    return ANIM_ROLL_COMPLETED;
}

/* ------------------------------------------------------------------------------------------ */
/* Timing & attunement helpers                                                                */
/* ------------------------------------------------------------------------------------------ */

/* Map themed attunement range 11..111 to a speed modifier range 0..30 */
static uint8_t get_attunement_speed(void)
{
    uint8_t value = attunement_get_value();

    if (value <= ATTUNEMENT_MIN_VALUE) {
        return 0U;
    }

    if (value >= ATTUNEMENT_MAX_VALUE) {
        return 30U;
    }

    return (uint8_t)(((uint16_t)(value - ATTUNEMENT_MIN_VALUE) * 30U) /
                     (uint16_t)(ATTUNEMENT_MAX_VALUE - ATTUNEMENT_MIN_VALUE));
}

static uint16_t clamp_duration(uint16_t base_ms,
                               uint8_t speed_mod,
                               uint16_t min_ms)
{
    int16_t adjusted = (int16_t)base_ms - (int16_t)speed_mod;

    if (adjusted < (int16_t)min_ms) {
        adjusted = (int16_t)min_ms;
    }

    return (uint16_t)adjusted;
}

/* ------------------------------------------------------------------------------------------ */
/* Animation-local RNG helpers                                                                */
/* ------------------------------------------------------------------------------------------ */

#if (CHARACTER_ID == CHARACTER_FAWN) || (CHARACTER_ID == CHARACTER_VII)
static uint16_t rng_next(void)
{
    /* Tiny xorshift-style PRNG, good enough for sparkle selection. */
    rng_state ^= (uint16_t)(rng_state << 7);
    rng_state ^= (uint16_t)(rng_state >> 9);
    rng_state ^= (uint16_t)(rng_state << 8);
    rng_state ^= (uint16_t)millis();

    if (rng_state == 0U) {
        rng_state = 0x1D2BU;
    }

    return rng_state;
}

static void pick_distinct_random_leds(uint8_t *out, uint8_t count)
{
    uint8_t chosen = 0U;
    uint8_t candidate;
    uint8_t i;
    uint8_t duplicate;

    while (chosen < count) {
        candidate = (uint8_t)(rng_next() % LED_COUNT);
        duplicate = 0U;

        for (i = 0U; i < chosen; i++) {
            if (out[i] == candidate) {
                duplicate = 1U;
                break;
            }
        }

        if (duplicate == 0U) {
            out[chosen++] = candidate;
        }
    }
}
#endif

/* ------------------------------------------------------------------------------------------ */
/* Character-specific nat20 animation                                                         */
/* ------------------------------------------------------------------------------------------ */
/*
 * Each character defines a three-phase nat20 animation.
 * The phases are meant to reflect the player character’s personality
 * and class, building toward a distinct grand finale.
 *
 * Depending on class and the character's personality, the animations
 * are either deterministic or include randomised elements.
 *
 * Attunement slightly influences animation speed, but not structure.
 */

/*
 * Ashtan — Artificer
 *
 * This animation is based on how Ashtan feels to me:
 * logical, rational, reliable, structured. With a bit of hesitation
 * at first, a hint of playfulness in between, always a clear goal.
 * There's no randomness in this animation, things have a clear cause
 * and an effect.
 *
 * It starts slightly hesitant, but already follows a clear structure.
 * As it progresses, it becomes bolder while keeping that logic intact:
 * things begin to click into place like small gears, then larger ones,
 * forming a bigger picture.
 *
 * There’s a brief playful moment in the blinking clusters, before
 * everything finally clicks into place.
 *
 * Overall it should feel structured and goal-driven, like something
 * carefully assembling itself step by step.
 */

#if CHARACTER_ID == CHARACTER_ASHTAN

static void nat20_phase1(void)
{
    static const uint8_t s1[] = { 0U, 19U };
    static const uint8_t s2[] = { 0U, 19U, 3U, 12U };
    static const uint8_t s3[] = { 0U, 19U, 3U, 12U, 7U, 16U };
    static const uint8_t s4[] = { 0U, 19U, 3U, 12U, 7U, 16U, 4U, 13U };
    static const uint8_t s5[] = { 0U, 19U, 3U, 12U, 7U, 16U, 4U, 13U, 6U, 15U };
    static const uint8_t s6[] = { 0U, 19U, 3U, 12U, 7U, 16U, 4U, 13U, 6U, 15U, 2U, 11U };
    static const uint8_t s7[] = { 0U, 19U, 3U, 12U, 7U, 16U, 4U, 13U, 6U, 15U, 2U, 11U, 5U, 14U };
    static const uint8_t s8[] = { 0U, 19U, 3U, 12U, 7U, 16U, 4U, 13U, 6U, 15U, 2U, 11U, 5U, 14U, 8U, 17U };

    uint8_t  speed_mod = get_attunement_speed();
    uint16_t hold      = clamp_duration(400U, speed_mod, 100U);
    uint16_t finale    = clamp_duration(600U, speed_mod, 200U);

    play_led_group_us(s1, ARRAY_LEN(s1), hold, 300U);
    play_led_group_us(s2, ARRAY_LEN(s2), hold, 300U);
    play_led_group_us(s3, ARRAY_LEN(s3), hold, 300U);
    play_led_group_us(s4, ARRAY_LEN(s4), hold, 300U);
    play_led_group_us(s5, ARRAY_LEN(s5), hold, 300U);
    play_led_group_us(s6, ARRAY_LEN(s6), hold, 300U);
    play_led_group_us(s7, ARRAY_LEN(s7), hold, 300U);
    play_led_group_us(s8, ARRAY_LEN(s8), hold, 300U);

    play_all_leds(finale);
}

static void nat20_phase2(void)
{
    static const uint8_t s1[] = { 16U, 17U, 18U };
    static const uint8_t s2[] = { 13U, 14U, 15U, 16U, 17U, 18U };
    static const uint8_t s3[] = { 1U, 2U, 3U, 13U, 14U, 15U, 16U, 17U, 18U };
    static const uint8_t s4[] = { 4U, 5U, 6U, 1U, 2U, 3U, 13U, 14U, 15U, 16U, 17U, 18U };
    static const uint8_t s5[] = {
        0U, 10U, 11U, 12U,
        4U, 5U, 6U,
        1U, 2U, 3U,
        13U, 14U, 15U,
        16U, 17U, 18U
    };

    uint8_t  speed_mod = get_attunement_speed();
    uint16_t hold      = clamp_duration(400U, speed_mod, 100U);
    uint16_t finale    = clamp_duration(600U, speed_mod, 200U);

    play_led_group_us(s1, ARRAY_LEN(s1), hold, 300U);
    play_led_group_us(s2, ARRAY_LEN(s2), hold, 300U);
    play_led_group_us(s3, ARRAY_LEN(s3), hold, 300U);
    play_led_group_us(s4, ARRAY_LEN(s4), hold, 300U);
    play_led_group_us(s5, ARRAY_LEN(s5), hold, 300U);

    play_all_leds(finale);
}

static void nat20_phase3(void)
{
    static const uint8_t s1[]  = { 4U, 5U, 6U };
    static const uint8_t s2[]  = { 13U, 14U, 15U };
    static const uint8_t s3[]  = { 3U, 2U, 1U };
    static const uint8_t s4[]  = { 18U, 17U, 16U };
    static const uint8_t s5[]  = { 7U, 8U, 9U };
    static const uint8_t s6[]  = { 10U, 11U, 12U };
    static const uint8_t s7[]  = { 3U, 2U, 1U, 10U, 11U, 12U };
    static const uint8_t s8[]  = { 7U, 8U, 9U, 19U, 18U, 17U };
    static const uint8_t s9[]  = { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U };
    static const uint8_t s10[] = { 10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U };

    uint8_t  speed_mod = get_attunement_speed();
    uint16_t hold      = clamp_duration(280U, speed_mod, 100U);
    uint16_t finale    = clamp_duration(600U, speed_mod, 200U);

    play_led_group_ms(s1,  ARRAY_LEN(s1),  hold, 2U);
    play_led_group_ms(s2,  ARRAY_LEN(s2),  hold, 2U);
    play_led_group_ms(s3,  ARRAY_LEN(s3),  hold, 2U);
    play_led_group_ms(s4,  ARRAY_LEN(s4),  hold, 2U);
    play_led_group_ms(s5,  ARRAY_LEN(s5),  hold, 2U);
    play_led_group_ms(s6,  ARRAY_LEN(s6),  hold, 2U);
    play_led_group_ms(s7,  ARRAY_LEN(s7),  hold, 2U);
    play_led_group_ms(s8,  ARRAY_LEN(s8),  hold, 2U);
    play_led_group_ms(s9,  ARRAY_LEN(s9),  hold, 2U);
    play_led_group_ms(s10, ARRAY_LEN(s10), hold, 2U);

    play_all_leds(finale);
}

/*
 * Bartholomew — Sorcerer
 *
 * This animation is based on how Bartholomew feels to me:
 * something unknown and slightly dark, with a more lighthearted side
 * beneath it.
 *
 * It begins with a full circle, which then breaks apart as LEDs
 * randomly go dark: like darkness creeping in and taking over.
 *
 * The middle becomes more restless, almost like a chase.
 * Either Bartholomew searching for something...or something
 * searching for him.
 *
 * At the end, playful pairs appear. They can feel like a glimpse
 * of his lighter side, or like pairs of eyes watching from the dark.
 *
 * Overall it should feel unpredictable and slightly uneasy, with that sense
 * of the unknown always present.
 */
#elif CHARACTER_ID == CHARACTER_BARTHOLOMEW

static void play_filtered_step(uint8_t skip_modulus, uint16_t hold_ms)
{
    uint32_t start = millis();
    uint8_t i;

    while ((uint32_t)(millis() - start) < hold_ms) {
        for (i = 0U; i < LED_COUNT; i++) {
            if ((i % skip_modulus) != 0U) {
                charlieplex_led_on(i);
                _delay_us(500);
            }
        }
    }

    charlieplex_all_off();
}

static void nat20_phase1(void)
{
    uint8_t  speed_mod  = get_attunement_speed();
    uint16_t full_glow  = clamp_duration(1000U, speed_mod, 200U);
    uint16_t step_hold  = clamp_duration(700U,  speed_mod, 150U);
    uint16_t short_pause = clamp_duration(100U, speed_mod, 20U);
    uint16_t long_pause  = clamp_duration(400U, speed_mod, 50U);

    play_all_leds(full_glow);

    play_filtered_step(5U, step_hold);
    animation_delay_ms(short_pause);

    play_filtered_step(4U, step_hold);
    animation_delay_ms(short_pause);

    play_filtered_step(3U, step_hold);
    animation_delay_ms(short_pause);

    play_filtered_step(2U, step_hold);
    animation_delay_ms(long_pause);

    charlieplex_all_off();
}

static void nat20_phase2(void)
{
    static const uint8_t p1[]  = { 3U, 4U };
    static const uint8_t p2[]  = { 2U, 5U };
    static const uint8_t p3[]  = { 1U, 6U };
    static const uint8_t p4[]  = { 7U, 0U };
    static const uint8_t p5[]  = { 8U, 10U };
    static const uint8_t p6[]  = { 9U, 11U };
    static const uint8_t p7[]  = { 19U, 12U };

    static const uint8_t p8[]  = { 15U, 16U };
    static const uint8_t p9[]  = { 14U, 17U };
    static const uint8_t p10[] = { 13U, 18U };
    static const uint8_t p11[] = { 12U, 19U };
    static const uint8_t p12[] = { 11U, 9U };
    static const uint8_t p13[] = { 10U, 8U };
    static const uint8_t p14[] = { 0U, 7U };

    static const uint8_t p15[] = { 6U, 7U };
    static const uint8_t p16[] = { 5U, 8U };
    static const uint8_t p17[] = { 4U, 9U };
    static const uint8_t p18[] = { 3U, 19U };
    static const uint8_t p19[] = { 2U, 18U };
    static const uint8_t p20[] = { 1U, 17U };
    static const uint8_t p21[] = { 0U, 16U };
    static const uint8_t p22[] = { 10U, 15U };
    static const uint8_t p23[] = { 11U, 14U };
    static const uint8_t p24[] = { 12U, 13U };

    uint8_t  speed_mod     = get_attunement_speed();
    uint16_t delay_per_pair = clamp_duration(150U, speed_mod, 30U);

    play_led_group_ms(p1,  ARRAY_LEN(p1),  delay_per_pair, 5U);
    play_led_group_ms(p2,  ARRAY_LEN(p2),  delay_per_pair, 5U);
    play_led_group_ms(p3,  ARRAY_LEN(p3),  delay_per_pair, 5U);
    play_led_group_ms(p4,  ARRAY_LEN(p4),  delay_per_pair, 5U);
    play_led_group_ms(p5,  ARRAY_LEN(p5),  delay_per_pair, 5U);
    play_led_group_ms(p6,  ARRAY_LEN(p6),  delay_per_pair, 5U);
    play_led_group_ms(p7,  ARRAY_LEN(p7),  delay_per_pair, 5U);

    play_led_group_ms(p8,  ARRAY_LEN(p8),  delay_per_pair, 5U);
    play_led_group_ms(p9,  ARRAY_LEN(p9),  delay_per_pair, 5U);
    play_led_group_ms(p10, ARRAY_LEN(p10), delay_per_pair, 5U);
    play_led_group_ms(p11, ARRAY_LEN(p11), delay_per_pair, 5U);
    play_led_group_ms(p12, ARRAY_LEN(p12), delay_per_pair, 5U);
    play_led_group_ms(p13, ARRAY_LEN(p13), delay_per_pair, 5U);
    play_led_group_ms(p14, ARRAY_LEN(p14), delay_per_pair, 5U);

    play_led_group_ms(p15, ARRAY_LEN(p15), delay_per_pair, 5U);
    play_led_group_ms(p16, ARRAY_LEN(p16), delay_per_pair, 5U);
    play_led_group_ms(p17, ARRAY_LEN(p17), delay_per_pair, 5U);
    play_led_group_ms(p18, ARRAY_LEN(p18), delay_per_pair, 5U);
    play_led_group_ms(p19, ARRAY_LEN(p19), delay_per_pair, 5U);
    play_led_group_ms(p20, ARRAY_LEN(p20), delay_per_pair, 5U);
    play_led_group_ms(p21, ARRAY_LEN(p21), delay_per_pair, 5U);
    play_led_group_ms(p22, ARRAY_LEN(p22), delay_per_pair, 5U);
    play_led_group_ms(p23, ARRAY_LEN(p23), delay_per_pair, 5U);
    play_led_group_ms(p24, ARRAY_LEN(p24), delay_per_pair, 5U);

    charlieplex_all_off();
}

static void nat20_phase3(void)
{
    static const uint8_t p1[] = { 12U, 9U };
    static const uint8_t p2[] = { 18U, 3U };
    static const uint8_t p3[] = { 8U, 11U };
    static const uint8_t p4[] = { 17U, 2U };
    static const uint8_t p5[] = { 5U, 14U };
    static const uint8_t p6[] = { 16U, 1U };
    static const uint8_t p7[] = { 10U, 7U };
    static const uint8_t p8[] = { 15U, 6U };
    static const uint8_t p9[] = { 2U, 17U };

    uint8_t  speed_mod      = get_attunement_speed();
    uint16_t delay_per_pair = clamp_duration(300U, speed_mod, 60U);

    play_led_group_ms(p1, ARRAY_LEN(p1), delay_per_pair, 5U);
    play_led_group_ms(p2, ARRAY_LEN(p2), delay_per_pair, 5U);
    play_led_group_ms(p3, ARRAY_LEN(p3), delay_per_pair, 5U);
    play_led_group_ms(p4, ARRAY_LEN(p4), delay_per_pair, 5U);
    play_led_group_ms(p5, ARRAY_LEN(p5), delay_per_pair, 5U);
    play_led_group_ms(p6, ARRAY_LEN(p6), delay_per_pair, 5U);
    play_led_group_ms(p7, ARRAY_LEN(p7), delay_per_pair, 5U);
    play_led_group_ms(p8, ARRAY_LEN(p8), delay_per_pair, 5U);
    play_led_group_ms(p9, ARRAY_LEN(p9), delay_per_pair, 5U);

    charlieplex_all_off();
}

/*
 * Fawn — Druid
 *
 * This animation is based on how Fawn feels to me:
 * soft, light, and a bit carefree (almost lucky) but with
 * a quiet sense of energy and power behind it.
 *
 * It starts small, like a tiny sprout at the top of the board,
 * slowly growing and becoming more lively and excited.
 * Like Fawn, it begins gently and grows into something bigger,
 * into being part of an adventure.
 *
 * It then shifts into a cheerful, energetic chase: still
 * lighthearted, but now clearly driven by that underlying power.
 *
 * In the end, small lights appear and move around the board
 * like little sprouts popping up, almost dancing.
 *
 * Overall it should feel organic and growing, light and joyful,
 * but never weak, there’s always energy behind it.
 */
#elif CHARACTER_ID == CHARACTER_FAWN

static void play_single_path(const uint8_t *path,
                             uint8_t count,
                             uint16_t step_ms,
                             uint16_t final_ms)
{
    uint8_t i;

    if (count == 0U) {
        return;
    }

    for (i = 0U; i < (uint8_t)(count - 1U); i++) {
        show_led_for_ms(path[i], step_ms);
    }

    show_led_for_ms(path[count - 1U], final_ms);
    charlieplex_all_off();
}

static void nat20_phase1(void)
{
    static const uint8_t left_1_5[]   = { 0U, 1U, 2U, 3U, 4U };
    static const uint8_t right_1_13[] = { 0U, 10U, 11U, 12U, 13U };
    static const uint8_t left_1_7[]   = { 0U, 1U, 2U, 3U, 4U, 5U, 6U };
    static const uint8_t right_1_17[] = { 0U, 10U, 11U, 12U, 13U, 14U, 15U, 16U };
    static const uint8_t left_1_20[]  = { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 19U };
    static const uint8_t right_1_19[] = { 0U, 10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U };

    uint8_t  speed_mod = get_attunement_speed();
    uint16_t d100      = clamp_duration(100U, speed_mod, 20U);
    uint16_t d70       = clamp_duration(70U,  speed_mod, 20U);
    uint16_t d50       = clamp_duration(50U,  speed_mod, 20U);
    uint16_t d150      = clamp_duration(150U, speed_mod, 30U);

    play_single_path(left_1_5,   ARRAY_LEN(left_1_5),   d100, d150);
    play_single_path(right_1_13, ARRAY_LEN(right_1_13), d100, d150);
    play_single_path(left_1_7,   ARRAY_LEN(left_1_7),   d70,  d150);
    play_single_path(right_1_17, ARRAY_LEN(right_1_17), d70,  d150);
    play_single_path(left_1_20,  ARRAY_LEN(left_1_20),  d50,  d150);
    play_single_path(right_1_19, ARRAY_LEN(right_1_19), d50,  d150);
}

static void nat20_phase2(void)
{
    static const uint8_t ripple_left[] = {
        0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U,
        19U, 18U, 17U, 16U, 15U, 14U, 13U, 12U, 11U, 10U
    };
    static const uint8_t ripple_right[] = {
        0U, 10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U,
        19U, 9U, 8U, 7U, 6U, 5U, 4U, 3U, 2U, 1U, 0U
    };

    uint8_t  speed_mod = get_attunement_speed();
    uint16_t d         = clamp_duration(40U, speed_mod, 10U);
    uint8_t i;

    for (i = 0U; i < ARRAY_LEN(ripple_left); i++) {
        show_led_for_ms(ripple_left[i], d);
    }

    for (i = 0U; i < ARRAY_LEN(ripple_right); i++) {
        show_led_for_ms(ripple_right[i], d);
    }

    for (i = 0U; i < ARRAY_LEN(ripple_left); i++) {
        show_led_for_ms(ripple_left[i], d);
    }

    charlieplex_all_off();
}

static void nat20_phase3(void)
{
    uint8_t  speed_mod = get_attunement_speed();
    uint16_t frame_ms  = clamp_duration(200U, speed_mod, 50U);
    uint8_t  leds[3];
    uint8_t  round;

    for (round = 0U; round < 10U; round++) {
        pick_distinct_random_leds(leds, 3U);
        play_led_group_ms(leds, 3U, frame_ms, 5U);
        charlieplex_all_off();
    }
}

/*
 * Salem — Fighter
 *
 * This animation is based on how Salem feels to me:
 * direct, straightforward, no fluff, a warrior’s heart driven by
 * strength and honour.
 *
 * It begins with sharp impulses at the corners of the board,
 * flaring up like punches or heartbeats.
 * These pulses grow in size and intensity, becoming stronger,
 * more purposeful, more directed, like a heart that’s set on
 * making an impact.
 *
 * As it builds, everything becomes more forceful and focused,
 * until that warrior’s heart is beating with full power.
 *
 * Overall it should feel steady and honest, with no unnecessary
 * movement. Just raw force, intent, and conviction.
 */
#elif CHARACTER_ID == CHARACTER_SALEM

static void nat20_phase1(void)
{
    static const uint8_t g1[] = { 0U, 1U, 10U };
    static const uint8_t g2[] = { 9U, 19U, 18U };
    static const uint8_t g3[] = { 12U, 13U };
    static const uint8_t g4[] = { 6U, 7U };
    static const uint8_t g5[] = { 15U, 16U };
    static const uint8_t g6[] = { 3U, 4U };

    static const uint8_t *groups[] = { g1, g2, g3, g4, g5, g6 };
    static const uint8_t lengths[] = {
        ARRAY_LEN(g1), ARRAY_LEN(g2), ARRAY_LEN(g3),
        ARRAY_LEN(g4), ARRAY_LEN(g5), ARRAY_LEN(g6)
    };

    uint8_t  speed_mod   = get_attunement_speed();
    uint16_t short_pulse = clamp_duration(200U, speed_mod, 50U);
    uint16_t long_pulse  = clamp_duration(400U, speed_mod, 100U);
    uint16_t pause_ms    = clamp_duration(120U, speed_mod, 30U);
    uint8_t rep;
    uint8_t i;

    for (rep = 0U; rep < 2U; rep++) {
        for (i = 0U; i < ARRAY_LEN(groups); i++) {
            play_led_group_ms(groups[i], lengths[i], short_pulse, 3U);
            animation_delay_ms(pause_ms);
            play_led_group_ms(groups[i], lengths[i], long_pulse, 3U);
            animation_delay_ms(pause_ms);
        }
    }
}

static void nat20_phase2(void)
{
    static const uint8_t a[] = {
        10U, 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 19U, 18U
    };
    static const uint8_t b[] = {
        1U, 0U, 10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 9U
    };

    uint8_t  speed_mod   = get_attunement_speed();
    uint16_t short_pulse = clamp_duration(250U, speed_mod, 50U);
    uint16_t long_pulse  = clamp_duration(450U, speed_mod, 100U);
    uint16_t pause_ms    = clamp_duration(120U, speed_mod, 30U);
    uint8_t rep;

    for (rep = 0U; rep < 3U; rep++) {
        play_led_group_us(a, ARRAY_LEN(a), short_pulse, 300U);
        animation_delay_ms(pause_ms);
        play_led_group_us(a, ARRAY_LEN(a), long_pulse, 300U);
        animation_delay_ms(pause_ms);

        play_led_group_us(b, ARRAY_LEN(b), short_pulse, 300U);
        animation_delay_ms(pause_ms);
        play_led_group_us(b, ARRAY_LEN(b), long_pulse, 300U);
        animation_delay_ms(pause_ms);
    }
}

static void nat20_phase3(void)
{
    uint8_t  speed_mod   = get_attunement_speed();
    uint16_t short_time  = clamp_duration(200U, speed_mod, 50U);
    uint16_t long_time   = clamp_duration(400U, speed_mod, 100U);
    uint16_t short_pause = clamp_duration(100U, speed_mod, 20U);
    uint16_t long_pause  = clamp_duration(180U, speed_mod, 30U);
    uint8_t beat;

    for (beat = 0U; beat < 3U; beat++) {
        play_all_leds(short_time);
        animation_delay_ms(short_pause);

        play_all_leds(long_time);
        animation_delay_ms(long_pause);
    }

    play_all_leds(800U);
}

/*
 * Vii — Bard
 *
 * This animation is based on how Vii feels to me:
 * curious, investigative, a little mischievous and chaotic,
 * always looking for a good story.
 *
 * It begins with pairs of LEDs moving like a cheerful dance
 * around the board, as if starting a melody. Large parts of
 * the board are still dark, the story isn’t known yet.
 *
 * As it progresses, the movement goes forward but also back,
 * like steps in a dance or notes in a melody revisiting earlier
 * themes. It reflects her curiosity about the past and that
 * understanding it is part of moving forward.
 *
 * In the end, the melody becomes more wild and free, more notes,
 * more movement, more pairs, until the whole board lights up and the story
 * finally reveals itself.
 *
 * Overall it should feel playful and expressive, a bit chaotic,
 * always searching, always unfolding into something bigger.
 */
#elif CHARACTER_ID == CHARACTER_VII

static void nat20_phase1(void)
{
    static const uint8_t frames[][2] = {
        { 0U, 19U },
        { 1U, 18U },
        { 2U, 17U },
        { 3U, 16U },
        { 4U, 15U },
        { 5U, 14U },
        { 6U, 13U },
        { 7U, 12U },
        { 8U, 11U },
        { 9U, 10U },
        { 19U, 0U }
    };
    static const uint8_t repeats[] = { 1U, 2U, 3U, 1U, 2U, 3U, 1U, 2U, 3U, 1U, 2U };

    uint8_t  speed_mod   = get_attunement_speed();
    uint16_t short_delay = clamp_duration(100U, speed_mod, 20U);
    uint16_t pause_ms    = clamp_duration(80U,  speed_mod, 20U);
    uint8_t i;
    uint8_t rep;

    for (i = 0U; i < ARRAY_LEN(frames); i++) {
        for (rep = 0U; rep < repeats[i]; rep++) {
            play_led_group_ms(frames[i], 2U, short_delay, 2U);
            animation_delay_ms(pause_ms);
        }
    }
}

static void nat20_phase2(void)
{
    static const uint8_t frames[][2] = {
        { 18U, 4U }, { 17U, 5U }, { 16U, 6U }, { 17U, 5U }, { 18U, 4U },
        { 3U, 15U }, { 2U, 16U }, { 1U, 17U }, { 2U, 16U }, { 3U, 15U },
        { 9U, 1U },  { 8U, 2U },  { 7U, 3U },  { 8U, 2U },  { 9U, 1U },
        { 12U, 16U }, { 11U, 17U }, { 10U, 18U }, { 11U, 17U }, { 12U, 16U },
        { 4U, 12U }, { 5U, 11U }, { 6U, 10U }, { 5U, 11U }, { 4U, 12U },
        { 15U, 9U }, { 14U, 8U }, { 13U, 7U }, { 14U, 8U }, { 15U, 9U }
    };

    uint8_t  speed_mod = get_attunement_speed();
    uint16_t pulse_ms  = clamp_duration(120U, speed_mod, 20U);
    uint16_t pause_ms  = clamp_duration(80U,  speed_mod, 20U);
    uint8_t round;
    uint8_t i;

    for (round = 0U; round < 2U; round++) {
        for (i = 0U; i < ARRAY_LEN(frames); i++) {
            play_led_group_ms(frames[i], 2U, pulse_ms, 2U);
            animation_delay_ms(pause_ms);
        }
    }
}

static void nat20_phase3(void)
{
    uint8_t  speed_mod    = get_attunement_speed();
    uint16_t loop_ms      = clamp_duration(180U, speed_mod, 50U);
    uint16_t sparkle_pause = clamp_duration(50U, speed_mod, 10U);
    uint8_t  leds[3];
    uint8_t  round;

    for (round = 0U; round < 16U; round++) {
        pick_distinct_random_leds(leds, 3U);
        play_led_group_ms(leds, 3U, loop_ms, 3U);
        animation_delay_ms(sparkle_pause);
    }

    play_all_leds(1000U);
}

#else
#error "Invalid CHARACTER_ID"
#endif

/* ------------------------------------------------------------------------------------------ */
/* Public API                                                                                 */
/* ------------------------------------------------------------------------------------------ */

void animations_init(void)
{
    godmode_brightness  = 0U;
    godmode_direction   = GODMODE_PULSE_STEP;
    godmode_last_update = 0UL;

    rng_state = (uint16_t)(((uint16_t)attunement_get_value() << 8)
                         ^ RNG_INITIAL_SEED);
    if (rng_state == 0U) {
        rng_state = RNG_INITIAL_SEED;
    }

    charlieplex_all_off();
}

void animations_godmode_pulse_step(uint32_t now_ms)
{
    uint8_t burst_count;
    uint16_t i;
    int16_t next;

    if ((now_ms - godmode_last_update) < GODMODE_UPDATE_MS) {
        return;
    }

    godmode_last_update = now_ms;

    next = (int16_t)godmode_brightness + (int16_t)godmode_direction;

    if (next >= 255) {
        next = 255;
        godmode_direction = (int8_t)(-godmode_direction);
    } else if (next <= 0) {
        next = 0;
        godmode_direction = (int8_t)(-godmode_direction);
    }

    godmode_brightness = (uint8_t)next;

    burst_count = (uint8_t)(1U + ((uint16_t)godmode_brightness * 9U) / 255U);

    for (i = 0U; i < burst_count; i++) {
        charlieplex_led_on(19U);
        _delay_us(GODMODE_PWM_LOW_US);
        charlieplex_all_off();
        _delay_us(GODMODE_PWM_HIGH_US);
    }
}

void animations_flash_all(uint8_t times, uint16_t on_ms, uint16_t off_ms)
{
    uint8_t i;

    for (i = 0U; i < times; i++) {
        play_all_leds(on_ms);
        animation_delay_ms(off_ms);
    }
}

anim_roll_status_t animations_play_roll(uint8_t result)
{
    uint8_t i;
    uint8_t result_led_index;
    uint8_t stop_step;
    anim_roll_status_t status;

    if ((result < 1U) || (result > LED_COUNT)) {
        return ANIM_ROLL_INVALID_INPUT;
    }

    result_led_index = (uint8_t)(result - 1U);
    stop_step = result_to_ring_step(result_led_index);

    charlieplex_all_off();

    /* Abortable startup ring */
    for (i = 0U; i < ROLL_STARTUP_ROUNDS; i++) {
        status = play_ring_once_abortable(ROLL_STARTUP_STEP_MS);
        if (status != ANIM_ROLL_COMPLETED) {
            return status;
        }
    }

    /* Abortable roll-up */
    for (i = 0U; i <= stop_step; i++) {
        status = show_led_for_ms_abortable(ring_order[i], ROLL_UP_STEP_MS);
        if (status != ANIM_ROLL_COMPLETED) {
            return status;
        }
    }

    /* From here on, result is committed: not abortable anymore */
    if (result_led_index == 19U) {
        show_led_for_ms(result_led_index, RESULT_HOLD_NAT20_MS);
    } else {
        show_led_for_ms(result_led_index, RESULT_HOLD_NORMAL_MS);
    }

    charlieplex_all_off();
    return ANIM_ROLL_COMPLETED;
}

void animations_play_attunement(void)
{
    uint8_t round;

    for (round = 0U; round < ATTUNEMENT_ROUNDS; round++) {
        play_ring_once(ATTUNEMENT_STEP_MS);
    }

    animation_delay_ms(ATTUNEMENT_PAUSE_MS);
    animations_play_nat20();
}

void animations_play_nat20(void)
{
    nat20_phase1();
    nat20_phase2();
    nat20_phase3();
    animation_delay_ms(NAT20_END_PAUSE_MS);
}
