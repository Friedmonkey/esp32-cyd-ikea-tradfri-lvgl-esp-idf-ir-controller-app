#include "controller_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "fried_tradfri_client.h"

typedef void (*QueueFunc)(void*);

typedef struct {
    QueueFunc func;
    void* context;
} QueueItem;

static QueueHandle_t operationQueue;

uint8_t is_queue_empty()
{
	return (uxQueueMessagesWaiting(operationQueue) < 1);
}

static void controller_task(void* arg)
{
	tradfri_init("192.168.2.9", "tradfri_12345", "IaY5AQRXw1awfqEt");
	printf("Tradfri client init complete!\n");

    QueueItem item;
    while(1)
    {
        if(xQueueReceive(operationQueue, &item, portMAX_DELAY))
        {
            if(item.func)
            {
                item.func(item.context);
            }
        }
    }
}

void queue_init()
{
    operationQueue = xQueueCreate(10, sizeof(QueueItem));
    xTaskCreate(controller_task, "controller_task", 8*1024, NULL, 5, NULL);
}

typedef struct {
    char deviceId[16];
    bool on;
} PowerCtx;
static void set_power_func(void* ctx)
{
    PowerCtx* p = (PowerCtx*)ctx;
    SetOnTradfriLamp(p->deviceId, p->on);
    free(p);
}
void enqueue_set_power(const char* deviceId, bool on)
{
    PowerCtx* ctx = malloc(sizeof(PowerCtx));
    strcpy(ctx->deviceId, deviceId);
    ctx->on = on;

    QueueItem item = { .func = set_power_func, .context = ctx };
    xQueueSend(operationQueue, &item, portMAX_DELAY);
}


typedef struct {
    char deviceId[16];
    uint8_t brightness;
    uint8_t transition;
} BrightnessCtx;
static void set_brightness_func(void* ctx)
{
    BrightnessCtx* b = (BrightnessCtx*)ctx;
    SetBrightnessTradfriLampTransition(b->deviceId, b->brightness, b->transition);
    free(b);
}
void enqueue_set_brightness(const char* deviceId, uint8_t brightness, uint8_t transition)
{
    BrightnessCtx* ctx = malloc(sizeof(BrightnessCtx));
    strcpy(ctx->deviceId, deviceId);
    ctx->brightness = brightness;
    ctx->transition = transition;

    QueueItem item = { .func = set_brightness_func, .context = ctx };
    xQueueSend(operationQueue, &item, portMAX_DELAY);
}



typedef struct {
    char deviceId[16];
    uint16_t x;
    uint16_t y;
    uint8_t transition;
} ColorCtx;
static void set_color_func(void* ctx)
{
    ColorCtx* c = (ColorCtx*)ctx;

    printf(
        "Setting XY color of %s to (%u, %u) with transition %u\n",
        c->deviceId, c->x, c->y, c->transition
    );

    // Example real call:
    SetColorTradfriLampTransition(c->deviceId, c->x, c->y, c->transition);

    free(c);
}
void enqueue_set_color(
    const char* deviceId,
    uint16_t x,
    uint16_t y,
    uint8_t transition
){
    ColorCtx* ctx = malloc(sizeof(ColorCtx));
    strcpy(ctx->deviceId, deviceId);
    ctx->x = x;
    ctx->y = y;
    ctx->transition = transition;

    QueueItem item = { .func = set_color_func, .context = ctx };
    xQueueSend(operationQueue, &item, portMAX_DELAY);
}



typedef struct {
    char deviceId[16];
    brightness_callback_t cb;
} GetBrightnessCtx;
static void get_brightness_func(void* ctx)
{
    GetBrightnessCtx* g = (GetBrightnessCtx*)ctx;

    uint8_t bri = GetBrightnessTradfriLamp(g->deviceId);

    if (g->cb)
        g->cb(g->deviceId, bri);

    free(g);
}
void enqueue_get_brightness(const char* deviceId, brightness_callback_t cb)
{
    GetBrightnessCtx* ctx = malloc(sizeof(GetBrightnessCtx));
    strcpy(ctx->deviceId, deviceId);
    ctx->cb = cb;

    QueueItem item = { .func = get_brightness_func, .context = ctx };
    xQueueSend(operationQueue, &item, portMAX_DELAY);
}
