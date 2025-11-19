#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "fried_esp_cyd_screen.h"
//export from squareline
#include "ui.h"


#include "esp_log.h"
#include "controller_queue.h"
#include "remote_queue.h"

//#include "fdata.h"

#include <math.h>
#include <stdint.h>
#include <string.h>


static const char* TRADFRI_TAG = "FRIED_DISPLAY_TRADFRI_CONTROLLER";
const char* c_deviceId = "65574";
const uint8_t brightness_factor = 15;
static uint8_t transitionTime = 5;
static uint8_t last_known_brightness = 50;


static const char* commandNames[] = {
    "Bright+",      "Bright-",    "Power Off",  "Power On",
    "Red",          "Green",      "Blue",       "White",
    "Orange",       "Lime",       "Blue2",      "Flash",
    "Light orange", "Skyblue",    "Dark purple","Strobe",
    "Yellow orange","Cyan light", "Purple",     "Fade",
    "Yellow",       "Cyan dark",  "Pink",       "Smooth"
};

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} simple_rgb_t;

static const simple_rgb_t c_Red          = {255,   0,   0};
static const simple_rgb_t c_Green        = {  0, 255,   0};
static const simple_rgb_t c_Blue         = {  0,   0, 255};
static const simple_rgb_t c_White        = {255, 255, 255};

static const simple_rgb_t c_Orange       = {255, 165,   0};
static const simple_rgb_t c_Lime         = { 50, 255,  50};
static const simple_rgb_t c_Blue2        = {  0,   0, 200};

static const simple_rgb_t c_LightOrange  = {255, 200, 150};
static const simple_rgb_t c_SkyBlue      = {135, 206, 235};
static const simple_rgb_t c_DarkPurple   = { 75,   0, 130};

static const simple_rgb_t c_YellowOrange = {255, 200,   0};
static const simple_rgb_t c_CyanLight    = {  0, 255, 255};
static const simple_rgb_t c_Purple       = {128,   0, 128};

static const simple_rgb_t c_Yellow       = {255, 255,   0};
static const simple_rgb_t c_CyanDark     = {  0, 150, 150};
static const simple_rgb_t c_Pink         = {255, 105, 180};


static const simple_rgb_t* colorCommands[] = {
    NULL,              // Bright+
    NULL,              // Bright-
    NULL,              // Power Off
    NULL,              // Power On
    &c_Red,
    &c_Green,
    &c_Blue,
    &c_White,
    &c_Orange,
    &c_Lime,
    &c_Blue2,
    NULL,              // Flash
    &c_LightOrange,
    &c_SkyBlue,
    &c_DarkPurple,
    NULL,              // Strobe
    &c_YellowOrange,
    &c_CyanLight,
    &c_Purple,
    NULL,              // Fade
    &c_Yellow,
    &c_CyanDark,
    &c_Pink,
    NULL               // Smooth
};


// approximate gamma correction (0..255 in, 0..255 out)
static inline float srgb_to_linear(uint8_t c)
{
    float v = (float)c / 255.0f;
    if (v <= 0.04045f) return v / 12.92f;
    return powf((v + 0.055f) / 1.055f, 2.4f);
}

// Convert RGB (0..255) to XY (0..65535) using standard sRGB -> XYZ matrix
void rgb_to_xy_uint16(uint16_t r_in, uint16_t g_in, uint16_t b_in, uint16_t* x, uint16_t* y)
{
    // clamp inputs to 0..255 just in case
    uint8_t r8 = (r_in > 255) ? 255 : (uint8_t)r_in;
    uint8_t g8 = (g_in > 255) ? 255 : (uint8_t)g_in;
    uint8_t b8 = (b_in > 255) ? 255 : (uint8_t)b_in;

    // linearize (gamma expand)
    float r = srgb_to_linear(r8);
    float g = srgb_to_linear(g8);
    float b = srgb_to_linear(b8);

    // sRGB D65 matrix to XYZ (wide gamut)
    // values from standard sRGB to XYZ conversion
    float X = r * 0.4124564f + g * 0.3575761f + b * 0.1804375f;
    float Y = r * 0.2126729f + g * 0.7151522f + b * 0.0721750f;
    float Z = r * 0.0193339f + g * 0.1191920f + b * 0.9503041f;

    float sum = X + Y + Z;
    if (sum <= 1e-6f) {
        // black / near-zero -> use middle gray-ish xy (prevent div by zero)
        *x = 32768;
        *y = 32768;
        return;
    }

    float xf = X / sum;
    float yf = Y / sum;

    // scale to 0..65535 and clamp
    int xi = (int)roundf(xf * 65535.0f);
    int yi = (int)roundf(yf * 65535.0f);

    if (xi < 0) { xi = 0; }
    else if (xi > 65535) { xi = 65535; }

    if (yi < 0) { yi = 0; }
    else if (yi > 65535) { yi = 65535; }


    *x = (uint16_t)xi;
    *y = (uint16_t)yi;
}

