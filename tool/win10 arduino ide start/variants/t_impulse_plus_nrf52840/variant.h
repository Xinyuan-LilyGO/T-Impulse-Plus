/*
  Copyright (c) 2014-2015 Arduino LLC. All right reserved.
  Copyright (c) 2016 Sandeep Mistry. All right reserved.
  Copyright (c) 2018 Adafruit Industries.

  This library is free software; you can redistribute it and/or modify it under
  the terms of the GNU Lesser General Public License as published by the Free
  Software Foundation; either version 2.1 of the License, or (at your option)
  any later version.
*/

#ifndef _VARIANT_T_IMPULSE_PLUS_NRF52840_
#define _VARIANT_T_IMPULSE_PLUS_NRF52840_

#define VARIANT_MCK (64000000ul)
#define USE_LFXO

#include "WVariant.h"

#ifdef __cplusplus
extern "C" {
#endif

#define _PINNUM(port, pin) ((port) * 32 + (pin))

#define PINS_COUNT (48)
#define NUM_DIGITAL_PINS (48)
#define NUM_ANALOG_INPUTS (6)
#define NUM_ANALOG_OUTPUTS (0)

#define PIN_LED1 (_PINNUM(0, 17))
#define LED_BUILTIN PIN_LED1
#define LED_BLUE PIN_LED1
#define LED_STATE_ON 0

#define PIN_BUTTON1 (_PINNUM(0, 24))

#define PIN_A0 (3)
#define PIN_A1 (4)
#define PIN_A2 (28)
#define PIN_A3 (29)
#define PIN_A4 (30)
#define PIN_A5 (31)
#define PIN_A6 (0xff)
#define PIN_A7 (0xff)

static const uint8_t A0 = PIN_A0;
static const uint8_t A1 = PIN_A1;
static const uint8_t A2 = PIN_A2;
static const uint8_t A3 = PIN_A3;
static const uint8_t A4 = PIN_A4;
static const uint8_t A5 = PIN_A5;
static const uint8_t A6 = PIN_A6;
static const uint8_t A7 = PIN_A7;
#define ADC_RESOLUTION 14

#define PIN_AREF (2)
#define PIN_NFC1 (9)
#define PIN_NFC2 (10)
static const uint8_t AREF = PIN_AREF;

#define PIN_SERIAL1_RX (33)
#define PIN_SERIAL1_TX (34)
#define PIN_SERIAL2_RX (8)
#define PIN_SERIAL2_TX (6)

#define SPI_INTERFACES_COUNT 1
#define PIN_SPI_MISO (46)
#define PIN_SPI_MOSI (45)
#define PIN_SPI_SCK (47)
static const uint8_t SS = 44;
static const uint8_t MOSI = PIN_SPI_MOSI;
static const uint8_t MISO = PIN_SPI_MISO;
static const uint8_t SCK = PIN_SPI_SCK;

#define WIRE_INTERFACES_COUNT 2
#define PIN_WIRE_SDA (26)
#define PIN_WIRE_SCL (27)
#define PIN_WIRE1_SDA (26)
#define PIN_WIRE1_SCL (27)

#ifdef __cplusplus
}
#endif

#endif
