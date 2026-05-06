/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#ifndef EEPROM_STORE_H
#define EEPROM_STORE_H

#include <stdbool.h>
#include <stdint.h>

/* Return true if EEPROM contains a valid stored attunement. */
bool eeprom_store_has_attunement(void);

/* Read stored attunement, or return ATTUNEMENT_MIN_VALUE if unset. */
uint8_t eeprom_store_read_attunement(void);

/* Store attunement value and mark it valid. */
void eeprom_store_write_attunement(uint8_t value);

/*
 * Invalidate the stored attunement flag.
 *
 * Intended for diagnostics and test builds, normal firmware currently
 * re-attunes by overwriting the stored value.
 */
void eeprom_store_clear_attunement(void);

#endif /* EEPROM_STORE_H */
