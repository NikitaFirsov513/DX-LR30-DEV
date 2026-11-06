#ifndef APP_DIAGNOSTICS_H
#define APP_DIAGNOSTICS_H
#include "radio.h"
#include <stddef.h>

#define DIAGNOSTICS_DEBOUNCE_MS 30U
#define DIAGNOSTICS_REPORT_BYTES 640U

typedef struct {
    bool raw_pressed, stable_pressed, pending;
    uint32_t changed_at;
} diagnostics_button_t;

/* A held key produces one request; requests wait until the radio leaves TX. */
void diagnostics_button_init(diagnostics_button_t *button, bool pressed, uint32_t now);
bool diagnostics_button_poll(diagnostics_button_t *button, bool pressed, uint32_t now, bool can_report);
/* Returns zero on formatting failure/truncation. No UART/radio access here. */
size_t diagnostics_format(char *data, size_t capacity, const radio_stats_t *stats,
                          radio_state_t state, uint32_t uptime_ms,
                          uint32_t uart_errors, uint32_t uart_dropped);
#endif
