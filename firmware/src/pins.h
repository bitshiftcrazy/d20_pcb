/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#ifndef PINS_H
#define PINS_H

#include <avr/io.h>

/*
 * ATtiny84 pin mapping for the d20 PCB
 *
 *                    ATtiny84
 *               +-----------------+
 *         VCC --| 1            14 |-- GND
 *   NC / PB0  --| 2            13 |-- PA0 / A
 * BUTTON / PB1 -| 3            12 |-- PA1 / B
 * RESET / PB3 --| 4            11 |-- PA2 / C
 *      E / PB2 -| 5            10 |-- PA3 / D
 *   PAD / PA7 --| 6             9 |-- PA4 / SCK
 *   MOSI / PA6 -| 7             8 |-- PA5 / MISO
 *               +-----------------+
 *
 * Charlieplex lines:
 *   A -> PA0
 *   B -> PA1
 *   C -> PA2
 *   D -> PA3
 *   E -> PB2
 *
 * Other signals:
 *   BUTTON -> PB1
 *   TOUCH  -> PA7 (ADC7)
 */

/* ------------------------------------------------------------------------- */
/* Charlieplex LED driver pins                                               */
/* ------------------------------------------------------------------------- */

#define LED_A_PORT    PORTA
#define LED_A_DDR     DDRA
#define LED_A_PIN     PA0

#define LED_B_PORT    PORTA
#define LED_B_DDR     DDRA
#define LED_B_PIN     PA1

#define LED_C_PORT    PORTA
#define LED_C_DDR     DDRA
#define LED_C_PIN     PA2

#define LED_D_PORT    PORTA
#define LED_D_DDR     DDRA
#define LED_D_PIN     PA3

#define LED_E_PORT    PORTB
#define LED_E_DDR     DDRB
#define LED_E_PIN     PB2

/* ------------------------------------------------------------------------- */
/* Button input                                                              */
/* ------------------------------------------------------------------------- */

#define BUTTON_PORT   PORTB
#define BUTTON_DDR    DDRB
#define BUTTON_PINREG PINB
#define BUTTON_BIT    PB1

/* ------------------------------------------------------------------------- */
/* Capacitive touch pad                                                      */
/* ------------------------------------------------------------------------- */

#define TOUCH_PORT    PORTA
#define TOUCH_DDR     DDRA
#define TOUCH_PINREG  PINA
#define TOUCH_BIT     PA7

/* ADC channel for touch pad */
#define TOUCH_ADC_CHANNEL 7

#endif /* PINS_H */
