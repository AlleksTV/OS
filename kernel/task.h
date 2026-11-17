/* task.h — задачи (потоки ядра) и планировщик */
#ifndef TASK_H
#define TASK_H

#include "types.h"
#include "interrupts.h"

#define MAX_TASKS       6
#define TASK_STACK_SIZE 4096

enum task_state {
    TASK_UNUSED = 0,    /* Слот свободен */
    TASK_READY,         /* Готова выполняться, ждёт своей очереди */
    TASK_RUNNING,       /* Выполняется прямо сейчас */
    TASK_DEAD,          /* Завершилась, слот можно использовать снова */
};

struct task {
    int      id;                /* Номер задачи (= номер слота) */
    char     name[16];          /* Имя для команды ps */
    int      state;             /* enum task_state */
    uint32_t esp;               /* Сохранённый указатель стека */
    void    *stack;             /* Выделенная под стек память */
    void   (*entry)(void);      /* Функция, с которой задача начинает работу */
    uint32_t ticks;             /* Сколько тиков таймера задача выполнялась */
};

void          tasking_init(void);
int           task_create(const char *name, void (*entry)(void));
bool          task_kill(int id);
void          task_exit(void);
struct task  *task_get(int id);
int           task_current_id(void);
struct regs  *schedule(struct regs *r);

#endif
