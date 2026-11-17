/* ============================================================================
 * interrupts.c — настройка прерываний
 * ============================================================================
 * ПРЕРЫВАНИЕ — это сигнал процессору: "брось текущую работу, есть срочное
 * дело". Процессор сохраняет, где он был, выполняет обработчик и возвращается.
 *
 * Бывают:
 *   - ИСКЛЮЧЕНИЯ (векторы 0..31) — их порождает сам процессор при ошибке:
 *     деление на ноль, неверная инструкция, обращение к запрещённой памяти.
 *   - АППАРАТНЫЕ ПРЕРЫВАНИЯ (IRQ) — от устройств: тикнул таймер, нажата
 *     клавиша, диск закончил чтение.
 *   - ПРОГРАММНЫЕ — команда INT n (так программы вызывают ядро).
 *
 * Чтобы процессор знал, где лежат обработчики, ОС заполняет таблицу IDT
 * (Interrupt Descriptor Table) из 256 записей и сообщает её адрес командой LIDT.
 *
 * Аппаратные прерывания проходят через микросхему PIC 8259 (точнее, две:
 * ведущую и ведомую). Её надо перенастроить: по умолчанию IRQ 0..7 попадают
 * на векторы 8..15 и путаются с исключениями процессора.
 *
 * Примечание: в текстовом режиме VGA нет русских букв (используется
 * кодовая страница 437), поэтому все сообщения на экране — на английском.
 *
 * Подробно: docs/05-preryvaniya.md
 * ==========================================================================*/
#include "interrupts.h"
#include "ports.h"
#include "vga.h"
#include "task.h"

/* ---------------------------- IDT ---------------------------------------- */

/* Одна запись IDT (8 байт). Формат задан процессором. */
struct idt_entry {
    uint16_t offset_low;    /* Адрес обработчика, биты 0..15 */
    uint16_t selector;      /* Селектор сегмента кода (0x08 из нашей GDT) */
    uint8_t  zero;          /* Всегда 0 */
    uint8_t  type_attr;     /* Тип шлюза и права доступа */
    uint16_t offset_high;   /* Адрес обработчика, биты 16..31 */
} __attribute__((packed));  /* packed: компилятор не должен добавлять пустоты */

