#pragma once
/*
** Decode one MAX31855 32-bit SPI word into TC + cold-junction temps.
** Matches Adafruit_MAX31855 bit layout (fault mask 0x07, TC 0.25 C, internal 0.0625 C).
*/
#include <Arduino.h>
#include <SPI.h>
#include <math.h>
#include <stdint.h>

struct Max31855Sample {
  double tc_c;
  double internal_c;
  uint8_t fault;
  bool ok;
};

static inline Max31855Sample Max31855_Decode(uint32_t v, uint8_t fault_mask = 0x07) {
  Max31855Sample s;
  s.fault = (uint8_t)(v & 0x07);
  s.ok = false;
  s.tc_c = NAN;
  s.internal_c = NAN;

  /* Internal / cold junction — bits [15:4], 0.0625 C LSB */
  {
    uint32_t iv = v >> 4;
    float internal = (float)(iv & 0x7FF);
    if (iv & 0x800) {
      int16_t tmp = (int16_t)(0xF800 | (iv & 0x7FF));
      internal = (float)tmp;
    }
    s.internal_c = internal * 0.0625f;
  }

  if (v & fault_mask)
    return s;

  int32_t tv = (int32_t)v;
  if (tv & 0x80000000)
    tv = (int32_t)(0xFFFFC000 | ((tv >> 18) & 0x00003FFF));
  else
    tv >>= 18;
  s.tc_c = (double)tv * 0.25;
  s.ok = true;
  return s;
}

static inline uint32_t Max31855_SpiRead32(SPIClass *spi, uint8_t cs) {
  uint8_t buf[4];
  spi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(cs, LOW);
  for (int i = 0; i < 4; i++)
    buf[i] = spi->transfer(0);
  digitalWrite(cs, HIGH);
  spi->endTransaction();
  return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
         ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
}
