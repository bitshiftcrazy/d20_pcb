/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#include "charlieplex.h"

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <string.h>

#include "config.h"
#include "pins.h"

/* ------------------------------------------------------------------------- */
/* Charlieplex pin order                                                     */
/* ------------------------------------------------------------------------- */
/*
 * The matrix uses this fixed pin order:
 *   0 -> A
 *   1 -> B
 *   2 -> C
 *   3 -> D
 *   4 -> E
 *
 * Do not change this order without updating the matrix definition.
 */

typedef enum {
    CP_PIN_A = 0,
    CP_PIN_B = 1,
    CP_PIN_C = 2,
    CP_PIN_D = 3,
    CP_PIN_E = 4
} cp_pin_index_t;

/* ------------------------------------------------------------------------- */
/* Pin state representation                                                  */
/* ------------------------------------------------------------------------- */

typedef enum {
    CP_INPUT  = 0,
    CP_OUTPUT = 1
} cp_mode_t;

typedef enum {
    CP_LOW  = 0,
    CP_HIGH = 1
} cp_level_t;

/* ------------------------------------------------------------------------- */
/* Matrix entry                                                              */
/* ------------------------------------------------------------------------- */

typedef struct {
    uint8_t mode[CHARLIE_PIN_COUNT];
    uint8_t level[CHARLIE_PIN_COUNT];
} cp_led_config_t;

/* ------------------------------------------------------------------------- */
/* Charlieplex matrix                                                        */
/* ------------------------------------------------------------------------- */

/*
 * Charlieplex truth table for the 20 LEDs on the d20 PCB.
 *
 * Each matrix entry is indexed by logical LED number [0..19] and stores
 * the required state for the five charlieplex lines in fixed pin order:
 *
 *   [A, B, C, D, E]
 *
 * For each entry:
 * - exactly one line is CP_OUTPUT + CP_HIGH  -> current source
 * - exactly one line is CP_OUTPUT + CP_LOW   -> current sink
 * - all remaining lines are CP_INPUT         -> high impedance
 *
 * Entries are grouped by pin pair, each pair contains both current
 * directions for the corresponding two charlieplex lines.
 *
 * The matrix is intentionally stored explicitly for readability.
 *
 * Note: I saw this handy way of storing a charlieplexing matrix
 * somewhere online, if you know the original source, pls
 * let me know, I'd like to credit them!
 */

static const cp_led_config_t charlie_matrix[LED_COUNT] PROGMEM = {
    /* A <-> B */
    { {CP_OUTPUT, CP_OUTPUT, CP_INPUT,  CP_INPUT,  CP_INPUT},
      {CP_HIGH,   CP_LOW,    CP_LOW,    CP_LOW,    CP_LOW} },

    { {CP_OUTPUT, CP_OUTPUT, CP_INPUT,  CP_INPUT,  CP_INPUT},
      {CP_LOW,    CP_HIGH,   CP_LOW,    CP_LOW,    CP_LOW} },

    /* A <-> C */
    { {CP_OUTPUT, CP_INPUT,  CP_OUTPUT, CP_INPUT,  CP_INPUT},
      {CP_HIGH,   CP_LOW,    CP_LOW,    CP_LOW,    CP_LOW} },

    { {CP_OUTPUT, CP_INPUT,  CP_OUTPUT, CP_INPUT,  CP_INPUT},
      {CP_LOW,    CP_LOW,    CP_HIGH,   CP_LOW,    CP_LOW} },

    /* B <-> D */
    { {CP_INPUT,  CP_OUTPUT, CP_INPUT,  CP_OUTPUT, CP_INPUT},
      {CP_LOW,    CP_HIGH,   CP_LOW,    CP_LOW,    CP_LOW} },

    { {CP_INPUT,  CP_OUTPUT, CP_INPUT,  CP_OUTPUT, CP_INPUT},
      {CP_LOW,    CP_LOW,    CP_LOW,    CP_HIGH,   CP_LOW} },

    /* A <-> D */
    { {CP_OUTPUT, CP_INPUT,  CP_INPUT,  CP_OUTPUT, CP_INPUT},
      {CP_HIGH,   CP_LOW,    CP_LOW,    CP_LOW,    CP_LOW} },

    { {CP_OUTPUT, CP_INPUT,  CP_INPUT,  CP_OUTPUT, CP_INPUT},
      {CP_LOW,    CP_LOW,    CP_LOW,    CP_HIGH,   CP_LOW} },

    /* B <-> C */
    { {CP_INPUT,  CP_OUTPUT, CP_OUTPUT, CP_INPUT,  CP_INPUT},
      {CP_LOW,    CP_HIGH,   CP_LOW,    CP_LOW,    CP_LOW} },

    { {CP_INPUT,  CP_OUTPUT, CP_OUTPUT, CP_INPUT,  CP_INPUT},
      {CP_LOW,    CP_LOW,    CP_HIGH,   CP_LOW,    CP_LOW} },

    /* C <-> E */
    { {CP_INPUT,  CP_INPUT,  CP_OUTPUT, CP_INPUT,  CP_OUTPUT},
      {CP_LOW,    CP_LOW,    CP_HIGH,   CP_LOW,    CP_LOW} },

    { {CP_INPUT,  CP_INPUT,  CP_OUTPUT, CP_INPUT,  CP_OUTPUT},
      {CP_LOW,    CP_LOW,    CP_LOW,    CP_LOW,    CP_HIGH} },

    /* C <-> D */
    { {CP_INPUT,  CP_INPUT,  CP_OUTPUT, CP_OUTPUT, CP_INPUT},
      {CP_LOW,    CP_LOW,    CP_HIGH,   CP_LOW,    CP_LOW} },

    { {CP_INPUT,  CP_INPUT,  CP_OUTPUT, CP_OUTPUT, CP_INPUT},
      {CP_LOW,    CP_LOW,    CP_LOW,    CP_HIGH,   CP_LOW} },

    /* D <-> E */
    { {CP_INPUT,  CP_INPUT,  CP_INPUT,  CP_OUTPUT, CP_OUTPUT},
      {CP_LOW,    CP_LOW,    CP_LOW,    CP_HIGH,   CP_LOW} },

    { {CP_INPUT,  CP_INPUT,  CP_INPUT,  CP_OUTPUT, CP_OUTPUT},
      {CP_LOW,    CP_LOW,    CP_LOW,    CP_LOW,    CP_HIGH} },

    /* B <-> E */
    { {CP_INPUT,  CP_OUTPUT, CP_INPUT,  CP_INPUT,  CP_OUTPUT},
      {CP_LOW,    CP_HIGH,   CP_LOW,    CP_LOW,    CP_LOW} },

    { {CP_INPUT,  CP_OUTPUT, CP_INPUT,  CP_INPUT,  CP_OUTPUT},
      {CP_LOW,    CP_LOW,    CP_LOW,    CP_LOW,    CP_HIGH} },

    /* A <-> E */
    { {CP_OUTPUT, CP_INPUT,  CP_INPUT,  CP_INPUT,  CP_OUTPUT},
      {CP_HIGH,   CP_LOW,    CP_LOW,    CP_LOW,    CP_LOW} },

    { {CP_OUTPUT, CP_INPUT,  CP_INPUT,  CP_INPUT,  CP_OUTPUT},
      {CP_LOW,    CP_LOW,    CP_LOW,    CP_LOW,    CP_HIGH} }
};

