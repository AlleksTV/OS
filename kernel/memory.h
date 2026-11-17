/* memory.h — менеджер динамической памяти (куча) ядра */
#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"

#define HEAP_START 0x100000          /* Куча начинается с 1 МБ */
#define HEAP_SIZE  (4 * 1024 * 1024) /* и занимает 4 МБ */

void  memory_init(void);
void *kmalloc(size_t size);
void  kfree(void *ptr);
void  memory_dump(void);             /* Вывести список блоков (команда mem) */

#endif
