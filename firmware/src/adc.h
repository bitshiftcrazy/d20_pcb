/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#ifndef ADC_H
#define ADC_H

#include <stdint.h>

/*
 * Initialize the ADC peripheral for single-shot reads.
 * Blocking.
 */
void adc_init(void);

/*
 * Perform one ADC conversion on the selected channel.
 * Blocking
 */
uint16_t adc_read(uint8_t channel);

/*
 * Perform multiple ADC conversions and return the arithmetic mean.
 * Blocking.
 */
uint16_t adc_read_avg(uint8_t channel, uint8_t samples);

#endif /* ADC_H */
