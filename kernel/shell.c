/* ============================================================================
 * shell.c — командная оболочка (интерпретатор команд)
 * ============================================================================
 * Оболочка — это программа, через которую пользователь общается с ОС.
 * Её работа — бесконечный цикл "Read-Eval-Print Loop" (REPL):
 *
 *     while (true) {
 *         напечатать приглашение "minios> ";
 *         прочитать строку с клавиатуры;      // keyboard.c
 *         разбить строку на слова (argv);
 *         найти команду в таблице и выполнить;
 *     }
 *
 * Сама оболочка не умеет работать с железом — она вызывает функции ядра:
 * файловой системы, менеджера памяти, планировщика и т.д. В настоящих ОС
 * оболочка (bash, cmd.exe) — обычная пользовательская программа, которая
 * обращается к ядру через СИСТЕМНЫЕ ВЫЗОВЫ. У нас для простоты она
 * работает прямо внутри ядра.
 * ==========================================================================*/
#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "lib.h"
#include "memory.h"
#include "timer.h"
#include "task.h"
#include "fs.h"
#include "programs.h"
#include "ports.h"

#define MAX_LINE 256
#define MAX_ARGS 16

extern char __kernel_end[];      /* Конец ядра в памяти (из linker.ld) */

static char original_line[MAX_LINE];   /* Копия строки до разбиения */

/* --------------------------- Ввод строки --------------------------------- */

static void read_line(char *buf, int max) {
    int len = 0;
    for (;;) {
        char c = keyboard_getchar();
        if (c == '\n') {
            vga_putc('\n');
            break;
        } else if (c == '\b') {
            if (len > 0) { len--; vga_putc('\b'); }
        } else if (c >= ' ' && len < max - 1) {
            buf[len++] = c;
            vga_putc(c);             /* "Эхо": показываем введённый символ */
        }
    }
    buf[len] = 0;
}

/* Разбить строку на слова по пробелам. "write a.txt hi" ->
 * argv[0]="write", argv[1]="a.txt", argv[2]="hi". Пробелы заменяются
 * нулями прямо в строке, а argv указывает на начала слов. */
static int parse(char *line, char **argv) {
    int argc = 0;
    while (*line && argc < MAX_ARGS) {
        while (*line == ' ') *line++ = 0;   /* Пропускаем пробелы */
        if (!*line) break;
        argv[argc++] = line;                 /* Начало слова */
        while (*line && *line != ' ') line++;
    }
    return argc;
}

/* Текст исходной строки, начиная с слова номер n (для write/echo,
 * где нужно сохранить пробелы внутри текста) */
static const char *rest_of_line(int n) {
    const char *p = original_line;
    for (int i = 0; i < n; i++) {
        while (*p == ' ') p++;
        while (*p && *p != ' ') p++;
    }
    while (*p == ' ') p++;
    return p;
}

/* ------------------------------ Команды ---------------------------------- */

static void cmd_help(int argc, char **argv);

static void cmd_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    vga_clear();
}

static void cmd_echo(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("%s\n", rest_of_line(1));
}

static void cmd_info(int argc, char **argv) {
    (void)argc; (void)argv;
    uint32_t kernel_size = (uint32_t)__kernel_end - 0x10000;
    kprintf("MiniOS v1.0 - educational 32-bit x86 operating system\n");
    kprintf("CPU mode: 32-bit protected mode, ring 0, flat memory model\n");
    kprintf("RAM (from CMOS): %u MB\n", cmos_memory_kb() / 1024);
    kprintf("Kernel size: %u bytes, timer: %u Hz\n\n", kernel_size, TIMER_HZ);
    kprintf("Physical memory map:\n");
    kprintf("  0x00000000-0x000004FF  BIOS interrupt table and data\n");
    kprintf("  0x00007C00-0x00007DFF  bootloader (MBR, 512 bytes)\n");
    kprintf("  0x00010000-0x%08x  kernel: code, data, bss\n", (uint32_t)__kernel_end);
    kprintf("  0x0008FFFF  <-         kernel stack (grows down from 0x90000)\n");
    kprintf("  0x000B8000-0x000B8F9F  VGA text video memory (80x25x2)\n");
    kprintf("  0x%08x-0x%08x  kernel heap (kmalloc)\n", HEAP_START, HEAP_START + HEAP_SIZE - 1);
}

static void cmd_mem(int argc, char **argv) {
    (void)argc; (void)argv;
    memory_dump();
}

