/* ============================================================================
 * interrupts.h — таблица прерываний (IDT), контроллер прерываний (PIC)
 * ==========================================================================*/
#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "types.h"

/* Состояние процессора в момент прерывания.
 * Порядок полей СТРОГО соответствует порядку, в котором регистры
 * сохраняются в стек в isr.asm (isr_common). */
struct regs {
    uint32_t gs, fs, es, ds;                          /* push вручную */
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax; /* pusha */
    uint32_t int_no, err_code;                        /* из заглушки */
    uint32_t eip, cs, eflags;                         /* кладёт процессор */
};

/* Тип функции-обработчика аппаратного прерывания */
typedef void (*irq_handler_t)(struct regs *r);

void interrupts_init(void);
void irq_install_handler(int irq, irq_handler_t handler);

#endif
