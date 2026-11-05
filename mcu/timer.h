#ifndef MCU_TIMER_H
#define MCU_TIMER_H
#include <stdint.h>
void timer_init(void);
void timer_delay_us(uint16_t duration);
void timer_idle_init(void (*callback)(void));
void timer_idle_restart(void);
void timer_idle_cancel(void);
#endif
