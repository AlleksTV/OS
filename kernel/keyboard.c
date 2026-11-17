/* ============================================================================
 * keyboard.c — драйвер клавиатуры
 * ============================================================================
 * Как работает клавиатура?
 *   1. Пользователь нажимает клавишу.
 *   2. Контроллер клавиатуры кладёт в порт 0x60 СКАН-КОД — номер клавиши
 *      (не символ!) и посылает прерывание IRQ1.
 *   3. Наш обработчик читает скан-код и переводит его в символ по таблице
 *      раскладки, учитывая Shift и Caps Lock.
 *   4. Символ кладётся в КОЛЬЦЕВОЙ БУФЕР, откуда его потом заберёт
 *      оболочка (shell) функцией keyboard_getchar().
 *
 * У каждой клавиши два скан-кода: "нажата" (make) и "отпущена" (break).
 * Код отпускания = код нажатия + 0x80 (старший бит установлен).
 * Например, 'A': нажатие 0x1E, отпускание 0x9E.
 *
 * Буфер нужен, потому что прерывание приходит в ЛЮБОЙ момент, а оболочка
 * читает символы в своём темпе. Это классическая задача
 * "производитель-потребитель".
 * ==========================================================================*/
#include "keyboard.h"
#include "interrupts.h"
#include "ports.h"

#define KBD_DATA_PORT 0x60
#define BUFFER_SIZE   256

/* Таблица перевода скан-кодов (набор 1, раскладка US) в символы.
 * Индекс — скан-код, значение — символ (0 = клавиша без символа). */
static const char keymap[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   /* 0x1D Ctrl */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   /* 0x2A Левый Shift */
    '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0,   /* 0x36 Правый Shift */
    '*', 0, /* Alt */ ' ', 0, /* Caps Lock */
};

/* То же самое с нажатым Shift */
static const char keymap_shift[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t','Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
    0,
    '*', 0, ' ', 0,
};

#define SC_LSHIFT    0x2A
#define SC_RSHIFT    0x36
#define SC_CAPSLOCK  0x3A
#define SC_EXTENDED  0xE0

static bool shift_pressed = false;
static bool caps_lock = false;
static bool extended = false;

/* Кольцевой буфер: head — куда писать, tail — откуда читать.
 * Когда индекс доходит до конца массива, он "заворачивается" на начало. */
static volatile char buffer[BUFFER_SIZE];
static volatile uint32_t head = 0, tail = 0;

static void buffer_put(char c) {
    uint32_t next = (head + 1) % BUFFER_SIZE;
    if (next != tail) {          /* Если буфер полон — символ теряется */
        buffer[head] = c;
        head = next;
    }
}

/* Обработчик прерывания IRQ1 */
static void keyboard_handler(struct regs *r) {
    (void)r;
    uint8_t scancode = inb(KBD_DATA_PORT);

    if (scancode == SC_EXTENDED) {      /* Префикс "расширенной" клавиши */
        extended = true;                /* (стрелки, Home, End ...) */
        return;
    }
    if (extended) {                     /* Расширенные клавиши пропускаем */
        extended = false;
        return;
    }

    if (scancode & 0x80) {              /* Клавиша ОТПУЩЕНА */
        uint8_t key = scancode & 0x7F;
        if (key == SC_LSHIFT || key == SC_RSHIFT)
            shift_pressed = false;
        return;
    }

    /* Клавиша НАЖАТА */
    if (scancode == SC_LSHIFT || scancode == SC_RSHIFT) {
        shift_pressed = true;
        return;
    }
    if (scancode == SC_CAPSLOCK) {
        caps_lock = !caps_lock;
        return;
    }

    char c = shift_pressed ? keymap_shift[scancode] : keymap[scancode];
    if (caps_lock) {                    /* Caps Lock меняет регистр букв */
        if (c >= 'a' && c <= 'z') c -= 32;
        else if (c >= 'A' && c <= 'Z') c += 32;
    }
    if (c)
        buffer_put(c);
}

void keyboard_init(void) {
    while (inb(0x64) & 1)               /* Выбросить то, что уже лежит */
        inb(KBD_DATA_PORT);             /* в буфере контроллера */
    irq_install_handler(1, keyboard_handler);
}

char keyboard_getchar(void) {
    while (head == tail)    /* Буфер пуст — спим до следующего прерывания. */
        hlt();              /* Пока мы спим, планировщик отдаёт процессор
                               другим задачам. */
    char c = buffer[tail];
    tail = (tail + 1) % BUFFER_SIZE;
    return c;
}
