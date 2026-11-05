#ifndef MCU_CLOCK_H
#define MCU_CLOCK_H
#include <stdint.h>
void clock_init(void);
uint32_t clock_millis(void);
void clock_delay_ms(uint32_t duration);
#endif
