#include "node_console.h"
#include <stdio.h>
#include <string.h>

static bool hex32(const char *s, uint32_t *result)
{
    if (strlen(s) != 8) return false;
    uint32_t value=0;
    for (unsigned i=0; i<8; i++) {
        unsigned digit;
        if (s[i] >= '0' && s[i] <= '9') digit=(unsigned)(s[i]-'0');
        else if (s[i] >= 'a' && s[i] <= 'f') digit=(unsigned)(s[i]-'a')+10U;
        else if (s[i] >= 'A' && s[i] <= 'F') digit=(unsigned)(s[i]-'A')+10U;
        else return false;
        value=(value << 4)|digit;
    }
    *result=value; return true;
}
size_t node_console_info(const node_console_t *console, const network_t *net,
                          uint32_t request, const char *status, char *reply, size_t capacity)
{
    int size=snprintf(reply,capacity,
        "[node] req=%08lX status=%s id=%08lX dst=%08lX boot=%08lX next=%08lX mtu=%u "
        "rx=%lu foreign=%lu duplicate=%lu last_src=%08lX last_dst=%08lX last_seq=%08lX last_kind=%u raw=%u version=1\r\n",
        (unsigned long)request,status,(unsigned long)net->node_id,(unsigned long)net->destination,
        (unsigned long)net->session,(unsigned long)net->next_sequence,(unsigned)NET_MAX_PAYLOAD,
        (unsigned long)net->stats.rx_accepted,(unsigned long)net->stats.rx_foreign,
        (unsigned long)net->stats.rx_duplicate,(unsigned long)net->stats.last_seen.source,
        (unsigned long)net->stats.last_seen.destination,(unsigned long)net->stats.last_seen.sequence,
        (unsigned)net->stats.last_result,(unsigned)console->raw_next);
    return size > 0 && (size_t)size < capacity ? (size_t)size : 0U;
}
bool node_console_process(node_console_t *console, network_t *net,
                          const uint8_t *data, uint8_t length, char *reply, size_t capacity)
{
    if (capacity) reply[0]='\0';
    if (console->raw_next) { console->raw_next=false; return false; }
    static const char prefix[]="!PJ26 ";
    if (length < sizeof(prefix)-1U || memcmp(data,prefix,sizeof(prefix)-1U)) return false;
    char text[NET_MAX_PAYLOAD+1U];
    const char *status="ERROR";
    uint32_t request=0, value=0;
    if (length > NET_MAX_PAYLOAD || memchr(data,0,length)) goto respond;
    memcpy(text,data,length); text[length]='\0';
    /* Space, tab, CR and LF are separators; extra tokens are rejected. */
    char *tokens[5]={0}; unsigned count=0; char *p=text;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (!*p) break;
        if (count == 5) goto respond;
        tokens[count++]=p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') p++;
        if (*p) *p++='\0';
    }
    if (count < 2 || strcmp(tokens[0],"!PJ26")) goto respond;
    bool simple=!strcmp(tokens[1],"INFO") || !strcmp(tokens[1],"RAW");
    unsigned args=simple ? 2U : 3U;
    if (count != args && count != args+1U) goto respond;
    if (count == args+1U && !hex32(tokens[args],&request)) goto respond;
    if (simple) {
        if (!strcmp(tokens[1],"RAW")) console->raw_next=true;
        status="OK";
    } else if (hex32(tokens[2],&value)) {
        if ((!strcmp(tokens[1],"TO") && net_set_destination(net,value)) ||
            (!strcmp(tokens[1],"ID") && net_set_id(net,value))) status="OK";
    }
respond:
    (void)node_console_info(console,net,request,status,reply,capacity);
    return true;
}
size_t node_console_diagnostics(const network_t *net, char *reply, size_t capacity)
{
    const net_stats_t *s=&net->stats;
    int size=snprintf(reply,capacity,
        "[net] id=%08lX dst=%08lX boot=%08lX next=%08lX mtu=%u\r\n"
        "[net] prepared=%lu tx_bytes=%lu accepted=%lu rx_bytes=%lu broadcast=%lu foreign=%lu duplicate=%lu invalid=%lu self=%lu\r\n"
        "[net] last_tx_valid=%u src=%08lX dst=%08lX seq=%08lX boot=%08lX\r\n"
        "[net] last_seen_valid=%u kind=%u src=%08lX dst=%08lX seq=%08lX boot=%08lX\r\n",
        (unsigned long)net->node_id,(unsigned long)net->destination,(unsigned long)net->session,
        (unsigned long)net->next_sequence,(unsigned)NET_MAX_PAYLOAD,
        (unsigned long)s->tx_prepared,(unsigned long)s->tx_bytes,(unsigned long)s->rx_accepted,
        (unsigned long)s->rx_bytes,(unsigned long)s->rx_broadcast,(unsigned long)s->rx_foreign,
        (unsigned long)s->rx_duplicate,(unsigned long)s->rx_invalid,(unsigned long)s->rx_self,
        (unsigned)s->last_tx_valid,(unsigned long)s->last_tx.source,(unsigned long)s->last_tx.destination,
        (unsigned long)s->last_tx.sequence,(unsigned long)s->last_tx.session,
        (unsigned)s->last_seen_valid,(unsigned)s->last_result,(unsigned long)s->last_seen.source,
        (unsigned long)s->last_seen.destination,(unsigned long)s->last_seen.sequence,(unsigned long)s->last_seen.session);
    return size > 0 && (size_t)size < capacity ? (size_t)size : 0U;
}
