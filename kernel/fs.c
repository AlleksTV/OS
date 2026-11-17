/* ============================================================================
 * fs.c — файловая система в оперативной памяти
 * ============================================================================
 * ФАЙЛОВАЯ СИСТЕМА отвечает на вопрос "как хранить данные так, чтобы их
 * можно было найти по имени". Диск — это просто массив пронумерованных
 * секторов; файловая система добавляет поверх него понятия "файл",
 * "имя", "размер", "каталог".
 *
 * Любая ФС состоит из двух частей:
 *   1. МЕТАДАННЫЕ — "оглавление": какие файлы есть, как называются,
 *      какого размера, где лежат их данные. (У нас — массив files[].)
 *   2. ДАННЫЕ — само содержимое файлов. (У нас — память из kmalloc.)
 *
 * Наша ФС хранит всё в оперативной памяти, поэтому после перезагрузки
 * файлы исчезают. Настоящие ФС (FAT, ext4, NTFS) хранят и оглавление, и
 * данные на диске. Подробно: docs/09-fajlovaya-sistema.md
 * ==========================================================================*/
#include "fs.h"
#include "memory.h"
#include "lib.h"

static struct file files[FS_MAX_FILES];   /* "Таблица файлов" (каталог) */

struct file *fs_find(const char *name) {
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (files[i].used && strcmp(files[i].name, name) == 0)
            return &files[i];
    return NULL;
}

struct file *fs_get(int index) {
    if (index < 0 || index >= FS_MAX_FILES || !files[index].used)
        return NULL;
    return &files[index];
}

/* Создать файл или перезаписать существующий.
 * Возвращает 0 при успехе, -1 при ошибке. */
int fs_write(const char *name, const char *data, uint32_t size) {
    if (!name[0] || strlen(name) >= FS_NAME_LEN) return -1;

    struct file *f = fs_find(name);
    if (!f) {                                   /* Новый файл: ищем */
        for (int i = 0; i < FS_MAX_FILES; i++)  /* свободную запись */
            if (!files[i].used) { f = &files[i]; break; }
        if (!f) return -1;                      /* Таблица заполнена */
        strncpy(f->name, name, FS_NAME_LEN);
        f->data = NULL;
        f->size = 0;
        f->used = true;
    }

    char *buf = kmalloc(size + 1);              /* +1 для завершающего нуля */
    if (!buf) return -1;
    memcpy(buf, data, size);
    buf[size] = 0;

    kfree(f->data);                             /* Старое содержимое — в кучу */
    f->data = buf;
    f->size = size;
    return 0;
}

/* Дописать данные в конец файла (создаёт файл, если его нет) */
int fs_append(const char *name, const char *data, uint32_t size) {
    struct file *f = fs_find(name);
    if (!f) return fs_write(name, data, size);

    char *buf = kmalloc(f->size + size + 1);
    if (!buf) return -1;
    memcpy(buf, f->data, f->size);
    memcpy(buf + f->size, data, size);
    buf[f->size + size] = 0;

    kfree(f->data);
    f->data = buf;
    f->size += size;
    return 0;
}

bool fs_delete(const char *name) {
    struct file *f = fs_find(name);
    if (!f) return false;
    kfree(f->data);                 /* Освобождаем данные */
    memset(f, 0, sizeof(*f));       /* Освобождаем запись в таблице */
    return true;
}

void fs_init(void) {
    memset(files, 0, sizeof(files));

    const char *readme =
        "Welcome to MiniOS!\n"
        "This file lives in a RAM file system: the file table is an array\n"
        "in kernel memory and file contents are allocated with kmalloc().\n"
        "Try: ls, cat readme.txt, write note.txt Hello, rm note.txt\n";
    fs_write("readme.txt", readme, strlen(readme));

    const char *hello = "Hello, world!\n";
    fs_write("hello.txt", hello, strlen(hello));
}
