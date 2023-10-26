#include <stdio.h>
#include <stdlib.h>
#include "nozaos.h"
#include "ws2812.h"

#define LED_PIN 16
#define LED_LENGTH 1

static int test_ws2812(int argc, char **argv)
{
	ws2812_t led_strip;
	ws2812_init2(&led_strip, LED_PIN, LED_LENGTH, pio0, 0, LED_FORMAT_RGB);

	 // 1. Set all LEDs to red!
    printf("1. Set all LEDs to red!\n");
    ws2812_fill1(&led_strip, RGB(255, 0, 0));
    ws2812_show(&led_strip);
	noza_thread_sleep_ms(1000, NULL);

    // 2. Set all LEDs to green!
    printf("2. Set all LEDs to green!\n");
    ws2812_fill1(&led_strip, RGB(0, 255, 0));
    ws2812_show(&led_strip);
    noza_thread_sleep_ms(1000, NULL);

    // 3. Set all LEDs to blue!
    printf("3. Set all LEDs to blue!\n");
    ws2812_fill1(&led_strip, RGB(0, 0, 255));
    ws2812_show(&led_strip);
	noza_thread_sleep_ms(1000, NULL);

    // 4. Set half LEDs to red and half to blue!
    printf("4. Set half LEDs to red and half to blue!\n");
    ws2812_fill3(&led_strip, RGB(255, 0, 0), 0, LED_LENGTH / 2 );
    ws2812_fill2(&led_strip, RGB(0, 0, 255), LED_LENGTH / 2 );
    ws2812_show(&led_strip);
	noza_thread_sleep_ms(1000, NULL);

	// 5. Do some fancy animation
    printf("5. Do some fancy animation\n");
    while (true) {
        // Pick a random color
        uint32_t color = (uint32_t)rand();
        // Pick a random direction
        int8_t dir = (rand() & 1 ? 1 : -1);
        // Setup start and end offsets for the loop
        uint8_t start = (dir > 0 ? 0 : LED_LENGTH);
        uint8_t end = (dir > 0 ? LED_LENGTH : 0);
        for (uint8_t ledIndex = start; ledIndex != end; ledIndex += dir) {
            ws2812_set_pixel(&led_strip, ledIndex, color);
            ws2812_show(&led_strip);
            noza_thread_sleep_ms(50, NULL);
        }
    }
}

#include "user/console/noza_console.h"
void __attribute__((constructor(1000))) register_led_example()
{
    console_add_command("led", test_ws2812, "ws2812 led example program");
}