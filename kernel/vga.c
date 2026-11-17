/* ============================================================================
 * vga.c — драйвер текстового экрана VGA
 * ============================================================================
 * Как ОС выводит текст на экран?
 * В текстовом режиме 80x25 видеокарта постоянно (60 раз в секунду) читает
 * специальную область памяти — ВИДЕОПАМЯТЬ — по адресу 0xB8000 и рисует
 * по ней символы. Это называется "отображение в память" (memory-mapped I/O).
 *
 * Каждая клетка экрана = 2 байта:
 *      байт 0: ASCII-код символа
 *      байт 1: атрибут (цвет): [фон 4 бита][символ 4 бита]
 *
 * Клетка (x, y) находится по адресу 0xB8000 + (y * 80 + x) * 2.
 * Чтобы вывести букву 'A' белым по синему в левый верхний угол, достаточно:
 *      *(uint16_t*)0xB8000 = 'A' | (0x1F << 8);
 * Никаких специальных команд — просто запись в память!
 *
 * Курсор (мигающая черта) — отдельно: его положение задаётся через порты
 * видеоконтроллера 0x3D4 (номер регистра) и 0x3D5 (значение).
 * ==========================================================================*/
#include "vga.h"
#include "ports.h"
#include "lib.h"

static uint16_t *const video = (uint16_t *)0xB8000;   /* Видеопамять */
static int cursor_x = 0;
static int cursor_y = VGA_TOP;
static uint8_t color = VGA_COLOR(LIGHT_GRAY, BLACK);

/* Собрать 16-битное значение клетки: символ + цвет */
static inline uint16_t cell(char c, uint8_t attr) {
    return (uint16_t)(uint8_t)c | ((uint16_t)attr << 8);
}

/* Переместить аппаратный курсор в позицию (cursor_x, cursor_y) */
static void update_cursor(void) {
    uint16_t pos = (uint16_t)(cursor_y * VGA_WIDTH + cursor_x);
    outb(0x3D4, 0x0F);                  /* Регистр 0x0F: младший байт позиции */
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);                  /* Регистр 0x0E: старший байт позиции */
    outb(0x3D5, (uint8_t)(pos >> 8));
}

/* Прокрутка: сдвигаем все строки (кроме строки состояния) на одну вверх,
 * а последнюю строку очищаем. Это просто копирование памяти! */
static void scroll(void) {
    memmove(&video[VGA_TOP * VGA_WIDTH],
            &video[(VGA_TOP + 1) * VGA_WIDTH],
            (VGA_HEIGHT - VGA_TOP - 1) * VGA_WIDTH * 2);
    for (int x = 0; x < VGA_WIDTH; x++)
        video[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = cell(' ', color);
    cursor_y = VGA_HEIGHT - 1;
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    color = VGA_COLOR(fg, bg);
}

/* Очистить рабочую область экрана (строка состояния не трогается) */
void vga_clear(void) {
    for (int i = VGA_TOP * VGA_WIDTH; i < VGA_WIDTH * VGA_HEIGHT; i++)
        video[i] = cell(' ', color);
    cursor_x = 0;
    cursor_y = VGA_TOP;
    update_cursor();
}

void vga_init(void) {
    /* Строка состояния: белый текст на синем фоне */
    for (int x = 0; x < VGA_WIDTH; x++)
        video[x] = cell(' ', VGA_COLOR(WHITE, BLUE));
    vga_write_at(1, 0, "MiniOS v1.0", VGA_COLOR(YELLOW, BLUE));
    vga_clear();
}

/* Вывести один символ в позицию курсора с обработкой управляющих символов */
void vga_putc(char c) {
    if (c == '\n') {                    /* Перевод строки */
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {             /* Возврат каретки */
        cursor_x = 0;
    } else if (c == '\b') {             /* Backspace: шаг назад и стереть */
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > VGA_TOP) {
            cursor_y--;
            cursor_x = VGA_WIDTH - 1;
        }
        video[cursor_y * VGA_WIDTH + cursor_x] = cell(' ', color);
    } else if (c == '\t') {             /* Табуляция: до позиции, кратной 8 */
        cursor_x = (cursor_x + 8) & ~7;
    } else {                            /* Обычный символ — пишем в видеопамять */
        video[cursor_y * VGA_WIDTH + cursor_x] = cell(c, color);
        cursor_x++;
    }

    if (cursor_x >= VGA_WIDTH) {        /* Дошли до правого края — перенос */
        cursor_x = 0;
        cursor_y++;
    }
    if (cursor_y >= VGA_HEIGHT)         /* Ушли за нижний край — прокрутка */
        scroll();

    update_cursor();
}

void vga_print(const char *s) {
    while (*s) vga_putc(*s++);
}

/* printf для ядра: разбор формата делает функция format() из lib.c,
 * а каждый получившийся символ выводится через vga_putc */
static void vga_out(char c, void *ctx) {
    (void)ctx;
    vga_putc(c);
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    format(vga_out, NULL, fmt, args);
    va_end(args);
}

void vga_write_at(int x, int y, const char *s, uint8_t attr) {
    while (*s && x < VGA_WIDTH)
        video[y * VGA_WIDTH + x++] = cell(*s++, attr);
}
