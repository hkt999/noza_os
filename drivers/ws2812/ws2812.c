#include "ws2812.h"
#include "ws2812.pio.h"
#include <stdio.h>

static void ws2812_initialize(ws2812_t *m, uint32_t pin, uint32_t length, PIO pio, uint32_t sm, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4)
{
	m->pin = pin;
	m->length = length;
	m->pio = pio;
	m->sm = sm;
	if (length > MAX_LENGTH) {
		length = MAX_LENGTH;
		printf("ws2812: length is too long, set to %d\n", length);
	}
	m->bytes[0] = b1;
	m->bytes[1] = b2;
	m->bytes[2] = b3;
	m->bytes[3] = b4;
	uint32_t offset = pio_add_program(pio, &ws2812_program);
	uint32_t bits = (b1 == LED_NONE ? 24 : 32);
	ws2812_program_init(pio, sm, offset, pin, 800000, bits);
}

void ws2812_init1(ws2812_t *m, uint32_t pin, uint32_t length, PIO pio, uint32_t sm)
{
    ws2812_initialize(m, pin, length, pio, sm, LED_NONE, LED_GREEN, LED_RED, LED_BLUE);
}

void ws2812_init2(ws2812_t *m, uint32_t pin, uint32_t length, PIO pio, uint32_t sm, uint32_t format)
{
	switch (format) {
        case LED_FORMAT_RGB:
            ws2812_initialize(m, pin, length, pio, sm, LED_NONE, LED_RED, LED_GREEN, LED_BLUE);
            break;
        case LED_FORMAT_GRB:
            ws2812_initialize(m, pin, length, pio, sm, LED_NONE, LED_GREEN, LED_RED, LED_BLUE);
            break;
        case LED_FORMAT_WRGB:
            ws2812_initialize(m, pin, length, pio, sm, LED_WHITE, LED_RED, LED_GREEN, LED_BLUE);
            break;
	}
}

void ws2812_init3(ws2812_t *m, uint32_t pin, uint32_t length, PIO pio, uint32_t sm, uint32_t b1, uint32_t b2, uint32_t b3)
{
	ws2812_initialize(m, pin, length, pio, sm, b1, b1, b2, b3);
}
void ws2812_init4(ws2812_t *m, uint32_t pin, uint32_t length, PIO pio, uint32_t sm, uint32_t b1, uint32_t b2, uint32_t b3, uint32_t b4)
{
    ws2812_initialize(m, pin, length, pio, sm, b1, b2, b3, b4);
}

uint32_t convert_rgb_data(ws2812_t *m,uint32_t rgbw)
{
	uint32_t result = 0;
    for (uint32_t b = 0; b < 4; b++) {
        switch (m->bytes[b]) {
            case LED_RED:
                result |= (rgbw & 0xff);
                break;
            case LED_GREEN:
                result |= (rgbw & 0xFF00) >> 8;
                break;
            case LED_BLUE:
                result |= (rgbw & 0xFF0000) >> 16;
                break;
            case LED_WHITE:
                result |= (rgbw & 0xFF000000) >> 24;
                break;
        }
        result <<= 8;
    }
    return result;
}


void ws2812_set_pixel(ws2812_t *m,uint32_t index, uint32_t color)
{
	if (index < m->length) {
		m->data[index] = convert_rgb_data(m, color);
	}
}

void ws2812_set_pixel_rgb(ws2812_t *m, uint32_t index, uint32_t red, uint32_t green, uint32_t blue)
{
	ws2812_set_pixel(m, index, RGB(red, green, blue));
}

void ws2812_set_pixel_rgbw(ws2812_t *m, uint32_t index, uint32_t red, uint32_t green, uint32_t blue, uint32_t white)
{
	ws2812_set_pixel(m, index, RGBW(red, green, blue, white));
}

void ws2812_fill1(ws2812_t *m, uint32_t color)
{
	ws2812_fill3(m, color, 0, m->length);
}

void ws2812_fill2(ws2812_t *m, uint32_t color, uint32_t first)
{
	ws2812_fill3(m, color, first, m->length-first);
}

void ws2812_fill3(ws2812_t *m, uint32_t color, uint32_t first, uint32_t count)
{
	uint32_t last = first + count;
	if (last > m->length) {
		last = m->length;
	}

	color = convert_rgb_data(m, color);
	for (uint32_t i = first; i < last; i++) {
		m->data[i] = color;
	}
}

void ws2812_show(ws2812_t *m)
{
	for (uint32_t i=0; i<m->length; i++) {
		pio_sm_put_blocking(m->pio, m->sm, m->data[i]);
	}
}

