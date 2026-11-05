#include "uart.h"
#include "timer.h"
#include "clock.h"
#include "config.h"
#include "stm32f1xx.h"

static void (*receive_callback)(uint8_t);
static void (*error_callback)(void);
static volatile uint32_t errors;

void uart_init(uint32_t baud, void (*receive)(uint8_t), void (*error)(void))
{
    receive_callback = receive;
    error_callback = error;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC->APB2ENR;
    USART1->CR1 = 0;
    USART1->CR2 = 0;
    USART1->CR3 = USART_CR3_EIE;
    USART1->BRR = (BOARD_APB2_HZ + baud / 2U) / baud;
    (void)USART1->SR;
    (void)USART1->DR;
    NVIC_SetPriority(USART1_IRQn, 0U);
    NVIC_ClearPendingIRQ(USART1_IRQn);
    NVIC_EnableIRQ(USART1_IRQn);
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_PEIE;
}

bool uart_write(const uint8_t *data, size_t length)
{
    uint32_t started = clock_millis();
    for (size_t i = 0; i < length; i++) {
        while (!(USART1->SR & USART_SR_TXE)) {
            if ((uint32_t)(clock_millis() - started) >= BOARD_UART_TIMEOUT_MS) return false;
        }
        USART1->DR = data[i];
    }
    while (!(USART1->SR & USART_SR_TC)) {
        if ((uint32_t)(clock_millis() - started) >= BOARD_UART_TIMEOUT_MS) return false;
    }
    return true;
}

void USART1_IRQHandler(void)
{
    uint32_t status = USART1->SR;
    if (status & (USART_SR_RXNE | USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE)) {
        uint8_t value = (uint8_t)USART1->DR; /* SR then DR clears RXNE and error flags. */
        timer_idle_cancel();
        if (status & (USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE)) {
            errors++;
            if (error_callback) error_callback();
        } else if (receive_callback) {
            receive_callback(value);
        }
        timer_idle_restart();
    }
}

uint32_t uart_error_count(void) { return errors; }

int _write(int fd, char *data, int length)
{
    (void)fd;
    if (length < 0 || !uart_write((const uint8_t *)data, (size_t)length)) return -1;
    return length;
}
