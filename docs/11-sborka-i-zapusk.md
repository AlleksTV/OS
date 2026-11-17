# 11. Сборка, запуск и отладка

## 11.1. Самый простой способ: готовый ISO в VirtualBox

Собирать ничего не нужно: готовые образы лежат в папке [`release/`](../release):

| Файл | Что это | Как подключать |
|---|---|---|
| [`release/minios.iso`](../release/minios.iso) | образ CD-диска | в оптический привод виртуальной машины (**основной способ**) |
| [`release/minios.img`](../release/minios.img) | образ дискеты 1,44 МБ / жёсткого диска | в дисковод (floppy) VirtualBox или в QEMU |

На GitHub откройте файл `release/minios.iso` и нажмите кнопку скачивания (**Download raw file**).

### Создание виртуальной машины (VirtualBox 6.x / 7.x)

1. **Машина → Создать** (Machine → New).
   * **Имя:** `MiniOS`.
   * **Образ ISO:** оставьте **пустым**. Иначе VirtualBox 7 попробует сделать «автоматическую установку»;
     если ISO уже выбран, поставьте галочку **«Пропустить автоматическую установку»** (Skip Unattended Installation).
   * **Тип:** `Other`, **Версия:** `Other/Unknown` (именно 32-битный вариант, **не** `Other/Unknown (64-bit)`).
2. **Оборудование:** оперативная память — `64 МБ` (хватит и 32), процессоров — `1`.
   Галочка **«Включить EFI»** должна быть **снята** (MiniOS загружается через BIOS).
3. **Жёсткий диск:** выберите **«Не подключать виртуальный жёсткий диск»** (Do Not Add a Virtual Hard Disk).
   На предупреждение ответьте «Продолжить».
4. Нажмите **«Готово»**.
5. Откройте **Настроить → Носители** (Settings → Storage). У контроллера **IDE** выберите пустой
   оптический привод (значок диска «Пусто»), справа нажмите на значок диска →
   **«Выбрать файл диска…»** → укажите `minios.iso`.
6. **Настроить → Система → Порядок загрузки:** «Оптический диск» должен стоять в списке выше
   «Жёсткого диска» (по умолчанию так и есть).
7. Нажмите **«Запустить»**.

Через 1–2 секунды появятся строки `[ OK ] ...` и приглашение `minios>`.
Щёлкните мышью внутри окна, чтобы клавиатура передавалась в виртуальную машину; вернуть
клавиатуру и мышь в Windows — клавиша **Host** (по умолчанию **правый Ctrl**).

**Вариант с дискетой.** Вместо ISO можно подключить `minios.img`: **Носители → Добавить контроллер → Floppy**,
затем добавить в него дисковод с файлом `minios.img`. Загрузка с дискеты стоит в порядке загрузки первой.

### Если что-то не работает

| Сообщение | Причина и решение |
|---|---|
| `FATAL: No bootable medium found! System halted.` | ISO не подключён (шаг 5) или выключен в порядке загрузки (шаг 6) |
| `VT-x is not available`, `VERR_VMX_NO_VMX`, `AMD-V is disabled` | в BIOS компьютера выключена аппаратная виртуализация (Intel VT-x / AMD-V); без неё VirtualBox 6.1+ не запускает никакие ВМ — попросите администратора включить её |
| Чёрный экран, ничего не происходит | проверьте, что выключен EFI (шаг 2) и тип ВМ — `Other/Unknown` |
| `DISK ERROR!` | файл образа повреждён при скачивании — скачайте заново (размер `minios.iso` — 1 851 392 байта) |
| Клавиатура не реагирует | щёлкните мышью внутри окна виртуальной машины |
| `date` показывает время на несколько часов вперёд/назад | VirtualBox передаёт в часы CMOS местное время, QEMU — время по Гринвичу (UTC) |

Команда `halt` в VirtualBox выключает виртуальную машину (через порт ACPI `0x4004`), `reboot` перезагружает её.

### Как сделать ISO самому

```bash
sudo apt install xorriso      # программа для создания ISO
make iso                      # → build/minios.iso
make run-iso                  # проверить ISO в QEMU — так же, как его загрузит VirtualBox
make release                  # обновить файлы в папке release/
```