static void SetTradfriColor(uint8_t r, uint8_t g, uint8_t b, uint8_t t)
{
	uint16_t x,y;
	rgb_to_xy_uint16(r, g, b, &x, &y);
	enqueue_set_color(c_deviceId, x, y, t);
}
static void SetColor(uint8_t r, uint8_t g, uint8_t b, uint8_t t)
{
	lv_obj_t* screen = lv_scr_act();
	
	lv_color_t color = lv_color_make(r, g, b);
	
    lv_obj_set_style_bg_color(screen, color, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
	SetTradfriColor(r, g, b, t);
}
static void SetColorFromColor(lv_color_t col, uint8_t t)
{
	lv_obj_t* screen = lv_scr_act();
	
	uint32_t c32 = lv_color_to32(col);

    // Extract channels (AA RR GG BB)
    uint8_t r = (c32 >> 16) & 0xFF;
    uint8_t g = (c32 >> 8) & 0xFF;
    uint8_t b = (c32 >> 0) & 0xFF;
	
	SetColor(r,g,b, t);
}


void colorWheelFunction(lv_event_t* e)
{
    lv_obj_t* cw = lv_event_get_target(e);

    // RGB565 color from the wheel
    lv_color_t color = lv_colorwheel_get_rgb(cw);

	if (is_queue_empty())	
	{
		SetColorFromColor(color, transitionTime);  //1.5 roughly the duration that it takes for a call
	}
}

void sliderTimeFunction(lv_event_t* e)
{
    lv_obj_t* slider = lv_event_get_target(e);
    transitionTime = lv_slider_get_value(slider);
}

void sliderBrightnessFunction(lv_event_t* e)
{
    lv_obj_t* slider = lv_event_get_target(e);
    int brightness = lv_slider_get_value(slider);

	if (brightness < 1)
		brightness = 1;
	//otherwise the screen turns off and its hard to find the slider again lol
    fs_set_brightness(brightness);
}

void resetBackgroundButtonFunction(lv_event_t* e)
{
	SetColor(255, 255, 255, transitionTime);
}

void changeColorRed(lv_event_t* e)
{
	SetColor(255, 0, 0, transitionTime);
}
void changeColorGreen(lv_event_t* e)
{
	SetColor(0, 255, 0, transitionTime);
}
void changeColorBlue(lv_event_t* e)
{
	SetColor(0, 0, 255, transitionTime);
}

void IncreaseBrightness()
{
    int new_brightness = last_known_brightness + brightness_factor;
    if (new_brightness > 254) new_brightness = 254;
    last_known_brightness = new_brightness;

    enqueue_set_brightness(c_deviceId, last_known_brightness, 5);
}

void DecreaseBrightness()
{
    int new_brightness = last_known_brightness - brightness_factor;
    if (new_brightness < 0) new_brightness = 0;
    last_known_brightness = new_brightness;

    enqueue_set_brightness(c_deviceId, last_known_brightness, 5);
}



static void my_nec_callback(uint16_t address, uint16_t command, bool repeat)
{
    uint8_t cmd8 = command & 0xFF;

    if (!repeat && cmd8 <= 0x17) {
        const simple_rgb_t* col = colorCommands[cmd8];
		const char *commandName = commandNames[cmd8];
        if (col) {
            ESP_LOGI("NEC", "Setting color to: %s", commandName);
            SetColor(col->r, col->g, col->b, 10);
        } else {
            ESP_LOGI("NEC", "Non-color command: %s", commandName);
			if (strcmp(commandName, "Bright+") == 0)
			{
				IncreaseBrightness();
			}
			else if (strcmp(commandName, "Bright-") == 0)
			{
				DecreaseBrightness();
			}
			else if (strcmp(commandName, "Power On") == 0)
			{
				enqueue_set_power(c_deviceId, true);
			}
			else if (strcmp(commandName, "Power Off") == 0)
			{
				enqueue_set_power(c_deviceId, false);
			}

        }
    }
}

void on_brightness_read(const char* deviceId, uint8_t brightness)
{
    printf("Brightness of %s is %u\n", deviceId, brightness);
	last_known_brightness = brightness;
	lv_scr_load(ui_app_screen);
}


void app_main(void)
{
	queue_init();
    fs_init();
	fs_set_brightness(25);
	
	//squareline studio ui init func
	ui_init();
	remote_queue_init(my_nec_callback);

	enqueue_get_brightness(c_deviceId, on_brightness_read);
}
