#ifndef APP_NETWORK_H
#define APP_NETWORK_H
#include <stdbool.h>
#include <stdint.h>

#define NET_BROADCAST UINT32_MAX
#define NET_HEADER_BYTES 21U
#define NET_MAX_PAYLOAD (255U - NET_HEADER_BYTES)
#define NET_VERSION 1U
#define NET_DUP_ENTRIES 32U
#define NET_DUP_WINDOW_MS 60000U

typedef struct {
    uint32_t source, destination, session, sequence;
} net_header_t;
typedef enum {
    NET_ACCEPTED=0, NET_FOREIGN=1, NET_DUPLICATE=2, NET_SELF=3, NET_INVALID=4
} net_rx_result_t;
typedef struct {
    uint32_t tx_prepared, tx_bytes, rx_accepted, rx_bytes, rx_broadcast;
    uint32_t rx_foreign, rx_duplicate, rx_invalid, rx_self;
    net_header_t last_tx, last_seen;
    net_rx_result_t last_result;
    bool last_tx_valid, last_seen_valid;
} net_stats_t;
typedef struct {
    uint32_t source, session, sequence, received_at;
} net_duplicate_t;
typedef struct {
    uint32_t node_id, destination, session, next_sequence;
    net_stats_t stats;
    net_duplicate_t duplicates[NET_DUP_ENTRIES];
    uint8_t next_duplicate;
} network_t;
typedef struct {
    net_header_t header;
    const uint8_t *data;
    uint8_t length;
} net_packet_t;

bool net_node_id_valid(uint32_t id);
uint32_t net_id_from_uid(const uint32_t uid[3]);
uint32_t net_session_tag(uint32_t id, uint32_t first_sample, uint32_t second_sample);
bool net_init(network_t *net, uint32_t id, uint32_t session);
bool net_set_id(network_t *net, uint32_t id);
bool net_set_destination(network_t *net, uint32_t destination);
/* Main-loop-only; no hardware access, allocation or packed-struct serialization. */
bool net_encode(network_t *net, const uint8_t *payload, uint8_t length,
                uint8_t wire[255], uint8_t *wire_length);
bool net_decode(const uint8_t *wire, uint8_t length, net_packet_t *packet);
net_rx_result_t net_receive(network_t *net, const uint8_t *wire, uint8_t length,
                            uint32_t now, net_packet_t *packet);
#endif
