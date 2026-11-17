/* keyboard.h — драйвер клавиатуры PS/2 */
#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_init(void);
char keyboard_getchar(void);    /* Ждать нажатия и вернуть символ */

#endif
