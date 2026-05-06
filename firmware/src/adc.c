/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#include "adc.h"

#include <avr/io.h>

/*
 * Minimal blocking ADC driver
 *
 * Uses single-shot conversions with a fixed prescaler.
 */

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

void adc_init(void)
{
    /* Enable ADC, prescaler = 64 (8 MHz / 64 = 125 kHz ADC clock). */
    ADCSRA =
        (1U << ADEN)  |
        (1U << ADPS2) |
        (1U << ADPS1);

    /* No auto trigger, default settings. */
    ADCSRB = 0;

    /* do NOT touch ADMUX here */
}

uint16_t adc_read(uint8_t channel)
{
    /* ATtiny84 uses ADMUX MUX[5:0] for channel selection. */
    ADMUX = (ADMUX & 0xC0U) | (channel & 0x3FU);

    ADCSRA |= (1U << ADSC);
    while (ADCSRA & (1U << ADSC))
        ;

    return ADC;
}

uint16_t adc_read_avg(uint8_t channel, uint8_t samples)
{
    if (samples == 0U) {
        return 0U;
    }

    uint32_t sum = 0U;

    for (uint8_t i = 0U; i < samples; i++) {
        sum += adc_read(channel);
    }

    return (uint16_t)(sum / samples);
}
