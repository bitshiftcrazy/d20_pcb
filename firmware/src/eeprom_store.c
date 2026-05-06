/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#include "eeprom_store.h"

#include <avr/eeprom.h>

#include "config.h"

bool eeprom_store_has_attunement(void)
{
    uint8_t flag = eeprom_read_byte((const uint8_t *)EEPROM_ADDR_FLAG);
    return (flag == ATTUNEMENT_FLAG);
}

uint8_t eeprom_store_read_attunement(void)
{
    if (!eeprom_store_has_attunement()) {
        return ATTUNEMENT_MIN_VALUE;
    }

    return eeprom_read_byte((const uint8_t *)EEPROM_ADDR_ATTUNEMENT);
}

void eeprom_store_write_attunement(uint8_t value)
{
    /*
     * Write value first, then flag.
     * This avoids marking EEPROM as valid before the payload is stored.
     */
    eeprom_update_byte((uint8_t *)EEPROM_ADDR_ATTUNEMENT, value);
    eeprom_update_byte((uint8_t *)EEPROM_ADDR_FLAG, ATTUNEMENT_FLAG);
}

void eeprom_store_clear_attunement(void)
{
    eeprom_update_byte((uint8_t *)EEPROM_ADDR_FLAG, 0xFF);
}

