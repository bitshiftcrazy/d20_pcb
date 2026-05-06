/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#ifndef ATTUNEMENT_H
#define ATTUNEMENT_H

#include <stdbool.h>
#include <stdint.h>

/* Load cached attunement state from EEPROM into RAM. */
void attunement_init(void);

/* Return whether a valid attunement value is currently available. */
bool attunement_has_value(void);

/* Return the cached attunement value (may be ATTUNEMENT_MIN_VALUE if unset). */
uint8_t attunement_get_value(void);

/* Reload attunement state from EEPROM, returns true if valid data exists. */
bool attunement_load(void);

/*
 * Measure a new attunement value (does not store or update EEPROM).
 * Blocking.
 */
uint8_t attunement_measure(void);

/*
 * Measure, store, and cache a new attunement value.
 * Blocking.
 */
uint8_t attunement_measure_and_store(void);

#endif /* ATTUNEMENT_H */
