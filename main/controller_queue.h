#pragma once
#include <stdint.h>

void queue_init();
uint8_t is_queue_empty();
void enqueue_set_brightness(const char* deviceId, int brightness);
void enqueue_set_color(
    const char* deviceId,
    uint16_t x,
    uint16_t y,
    uint8_t transition
);