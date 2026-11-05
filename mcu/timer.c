#include "timer.h"
#include "config.h"
#include "stm32f1xx.h"
#include "critical.h"

static void (*idle_callback)(void);
_Static_assert(BOARD_UART_IDLE_US > 0U && BOARD_UART_IDLE_US <= 65536U, "TIM3 idle period");

void timer_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM3EN;
    (void)RCC->APB1ENR;
    TIM2->CR1 = 0;
    TIM2->PSC = BOARD_TIMER_HZ / 1000000U - 1U;
    TIM2->ARR = 0xffffU;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0;
    TIM2->CR1 = TIM_CR1_CEN;
}

void timer_delay_us(uint16_t duration)
{
    uint16_t started = (uint16_t)TIM2->CNT;
    while ((uint16_t)((uint16_t)TIM2->CNT - started) < duration) { __NOP(); }
}

void timer_idle_init(void (*callback)(void))
{
    idle_callback = callback;
    TIM3->CR1 = TIM_CR1_OPM | TIM_CR1_URS;
    TIM3->PSC = BOARD_TIMER_HZ / 1000000U - 1U;
    TIM3->ARR = BOARD_UART_IDLE_US - 1U;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->SR = 0;
    TIM3->DIER = TIM_DIER_UIE;
    NVIC_SetPriority(TIM3_IRQn, 2U);
    NVIC_ClearPendingIRQ(TIM3_IRQn);
    NVIC_EnableIRQ(TIM3_IRQn);
}

void timer_idle_cancel(void)
{
    TIM3->CR1 &= ~TIM_CR1_CEN;
    TIM3->SR = 0;
    NVIC_ClearPendingIRQ(TIM3_IRQn);
}

void timer_idle_restart(void)
{
    timer_idle_cancel();
    TIM3->CNT = 0;
    TIM3->CR1 |= TIM_CR1_CEN;
}

void TIM3_IRQHandler(void)
{
    /* UART has higher priority. Serialize buffer flush against a new RX byte. */
    uint32_t saved = irq_lock();
    if (TIM3->SR & TIM_SR_UIF) {
        TIM3->SR = 0;
        if (idle_callback) idle_callback();
    }
    irq_restore(saved);
}
