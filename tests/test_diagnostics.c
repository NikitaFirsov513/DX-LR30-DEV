#include "diagnostics.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    diagnostics_button_t button;
    diagnostics_button_init(&button, false, 0);
    assert(!diagnostics_button_poll(&button, false, 100, true));
    assert(!diagnostics_button_poll(&button, true, 101, true));
    assert(!diagnostics_button_poll(&button, false, 105, true)); /* contact bounce */
    assert(!diagnostics_button_poll(&button, true, 110, true));
    assert(!diagnostics_button_poll(&button, true, 139, true));
    assert(diagnostics_button_poll(&button, true, 140, true));
    assert(!diagnostics_button_poll(&button, true, 2000, true)); /* held, no flooding */
    assert(!diagnostics_button_poll(&button, false, 2001, true));
    assert(!diagnostics_button_poll(&button, false, 2031, true));
    assert(!diagnostics_button_poll(&button, true, 2032, false));
    assert(!diagnostics_button_poll(&button, true, 2062, false)); /* TX in progress */
    assert(!diagnostics_button_poll(&button, false, 2100, false));
    assert(!diagnostics_button_poll(&button, false, 2130, false));
    assert(diagnostics_button_poll(&button, false, 2131, true)); /* request survives release */
    assert(!diagnostics_button_poll(&button, false, 2132, true));
    diagnostics_button_init(&button, false, 0xfffffff0U);
    assert(!diagnostics_button_poll(&button, true, 0xfffffff1U, true));
    assert(diagnostics_button_poll(&button, true, 0x10U, true)); /* tick rollover */
    diagnostics_button_init(&button, true, 0);
    assert(!diagnostics_button_poll(&button, true, 100, true)); /* held during boot */

    char report[DIAGNOSTICS_REPORT_BYTES];
    radio_stats_t stats = {0};
    size_t size = diagnostics_format(report, sizeof report, &stats, RADIO_RECEIVING, 123, 0, 0);
    assert(size == strlen(report) && strstr(report, "state=RX uptime_ms=123"));
    assert(strstr(report, "tx_done=0") && strstr(report, "rssi=N/A snr=N/A"));
    stats = (radio_stats_t){
        .tx_started=UINT32_MAX, .tx_packets=UINT32_MAX, .tx_bytes=UINT32_MAX,
        .rx_packets=UINT32_MAX, .rx_bytes=UINT32_MAX, .crc_errors=UINT32_MAX,
        .header_errors=UINT32_MAX, .rx_no_crc=UINT32_MAX, .tx_timeouts=UINT32_MAX,
        .rx_timeouts=UINT32_MAX, .tx_watchdogs=UINT32_MAX, .transport_errors=UINT32_MAX,
        .last_rx_valid=true, .last_rssi_dbm=-100, .last_snr_db=-8
    };
    size = diagnostics_format(report, sizeof report, &stats, RADIO_FAULT, UINT32_MAX, UINT32_MAX, UINT32_MAX);
    assert(size && size == strlen(report)); /* Maximum counters must still fit. */
    assert(strstr(report, "radio_err=4294967295") && strstr(report, "uart_drop=4294967295"));
    assert(strstr(report, "rssi=-100 dBm snr=-8 dB"));
    stats.last_snr_db = 5;
    assert(diagnostics_format(report, sizeof report, &stats, RADIO_TRANSMITTING, 1, 2, 3));
    assert(strstr(report, "state=TX") && strstr(report, "snr=5 dB"));
    assert(!diagnostics_format(report, 10, &stats, RADIO_RECEIVING, 0, 0, 0));
    assert(!diagnostics_format(NULL, 0, &stats, RADIO_RECEIVING, 0, 0, 0));
    puts("PASS: KEY debounce/hold/deferred report/rollover, RSSI/SNR and bounded diagnostic output");
    return 0;
}
