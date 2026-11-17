/* timer.h — системный таймер (PIT) и часы реального времени (RTC) */
#ifndef TIMER_H
#define TIMER_H

#include "types.h"

#define TIMER_HZ 100                 /* Тиков таймера в секунду */

struct rtc_time {                    /* Дата и время из CMOS-часов */
    int year, month, day, hour, minute, second;
};

void     timer_init(void);
uint32_t timer_ticks(void);           /* Сколько тиков прошло с загрузки */
void     sleep_ms(uint32_t ms);       /* Подождать указанное число миллисекунд */
void     rtc_read(struct rtc_time *t);
uint32_t cmos_memory_kb(void);        /* Объём ОЗУ по данным CMOS */

#endif
