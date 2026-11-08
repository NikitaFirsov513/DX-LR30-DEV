#ifndef APP_NODE_CONSOLE_H
#define APP_NODE_CONSOLE_H
#include "network.h"
#include <stddef.h>

#define NODE_REPLY_BYTES 640U
typedef struct { bool raw_next; } node_console_t;
/* Local UART input only. Returns true when consumed as a command, false for payload.
 * Commands must occupy one UART frame; no radio payload is parsed as a command.
 */
bool node_console_process(node_console_t *console, network_t *net,
                          const uint8_t *data, uint8_t length, char *reply, size_t capacity);
size_t node_console_info(const node_console_t *console, const network_t *net,
                          uint32_t request, const char *status, char *reply, size_t capacity);
size_t node_console_diagnostics(const network_t *net, char *reply, size_t capacity);
#endif
