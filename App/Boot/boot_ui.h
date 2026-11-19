/*
 * boot_ui.h - GalaxyBoot status on the OLED (10 characters x 6 lines, 5x7
 * font, upper case) and on the console (USART1, 115200).
 */
#ifndef BOOT_UI_H
#define BOOT_UI_H

#define BOOT_UI_COLS  10
#define BOOT_UI_ROWS  6

void BootUi_Init(void);
void BootUi_Clear(void);
void BootUi_Line(int row, const char *text);      /* replaces the row */
void BootUi_Bar(int row, unsigned percent);       /* progress bar across the row */
void BootUi_Show(void);                           /* sends the picture to the OLED */

/* one console line (CR LF added) */
void Boot_Log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