/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

static void cp_set_mode(volatile uint8_t *ddr, uint8_t bit, cp_mode_t mode)
{
    if (mode == CP_OUTPUT) {
        *ddr |= (1U << bit);
    } else {
        *ddr &= (uint8_t)~(1U << bit);
    }
}

static void cp_set_level(volatile uint8_t *port, uint8_t bit, cp_level_t level)
{
    if (level == CP_HIGH) {
        *port |= (1U << bit);
    } else {
        *port &= (uint8_t)~(1U << bit);
    }
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

void charlieplex_init(void)
{
    charlieplex_all_off();
}

void charlieplex_all_off(void)
{
    /* Put all charlieplex lines into high impedance. */
    LED_A_DDR &= (uint8_t)~(1U << LED_A_PIN);
    LED_B_DDR &= (uint8_t)~(1U << LED_B_PIN);
    LED_C_DDR &= (uint8_t)~(1U << LED_C_PIN);
    LED_D_DDR &= (uint8_t)~(1U << LED_D_PIN);
    LED_E_DDR &= (uint8_t)~(1U << LED_E_PIN);

    /* Clear output latches so later output enables start from a known low state. */
    LED_A_PORT &= (uint8_t)~(1U << LED_A_PIN);
    LED_B_PORT &= (uint8_t)~(1U << LED_B_PIN);
    LED_C_PORT &= (uint8_t)~(1U << LED_C_PIN);
    LED_D_PORT &= (uint8_t)~(1U << LED_D_PIN);
    LED_E_PORT &= (uint8_t)~(1U << LED_E_PIN);
}

void charlieplex_led_on(uint8_t led_index)
{
    cp_led_config_t cfg;

    if (led_index >= LED_COUNT) {
        return;
    }

    memcpy_P(&cfg, &charlie_matrix[led_index], sizeof(cfg));

    /* Avoid ghosting during transitions. */
    charlieplex_all_off();

    /*
     * Set output latch levels while all lines are still high-Z, then enable
     * only the required outputs. This avoids transient drive conflicts and
     * ghosting.
     */
    cp_set_level(&LED_A_PORT, LED_A_PIN, (cp_level_t)cfg.level[CP_PIN_A]);
    cp_set_level(&LED_B_PORT, LED_B_PIN, (cp_level_t)cfg.level[CP_PIN_B]);
    cp_set_level(&LED_C_PORT, LED_C_PIN, (cp_level_t)cfg.level[CP_PIN_C]);
    cp_set_level(&LED_D_PORT, LED_D_PIN, (cp_level_t)cfg.level[CP_PIN_D]);
    cp_set_level(&LED_E_PORT, LED_E_PIN, (cp_level_t)cfg.level[CP_PIN_E]);

    cp_set_mode(&LED_A_DDR, LED_A_PIN, (cp_mode_t)cfg.mode[CP_PIN_A]);
    cp_set_mode(&LED_B_DDR, LED_B_PIN, (cp_mode_t)cfg.mode[CP_PIN_B]);
    cp_set_mode(&LED_C_DDR, LED_C_PIN, (cp_mode_t)cfg.mode[CP_PIN_C]);
    cp_set_mode(&LED_D_DDR, LED_D_PIN, (cp_mode_t)cfg.mode[CP_PIN_D]);
    cp_set_mode(&LED_E_DDR, LED_E_PIN, (cp_mode_t)cfg.mode[CP_PIN_E]);
}
