/* ============================================================================
 * task.c — многозадачность и планировщик
 * ============================================================================
 * У компьютера (в нашем случае) ОДИН процессор, а задач хочется выполнять
 * несколько одновременно. Решение — РАЗДЕЛЕНИЕ ВРЕМЕНИ: процессор очень
 * быстро переключается между задачами (у нас 100 раз в секунду), и
 * человеку кажется, что они работают параллельно.
 *
 * Что такое "задача" с точки зрения процессора? Это всего лишь:
 *   - значения регистров (EIP — где выполняемся, ESP — где стек, EAX ...);
 *   - собственный стек (локальные переменные, адреса возврата).
 * Всё вместе называется КОНТЕКСТОМ задачи.
 *
 * Как происходит переключение (ПЕРЕКЛЮЧЕНИЕ КОНТЕКСТА):
 *   1. Тикает таймер -> прерывание IRQ0.
 *   2. isr_common (isr.asm) сохраняет ВСЕ регистры текущей задачи
 *      в её же стек и передаёт указатель на них в C.
 *   3. schedule() запоминает этот указатель в структуре текущей задачи,
 *      выбирает следующую задачу и возвращает ЕЁ сохранённый указатель.
 *   4. isr_common делает ESP = этот указатель, восстанавливает регистры
 *      (уже другой задачи!) и выполняет IRET — процессор продолжает
 *      выполнять другую задачу с того места, где её когда-то прервали.
 *
 * Это ВЫТЕСНЯЮЩАЯ многозадачность: задача не решает сама, когда отдать
 * процессор — его отнимает таймер. Алгоритм выбора следующей задачи —
 * "по кругу" (Round Robin): все задачи по очереди получают равный квант.
 *
 * Подробно: docs/08-mnogozadachnost.md
 * ==========================================================================*/
#include "task.h"
#include "memory.h"
#include "ports.h"
#include "lib.h"

static struct task tasks[MAX_TASKS];
static int current = 0;             /* Номер выполняющейся задачи */

/* Задача 0 — это само ядро/оболочка, которое уже выполняется на стеке,
 * созданном загрузчиком. Её не нужно создавать, только зарегистрировать. */
void tasking_init(void) {
    memset(tasks, 0, sizeof(tasks));
    tasks[0].id = 0;
    strncpy(tasks[0].name, "shell", sizeof(tasks[0].name));
    tasks[0].state = TASK_RUNNING;
    current = 0;
}

/* Сюда "возвращается" из прерывания новая задача при первом запуске.
 * Вызываем её функцию, а когда та закончится — завершаем задачу. */
static void task_start(void) {
    tasks[current].entry();
    task_exit();
}

int task_create(const char *name, void (*entry)(void)) {
    uint32_t flags = irq_save();

    /* Ищем свободный слот (или слот завершившейся задачи) */
    int id = -1;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED || tasks[i].state == TASK_DEAD) {
            id = i;
            break;
        }
    }
    if (id < 0) { irq_restore(flags); return -1; }

    struct task *t = &tasks[id];
    if (t->stack) kfree(t->stack);          /* Освобождаем стек старой задачи */

    t->stack = kmalloc(TASK_STACK_SIZE);
    if (!t->stack) { t->state = TASK_UNUSED; irq_restore(flags); return -1; }

    /* ГЛАВНЫЙ ТРЮК: готовим стек новой задачи так, будто она уже была
     * прервана таймером. Кладём на вершину стека "сохранённые регистры"
     * (struct regs). Когда планировщик впервые выберет эту задачу,
     * isr_common "восстановит" эти регистры, а IRET прыгнет на EIP,
     * то есть на task_start. */
    uint32_t top = (uint32_t)t->stack + TASK_STACK_SIZE;
    struct regs *r = (struct regs *)(top - sizeof(struct regs));
    memset(r, 0, sizeof(struct regs));
    r->ds = r->es = r->fs = r->gs = 0x10;   /* Сегмент данных ядра */
    r->cs     = 0x08;                       /* Сегмент кода ядра */
    r->eip    = (uint32_t)task_start;       /* Откуда начать выполнение */
    r->eflags = 0x202;                      /* Бит 9 (IF) = прерывания разрешены */

    t->id    = id;
    t->esp   = (uint32_t)r;
    t->entry = entry;
    t->ticks = 0;
    strncpy(t->name, name, sizeof(t->name));
    t->state = TASK_READY;                  /* Готова! Планировщик её найдёт */

    irq_restore(flags);
    return id;
}

/* Завершение текущей задачи (вызывается из неё самой) */
void task_exit(void) {
    cli();
    tasks[current].state = TASK_DEAD;
    sti();
    for (;;) hlt();     /* Ждём таймера: планировщик больше нас не выберет */
}

/* Принудительно завершить задачу по номеру (команда kill) */
bool task_kill(int id) {
    if (id <= 0 || id >= MAX_TASKS) return false;   /* Оболочку не убиваем */
    uint32_t flags = irq_save();
    bool ok = tasks[id].state == TASK_READY || tasks[id].state == TASK_RUNNING;
    if (ok) tasks[id].state = TASK_DEAD;
    irq_restore(flags);
    return ok;
}

struct task *task_get(int id) {
    if (id < 0 || id >= MAX_TASKS) return NULL;
    return &tasks[id];
}

int task_current_id(void) {
    return current;
}

/* ПЛАНИРОВЩИК. Вызывается из обработчика таймера (прерывания запрещены).
 * Получает регистры прерванной задачи, возвращает регистры следующей. */
struct regs *schedule(struct regs *r) {
    struct task *cur = &tasks[current];

    cur->esp = (uint32_t)r;                 /* 1. Сохраняем контекст текущей */
    cur->ticks++;
    if (cur->state == TASK_RUNNING)
        cur->state = TASK_READY;

    /* 2. Ищем следующую готовую задачу по кругу: current+1, current+2, ... */
    int next = current;
    for (int i = 1; i <= MAX_TASKS; i++) {
        int candidate = (current + i) % MAX_TASKS;
        if (tasks[candidate].state == TASK_READY) {
            next = candidate;
            break;
        }
    }

    /* 3. Переключаемся на неё */
    current = next;
    tasks[current].state = TASK_RUNNING;
    return (struct regs *)tasks[current].esp;
}
