#include "network.h"
#include "node_console.h"
#include "serial_frames.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t payload[255], wire[255], size;
static network_t a,b,c;
static net_packet_t packet;

static void setup(void)
{
    assert(net_init(&a,0x11223344U,0x55667788U));
    assert(net_init(&b,0xaabbccddU,0x12345678U));
    assert(net_init(&c,0x01020304U,0x87654321U));
    for (unsigned i=0;i<sizeof payload;i++) payload[i]=(uint8_t)i;
}
static void codec(void)
{
    setup();
    assert(!net_init(&a,0,1) && !net_init(&a,NET_BROADCAST,1) && !net_init(&a,1,0));
    assert(!net_set_id(&a,0) && !net_set_destination(&a,0));
    assert(net_set_destination(&a,b.node_id));
    assert(net_encode(&a,payload,NET_MAX_PAYLOAD,wire,&size) && size==255);
    const uint8_t expected[]={0x50,0x4e,1,1,234,0x44,0x33,0x22,0x11,0xdd,0xcc,0xbb,0xaa,
                              0x88,0x77,0x66,0x55,1,0,0,0};
    assert(!memcmp(wire,expected,sizeof expected)); /* Independently specified wire format. */
    assert(net_decode(wire,size,&packet) && packet.length==234 && !memcmp(packet.data,payload,234));
    assert(packet.header.source==a.node_id && packet.header.destination==b.node_id);
    assert(!net_encode(&a,payload,235,wire,&size) && !net_encode(&a,payload,0,wire,&size));
    assert(!net_encode(&a,NULL,1,wire,&size) && !net_decode(NULL,255,&packet));
    for (unsigned i=0;i<NET_HEADER_BYTES+1U;i++) assert(!net_decode(wire,(uint8_t)i,&packet));
    uint8_t saved=wire[0];wire[0]=0;assert(!net_decode(wire,255,&packet));wire[0]=saved;
    saved=wire[2];wire[2]=2;assert(!net_decode(wire,255,&packet));wire[2]=saved;
    saved=wire[3];wire[3]=2;assert(!net_decode(wire,255,&packet));wire[3]=saved;
    wire[4]=233;assert(!net_decode(wire,255,&packet));wire[4]=234;
    wire[4]=0;assert(!net_decode(wire,255,&packet));wire[4]=234;
    memset(wire+5,0,4);assert(!net_decode(wire,255,&packet));
    memset(wire+5,0xff,4);assert(!net_decode(wire,255,&packet));
    assert(net_encode(&a,payload,1,wire,&size));
    memset(wire+9,0,4);assert(!net_decode(wire,size,&packet));
    assert(net_encode(&a,payload,1,wire,&size));
    memset(wire+13,0,4);assert(!net_decode(wire,size,&packet));
    a.next_sequence=UINT32_MAX;
    assert(net_encode(&a,payload,1,wire,&size));assert(net_decode(wire,size,&packet));
    assert(packet.header.sequence==UINT32_MAX);
    assert(net_encode(&a,payload,1,wire,&size));assert(net_decode(wire,size,&packet));
    assert(packet.header.sequence==0);
}
static void addressing(void)
{
    setup();
    assert(net_encode(&a,payload,3,wire,&size));
    assert(net_receive(&b,wire,size,100,&packet)==NET_ACCEPTED);
    assert(net_receive(&c,wire,size,100,&packet)==NET_ACCEPTED);
    assert(b.stats.rx_broadcast==1 && c.stats.rx_broadcast==1);
    assert(net_receive(&a,wire,size,100,&packet)==NET_SELF && a.stats.rx_self==1);
    assert(net_set_destination(&a,b.node_id));
    assert(net_encode(&a,payload,3,wire,&size));
    assert(net_receive(&b,wire,size,101,&packet)==NET_ACCEPTED);
    assert(net_receive(&c,wire,size,101,&packet)==NET_FOREIGN && c.stats.rx_foreign==1);
    assert(net_receive(&b,wire,size,102,&packet)==NET_DUPLICATE);
    assert(b.stats.rx_accepted==2 && b.stats.rx_duplicate==1 && b.stats.rx_bytes==6);
    assert(b.stats.last_seen.source==a.node_id && b.stats.last_seen.destination==b.node_id);
    assert(b.stats.last_seen.sequence==2 && b.stats.last_result==NET_DUPLICATE);
    /* Changing a local address allows the previously foreign packet: no cache poisoning. */
    assert(net_set_id(&c,b.node_id));
    assert(net_receive(&c,wire,size,103,&packet)==NET_ACCEPTED);
    assert(net_receive(&b,(const uint8_t *)"old raw firmware",16,104,&packet)==NET_INVALID);
    assert(b.stats.rx_invalid==1);
    assert(net_set_destination(&b,a.node_id));
    assert(net_encode(&b,payload,3,wire,&size));
    assert(net_receive(&a,wire,size,105,&packet)==NET_ACCEPTED); /* B -> A */
}
static void dedup(void)
{
    setup();
    assert(net_encode(&a,payload,3,wire,&size));
    assert(net_receive(&b,wire,size,0xfffffff0U,&packet)==NET_ACCEPTED);
    assert(net_receive(&b,wire,size,0x20U,&packet)==NET_DUPLICATE); /* tick wrap */
    assert(net_receive(&b,wire,size,0xfffffff0U+NET_DUP_WINDOW_MS,&packet)==NET_ACCEPTED);
    a.session++;
    a.next_sequence=1;
    assert(net_encode(&a,payload,3,wire,&size));
    assert(net_receive(&b,wire,size,60001,&packet)==NET_ACCEPTED); /* new boot/session */
    uint8_t first[255], first_size;
    assert(net_encode(&a,payload,3,first,&first_size));
    assert(net_receive(&b,first,first_size,60002,&packet)==NET_ACCEPTED);
    for (unsigned i=0;i<NET_DUP_ENTRIES;i++) {
        assert(net_encode(&a,payload,3,wire,&size));
        assert(net_receive(&b,wire,size,60003+i,&packet)==NET_ACCEPTED);
    }
    assert(net_receive(&b,first,first_size,60100,&packet)==NET_ACCEPTED); /* bounded cache, documented eviction */
    uint32_t uid[3]={0x48ff6a06U,0x50696950U,0x29482367U};
    uint32_t id=net_id_from_uid(uid);
    assert(net_node_id_valid(id) && id==net_id_from_uid(uid));
    uid[0]++;assert(id!=net_id_from_uid(uid));
    assert(net_session_tag(id,100,200)!=net_session_tag(id,100,201));
}
static void command(node_console_t *console, network_t *net, const char *text, char *out)
{
    assert(node_console_process(console,net,(const uint8_t *)text,(uint8_t)strlen(text),out,NODE_REPLY_BYTES));
}
static void console_commands(void)
{
    setup();node_console_t console={0};char out[NODE_REPLY_BYTES];
    command(&console,&a,"!PJ26 INFO 1234ABCD\r\n",out);
    assert(strstr(out,"req=1234ABCD status=OK id=11223344 dst=FFFFFFFF"));
    assert(strstr(out,"mtu=234") && strstr(out,"raw=0"));
    command(&console,&a,"!PJ26 TO AABBCCDD",out);
    assert(a.destination==b.node_id && strstr(out,"status=OK"));
    command(&console,&a,"!PJ26 TO FFFFFFFF 01020304",out);
    assert(a.destination==NET_BROADCAST && strstr(out,"req=01020304"));
    command(&console,&a,"!PJ26 TO 00000000",out);assert(strstr(out,"status=ERROR"));
    command(&console,&a,"!PJ26 ID 00000001",out);assert(a.node_id==1);
    command(&console,&a,"!PJ26 ID FFFFFFFF",out);assert(a.node_id==1 && strstr(out,"status=ERROR"));
    command(&console,&a,"!PJ26 ID 00000000",out);assert(a.node_id==1 && strstr(out,"status=ERROR"));
    command(&console,&a,"!PJ26 TO FFFFFFGG",out);assert(strstr(out,"status=ERROR"));
    command(&console,&a,"!PJ26 TO FFFFFFFF EXTRA TOKENS",out);assert(strstr(out,"status=ERROR"));
    command(&console,&a,"!PJ26 RESET",out);assert(strstr(out,"status=ERROR"));
    const uint8_t binary[]={0,0xff,'!','P','J','2','6',' '};
    assert(!node_console_process(&console,&a,binary,sizeof binary,out,sizeof out));
    command(&console,&a,"!PJ26 RAW",out);assert(console.raw_next && strstr(out,"raw=1"));
    const char *literal="!PJ26 TO 00000002";
    assert(!node_console_process(&console,&a,(const uint8_t *)literal,(uint8_t)strlen(literal),out,sizeof out));
    assert(!console.raw_next && a.destination==NET_BROADCAST);
    /* Remote payload resembling a local command is just data after network decoding. */
    assert(net_encode(&b,(const uint8_t *)literal,(uint8_t)strlen(literal),wire,&size));
    assert(net_receive(&a,wire,size,10,&packet)==NET_ACCEPTED);
    assert(packet.length==strlen(literal) && !memcmp(packet.data,literal,packet.length));
    assert(a.destination==NET_BROADCAST);
    network_t max=a;
    memset(&max.stats,0xff,sizeof max.stats);
    max.stats.last_tx_valid=true;max.stats.last_seen_valid=true;max.stats.last_result=NET_DUPLICATE;
    assert(node_console_info(&console,&max,UINT32_MAX,"ERROR",out,sizeof out));
    assert(node_console_diagnostics(&max,out,sizeof out) && strlen(out)<sizeof out);
    assert(!node_console_diagnostics(&max,out,10));
    assert(!node_console_info(&console,&max,0,"OK",out,10));
}
static void framing(void)
{
    serial_frames_t frames;uint8_t data[255], length;
    serial_frames_init(&frames);
    assert(!serial_frames_set_limit(&frames,0));
    assert(serial_frames_set_limit(&frames,NET_MAX_PAYLOAD));
    for (unsigned i=0;i<255;i++) serial_frames_feed(&frames,(uint8_t)i);
    serial_frames_flush(&frames);
    assert(serial_frames_take(&frames,data,&length) && length==234);
    for (unsigned i=0;i<234;i++) assert(data[i]==(uint8_t)i);
    assert(!serial_frames_set_limit(&frames,100));
    assert(serial_frames_take(&frames,data,&length) && length==21);
    for (unsigned i=0;i<21;i++) assert(data[i]==(uint8_t)(234+i));
    assert(!serial_frames_take(&frames,data,&length));
    assert(serial_frames_set_limit(&frames,255));
}
int main(void)
{
    codec();addressing();dedup();console_commands();framing();
    puts("PASS: network wire bytes/MTU, unicast/broadcast/filtering, dedup/session/TTL/wrap, local commands/RAW, bounded diagnostics and UART chunking");
    return 0;
}
