#include "platforms.h"
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <util/delay.h>
#include <stdbool.h>

#include "main.h"
#include "usb_keyboard.h"
#include "io.h"
#include "hid.h"
#include "rawhid.h"
#include "rawhid_protocol.h"
#include "layout.h"
#include "matrix.h"
#include "timer.h"
#include "leds.h"

uint8_t matrix[5][14] = {
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13},
	{14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27},
	{28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41},
	{42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55},
	{56, 57, 58, 0, 0, 59, 0, 0, 0, 0, 60, 61, 62, 63},
};

uint8_t rows[] = {0, 1, 2, 3, 4};
uint8_t cols[] = {5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};

volatile bool should_scan = false;

void on_key_press(uint8_t key, bool event)
{
	if (USB_is_sleeping())
		USB_wakeup();
	else
		LAYOUT_set_key_state(key, event);
}

int main(void)
{
	clock_prescale_set(clock_div_1);

	USB_init();

	/* initialize with 64 keys */
	LAYOUT_init(64);

	LAYOUT_set((struct layout*)LAYOUT_BEGIN);
	LAYOUT_set_callback(&HID_set_scancode_state);

	MATRIX_init(5, rows, 14, cols, (const uint8_t*)matrix, &on_key_press);

	HID_commit_state();

	TIMER_init();
	LED_init();

	bool was_sleeping = false;
	while (true) {
		if (USB_is_sleeping()) {
			if (!was_sleeping)
				LED_set_indicators(0x00);
			was_sleeping = true;
		} else {
			if (was_sleeping)
				LED_set_indicators(HID_get_leds());
			was_sleeping = false;
		}
		if (should_scan) {
			should_scan = false;
			bool changed = MATRIX_scan();
			if (changed)
				HID_commit_state();
			if (HID_leds_changed())
				LED_set_indicators(HID_get_leds());
		}
		RAWHID_PROTOCOL_task();
	}

	while (1)
		;
}

void MAIN_sleep_timer_handler()
{
	if (!USB_is_sleeping())
		return;
	/* make sure to scan the matrix from time to time while asleep */
	should_scan = true;
}

void MAIN_handle_sof()
{
	static uint16_t num = 0;
	if (num > 20) {
		num = 0;
		should_scan = true;
	} else {
		++num;
	}
}