static void cmd_date(int argc, char **argv) {
    (void)argc; (void)argv;
    struct rtc_time t;
    rtc_read(&t);
    kprintf("%04d-%02d-%02d %02d:%02d:%02d (UTC, from CMOS real-time clock)\n",
            t.year, t.month, t.day, t.hour, t.minute, t.second);
}

static void cmd_uptime(int argc, char **argv) {
    (void)argc; (void)argv;
    uint32_t t = timer_ticks();
    kprintf("Uptime: %u.%02u s (%u timer ticks at %u Hz)\n",
            t / TIMER_HZ, t % TIMER_HZ, t, TIMER_HZ);
}

static void cmd_sleep(int argc, char **argv) {
    if (argc < 2 || !is_number(argv[1])) { kprintf("Usage: sleep <ms>\n"); return; }
    sleep_ms((uint32_t)atoi(argv[1]));
    kprintf("Woke up after %s ms\n", argv[1]);
}

static void cmd_ps(int argc, char **argv) {
    (void)argc; (void)argv;
    static const char *states[] = { "unused", "ready", "RUNNING", "dead" };
    kprintf("  ID  NAME        STATE     CPU TICKS\n");
    for (int i = 0; i < MAX_TASKS; i++) {
        struct task *t = task_get(i);
        if (t->state == TASK_UNUSED || t->state == TASK_DEAD) continue;
        kprintf("  %2d  %10s  %8s  %u\n", t->id, t->name, states[t->state], t->ticks);
    }
}

static void cmd_run(int argc, char **argv) {
    void (*entry)(void) = NULL;
    if (argc >= 2) {
        if (strcmp(argv[1], "counter") == 0) entry = counter_task;
        else if (strcmp(argv[1], "spinner") == 0) entry = spinner_task;
        else if (strcmp(argv[1], "clock") == 0) entry = clock_task;
    }
    if (!entry) { kprintf("Usage: run <counter|spinner|clock>\n"); return; }

    int id = task_create(argv[1], entry);
    if (id < 0) kprintf("Cannot create task: no free slots\n");
    else kprintf("Started task '%s' with id %d (see the status bar)\n", argv[1], id);
}

static void cmd_kill(int argc, char **argv) {
    if (argc < 2 || !is_number(argv[1])) { kprintf("Usage: kill <id>\n"); return; }
    int id = atoi(argv[1]);
    if (task_kill(id)) {
        status_clear_slot(id);
        kprintf("Task %d killed\n", id);
    } else {
        kprintf("No such task (task 0 is the shell and cannot be killed)\n");
    }
}

static void cmd_ls(int argc, char **argv) {
    (void)argc; (void)argv;
    int count = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        struct file *f = fs_get(i);
        if (!f) continue;
        kprintf("  %24s %6u bytes\n", f->name, f->size);
        count++;
    }
    kprintf("%d file(s)\n", count);
}

static void cmd_cat(int argc, char **argv) {
    if (argc < 2) { kprintf("Usage: cat <file>\n"); return; }
    struct file *f = fs_find(argv[1]);
    if (!f) { kprintf("cat: %s: no such file\n", argv[1]); return; }
    kprintf("%s", f->data);
    if (f->size && f->data[f->size - 1] != '\n') kprintf("\n");
}

static void write_common(int argc, char **argv, bool append) {
    if (argc < 3) { kprintf("Usage: %s <file> <text>\n", argv[0]); return; }
    const char *text = rest_of_line(2);
    /* Добавляем перевод строки в конце, как это делает echo > file */
    char buf[MAX_LINE + 1];
    int n = ksnprintf(buf, sizeof(buf), "%s\n", text);
    int rc = append ? fs_append(argv[1], buf, (uint32_t)n)
                    : fs_write(argv[1], buf, (uint32_t)n);
    if (rc < 0) kprintf("Error: cannot write file\n");
}

static void cmd_write(int argc, char **argv)  { write_common(argc, argv, false); }
static void cmd_append(int argc, char **argv) { write_common(argc, argv, true); }

static void cmd_rm(int argc, char **argv) {
    if (argc < 2) { kprintf("Usage: rm <file>\n"); return; }
    if (!fs_delete(argv[1])) kprintf("rm: %s: no such file\n", argv[1]);
}

