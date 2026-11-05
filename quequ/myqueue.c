#include "myqueue.h"
volatile Queue UartRxQueue = {0};
pQueue pUart1RxQueue = NULL;
static void put_byte(pQueue q, uint8_t value) {
    q->data[q->tail] = value;
    q->tail = (q->tail + 1U) % QUEUE_MAX_LENGTH;
    q->count++;
}
static uint8_t take_byte(pQueue q) {
    uint8_t value = q->data[q->front];
    q->front = (q->front + 1U) % QUEUE_MAX_LENGTH;
    q->count--;
    return value;
}
int queueIsEmpty(pQueue q) { return q == NULL || q->count == 0; }
void *queueEnqueue(pQueue q, void *data, unsigned short bytes) {
    if (!q || !data || !bytes || bytes > QUEUE_NODE_SIZE ||
        q->count + bytes + 2U > QUEUE_MAX_LENGTH) return NULL;
    put_byte(q, (uint8_t)bytes);
    put_byte(q, (uint8_t)(bytes >> 8));
    for (unsigned short i = 0; i < bytes; i++) put_byte(q, ((uint8_t *)data)[i]);
    return q;
}
unsigned short queueDequeue(pQueue q, void *data) {
    if (!q || !data || q->count < 2) return 0;
    unsigned short bytes = take_byte(q);
    bytes |= (unsigned short)take_byte(q) << 8;
    if (!bytes || bytes > QUEUE_NODE_SIZE || bytes > q->count) {
        EmptyTheQueue(q); return 0;
    }
    for (unsigned short i = 0; i < bytes; i++) ((uint8_t *)data)[i] = take_byte(q);
    return bytes;
}
void EmptyTheQueue(pQueue q) {
    if (q) q->front = q->tail = q->count = 0;
}
