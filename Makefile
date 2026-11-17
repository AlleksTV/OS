# =============================================================================
#  Makefile — сборка MiniOS
# =============================================================================
#  make        — собрать образ диска build/os.img
#  make run    — собрать и запустить в эмуляторе QEMU
#  make debug  — запустить QEMU и ждать подключения отладчика GDB
#  make clean  — удалить результаты сборки
#
#  Как устроена сборка (подробно: docs/11-sborka-i-zapusk.md):
#    1. Каждый .c компилируется GCC в объектный файл .o (32-битный код,
#       без стандартной библиотеки).
#    2. Каждый .asm ядра ассемблируется NASM в .o (формат ELF32).
#    3. Компоновщик ld по скрипту linker.ld собирает kernel.elf.
#    4. objcopy вырезает из ELF "голый" машинный код -> kernel.bin.
#    5. NASM собирает загрузчик boot.bin (ровно 512 байт), подставляя
#       в него размер ядра в секторах.
#    6. os.img = boot.bin + kernel.bin (+ дополнение нулями до 1 МБ).
# =============================================================================

CC      = gcc
AS      = nasm
LD      = ld
OBJCOPY = objcopy
QEMU    = qemu-system-i386

# Флаги компилятора:
#   -m32                 — генерировать 32-битный код x86
#   -ffreestanding       — "автономная" среда: нет libc, main() не обязателен
#   -fno-pie             — код по фиксированным адресам
#   -fno-stack-protector — защита стека требует поддержки libc, отключаем
#   -nostdlib            — не подключать стандартные библиотеки
#   -mgeneral-regs-only  — не использовать FPU/SSE (мы их не инициализируем)
#   -O2                  — оптимизация
#   -g                   — отладочная информация для GDB (попадает только в kernel.elf)
CFLAGS  = -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib \
          -fno-builtin -fno-asynchronous-unwind-tables -mgeneral-regs-only \
          -O2 -g -Wall -Wextra -std=gnu99
LDFLAGS = -m elf_i386 -T kernel/linker.ld -nostdlib
# Новые версии ld предупреждают о сегменте с правами RWX — для ядра это нормально.
# Старые версии (binutils < 2.39) такого флага не знают, поэтому сначала проверяем.
LDFLAGS += $(shell $(LD) --help 2>/dev/null | grep -q no-warn-rwx-segments && echo --no-warn-rwx-segments)

BUILD   = build
C_SRC   = $(wildcard kernel/*.c)
ASM_OBJ = $(BUILD)/entry.o $(BUILD)/isr.o
C_OBJ   = $(patsubst kernel/%.c,$(BUILD)/%.o,$(C_SRC))

.PHONY: all run run-curses debug clean

all: $(BUILD)/os.img

$(BUILD):
	mkdir -p $(BUILD)

# --- Ядро ---
$(BUILD)/%.o: kernel/%.c kernel/*.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: kernel/%.asm | $(BUILD)
	$(AS) -f elf32 $< -o $@

$(BUILD)/kernel.elf: $(ASM_OBJ) $(C_OBJ) kernel/linker.ld
	$(LD) $(LDFLAGS) $(ASM_OBJ) $(C_OBJ) -o $@

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@

# --- Загрузчик: размер ядра в секторах (по 512 байт, с округлением вверх) ---
$(BUILD)/boot.bin: boot/boot.asm $(BUILD)/kernel.bin
	$(AS) -f bin boot/boot.asm -o $@ \
	    -DKERNEL_SECTORS=$$(( ($$(wc -c < $(BUILD)/kernel.bin) + 511) / 512 ))

# --- Образ диска (дополняем нулями до 1 МБ) ---
$(BUILD)/os.img: $(BUILD)/boot.bin $(BUILD)/kernel.bin
	cat $^ > $@
	dd if=/dev/zero of=$@ bs=1 count=0 seek=1048576 2>/dev/null
	@echo
	@echo "Готово: $@  (ядро: $$(wc -c < $(BUILD)/kernel.bin) байт)"
	@echo "Запуск: make run"

# --- Запуск ---
run: $(BUILD)/os.img
	$(QEMU) -drive format=raw,file=$< -m 32M

# Запуск прямо в терминале (без графического окна).
# Выход: Esc, затем 2 (консоль QEMU), команда quit
run-curses: $(BUILD)/os.img
	$(QEMU) -drive format=raw,file=$< -m 32M -display curses

# Отладка: QEMU ждёт отладчик на порту 1234. В другом терминале:
#   gdb build/kernel.elf -ex "target remote :1234" -ex "break kmain" -ex "continue"
debug: $(BUILD)/os.img
	$(QEMU) -drive format=raw,file=$< -m 32M -s -S

clean:
	rm -rf build
