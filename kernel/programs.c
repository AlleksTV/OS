/* ============================================================================
 * programs.c — демонстрационные задачи, работающие "параллельно" с оболочкой
 * ============================================================================
 * Каждая из этих функций — бесконечный цикл. Если бы многозадачности не
 * было, запуск любой из них "повесил" бы систему: оболочка больше никогда
 * не получила бы управление. Но благодаря планировщику (task.c) процессор
 * каждые 10 мс переключается между задачами, и все они работают "одновременно".
 *
 * Задачи выводят результаты в строку состояния (верхняя синяя строка),
 * каждая — в свою ячейку, номер которой зависит от номера задачи.
 * ==========================================================================*/
#include "programs.h"
#include "task.h"
#include "timer.h"
#include "vga.h"
#include "lib.h"

#define STATUS_COLOR VGA_COLOR(WHITE, BLUE)
#define SLOT_WIDTH   11

/* Позиция ячейки задачи в строке состояния */
static int slot_x(int id) {
    if (id == 1) return 57;                 /* Слот 1 обычно занят часами */
    return 14 + (id - 2) * SLOT_WIDTH;      /* Слоты 2..5: x = 14, 25, 36, 47 */
}

void status_clear_slot(int id) {
    if (id >= 1)
        vga_write_at(slot_x(id), 0, "          ", STATUS_COLOR);
}

/* Часы: показывают время работы системы в правом верхнем углу.
 * Задача почти всё время спит (sleep_ms), просыпаясь 4 раза в секунду. */
void clock_task(void) {
    char text[20];
    for (;;) {
        uint32_t s = timer_ticks() / TIMER_HZ;
        ksnprintf(text, sizeof(text), "up %02u:%02u:%02u", s / 3600, (s / 60) % 60, s % 60);
        vga_write_at(68, 0, text, VGA_COLOR(LIGHT_GREEN, BLUE));
        sleep_ms(250);
    }
}

/* Счётчик: считает так быстро, как только может. Запустите два счётчика
 * (run counter; run counter) — они будут расти примерно с одинаковой
 * скоростью, потому что планировщик делит время поровну. */
void counter_task(void) {
    int id = task_current_id();
    char text[16];
    uint32_t n = 0;
    for (;;) {
        n++;
        if ((n & 0x3FF) == 0) {     /* Обновляем экран раз в 1024 шага */
            ksnprintf(text, sizeof(text), "%u:%7u", id, (n >> 10) % 10000000);
            vga_write_at(slot_x(id), 0, text, VGA_COLOR(YELLOW, BLUE));
        }
    }
}

/* Вертушка: крутящийся символ, меняется 10 раз в секунду */
void spinner_task(void) {
    int id = task_current_id();
    const char frames[] = "|/-\\";
    char text[16];
    uint32_t i = 0;
    for (;;) {
        ksnprintf(text, sizeof(text), "%u:spin %c", id, frames[i++ % 4]);
        vga_write_at(slot_x(id), 0, text, VGA_COLOR(LIGHT_CYAN, BLUE));
        sleep_ms(100);
    }
}
