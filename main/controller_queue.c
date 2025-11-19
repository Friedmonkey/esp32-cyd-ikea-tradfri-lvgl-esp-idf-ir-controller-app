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

// ------------------------
// Example enqueue function
// ------------------------
typedef struct { char deviceId[16]; int brightness; } BrightnessCtx;

static void set_brightness_func(void* ctx)
{
    BrightnessCtx* b = (BrightnessCtx*)ctx;
    printf("Setting brightness of %s to %d\n", b->deviceId, b->brightness);
    free(b);
}

void enqueue_set_brightness(const char* deviceId, int brightness)
{
    BrightnessCtx* ctx = malloc(sizeof(BrightnessCtx));
    strcpy(ctx->deviceId, deviceId);
    ctx->brightness = brightness;

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
