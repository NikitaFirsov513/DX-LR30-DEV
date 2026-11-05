#ifndef MCU_CRITICAL_H
#define MCU_CRITICAL_H
#include "stm32f1xx.h"
static inline uint32_t irq_lock(void)
{
    uint32_t saved = __get_PRIMASK();
    __disable_irq();
    __DMB();
    return saved;
}
static inline void irq_restore(uint32_t saved)
{
    __DMB();
    __set_PRIMASK(saved);
}
#endif
