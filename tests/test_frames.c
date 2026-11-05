#include "serial_frames.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static serial_frames_t frames;
static uint8_t output[255], length;

static void feed(unsigned size, uint8_t value)
{
    for (unsigned i = 0; i < size; i++) serial_frames_feed(&frames, (uint8_t)(value + i));
}

static void expect(unsigned size, uint8_t value)
{
    assert(serial_frames_take(&frames, output, &length));
    assert(length == size);
    for (unsigned i = 0; i < size; i++) assert(output[i] == (uint8_t)(value + i));
}

int main(void)
{
    serial_frames_init(&frames);
    serial_frames_flush(&frames);
    assert(!serial_frames_take(&frames, output, &length));
    feed(255, 0);
    expect(255, 0); /* Contains NUL and high-bit bytes. */
    feed(256, 73);
    expect(255, 73);
    assert(!serial_frames_take(&frames, output, &length));
    serial_frames_flush(&frames);
    expect(1, (uint8_t)(73 + 255));
    for (unsigned i = 0; i < 5000; i++) {
        unsigned size = i % 255U + 1U;
        feed(size, (uint8_t)i);
        serial_frames_flush(&frames);
        expect(size, (uint8_t)i);
    }
    for (unsigned i = 0; i < 7; i++) feed(255, (uint8_t)i);
    feed(255, 99);
    assert(frames.dropped == 1 && frames.used == 7U * 256U);
    for (unsigned i = 0; i < 7; i++) expect(255, (uint8_t)i);
    assert(!serial_frames_take(&frames, output, &length));
    feed(5, 1);
    serial_frames_discard(&frames);
    serial_frames_flush(&frames);
    assert(frames.dropped == 2 && !frames.used);
    feed(3, 42);
    serial_frames_flush(&frames);
    expect(3, 42);
    puts("PASS: binary bytes, 255/256 boundary, idle flush, wraparound, overflow and UART-error discard");
    return 0;
}
