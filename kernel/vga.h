/* ============================================================================
 * vga.h — драйвер текстового экрана VGA
 * ==========================================================================*/
#ifndef VGA_H
#define VGA_H

#include "types.h"

#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_TOP     1      /* Строка 0 занята строкой состояния */

/* 16 стандартных цветов текстового режима VGA */
enum vga_color {
    BLACK = 0, BLUE = 1, GREEN = 2, CYAN = 3, RED = 4, MAGENTA = 5,
    BROWN = 6, LIGHT_GRAY = 7, DARK_GRAY = 8, LIGHT_BLUE = 9,
    LIGHT_GREEN = 10, LIGHT_CYAN = 11, LIGHT_RED = 12, LIGHT_MAGENTA = 13,
    YELLOW = 14, WHITE = 15,
};

/* Байт цвета: старшие 4 бита — фон, младшие 4 бита — цвет символа */
#define VGA_COLOR(fg, bg) ((uint8_t)((bg) << 4 | (fg)))

void vga_init(void);
void vga_clear(void);
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_putc(char c);
void vga_print(const char *s);
void kprintf(const char *fmt, ...);

/* Запись строки в заданную позицию экрана, без перемещения курсора.
 * Используется фоновыми задачами для вывода в строку состояния. */
void vga_write_at(int x, int y, const char *s, uint8_t color);

#endif