/* Структура для команды LIDT: размер и адрес таблицы */
struct idt_pointer {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_pointer idt_ptr;
static irq_handler_t irq_handlers[16];

static void idt_set_gate(int n, uint32_t handler) {
    idt[n].offset_low  = handler & 0xFFFF;
    idt[n].offset_high = (handler >> 16) & 0xFFFF;
    idt[n].selector    = 0x08;
    idt[n].zero        = 0;
    /* 0x8E = 1000 1110:
     *   бит 7     = 1     — запись присутствует
     *   биты 5..6 = 00    — уровень привилегий 0 (ядро)
     *   биты 0..3 = 1110  — 32-битный шлюз прерывания (на время обработки
     *                       процессор сам запрещает другие прерывания) */
    idt[n].type_attr   = 0x8E;
}

/* Адреса заглушек из isr.asm */
extern void isr0(void),  isr1(void),  isr2(void),  isr3(void),  isr4(void),
            isr5(void),  isr6(void),  isr7(void),  isr8(void),  isr9(void),
            isr10(void), isr11(void), isr12(void), isr13(void), isr14(void),
            isr15(void), isr16(void), isr17(void), isr18(void), isr19(void),
            isr20(void), isr21(void), isr22(void), isr23(void), isr24(void),
            isr25(void), isr26(void), isr27(void), isr28(void), isr29(void),
            isr30(void), isr31(void);
extern void irq0(void),  irq1(void),  irq2(void),  irq3(void),  irq4(void),
            irq5(void),  irq6(void),  irq7(void),  irq8(void),  irq9(void),
            irq10(void), irq11(void), irq12(void), irq13(void), irq14(void),
            irq15(void);

/* ---------------------------- PIC ---------------------------------------- */

#define PIC1_CMD   0x20     /* Ведущий контроллер: порт команд */
#define PIC1_DATA  0x21     /*                     порт данных */
#define PIC2_CMD   0xA0     /* Ведомый контроллер */
#define PIC2_DATA  0xA1
#define PIC_EOI    0x20     /* Команда "End Of Interrupt" */

/* Перенастройка PIC: IRQ 0..7 -> векторы 32..39, IRQ 8..15 -> векторы 40..47.
 * Делается последовательностью "слов инициализации" ICW1..ICW4. */
static void pic_remap(void) {
    outb(PIC1_CMD, 0x11);  io_wait();   /* ICW1: начать инициализацию, будет ICW4 */
    outb(PIC2_CMD, 0x11);  io_wait();
    outb(PIC1_DATA, 32);   io_wait();   /* ICW2: первый вектор ведущего = 32 */
    outb(PIC2_DATA, 40);   io_wait();   /* ICW2: первый вектор ведомого = 40 */
    outb(PIC1_DATA, 4);    io_wait();   /* ICW3: ведомый подключён к IRQ2 (бит 2) */
    outb(PIC2_DATA, 2);    io_wait();   /* ICW3: ведомый, мой номер каскада = 2 */
    outb(PIC1_DATA, 0x01); io_wait();   /* ICW4: режим 8086 */
    outb(PIC2_DATA, 0x01); io_wait();

    /* Маски: бит = 1 означает "прерывание запрещено".
     * Разрешаем только IRQ0 (таймер), IRQ1 (клавиатура) и IRQ2 (каскад). */
    outb(PIC1_DATA, 0xF8);              /* 1111 1000 */
    outb(PIC2_DATA, 0xFF);
}

/* -------------------------- Исключения ----------------------------------- */

static const char *exception_names[32] = {
    "Division by zero", "Debug", "Non-maskable interrupt",
    "Breakpoint", "Overflow", "Bound range exceeded",
    "Invalid opcode", "Device not available",
    "Double fault", "Coprocessor segment overrun", "Invalid TSS",
    "Segment not present", "Stack-segment fault",
    "General protection fault", "Page fault", "Reserved",
    "x87 floating-point exception", "Alignment check", "Machine check",
    "SIMD floating-point exception", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved",
};

/* "Синий экран смерти": ядро не может продолжать работу после ошибки
 * процессора в собственном коде, поэтому показываем состояние и
 * останавливаемся. */
static void kernel_panic(struct regs *r) {
    vga_set_color(WHITE, RED);
    vga_clear();
    kprintf("\n  *** KERNEL PANIC ***\n\n");
    kprintf("  CPU exception #%u: %s\n", r->int_no, exception_names[r->int_no]);
    kprintf("  Error code: 0x%x\n\n", r->err_code);
    kprintf("  Registers at the moment of the fault:\n");
    kprintf("    EAX=%08x  EBX=%08x  ECX=%08x  EDX=%08x\n", r->eax, r->ebx, r->ecx, r->edx);
    kprintf("    ESI=%08x  EDI=%08x  EBP=%08x\n", r->esi, r->edi, r->ebp);
    kprintf("    EIP=%08x  CS=%04x  EFLAGS=%08x\n\n", r->eip, r->cs, r->eflags);
    kprintf("  EIP is the address of the instruction that caused the fault.\n");
    kprintf("  System halted. Please restart the computer.\n");
    for (;;) { cli(); hlt(); }
}

/* ---------------------- Главный обработчик -------------------------------- */

/* Эту функцию вызывает isr_common для ЛЮБОГО прерывания.
 * Возвращает указатель на регистры, которые нужно восстановить. */
struct regs *interrupt_handler(struct regs *r) {
    if (r->int_no < 32) {
        /* Исключение процессора */
        kernel_panic(r);
    } else if (r->int_no < 48) {
        /* Аппаратное прерывание IRQ */
        int irq = (int)r->int_no - 32;
        if (irq_handlers[irq])
            irq_handlers[irq](r);

        /* Сообщаем PIC, что прерывание обработано (EOI). Без этого
         * контроллер больше не пришлёт прерываний этого и более низкого
         * приоритета! Для IRQ 8..15 сообщаем обоим контроллерам. */
        if (irq >= 8) outb(PIC2_CMD, PIC_EOI);
        outb(PIC1_CMD, PIC_EOI);

        /* Таймер — повод для планировщика переключить задачу */
        if (irq == 0)
            r = schedule(r);
    }
    return r;
}

void irq_install_handler(int irq, irq_handler_t handler) {
    irq_handlers[irq] = handler;
}

void interrupts_init(void) {
    void (*isrs[32])(void) = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7, isr8, isr9, isr10,
        isr11, isr12, isr13, isr14, isr15, isr16, isr17, isr18, isr19, isr20,
        isr21, isr22, isr23, isr24, isr25, isr26, isr27, isr28, isr29, isr30,
        isr31,
    };
    void (*irqs[16])(void) = {
        irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7, irq8, irq9, irq10,
        irq11, irq12, irq13, irq14, irq15,
    };

    for (int i = 0; i < 32; i++) idt_set_gate(i, (uint32_t)isrs[i]);
    for (int i = 0; i < 16; i++) idt_set_gate(32 + i, (uint32_t)irqs[i]);

    pic_remap();

    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base  = (uint32_t)&idt;
    __asm__ volatile ("lidt %0" : : "m"(idt_ptr));   /* Загрузить IDT */
}
