/* ============================================================================
 * kernel.c — ГЛАВНЫЙ ФАЙЛ ЯДРА
 * ============================================================================
 * Сюда управление приходит из entry.asm. К этому моменту:
 *   - процессор в 32-битном защищённом режиме (сделал загрузчик);
 *   - есть стек, секция .bss обнулена (сделал entry.asm);
 *   - прерывания ЗАПРЕЩЕНЫ (загрузчик выполнил cli).
 *
 * Задача kmain — по очереди инициализировать все подсистемы ядра.
 * ПОРЯДОК ВАЖЕН: например, нельзя разрешать прерывания (sti), пока не
 * настроена таблица прерываний — иначе первый же тик таймера отправит
 * процессор по случайному адресу.
 * ==========================================================================*/
#include "types.h"
#include "ports.h"
#include "vga.h"
#include "interrupts.h"
#include "timer.h"
#include "keyboard.h"
#include "memory.h"
#include "task.h"
#include "fs.h"
#include "shell.h"
#include "programs.h"

/* Вывести строку журнала загрузки вида "[ OK ] сообщение" */
static void boot_ok(const char *msg) {
    vga_set_color(LIGHT_GRAY, BLACK);
    kprintf("[");
    vga_set_color(LIGHT_GREEN, BLACK);
    kprintf(" OK ");
    vga_set_color(LIGHT_GRAY, BLACK);
    kprintf("] %s\n", msg);
}

void kmain(void) {
    /* 1. Экран — первым делом, чтобы видеть, что происходит */
    vga_init();
    boot_ok("VGA text mode 80x25 (video memory at 0xB8000)");
    boot_ok("Running in 32-bit protected mode (GDT set by bootloader)");

    /* 2. Таблица прерываний и контроллер прерываний */
    interrupts_init();
    boot_ok("IDT loaded: 32 CPU exceptions + 16 hardware IRQs");
    boot_ok("PIC 8259 remapped: IRQ0-15 -> vectors 32-47");

    /* 3. Динамическая память */
    memory_init();
    boot_ok("Heap initialized: 4 MB at 0x100000 (kmalloc/kfree)");

    /* 4. Устройства */
    timer_init();
    boot_ok("PIT timer: 100 Hz (IRQ0)");
    keyboard_init();
    boot_ok("PS/2 keyboard driver (IRQ1)");

    /* 5. Многозадачность и файловая система */
    tasking_init();
    boot_ok("Scheduler: preemptive round-robin, task 0 = shell");
    fs_init();
    boot_ok("RAM file system mounted, 2 files created");

    /* 6. Всё готово — разрешаем прерывания. С этого момента таймер
     *    начинает тикать, а планировщик — переключать задачи. */
    sti();
    boot_ok("Interrupts enabled (sti)");

    task_create("clock", clock_task);
    boot_ok("Started background task 'clock' (see top right corner)");

    /* 7. Запускаем оболочку. Она работает вечно. */
    shell_run();
}
