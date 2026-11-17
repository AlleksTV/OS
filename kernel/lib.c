/* ============================================================================
 * lib.c — реализация мини-библиотеки (строки, память, форматирование)
 * ==========================================================================*/
#include "lib.h"

void *memset(void *dst, int value, size_t n) {
    uint8_t *p = (uint8_t *)dst;
    while (n--) *p++ = (uint8_t)value;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dst;
}

/* Как memcpy, но корректно работает, если области памяти перекрываются */
void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (n == 0) return 0;
    return (uint8_t)*a - (uint8_t)*b;
}

/* Копирует строку, но не более n-1 символов, и ВСЕГДА ставит завершающий 0 */
char *strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    if (n == 0) return dst;
    for (; i < n - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
    return dst;
}

bool is_number(const char *s) {
    if (*s == '-') s++;
    if (!*s) return false;
    while (*s) {
        if (*s < '0' || *s > '9') return false;
        s++;
    }
    return true;
}

int atoi(const char *s) {
    int sign = 1, result = 0;
    if (*s == '-') { sign = -1; s++; }
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }
    return result * sign;
}

/* Вывести беззнаковое число в системе счисления base с минимальной шириной */
static void print_unsigned(out_fn out, void *ctx, uint32_t value, uint32_t base,
                           int width, char pad, bool negative) {
    char buf[16];
    int len = 0;
    const char *digits = "0123456789ABCDEF";

    /* Получаем цифры в обратном порядке: 123 -> '3','2','1' */
    do {
        buf[len++] = digits[value % base];
        value /= base;
    } while (value);

    if (negative) {
        if (pad == '0') { out('-', ctx); width--; }
        else buf[len++] = '-';
    }
    while (width-- > len) out(pad, ctx);       /* Дополняем до нужной ширины */
    while (len--) out(buf[len], ctx);           /* Печатаем в правильном порядке */
}

void format(out_fn out, void *ctx, const char *fmt, va_list args) {
    for (; *fmt; fmt++) {
        if (*fmt != '%') { out(*fmt, ctx); continue; }
        fmt++;

        /* Разбираем ширину поля: "%5d" или "%02d" */
        char pad = ' ';
        int width = 0;
        if (*fmt == '0') { pad = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');

        switch (*fmt) {
        case 's': {
            const char *s = va_arg(args, const char *);
            if (!s) s = "(null)";
            int len = (int)strlen(s);
            while (*s) out(*s++, ctx);
            while (width-- > len) out(' ', ctx);    /* Строки дополняем справа */
            break;
        }
        case 'c': out((char)va_arg(args, int), ctx); break;
        case 'd': {
            int v = va_arg(args, int);
            if (v < 0) print_unsigned(out, ctx, (uint32_t)(-v), 10, width, pad, true);
            else       print_unsigned(out, ctx, (uint32_t)v, 10, width, pad, false);
            break;
        }
        case 'u': print_unsigned(out, ctx, va_arg(args, uint32_t), 10, width, pad, false); break;
        case 'x': print_unsigned(out, ctx, va_arg(args, uint32_t), 16, width, pad, false); break;
        case '%': out('%', ctx); break;
        case 0:   return;
        default:  out('%', ctx); out(*fmt, ctx); break;
        }
    }
}

/* ---- ksnprintf: вывод не на экран, а в буфер в памяти ---- */
struct buf_ctx { char *buf; size_t size; size_t pos; };

static void buf_out(char c, void *ctx) {
    struct buf_ctx *b = (struct buf_ctx *)ctx;
    if (b->pos + 1 < b->size)
        b->buf[b->pos++] = c;
}

int ksnprintf(char *buf, size_t size, const char *fmt, ...) {
    struct buf_ctx b = { buf, size, 0 };
    va_list args;
    va_start(args, fmt);
    format(buf_out, &b, fmt, args);
    va_end(args);
    if (size) buf[b.pos] = 0;
    return (int)b.pos;
}
