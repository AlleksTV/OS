/* ============================================================================
 * timer.c — системный таймер PIT и часы CMOS
 * ============================================================================
 * PIT (Programmable Interval Timer, микросхема 8253/8254) — генератор
 * импульсов с частотой 1 193 182 Гц. Если записать в него делитель N, он
 * будет вызывать прерывание IRQ0 с частотой 1193182 / N раз в секунду.
 *
 * Таймер — "сердцебиение" ОС. По его тикам:
 *   - считается время работы системы (uptime);
 *   - работают задержки (sleep);
 *   - ПЛАНИРОВЩИК отнимает процессор у одной задачи и отдаёт другой
 *     (вытесняющая многозадачность, см. task.c).
 *
 * CMOS — маленькая память с батарейкой на материнской плате. В ней хранятся
 * настройки BIOS и работают часы реального времени (RTC), которые идут даже
 * при выключенном компьютере. Доступ: порт 0x70 — номер ячейки, 0x71 — данные.
 * ==========================================================================*/
#include "timer.h"
#include "interrupts.h"
#include "ports.h"

#define PIT_FREQUENCY 1193182
#define PIT_CHANNEL0  0x40
#define PIT_COMMAND   0x43

/* volatile — значение меняется в обработчике прерывания, поэтому
 * компилятор не должен кешировать его в регистре */
static volatile uint32_t ticks = 0;

/* Обработчик IRQ0: вызывается TIMER_HZ раз в секунду */
static void timer_handler(struct regs *r) {
    (void)r;
    ticks++;
}

void timer_init(void) {
    uint32_t divisor = PIT_FREQUENCY / TIMER_HZ;   /* 11931 для 100 Гц */

    /* Команда 0x36 = 00 11 011 0:
     *   канал 0, запись младшего и старшего байта,
     *   режим 3 (генератор меандра), двоичный счёт */
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));    /* младший байт */
    outb(PIT_CHANNEL0, (uint8_t)(divisor >> 8));      /* старший байт */

    irq_install_handler(0, timer_handler);
}

uint32_t timer_ticks(void) {
    return ticks;
}

void sleep_ms(uint32_t ms) {
    uint32_t end = ticks + ms * TIMER_HZ / 1000;
    while (ticks < end)
        hlt();      /* Спим до следующего прерывания, не тратя процессор зря */
}

/* ---------------------------- CMOS / RTC -------------------------------- */

static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

/* Часы хранят числа в формате BCD: 0x59 означает 59 (две десятичные цифры
 * в двух половинках байта). Переводим в обычное двоичное число. */
static int bcd_to_bin(uint8_t v) {
    return (v & 0x0F) + (v >> 4) * 10;
}

void rtc_read(struct rtc_time *t) {
    while (cmos_read(0x0A) & 0x80)   /* Ждём, пока часы не обновляются */
        ;
    uint8_t status_b = cmos_read(0x0B);
    uint8_t sec  = cmos_read(0x00), min = cmos_read(0x02), hour = cmos_read(0x04);
    uint8_t day  = cmos_read(0x07), mon = cmos_read(0x08), year = cmos_read(0x09);

    if (!(status_b & 0x04)) {        /* Бит 2 = 0 -> значения в BCD */
        sec = bcd_to_bin(sec);  min = bcd_to_bin(min);
        hour = bcd_to_bin(hour & 0x7F) | (hour & 0x80);
        day = bcd_to_bin(day);  mon = bcd_to_bin(mon);  year = bcd_to_bin(year);
    }
    if (!(status_b & 0x02) && (hour & 0x80))   /* 12-часовой формат, PM */
        hour = ((hour & 0x7F) + 12) % 24;

    t->second = sec;  t->minute = min;  t->hour = hour;
    t->day = day;     t->month = mon;   t->year = 2000 + year;
}

/* Объём памяти в КБ. Ячейки CMOS:
 *   0x30/0x31 — память от 1 МБ до 16 МБ (в КБ)
 *   0x34/0x35 — память выше 16 МБ (в блоках по 64 КБ) */
uint32_t cmos_memory_kb(void) {
    uint32_t above_1mb  = cmos_read(0x30) | (cmos_read(0x31) << 8);
    uint32_t above_16mb = cmos_read(0x34) | (cmos_read(0x35) << 8);
    if (above_16mb)
        return 16 * 1024 + above_16mb * 64;
    return 1024 + above_1mb;
}
