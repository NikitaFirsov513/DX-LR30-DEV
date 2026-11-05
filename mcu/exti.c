#include "exti.h"

static void (*radio_notify)(void);
static uint32_t radio_mask;

void exti_radio_init(gpio_pin_t pin, void (*notify)(void))
{
    radio_notify = notify;
    radio_mask = 1UL << pin.bit;
    /* F1 AFIO EXTICR port encoding: A=0, B=1, C=2. */
    uint32_t port = (uint32_t)(((uintptr_t)pin.port - (uintptr_t)GPIOA) / 0x400U);
    uint32_t shift = (pin.bit & 3U) * 4U;
    AFIO->EXTICR[pin.bit / 4U] = (AFIO->EXTICR[pin.bit / 4U] & ~(0xfUL << shift)) | (port << shift);
    EXTI->IMR &= ~radio_mask;
    EXTI->FTSR &= ~radio_mask;
    EXTI->RTSR |= radio_mask;
    EXTI->PR = radio_mask;
    NVIC_SetPriority(EXTI15_10_IRQn, 1U);
    NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
    NVIC_EnableIRQ(EXTI15_10_IRQn);
    EXTI->IMR |= radio_mask;
    /* A completed radio command may have raised DIO1 before EXTI was enabled. */
    if (gpio_read(pin) && radio_notify) radio_notify();
}

void EXTI15_10_IRQHandler(void)
{
    if (EXTI->PR & radio_mask) {
        EXTI->PR = radio_mask;
        if (radio_notify) radio_notify();
    }
}
