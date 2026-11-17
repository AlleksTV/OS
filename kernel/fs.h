/* fs.h — простая файловая система в оперативной памяти (RAM FS) */
#ifndef FS_H
#define FS_H

#include "types.h"

#define FS_MAX_FILES   32
#define FS_NAME_LEN    24

struct file {
    char     name[FS_NAME_LEN];   /* Имя файла */
    char    *data;                /* Содержимое (выделено через kmalloc) */
    uint32_t size;                /* Размер в байтах */
    bool     used;                /* Занята ли эта запись таблицы */
};

void         fs_init(void);
struct file *fs_find(const char *name);
int          fs_write(const char *name, const char *data, uint32_t size);
int          fs_append(const char *name, const char *data, uint32_t size);
bool         fs_delete(const char *name);
struct file *fs_get(int index);     /* Для перебора всех файлов (команда ls) */

#endif