static void cmd_calc(int argc, char **argv) {
    if (argc != 4 || !is_number(argv[1]) || !is_number(argv[3]) || argv[2][1]) {
        kprintf("Usage: calc <a> <+|-|*|/|%%> <b>   example: calc 12 * 34\n");
        return;
    }
    int a = atoi(argv[1]), b = atoi(argv[3]);
    char op = argv[2][0];
    if ((op == '/' || op == '%') && b == 0) {
        kprintf("Error: division by zero (try the 'crash' command to see\n"
                "what the CPU does when nobody checks for this)\n");
        return;
    }
    switch (op) {
    case '+': kprintf("%d\n", a + b); break;
    case '-': kprintf("%d\n", a - b); break;
    case '*': kprintf("%d\n", a * b); break;
    case '/': kprintf("%d\n", a / b); break;
    case '%': kprintf("%d\n", a % b); break;
    default:  kprintf("Unknown operator '%c'\n", op);
    }
}

static void cmd_crash(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("Dividing by zero... the CPU will raise exception #0\n");
    sleep_ms(1000);
    volatile int zero = 0;          /* volatile: чтобы компилятор не "схитрил" */
    volatile int result = 42 / zero;
    (void)result;
}

static void cmd_reboot(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("Rebooting...\n");
    /* Классический способ: попросить контроллер клавиатуры (8042)
     * "дёрнуть" линию сброса процессора командой 0xFE */
    while (inb(0x64) & 0x02)    /* Ждём, пока контроллер готов принять команду */
        ;
    outb(0x64, 0xFE);
    for (;;) hlt();
}

static void cmd_halt(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("System halted. It is now safe to turn off your computer.\n");
    /* Выключение через ACPI в эмуляторах (в настоящем железе для этого
     * нужно разбирать таблицы ACPI — это сложно) */
    outw(0x604, 0x2000);        /* QEMU */
    outw(0xB004, 0x2000);       /* Bochs и старые QEMU */
    outw(0x4004, 0x3400);       /* VirtualBox */
    for (;;) { cli(); hlt(); }
}

/* Таблица команд: имя -> функция. Чтобы добавить новую команду,
 * достаточно написать функцию и добавить строку сюда. */
struct command {
    const char *name;
    const char *help;
    void (*fn)(int argc, char **argv);
};

static const struct command commands[] = {
    { "help",   "show this list of commands",              cmd_help   },
    { "clear",  "clear the screen",                        cmd_clear  },
    { "echo",   "echo <text> - print text",                cmd_echo   },
    { "info",   "system information and memory map",       cmd_info   },
    { "mem",    "show kernel heap blocks (kmalloc)",       cmd_mem    },
    { "date",   "current date and time from the RTC",      cmd_date   },
    { "uptime", "time since boot (timer ticks)",           cmd_uptime },
    { "sleep",  "sleep <ms> - wait using the timer",       cmd_sleep  },
    { "ps",     "list running tasks",                      cmd_ps     },
    { "run",    "run <counter|spinner|clock> - new task",  cmd_run    },
    { "kill",   "kill <id> - stop a task",                 cmd_kill   },
    { "ls",     "list files",                              cmd_ls     },
    { "cat",    "cat <file> - show file contents",         cmd_cat    },
    { "write",  "write <file> <text> - create/overwrite",  cmd_write  },
    { "append", "append <file> <text> - add to file",      cmd_append },
    { "rm",     "rm <file> - delete file",                 cmd_rm     },
    { "calc",   "calc <a> <op> <b> - calculator",          cmd_calc   },
    { "crash",  "divide by zero -> CPU exception demo",    cmd_crash  },
    { "reboot", "restart the computer",                    cmd_reboot },
    { "halt",   "power off (works in QEMU/Bochs/VBox)",    cmd_halt   },
};
#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

static void cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("Available commands:\n");
    for (uint32_t i = 0; i < NUM_COMMANDS; i++)
        kprintf("  %8s - %s\n", commands[i].name, commands[i].help);
}

/* ---------------------------- Главный цикл ------------------------------- */

void shell_run(void) {
    char line[MAX_LINE];
    char *argv[MAX_ARGS];

    kprintf("\nType 'help' to see the list of commands.\n\n");

    for (;;) {
        vga_set_color(LIGHT_GREEN, BLACK);
        kprintf("minios");
        vga_set_color(LIGHT_GRAY, BLACK);
        kprintf("> ");

        read_line(line, MAX_LINE);                          /* READ */
        strncpy(original_line, line, MAX_LINE);

        int argc = parse(line, argv);
        if (argc == 0) continue;

        const struct command *cmd = NULL;                   /* EVAL */
        for (uint32_t i = 0; i < NUM_COMMANDS; i++)
            if (strcmp(argv[0], commands[i].name) == 0)
                cmd = &commands[i];

        if (cmd) cmd->fn(argc, argv);                       /* PRINT — внутри */
        else kprintf("Unknown command: %s (type 'help')\n", argv[0]);
    }                                                       /* LOOP */
}
