#pragma once

#include "pico/types.h"
#include "hardware/pio.h"

#define LED_NONE		0
#define LED_RED			1
#define LED_GREEN		2
#define	LED_BLUE		3
#define LED_WHITE		4

#define LED_FORMAT_RGB		0
#define LED_FORMAT_GRB		1
#define LED_FORMAT_WRGB 	2

#define MAX_LENGTH	16

inline uint32_t RGB(uint32_t red, uint32_t green, uint32_t blue)
{
	return (blue<<16) | (green<<8) | red;
}

inline uint32_t RGBW(uint32_t red, uint32_t green, uint32_t blue, uint8_t white)
{
	return (white<<24) | (blue<<16) | (green<<8) | red;
}


typedef struct {
	uint32_t pin;
	uint32_t length;
	PIO pio;
	uint32_t sm;
	uint8_t bytes[4];
	uint32_t data[MAX_LENGTH];
} ws2812_t;

void ws2812_init1(ws2812_t *m, uint32_t pin, uint32_t length, PIO pio, uint32_t sm);
void ws2812_init2(ws2812_t *m, uint32_t pin, uint32_t length, PIO pio, uint32_t sm, uint32_t format);
void ws2812_init3(ws2812_t *m, uint32_t pin, uint32_t length, PIO pio, uint32_t sm, uint32_t b1, uint32_t b2, uint32_t b3);
void ws2812_init4(ws2812_t *m, uint32_t pin, uint32_t length, PIO pio, uint32_t sm, uint32_t b1, uint32_t b2, uint32_t b3, uint32_t b4);

void ws2812_set_pixel(ws2812_t *m,uint32_t index, uint32_t color);
void ws2812_set_pixel_rgb(ws2812_t *m, uint32_t index, uint32_t red, uint32_t green, uint32_t blue);
void ws2812_set_pixel_rgbw(ws2812_t *m, uint32_t index, uint32_t red, uint32_t green, uint32_t blue, uint32_t white);
void ws2812_fill1(ws2812_t *m, uint32_t color);
void ws2812_fill2(ws2812_t *m, uint32_t color, uint32_t first);
void ws2812_fill3(ws2812_t *m, uint32_t color, uint32_t first, uint32_t count);
void ws2812_show(ws2812_t *m);