Как устроена загрузка с CD — в [главе 2, раздел 2.5](02-zagruzchik.md#25-загрузка-с-cd-iso-и-с-дискеты).

## 11.2. Что понадобится для сборки

| Программа | Зачем |
|---|---|
| `gcc` (с поддержкой `-m32`) | компилятор C |
| `nasm` | ассемблер для `.asm` файлов |
| `ld`, `objcopy` (пакет binutils) | компоновщик и конвертер ELF → bin |
| `make` | автоматизация сборки |
| `qemu-system-i386` | эмулятор компьютера, в котором запускается ОС |
| `xorriso` (или `genisoimage`) | создание ISO-образа (только для `make iso`) |
| `gdb` (по желанию) | отладчик |

### Linux (Ubuntu, Debian, Mint)

```bash
sudo apt update
sudo apt install build-essential gcc-multilib nasm qemu-system-x86 xorriso gdb
```

### Windows

Проще всего через **WSL** (Windows Subsystem for Linux):

1. Откройте PowerShell от имени администратора: `wsl --install`, перезагрузите компьютер.
2. Запустите «Ubuntu» из меню Пуск и выполните команды для Linux выше.
3. Скопируйте проект (или `git clone`) внутрь WSL и выполните `make run`.
   В Windows 11 окно QEMU откроется само. Если окна нет — используйте `make run-curses`
   или соберите образ в WSL (`make`), а запустите `build/os.img` в QEMU для Windows
   (https://www.qemu.org/download/#windows):
   `qemu-system-i386 -drive format=raw,file=os.img`.

### macOS

```bash
brew install nasm qemu i686-elf-gcc i686-elf-binutils
make CC=i686-elf-gcc LD=i686-elf-ld OBJCOPY=i686-elf-objcopy run
```

(На macOS системный `gcc` — это clang, который не собирает 32-битный код для «голого железа»,
поэтому нужен кросс-компилятор.)

## 11.3. Команды

```bash
make              # собрать build/os.img
make iso          # собрать загрузочный CD build/minios.iso
make release      # скопировать готовые образы в папку release/
make run          # собрать и запустить в окне QEMU
make run-iso      # запустить ISO в QEMU (загрузка с CD, как в VirtualBox)
make run-curses   # запустить в текстовом режиме прямо в терминале
make debug        # запустить и ждать подключения отладчика
make clean        # удалить папку build
```

В режиме `run-curses` выйти из QEMU можно так: `Esc`, затем `2` (откроется консоль QEMU), ввести `quit`.
Или просто выполнить в MiniOS команду `halt`.

## 11.4. Что происходит при `make`

```
$ make
gcc -m32 -ffreestanding ... -c kernel/kernel.c -o build/kernel.o     ← каждый .c
...
nasm -f elf32 kernel/entry.asm -o build/entry.o                       ← каждый .asm ядра
nasm -f elf32 kernel/isr.asm -o build/isr.o
ld -m elf_i386 -T kernel/linker.ld ... -o build/kernel.elf            ← компоновка
objcopy -O binary build/kernel.elf build/kernel.bin                   ← ELF → голый код
nasm -f bin boot/boot.asm -o build/boot.bin -DKERNEL_SECTORS=31       ← загрузчик
cat build/boot.bin build/kernel.bin > build/os.img                    ← образ диска
dd if=/dev/zero of=build/os.img ... seek=1474560                     ← дополнить нулями до 1,44 МБ

$ make iso
xorriso -as mkisofs -V MINIOS -b minios.img -c boot.catalog \
        -o build/minios.iso build/iso                                 ← CD-образ El Torito
```

Ключевые флаги компилятора:

| Флаг | Зачем |
|---|---|
| `-m32` | 32-битный код (процессор у нас в 32-битном защищённом режиме) |
| `-ffreestanding` | «автономная» программа: нет стандартной библиотеки, нет ОС под нами |
| `-nostdlib` | не подключать libc |
| `-fno-builtin` | не подменять наши `memcpy`/`strlen` встроенными версиями компилятора |
| `-fno-stack-protector` | защита стека требует поддержки со стороны libc |
| `-fno-pie` | код рассчитан на фиксированные адреса (0x10000), а не на любые |
| `-mgeneral-regs-only` | не использовать регистры FPU/SSE — мы их не инициализировали и не сохраняем при переключении задач |
| `-g` | отладочная информация в `kernel.elf` (в `kernel.bin` не попадает) |

Полезно посмотреть на результат:

```bash
objdump -d build/kernel.elf | less       # дизассемблированное ядро
nm -n build/kernel.elf                   # все функции и переменные с адресами
ndisasm -b 16 build/boot.bin | less      # дизассемблированный загрузчик
hexdump -C build/boot.bin | tail -3      # в конце видна подпись 55 aa
```

## 11.5. Отладка с GDB

QEMU умеет работать как сервер отладки. В одном терминале:

```bash
make debug          # QEMU запустится «на паузе» и будет ждать отладчик на порту 1234
```

В другом:

```bash
gdb build/kernel.elf
(gdb) target remote :1234
(gdb) break kmain           # остановиться в начале ядра
(gdb) continue
(gdb) next                  # выполнить строку
(gdb) info registers        # посмотреть регистры
(gdb) break schedule        # поймать момент переключения задач
(gdb) print tasks           # посмотреть массив задач
(gdb) x/16xb 0xb8000        # посмотреть видеопамять
```

Для отладки загрузчика: `break *0x7c00` и `set architecture i8086`.

Монитор QEMU (в окне QEMU: `Ctrl+Alt+2`, возврат — `Ctrl+Alt+1`) тоже полезен:
`info registers`, `info pic`, `x /10i $eip`.

## 11.6. Запуск на реальном компьютере

Образ можно записать на флешку и загрузить реальный компьютер, если в BIOS/UEFI включён режим
**Legacy / CSM** (на многих новых ноутбуках его уже нет).

> ⚠️ Команда `dd` полностью перезапишет устройство. Ошибка в имени диска уничтожит данные.
> Трижды проверьте имя флешки командой `lsblk`.

```bash
sudo dd if=build/os.img of=/dev/sdX bs=512 conv=fsync
```

В Windows для этого подходит программа Rufus (режим DD) или balenaEtcher.

Про VirtualBox — в разделе [11.1](#111-самый-простой-способ-готовый-iso-в-virtualbox). Образ также запускается в Bochs.

## 11.7. Частые проблемы

| Симптом | Причина и решение |
|---|---|
| `gcc: error: unrecognized command-line option '-m32'` или нет `bits/...h` | установите `gcc-multilib` |
| `qemu-system-i386: command not found` | установите `qemu-system-x86` |
| `Could not initialize SDL` / нет окна | нет графики (SSH, WSL без GUI) — используйте `make run-curses` |
| На экране `DISK ERROR!` | образ повреждён — пересоберите (`make -B`) |
| Система перезагружается по кругу | «тройная ошибка» (triple fault) — обычно после ваших изменений в GDT/IDT. Запустите `qemu-system-i386 -drive format=raw,file=build/os.img -d int,cpu_reset -no-reboot` и смотрите последнее исключение |

---
[← 10. Оболочка](10-obolochka.md) · Далее: [12. Вопросы и термины →](12-voprosy-i-terminy.md)
