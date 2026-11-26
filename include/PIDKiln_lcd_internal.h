#pragma once
/*
 * Shared LCD helpers / fonts for PIDKiln_lcd*.cpp (local UI builds only).
 */
#include <U8g2lib.h>
#include "PIDKiln.h"

#define FONT4 u8g2_font_p01type_tr
#define FONT5 u8g2_font_micro_tr
#define FONT6 u8g2_font_5x8_tr
#define FONT7 u8g2_font_6x10_tr
#define FONT8 u8g2_font_bitcasual_tr

extern U8G2_ST7920_128X64_F_HW_SPI u8g2;

void DrawVline(uint16_t x, uint16_t y, uint16_t h);
void Draw_Marked_string(char *str, uint8_t pos);
void DrawMenuEl(char *msg, uint16_t y, uint8_t cnt, uint8_t el, boolean sel);
