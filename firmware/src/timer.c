/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#include "timer.h"

#include <avr/io.h>
#include <util/atomic.h>

/* ------------------------------------------------------------------------- */
/* Internal state                                                            */
/* ------------------------------------------------------------------------- */

static volatile uint32_t ms_counter = 0;

/* ------------------------------------------------------------------------- */
/* Interrupt handler                                                         */
/* ------------------------------------------------------------------------- */

/*
 * Timer0 Compare Match A interrupt
 *
 * F_CPU     = 8,000,000 Hz
 * Prescaler = 64
 * Timer tick = 8 us
 *
 * In CTC mode with OCR0A = 124:
 * (124 + 1) * 8 us = 1000 us = 1 ms
 */
ISR(TIM0_COMPA_vect)
{
    ms_counter++;
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

void timer_init(void)
{
    /* Stop timer while configuring */
    TCCR0A = 0;
    TCCR0B = 0;

    /* CTC mode: clear timer on compare match with OCR0A */
    TCCR0A = (1 << WGM01);

    /* 1 ms period at 8 MHz / 64 */
    OCR0A = 124;

    /* Clear counter */
    TCNT0 = 0;

    /* Enable Compare Match A interrupt */
    TIMSK0 = (1 << OCIE0A);

    /* Start timer with prescaler 64 */
    TCCR0B = (1 << CS01) | (1 << CS00);
}

uint32_t millis(void)
{
    uint32_t m;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        m = ms_counter;
    }

    return m;
}
