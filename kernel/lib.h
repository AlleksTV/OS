/* ============================================================================
 * lib.h — мини-"стандартная библиотека" ядра
 * ============================================================================
 * Функции работы со строками и памятью, которые в обычной программе
 * берутся из libc (string.h, stdlib.h). Здесь мы написали их сами.
 * ==========================================================================*/
#ifndef LIB_H
#define LIB_H

#include "types.h"

/* Переменное число аргументов (как у printf) — встроенные средства GCC */
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)

void  *memset(void *dst, int value, size_t n);
void  *memcpy(void *dst, const void *src, size_t n);
void  *memmove(void *dst, const void *src, size_t n);
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strncpy(char *dst, const char *src, size_t n);
int    atoi(const char *s);
bool   is_number(const char *s);

/* Форматированный вывод. Поддерживает %s %c %d %u %x %% и ширину (%5d, %02d).
 * Каждый получившийся символ передаётся функции out() вместе с ctx. */
typedef void (*out_fn)(char c, void *ctx);
void format(out_fn out, void *ctx, const char *fmt, va_list args);

/* Как snprintf: форматирует строку в буфер buf размером size */
int ksnprintf(char *buf, size_t size, const char *fmt, ...);

#endif
