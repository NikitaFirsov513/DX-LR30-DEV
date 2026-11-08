#include "identity.h"
#include "clock.h"
#include "stm32f1xx.h"

void identity_read_uid(uint32_t words[3])
{
    const volatile uint32_t *uid=(const volatile uint32_t *)UID_BASE;
    for (unsigned i=0; i<3; i++) words[i]=uid[i];
}
uint32_t identity_timing_sample(void)
{
    /* Read existing counters only; do not change SysTick/TIM2 configuration. */
    return clock_millis()*2654435761U ^ (SysTick->VAL << 8) ^ TIM2->CNT;
}
