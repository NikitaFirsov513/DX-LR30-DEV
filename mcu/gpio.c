#include "gpio.h"

void gpio_write(gpio_pin_t pin, bool high)
{
    pin.port->BSRR = 1UL << (pin.bit + (high ? 0U : 16U));
}

bool gpio_read(gpio_pin_t pin) { return (pin.port->IDR & (1UL << pin.bit)) != 0; }

void gpio_config(gpio_pin_t pin, gpio_mode_t mode, bool initial)
{
    /* Load ODR before enabling output, avoiding a low pulse on NSS/reset. */
    gpio_write(pin, initial);
    volatile uint32_t *cr = pin.bit < 8U ? &pin.port->CRL : &pin.port->CRH;
    uint32_t shift = (pin.bit & 7U) * 4U;
    *cr = (*cr & ~(0xfUL << shift)) | ((uint32_t)mode << shift);
}
