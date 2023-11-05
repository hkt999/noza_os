#include <stdio.h>
#include <stdlib.h>
#include "nozaos.h"
#include "ws2812.h"

#define LED_PIN 16
#define LED_LENGTH 1

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} cRGB;

#define MAX_HUE     360
#define SATURATION   1.0f
#define BRIGHTNESS   1.0f

cRGB HSVtoRGB(float h, float s, float v) {
    float r, g, b;
    int i = (int)(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);

    switch (i % 6) {
        case 0: r = v, g = t, b = p; break;
        case 1: r = q, g = v, b = p; break;
        case 2: r = p, g = v, b = t; break;
        case 3: r = p, g = q, b = v; break;
        case 4: r = t, g = p, b = v; break;
        case 5: r = v, g = p, b = q; break;
    }

    cRGB rgb = {
        .red = (uint8_t)(r * 255),
        .green = (uint8_t)(g * 255),
        .blue = (uint8_t)(b * 255)
    };

    return rgb;
}

uint32_t cRGB_to_color(cRGB rgb) {
    return (rgb.red << 16) | (rgb.green << 8) | rgb.blue;
}

void gradient_effect(ws2812_t *led_strip) {
    float hue = 0.0f;
    int counter = 100; // 10s

    printf("hue demo started (10s)\n");
    while (counter-->0) {
        cRGB currentColor = HSVtoRGB(hue, SATURATION, BRIGHTNESS);
        uint32_t color = cRGB_to_color(currentColor);

        for (uint8_t ledIndex = 0; ledIndex < LED_LENGTH; ledIndex++) {
            ws2812_set_pixel(led_strip, ledIndex, color);
            ws2812_show(led_strip);
            noza_thread_sleep_ms(100, NULL);
        }

        hue += 10.0f / MAX_HUE;
        if (hue >= 1.0f) {
            hue = 0.0f;
        }
    }
    uint32_t color = cRGB_to_color((cRGB){0, 0, 0});
    for (uint8_t ledIndex = 0; ledIndex < LED_LENGTH; ledIndex++) {
        ws2812_set_pixel(led_strip, ledIndex, color);
        ws2812_show(led_strip);
        noza_thread_sleep_ms(100, NULL);
    }
    printf("demo finished\n");
}

static int test_ws2812(int argc, char **argv)
{
	ws2812_t led_strip;
	ws2812_init2(&led_strip, LED_PIN, LED_LENGTH, pio0, 0, LED_FORMAT_GRB);

    // RED
    printf("red\n");
    ws2812_fill1(&led_strip, RGB(255, 0, 0));
    ws2812_show(&led_strip);
	noza_thread_sleep_ms(1000, NULL);

    // GREEN
    printf("green\n");
    ws2812_fill1(&led_strip, RGB(0, 255, 0));
    ws2812_show(&led_strip);
    noza_thread_sleep_ms(1000, NULL);

    // BLUE
    printf("blue\n");
    ws2812_fill1(&led_strip, RGB(0, 0, 255));
    ws2812_show(&led_strip);
	noza_thread_sleep_ms(1000, NULL);

    gradient_effect(&led_strip);
}

#include "user/console/noza_console.h"
void __attribute__((constructor(1000))) register_led_example()
{
    console_add_command("led", test_ws2812, "ws2812 led example program", 1024);
}