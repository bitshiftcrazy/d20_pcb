/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#include "attunement.h"

#include <util/delay.h>

#include "adc.h"
#include "config.h"
#include "eeprom_store.h"
#include "pins.h"
#include "animations.h"

/*
 * Attunement calculation
 * ----------------------
 *
 * Why do we do this?
 *
 * In D&D, some magic items form a bond with their bearer before they can be
 * used. This is called attunement. To simulate this magical bond for our
 * players and make the die feel "theirs" in a way, we calculate a unique
 * value from the ADC readings of the very first use (or an explicitly
 * triggered re-attunement).
 *
 * But why like this specifically? .+*^Magic^*+.
 *
 * Well, and a hint of pragmatism.
 * We want the resulting attunement value to stay in a reasonable,
 * non-disruptive range, so taking the raw readings is not an option.
 * But since this is a magical artifact and not just a mere die,
 * the way those readings are transformed should trigger a tiny
 * "wtf is this cursed sh**"-moment for the gentle reader of this code.
 *
 * How?
 *
 * Derive a stable-ish attunement value from:
 *   1) an ordered sequence of touch ADC readings
 *   2) the compile-time character name
 *
 * Strategy:
 * - H: Horner-style accumulation of the absolute ADC samples
 * - D: Horner-style accumulation of absolute deltas between consecutive samples
 * - N: Horner-style hash of CHARACTER_NAME
 *
 * Then combine:
 *   combined = H ^ rotl16(D, ROT_D) ^ rotl16(N, ROT_N)
 *
 * Finally fold and reduce to ATTUNEMENT_MIN_VALUE..ATTUNEMENT_MAX_VALUE.
 *
 * Sampling is blocking and uses ATTUNEMENT_SAMPLE_COUNT readings separated by
 * ATTUNEMENT_SAMPLE_DELAY_MS.
 */

/* ------------------------------------------------------------------------- */
/* Attunement tuning                                                         */
/* ------------------------------------------------------------------------- */

#define ATTUNEMENT_SAMPLE_COUNT     11U
#define ATTUNEMENT_SAMPLE_DELAY_MS   5U

#define ATTUNEMENT_BASE            257U /* Horner/hash mixing base */
#define ATTUNEMENT_ROT_D             5U /* Delta accumulator rotation */
#define ATTUNEMENT_ROT_N             9U /* Name accumulator rotation */

/* Themed seed values built around the recurring 11/12 motif. */
#define ATTUNEMENT_NAME_SEED    0x0C11U
#define ATTUNEMENT_SAMPLE_SEED  0x0B11U
#define ATTUNEMENT_DELTA_SEED   0xB10CU

/* ------------------------------------------------------------------------- */
/* Internal state                                                            */
/* ------------------------------------------------------------------------- */

static bool attunement_valid = false;
static uint8_t attunement_value = ATTUNEMENT_MIN_VALUE;

/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

/* Guard against cached/stored values above the supported attunement range. */
static uint8_t attunement_clamp(uint8_t value)
{
    if (value > ATTUNEMENT_MAX_VALUE) {
        return ATTUNEMENT_MAX_VALUE;
    }

    return value;
}

static uint16_t attunement_rotl16(uint16_t x, uint8_t r)
{
    return (uint16_t)((x << r) | (x >> (16U - r)));
}

static uint16_t attunement_hash_name(const char *name)
{
    uint16_t h = ATTUNEMENT_NAME_SEED;

    while (*name != '\0') {
        uint8_t c = (uint8_t)*name;

        if ((c >= (uint8_t)'a') && (c <= (uint8_t)'z')) {
            c = (uint8_t)(c - ((uint8_t)'a' - (uint8_t)'A'));
        }

        h = (uint16_t)(h * ATTUNEMENT_BASE + c);
        name++;
    }

    return h;
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

void attunement_init(void)
{
    (void)attunement_load();
}

bool attunement_has_value(void)
{
    return attunement_valid;
}

uint8_t attunement_get_value(void)
{
    return attunement_value;
}

bool attunement_load(void)
{
    if (eeprom_store_has_attunement()) {
        attunement_value =
            attunement_clamp(eeprom_store_read_attunement());
        attunement_valid = true;
        return true;
    }

    attunement_value = ATTUNEMENT_MIN_VALUE;
    attunement_valid = false;
    return false;
}

uint8_t attunement_measure(void)
{
    uint16_t h = ATTUNEMENT_SAMPLE_SEED;
    uint16_t d = ATTUNEMENT_DELTA_SEED;
    uint16_t prev;
    uint16_t first_sample;
    uint8_t i;
    uint16_t n;
    uint16_t combined;

    first_sample = adc_read(TOUCH_ADC_CHANNEL);
    h = (uint16_t)(h * ATTUNEMENT_BASE + first_sample);
    d = (uint16_t)(d * ATTUNEMENT_BASE);
    prev = first_sample;

    _delay_ms(ATTUNEMENT_SAMPLE_DELAY_MS);

    for (i = 1U; i < ATTUNEMENT_SAMPLE_COUNT; i++) {
        uint16_t sample = adc_read(TOUCH_ADC_CHANNEL);
        uint16_t delta = (sample > prev)
                       ? (uint16_t)(sample - prev)
                       : (uint16_t)(prev - sample);

        h = (uint16_t)(h * ATTUNEMENT_BASE + sample);
        d = (uint16_t)(d * ATTUNEMENT_BASE + delta);

        prev = sample;
        _delay_ms(ATTUNEMENT_SAMPLE_DELAY_MS);
    }

    n = attunement_hash_name(CHARACTER_NAME);

    combined = (uint16_t)(h
                        ^ attunement_rotl16(d, ATTUNEMENT_ROT_D)
                        ^ attunement_rotl16(n, ATTUNEMENT_ROT_N));

    /* Fold upper and lower bits before reducing to the attunement range. */
    combined ^= (uint16_t)(combined >> 8);
    combined ^= (uint16_t)(combined >> 4);
    combined ^= (uint16_t)(combined >> 11);

    return (uint8_t)(ATTUNEMENT_MIN_VALUE
           + (combined % (uint16_t)(ATTUNEMENT_MAX_VALUE
                                  - ATTUNEMENT_MIN_VALUE
                                  + 1U)));
}

uint8_t attunement_measure_and_store(void)
{
    uint8_t value = attunement_measure();

    value = attunement_clamp(value);
    eeprom_store_write_attunement(value);

    attunement_value = value;
    attunement_valid = true;

    return value;
}
