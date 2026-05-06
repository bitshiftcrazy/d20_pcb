/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#ifndef CONFIG_H
#define CONFIG_H

/* -------------------------------------------------------------------------- */
/* Core clock                                                                 */
/* -------------------------------------------------------------------------- */

#ifndef F_CPU
#define F_CPU                    8000000UL
#endif

/* -------------------------------------------------------------------------- */
/* Board-wide firmware constants                                              */
/* -------------------------------------------------------------------------- */

#define CHARLIE_PIN_COUNT        5U
#define LED_COUNT                20U
#define BUTTON_DEBOUNCE_MS       30UL
#define BUTTON_HOLD_TIME_MS      6000UL /* Hold button this long to re-attune */

/* -------------------------------------------------------------------------- */
/* EEPROM layout                                                              */
/* -------------------------------------------------------------------------- */

#define EEPROM_ADDR_ATTUNEMENT   1U /* EEPROM byte 0 is intentionally unused. */
#define EEPROM_ADDR_FLAG         3U  /* Some padding for readability in dump. */
#define ATTUNEMENT_FLAG          0xA5U  /* EEPROM marker for valid attunement */

/* -------------------------------------------------------------------------- */
/* Roll behavior                                                              */
/* -------------------------------------------------------------------------- */

#define NAT1_BIAS_TRIGGER        3U     /* Start cursed bias after 3 nat1s    */
#define NAT20_BIAS_TRIGGER       3U     /* Start blessed bias after 3 nat20s  */

#define LOW_BIAS_MIN_ROLL        0U
#define LOW_BIAS_MAX_ROLL        5U

#define HIGH_BIAS_MIN_ROLL       15U
#define HIGH_BIAS_MAX_ROLL       19U

#define UPSET_NAT1_TRIGGER       11U    /* The dice is no longer joking      */

#define UPSET_INPUT_DENOMINATOR  8U
#define UPSET_INPUT_THRESHOLD    2U     /* 25% chance to ignore roll input   */

#define UPSET_GLITCH_DENOMINATOR 6U
#define UPSET_GLITCH_THRESHOLD   2U     /* ~33% chance to stutter before roll */

#define GODMODE_MIN_ROLL         15U
#define GODMODE_MAX_ROLL         19U

/* -------------------------------------------------------------------------- */
/* Attunement range                                                           */
/* -------------------------------------------------------------------------- */

#define ATTUNEMENT_MIN_VALUE     11U    /* Themed attunement range: 11..111   */
#define ATTUNEMENT_MAX_VALUE     111U

/* -------------------------------------------------------------------------- */
/* Character selection                                                        */
/* -------------------------------------------------------------------------- */

#define CHARACTER_ASHTAN         1U
#define CHARACTER_BARTHOLOMEW    2U
#define CHARACTER_FAWN           3U
#define CHARACTER_SALEM          4U
#define CHARACTER_VII            5U

#ifndef CHARACTER_ID
#define CHARACTER_ID             CHARACTER_VII
#endif

#if CHARACTER_ID == CHARACTER_ASHTAN
#define CHARACTER_NAME           "ASHTAN"
#elif CHARACTER_ID == CHARACTER_BARTHOLOMEW
#define CHARACTER_NAME           "BARTHOLOMEW"
#elif CHARACTER_ID == CHARACTER_FAWN
#define CHARACTER_NAME           "FAWN"
#elif CHARACTER_ID == CHARACTER_SALEM
#define CHARACTER_NAME           "SALEM"
#elif CHARACTER_ID == CHARACTER_VII
#define CHARACTER_NAME           "VII"
#else
#error "Invalid CHARACTER_ID"
#endif

#endif /* CONFIG_H */
