/*
 * oled.h - 0.66" 64x48 SSD1306 OLED on SPI1 (PA5 SCK, PA7 MOSI, PA4 CS,
 * PB15 D/C, PB14 RESET).
 *
 * The panel uses columns 32..95 of the controller's 128 x 64 RAM.
 * Oled_Flush() takes a 1 bit per pixel frame in TouchGFX order: rows top to
 * bottom, 8 pixels per byte, most significant bit = leftmost pixel.
 */
#ifndef APP_OLED_H
#define APP_OLED_H

#include <stdint.h>

#define OLED_WIDTH   64
#define OLED_HEIGHT  48
#define OLED_FB_SIZE (OLED_WIDTH * OLED_HEIGHT / 8)

void Oled_Init(void);                       /* reset + init sequence (uses osDelay/HAL_Delay) */
void Oled_Flush(const uint8_t *fb);         /* OLED_FB_SIZE bytes, row-major 1bpp */
void Oled_SetContrast(uint8_t c);
void Oled_Power(int on);
void Oled_Invert(int on);

/* pure helper (tested on the PC): row-major 1bpp -> SSD1306 pages */
void Oled_ToPages(const uint8_t *fb, uint8_t pages[OLED_FB_SIZE]);

#endif
