#include "diagnostics.h"
#include <stdio.h>

void diagnostics_button_init(diagnostics_button_t *button, bool pressed, uint32_t now)
{
    *button = (diagnostics_button_t){.raw_pressed=pressed, .stable_pressed=pressed, .changed_at=now};
}

bool diagnostics_button_poll(diagnostics_button_t *button, bool pressed, uint32_t now, bool can_report)
{
    if (pressed != button->raw_pressed) {
        button->raw_pressed = pressed;
        button->changed_at = now;
    }
    if (pressed != button->stable_pressed &&
        (uint32_t)(now - button->changed_at) >= DIAGNOSTICS_DEBOUNCE_MS) {
        button->stable_pressed = pressed;
        if (pressed) button->pending = true;
    }
    if (!can_report || !button->pending) return false;
    button->pending = false;
    return true;
}

size_t diagnostics_format(char *data, size_t capacity, const radio_stats_t *stats,
                          radio_state_t state, uint32_t uptime_ms,
                          uint32_t uart_errors, uint32_t uart_dropped)
{
    const char *name = "UNKNOWN";
    switch (state) {
    case RADIO_STARTING: name = "STARTING"; break;
    case RADIO_RECEIVING: name = "RX"; break;
    case RADIO_TRANSMITTING: name = "TX"; break;
    case RADIO_FAULT: name = "FAULT"; break;
    }
    char quality[48];
    if (stats->last_rx_valid) {
        (void)snprintf(quality, sizeof quality, "rssi=%d dBm snr=%d dB",
                       (int)stats->last_rssi_dbm, (int)stats->last_snr_db);
    } else {
        (void)snprintf(quality, sizeof quality, "rssi=N/A snr=N/A");
    }
    int count = snprintf(data, capacity,
        "\r\n[diag] state=%s uptime_ms=%lu\r\n"
        "[diag] tx_started=%lu tx_done=%lu tx_bytes=%lu rx_ok=%lu rx_bytes=%lu\r\n"
        "[diag] crc_err=%lu header_err=%lu no_crc=%lu tx_timeout=%lu rx_timeout=%lu watchdog=%lu radio_err=%lu\r\n"
        "[diag] uart_err=%lu uart_drop=%lu %s\r\n",
        name, (unsigned long)uptime_ms,
        (unsigned long)stats->tx_started, (unsigned long)stats->tx_packets,
        (unsigned long)stats->tx_bytes, (unsigned long)stats->rx_packets, (unsigned long)stats->rx_bytes,
        (unsigned long)stats->crc_errors, (unsigned long)stats->header_errors, (unsigned long)stats->rx_no_crc,
        (unsigned long)stats->tx_timeouts, (unsigned long)stats->rx_timeouts,
        (unsigned long)stats->tx_watchdogs, (unsigned long)stats->transport_errors,
        (unsigned long)uart_errors, (unsigned long)uart_dropped, quality);
    return count > 0 && (size_t)count < capacity ? (size_t)count : 0U;
}
