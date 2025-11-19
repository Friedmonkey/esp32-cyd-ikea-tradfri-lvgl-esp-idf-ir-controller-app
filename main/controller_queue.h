#pragma once
#include <stdint.h>
#include <stdbool.h>

void queue_init();
uint8_t is_queue_empty();
void enqueue_set_color(
    const char* deviceId,
    uint16_t x,
    uint16_t y,
    uint8_t transition
);

void enqueue_set_power(const char* deviceId, bool on);
void enqueue_set_brightness(const char* deviceId, uint8_t brightness, uint8_t transition);
typedef void (*brightness_callback_t)(const char* deviceId, uint8_t brightness);
void enqueue_get_brightness(const char* deviceId, brightness_callback_t cb);
