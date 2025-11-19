#include "remote_queue.h"
#include "driver/rmt_rx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#define IR_RESOLUTION_HZ      1000000
#define IR_RX_GPIO_NUM        27

// NEC timing
#define NEC_LEADING_0  9000
#define NEC_LEADING_1  4500
#define NEC_ZERO_0     560
#define NEC_ZERO_1     560
#define NEC_ONE_0      560
#define NEC_ONE_1      1690
#define NEC_REPEAT_0   9000
#define NEC_REPEAT_1   2250

static remote_nec_callback_t g_cb = NULL;
static uint16_t s_address = 0;
static uint16_t s_command = 0;

// **Globals for the task**
static rmt_channel_handle_t s_ch;
static QueueHandle_t s_q;
static rmt_symbol_word_t s_buf[256];
static rmt_receive_config_t s_rx_cfg;

static inline bool nec_in_range(uint32_t dur, uint32_t spec)
{
    float tol = 0.15f;
    return dur >= spec * (1 - tol) && dur <= spec * (1 + tol);
}

static bool nec_logic0(rmt_symbol_word_t *s)
{
    return nec_in_range(s->duration0, NEC_ZERO_0) &&
           nec_in_range(s->duration1, NEC_ZERO_1);
}

static bool nec_logic1(rmt_symbol_word_t *s)
{
    return nec_in_range(s->duration0, NEC_ONE_0) &&
           nec_in_range(s->duration1, NEC_ONE_1);
}

static bool nec_parse_frame(rmt_symbol_word_t *s, size_t n)
{
    if (n < 34) return false;
    rmt_symbol_word_t *cur = s;
    uint16_t addr = 0, cmd = 0;

    if (!nec_in_range(cur->duration0, NEC_LEADING_0) ||
        !nec_in_range(cur->duration1, NEC_LEADING_1))
        return false;
    cur++;

    for (int i = 0; i < 16; i++, cur++) {
        if (cur->duration1 == 0) cur->duration1 = NEC_ZERO_1;
        if (nec_logic1(cur)) addr |= 1 << i;
        else if (!nec_logic0(cur)) return false;
    }

    for (int i = 0; i < 16; i++, cur++) {
        if (cur->duration1 == 0) cur->duration1 = NEC_ZERO_1;
        if (nec_logic1(cur)) cmd |= 1 << i;
        else if (!nec_logic0(cur)) return false;
    }

    s_address = addr;
    s_command = cmd;
    return true;
}

static bool nec_parse_repeat(rmt_symbol_word_t *s)
{
    return nec_in_range(s->duration0, NEC_REPEAT_0) &&
           nec_in_range(s->duration1, NEC_REPEAT_1);
}

static void parse_frame(rmt_symbol_word_t *s, size_t n)
{
    bool repeat = false;

    if (n == 34 && nec_parse_frame(s, n)) {
        repeat = false;
    } else if (n == 2 && nec_parse_repeat(s)) {
        repeat = true;
    } else {
        return;
    }

    if (g_cb) g_cb(s_address, s_command, repeat);
}

static bool rmt_rx_done_cb(rmt_channel_handle_t ch,
                           const rmt_rx_done_event_data_t *edata,
                           void *user_data)
{
    BaseType_t high_wakeup = pdFALSE;
    xQueueSendFromISR((QueueHandle_t)user_data, edata, &high_wakeup);
    return high_wakeup == pdTRUE;
}

static void nec_rx_task(void *param)
{
    rmt_rx_done_event_data_t rx;
    while (1)
    {
        if (xQueueReceive(s_q, &rx, pdMS_TO_TICKS(1000)) == pdPASS) {
            parse_frame(rx.received_symbols, rx.num_symbols);
            ESP_ERROR_CHECK(rmt_receive(s_ch, s_buf, sizeof(s_buf), &s_rx_cfg));
        }
    }
}

void remote_queue_init(remote_nec_callback_t cb)
{
    g_cb = cb;

    rmt_rx_channel_config_t cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = IR_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .gpio_num = IR_RX_GPIO_NUM,
    };

    ESP_ERROR_CHECK(rmt_new_rx_channel(&cfg, &s_ch));

    s_q = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
    assert(s_q);

    rmt_rx_event_callbacks_t cbs = {.on_recv_done = rmt_rx_done_cb};
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(s_ch, &cbs, s_q));

    s_rx_cfg.signal_range_min_ns = 1250;
    s_rx_cfg.signal_range_max_ns = 12000000;
    ESP_ERROR_CHECK(rmt_enable(s_ch));

    ESP_ERROR_CHECK(rmt_receive(s_ch, s_buf, sizeof(s_buf), &s_rx_cfg));

    // Create the FreeRTOS task
    xTaskCreate(nec_rx_task, "nec_rx_task", 2048, NULL, 5, NULL);
}
