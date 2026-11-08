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
#include "network.h"
#include "node_console.h"
#include "identity.h"
#endif
#include <stdio.h>
#include <string.h>

static serial_frames_t frames;
#ifndef UART_ONLY
static network_t network;
static node_console_t console;
static char service_output[NODE_REPLY_BYTES];
static volatile bool first_uart_sampled;
static volatile uint32_t first_uart_sample;
static bool session_seeded;
#endif

/* These callbacks run in IRQ context. They never access the radio or print. */
static void on_byte(uint8_t byte)
{
#ifndef UART_ONLY
    if (!first_uart_sampled) {
        first_uart_sample=identity_timing_sample();
        first_uart_sampled=true;
    }
#endif
    serial_frames_feed(&frames, byte);
}
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
    size_t size = diagnostics_format(service_output, sizeof service_output, &stats, radio_state(),
                                      clock_millis(), uart_error_count(), dropped);
    if (!size || !uart_write((const uint8_t *)service_output, size)) return false;
    size=node_console_diagnostics(&network,service_output,sizeof service_output);
    return size && uart_write((const uint8_t *)service_output,size);
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
    net_packet_t packet;
    if (net_receive(&network,data,length,clock_millis(),&packet) == NET_ACCEPTED) {
        if (!uart_write(packet.data,packet.length)) panic("UART TX timeout");
    }
}
#endif

int main(void)
{
    board_init();
    serial_frames_init(&frames);
#ifndef UART_ONLY
    uint32_t uid[3];
    identity_read_uid(uid);
    uint32_t id=net_id_from_uid(uid);
    (void)net_init(&network,id,net_session_tag(id,identity_timing_sample(),0));
    (void)serial_frames_set_limit(&frames,NET_MAX_PAYLOAD);
#endif
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
    printf("KEY: local radio/network statistics (9600 8N1)\r\n");
    (void)node_console_info(&console,&network,0,"OK",service_output,sizeof service_output);
    if (!uart_write((const uint8_t *)service_output,strlen(service_output))) panic("UART TX timeout");
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
        if (!session_seeded) {
            network.session=net_session_tag(network.node_id,first_uart_sample,identity_timing_sample());
            session_seeded=true;
        }
        if (node_console_process(&console,&network,data,length,service_output,sizeof service_output)) {
            if (!service_output[0] || !uart_write((const uint8_t *)service_output,strlen(service_output)))
                panic("Node console UART timeout");
            continue;
        }
        uint8_t wire[255], wire_length;
        if (!net_encode(&network,data,length,wire,&wire_length)) panic("Network encoding");
        if (!radio_send(wire, wire_length)) panic(radio_error());
#endif
    }
}
