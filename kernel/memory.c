/* ============================================================================
 * memory.c — менеджер памяти (аналог malloc/free)
 * ============================================================================
 * Программам постоянно нужна память "на время": под файл, под стек новой
 * задачи, под строку. Задача менеджера памяти — выдавать куски памяти по
 * запросу (kmalloc) и забирать их обратно (kfree), чтобы использовать снова.
 *
 * Область памяти, из которой выдаются куски, называется КУЧЕЙ (heap).
 * У нас это 4 МБ, начиная с адреса 1 МБ (0x100000).
 *
 * Алгоритм — "список блоков, первый подходящий" (first-fit):
 *   - Куча разбита на блоки. Перед каждым блоком лежит ЗАГОЛОВОК:
 *     размер блока, признак "свободен/занят" и указатель на следующий блок.
 *
 *     | загол. | данные 100 Б | загол. | данные 50 Б | загол. | свободно ... |
 *
 *   - kmalloc(n): идём по списку, находим первый СВОБОДНЫЙ блок размером
 *     не меньше n. Если он намного больше — РАЗРЕЗАЕМ его на два:
 *     занятый (n байт) и свободный (остаток).
 *   - kfree(p): помечаем блок свободным и СКЛЕИВАЕМ с соседними свободными
 *     блоками, чтобы бороться с ФРАГМЕНТАЦИЕЙ (когда свободной памяти много,
 *     но она разбита на мелкие кусочки).
 *
 * Настоящие ОС устроены сложнее: они ещё управляют физическими страницами
 * памяти и виртуальной памятью (страничная адресация). См. docs/07-pamyat.md
 * ==========================================================================*/
#include "memory.h"
#include "ports.h"
#include "vga.h"

struct block {
    uint32_t size;          /* Размер области данных (без заголовка) */
    uint32_t free;          /* 1 — свободен, 0 — занят */
    struct block *next;     /* Следующий блок в куче (или NULL) */
    uint32_t magic;         /* "Подпись" для проверки целостности */
};

#define BLOCK_MAGIC  0xC0FFEE42
#define HEADER_SIZE  sizeof(struct block)
#define MIN_SPLIT    16     /* Не режем блок, если остаток меньше этого */

static struct block *heap_head = NULL;

void memory_init(void) {
    /* Сначала вся куча — один большой свободный блок */
    heap_head = (struct block *)HEAP_START;
    heap_head->size  = HEAP_SIZE - HEADER_SIZE;
    heap_head->free  = 1;
    heap_head->next  = NULL;
    heap_head->magic = BLOCK_MAGIC;
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    size = (size + 7) & ~7u;            /* Выравниваем размер до 8 байт */

    uint32_t flags = irq_save();        /* Критическая секция: список блоков */
                                        /* не должен меняться из двух задач сразу */
    for (struct block *b = heap_head; b; b = b->next) {
        if (!b->free || b->size < size)
            continue;

        /* Нашли подходящий блок. Если он слишком большой — разрезаем. */
        if (b->size >= size + HEADER_SIZE + MIN_SPLIT) {
            struct block *rest = (struct block *)((uint8_t *)b + HEADER_SIZE + size);
            rest->size  = b->size - size - HEADER_SIZE;
            rest->free  = 1;
            rest->next  = b->next;
            rest->magic = BLOCK_MAGIC;
            b->size = size;
            b->next = rest;
        }
        b->free = 0;
        irq_restore(flags);
        return (uint8_t *)b + HEADER_SIZE;   /* Отдаём адрес ПОСЛЕ заголовка */
    }
    irq_restore(flags);
    return NULL;                        /* Память кончилась */
}

void kfree(void *ptr) {
    if (!ptr) return;
    struct block *b = (struct block *)((uint8_t *)ptr - HEADER_SIZE);
    if (b->magic != BLOCK_MAGIC) {      /* Это не наш блок — ошибка программиста */
        kprintf("kfree: invalid pointer 0x%x\n", (uint32_t)ptr);
        return;
    }

    uint32_t flags = irq_save();
    b->free = 1;

    /* Склеиваем соседние свободные блоки (проходим по всему списку) */
    for (struct block *cur = heap_head; cur && cur->next; ) {
        if (cur->free && cur->next->free) {
            cur->size += HEADER_SIZE + cur->next->size;
            cur->next  = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
    irq_restore(flags);
}

void memory_dump(void) {
    uint32_t used = 0, free_bytes = 0, count = 0;
    kprintf("  #   address     size       state\n");
    for (struct block *b = heap_head; b; b = b->next, count++) {
        if (count < 12)
            kprintf("  %2u  0x%08x  %8u   %s\n", count,
                    (uint32_t)b + HEADER_SIZE, b->size, b->free ? "free" : "USED");
        if (b->free) free_bytes += b->size; else used += b->size;
    }
    if (count > 12) kprintf("  ... (%u blocks total)\n", count);
    kprintf("  Heap: %u KB used, %u KB free, %u blocks\n",
            used / 1024, free_bytes / 1024, count);
}
