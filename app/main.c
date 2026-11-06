#include "board.h"
#include "config.h"
#include "clock.h"
#include "critical.h"
#include "timer.h"
#include "uart.h"
#include "serial_frames.h"
#ifndef UART_ONLY
#include "spi.h"
#include "radio.h"
#include "diagnostics.h"
#endif
#include <stdio.h>

static serial_frames_t frames;

/* These callbacks run in IRQ context. They never access the radio or print. */
static void on_byte(uint8_t byte) { serial_frames_feed(&frames, byte); }
static void on_idle(void) { serial_frames_flush(&frames); }
static void on_uart_error(void) { serial_frames_discard(&frames); }

#ifndef UART_ONLY
static bool write_radio_diagnostics(void)
{
    radio_stats_t stats;
    radio_get_stats(&stats);
    uint32_t saved = irq_lock();
    uint32_t dropped = frames.dropped;
    irq_restore(saved);
    char report[DIAGNOSTICS_REPORT_BYTES];
    size_t size = diagnostics_format(report, sizeof report, &stats, radio_state(),
                                      clock_millis(), uart_error_count(), dropped);
    return size && uart_write((const uint8_t *)report, size);
}
#endif

static void panic(const char *message)
{
    printf("ERROR: %s\r\n", message);
#ifndef UART_ONLY
    (void)write_radio_diagnostics();
#endif
    board_radio_off();
    for (;;) { __WFI(); }
}

#ifndef UART_ONLY
static void on_radio_receive(const uint8_t *data, uint8_t length)
{
    if (!uart_write(data, length)) panic("UART TX timeout");
}
#endif

int main(void)
{
    board_init();
    serial_frames_init(&frames);
    timer_idle_init(on_idle);
    uart_init(BOARD_UART_BAUD, on_byte, on_uart_error);
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("PJ26 CMSIS: UART1 9600 8N1; SYSCLK 72MHz\r\n");
#ifdef UART_ONLY
    printf("UART_ONLY: echo test; radio not initialized\r\n");
#else
    spi_init();
    if (!radio_init(on_radio_receive)) panic(radio_error());
    board_radio_irq_init(radio_notify_irq);
    diagnostics_button_t diagnostics;
    diagnostics_button_init(&diagnostics, board_key_pressed(), clock_millis());
    printf("Radio ready: 868100000 Hz; SF9 BW125 CR4/6; CRC on; +10 dBm\r\n");
    printf("KEY: local radio statistics (9600 8N1)\r\n");
#endif
    for (;;) {
#ifndef UART_ONLY
        radio_poll();
        if (radio_state() == RADIO_FAULT) panic(radio_error());
        if (diagnostics_button_poll(&diagnostics, board_key_pressed(), clock_millis(), radio_can_send())) {
            if (!write_radio_diagnostics()) panic("Diagnostics UART timeout");
            /* Poll the radio again after the blocking UART report before starting TX. */
            continue;
        }
        if (!radio_can_send()) continue;
#endif
        uint8_t data[FRAME_MAX_BYTES], length;
        uint32_t saved = irq_lock();
        bool available = serial_frames_take(&frames, data, &length);
        irq_restore(saved);
        if (!available) continue;
#ifdef UART_ONLY
        if (!uart_write(data, length)) panic("UART TX timeout");
#else
        if (!radio_send(data, length)) panic(radio_error());
#endif
    }
}
