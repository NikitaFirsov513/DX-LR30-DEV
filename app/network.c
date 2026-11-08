#include "network.h"
#include <string.h>

static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void write32(uint8_t *p, uint32_t value)
{
    for (unsigned i=0; i<4; i++) p[i]=(uint8_t)(value >> (8U*i));
}
static uint32_t hash_word(uint32_t hash, uint32_t word)
{
    for (unsigned i=0; i<4; i++) { hash ^= (uint8_t)(word >> (8U*i)); hash *= 16777619U; }
    return hash;
}
bool net_node_id_valid(uint32_t id) { return id != 0 && id != NET_BROADCAST; }
uint32_t net_id_from_uid(const uint32_t uid[3])
{
    uint32_t hash=2166136261U;
    for (unsigned i=0; i<3; i++) hash=hash_word(hash,uid[i]);
    return net_node_id_valid(hash) ? hash : 1U;
}
uint32_t net_session_tag(uint32_t id, uint32_t first_sample, uint32_t second_sample)
{
    uint32_t tag=hash_word(hash_word(hash_word(2166136261U,id),first_sample),second_sample);
    return tag ? tag : 1U;
}
bool net_init(network_t *net, uint32_t id, uint32_t session)
{
    if (!net || !net_node_id_valid(id) || !session) return false;
    memset(net,0,sizeof(*net));
    net->node_id=id; net->destination=NET_BROADCAST; net->session=session; net->next_sequence=1;
    return true;
}
bool net_set_id(network_t *net, uint32_t id)
{
    if (!net_node_id_valid(id)) return false;
    if (id != net->node_id) {
        net->node_id=id;
        memset(net->duplicates,0,sizeof(net->duplicates));
        net->next_duplicate=0;
    }
    return true;
}
bool net_set_destination(network_t *net, uint32_t destination)
{
    if (!destination) return false;
    net->destination=destination;
    return true;
}
bool net_encode(network_t *net, const uint8_t *payload, uint8_t length,
                uint8_t wire[255], uint8_t *wire_length)
{
    if (!net || !payload || !length || length > NET_MAX_PAYLOAD || !wire || !wire_length) return false;
    if (!net_node_id_valid(net->node_id) || !net->destination || !net->session) return false;
    wire[0]=0x50; wire[1]=0x4e; wire[2]=NET_VERSION; wire[3]=1; wire[4]=length;
    write32(wire+5,net->node_id); write32(wire+9,net->destination);
    write32(wire+13,net->session); write32(wire+17,net->next_sequence);
    memcpy(wire+NET_HEADER_BYTES,payload,length);
    *wire_length=(uint8_t)(NET_HEADER_BYTES+length);
    net->stats.last_tx=(net_header_t){net->node_id,net->destination,net->session,net->next_sequence};
    net->stats.last_tx_valid=true;
    net->next_sequence++; /* uint32 wrap is defined; zero is a valid sequence. */
    net->stats.tx_prepared++; net->stats.tx_bytes+=length;
    return true;
}
bool net_decode(const uint8_t *wire, uint8_t length, net_packet_t *packet)
{
    if (!wire || !packet || length < NET_HEADER_BYTES+1U || wire[0] != 0x50 || wire[1] != 0x4e ||
        wire[2] != NET_VERSION || wire[3] != 1 || !wire[4] || wire[4] > NET_MAX_PAYLOAD ||
        (unsigned)wire[4]+NET_HEADER_BYTES != length) return false;
    net_header_t h={read32(wire+5),read32(wire+9),read32(wire+13),read32(wire+17)};
    if (!net_node_id_valid(h.source) || !h.destination || !h.session) return false;
    *packet=(net_packet_t){h,wire+NET_HEADER_BYTES,wire[4]};
    return true;
}
net_rx_result_t net_receive(network_t *net, const uint8_t *wire, uint8_t length,
                            uint32_t now, net_packet_t *packet)
{
    net_packet_t candidate;
    if (!net_decode(wire,length,&candidate)) { net->stats.rx_invalid++; return NET_INVALID; }
    const net_header_t h=candidate.header;
    net->stats.last_seen=h; net->stats.last_seen_valid=true;
    net_rx_result_t result=NET_ACCEPTED;
    if (h.source == net->node_id) { net->stats.rx_self++; result=NET_SELF; }
    else if (h.destination != net->node_id && h.destination != NET_BROADCAST) {
        net->stats.rx_foreign++; result=NET_FOREIGN;
    } else {
        for (unsigned i=0; i<NET_DUP_ENTRIES; i++) {
            const net_duplicate_t *entry=&net->duplicates[i];
            if (entry->session && entry->source == h.source && entry->session == h.session &&
                entry->sequence == h.sequence && (uint32_t)(now-entry->received_at) < NET_DUP_WINDOW_MS) {
                net->stats.rx_duplicate++; result=NET_DUPLICATE; break;
            }
        }
    }
    net->stats.last_result=result;
    if (result != NET_ACCEPTED) return result;
    net->duplicates[net->next_duplicate]=(net_duplicate_t){h.source,h.session,h.sequence,now};
    net->next_duplicate=(uint8_t)((net->next_duplicate+1U)%NET_DUP_ENTRIES);
    net->stats.rx_accepted++; net->stats.rx_bytes+=candidate.length;
    if (h.destination == NET_BROADCAST) net->stats.rx_broadcast++;
    if (packet) *packet=candidate;
    return NET_ACCEPTED;
}
