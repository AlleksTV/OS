/* ============================================================================
 * types.h — базовые типы
 * ============================================================================
 * Ядро не может использовать стандартную библиотеку C (libc): её функции
 * (printf, malloc, fopen ...) сами обращаются к операционной системе, а ОС —
 * это мы. Поэтому всё нужное приходится писать самим, начиная с типов.
 * ==========================================================================*/
#ifndef TYPES_H
#define TYPES_H

typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;
typedef signed char         int8_t;
typedef short               int16_t;
typedef int                 int32_t;
typedef unsigned int        size_t;

#define NULL ((void*)0)
#define true  1
#define false 0
typedef int bool;

#endif
