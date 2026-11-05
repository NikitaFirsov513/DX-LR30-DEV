#include "clock.h"
#include "config.h"
#include "stm32f1xx.h"

static volatile uint32_t milliseconds;

_Static_assert(BOARD_HSE_HZ == 8000000U && BOARD_CORE_HZ == 72000000U,
               "Review clock tree before changing PJ26 clock frequencies");
_Static_assert(BOARD_APB1_HZ * 2U == BOARD_CORE_HZ && BOARD_APB2_HZ == BOARD_CORE_HZ,
               "Clock prescalers must match configuration");

static void clock_fail(void)
{
    /* Startup failed before UART is available. Inspect with SWD; no silent fallback. */
    __disable_irq();
    for (;;) { __NOP(); }
}

static void wait_bits(volatile uint32_t *reg, uint32_t mask, uint32_t expected)
{
    uint32_t started = clock_millis();
    while ((*reg & mask) != expected) {
        if ((uint32_t)(clock_millis() - started) >= 100U) clock_fail();
    }
}

void clock_init(void)
{
    /* Vendor SystemInit leaves HSI running at 8 MHz. SysTick bounds oscillator waits. */
    NVIC_SetPriorityGrouping(3U);
    if (SysTick_Config(BOARD_HSI_HZ / 1000U) != 0U) clock_fail();
    RCC->CR |= RCC_CR_HSEON;
    wait_bits(&RCC->CR, RCC_CR_HSERDY, RCC_CR_HSERDY);
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 |
                    RCC_CFGR_ADCPRE | RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL)) |
                RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_ADCPRE_DIV6 | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;
    RCC->CR |= RCC_CR_PLLON;
    wait_bits(&RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY);
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    wait_bits(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL);
    SystemCoreClockUpdate();
    if (SystemCoreClock != BOARD_CORE_HZ || SysTick_Config(SystemCoreClock / 1000U) != 0U)
        clock_fail();
}

void SysTick_Handler(void) { milliseconds++; }
uint32_t clock_millis(void) { return milliseconds; }
void clock_delay_ms(uint32_t duration)
{
    uint32_t started = clock_millis();
    while ((uint32_t)(clock_millis() - started) < duration) { __NOP(); }
}
