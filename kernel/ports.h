/* ============================================================================
 * ports.h — работа с портами ввода-вывода
 * ============================================================================
 * У процессоров x86 есть отдельное "адресное пространство портов"
 * (0x0000–0xFFFF). Через порты процессор общается с устройствами:
 *   0x20, 0xA0   — контроллер прерываний (PIC)
 *   0x40–0x43    — системный таймер (PIT)
 *   0x60, 0x64   — контроллер клавиатуры
 *   0x70, 0x71   — CMOS / часы реального времени
 *   0x3D4, 0x3D5 — видеоконтроллер (положение курсора)
 *
 * Для этого есть специальные команды процессора IN и OUT. В языке C их нет,
 * поэтому используем "ассемблерные вставки" (inline assembly) GCC.
 * ==========================================================================*/
#ifndef PORTS_H
#define PORTS_H

#include "types.h"

/* Записать байт в порт */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/* Прочитать байт из порта */
static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/* Записать 16-битное слово в порт */
static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

/* Небольшая пауза: запись в неиспользуемый порт 0x80.
 * Старым устройствам нужно время, чтобы "переварить" команду. */
static inline void io_wait(void) {
    outb(0x80, 0);
}

/* Запретить / разрешить аппаратные прерывания */
static inline void cli(void) { __asm__ volatile ("cli"); }
static inline void sti(void) { __asm__ volatile ("sti"); }

/* Остановить процессор до следующего прерывания (экономит энергию) */
static inline void hlt(void) { __asm__ volatile ("hlt"); }

/* Сохранить флаги и запретить прерывания. Возвращает старые флаги,
 * чтобы потом восстановить их функцией irq_restore(). Так делают
 * "критические секции" — участки кода, которые нельзя прерывать. */
static inline uint32_t irq_save(void) {
    uint32_t flags;
    __asm__ volatile ("pushf; pop %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static inline void irq_restore(uint32_t flags) {
    __asm__ volatile ("push %0; popf" : : "r"(flags) : "memory", "cc");
}

#endif
