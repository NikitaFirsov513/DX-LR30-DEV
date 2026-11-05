#include "stm32f1xx.h"

/* Keep fault handlers independent of UART and clocks. Inspect SCB via SWD. */
static void fault_stop(void)
{
    __disable_irq();
    for (;;) { __NOP(); }
}

void HardFault_Handler(void) { fault_stop(); }
void MemManage_Handler(void) { fault_stop(); }
void BusFault_Handler(void) { fault_stop(); }
void UsageFault_Handler(void) { fault_stop(); }
