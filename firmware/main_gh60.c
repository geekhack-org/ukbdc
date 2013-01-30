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

uint8_t matrix[5][14] = {
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13},
	{14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27},
	{28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41},
	{42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55},
	{56, 57, 58, 0, 0, 59, 0, 0, 0, 0, 60, 61, 62, 63},
};

uint8_t rows[] = {0, 1, 2, 3, 4};
uint8_t cols[] = {5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};

#define LED 19

volatile bool should_scan = false;

int main(void)
{
	clock_prescale_set(clock_div_1);

	IO_config(LED, OUTPUT);
	IO_set(LED, true);

	USB_init();
	while (USB_get_configuration() == 0)
		;

	/* initialize with 64 keys */
	LAYOUT_init(64);

	int ret = LAYOUT_set((struct layout*)LAYOUT_BEGIN);
	if (ret != 0) {
		for (int i = 0; i < 30; ++i) {
			IO_set(LED, false);
			_delay_ms(100);
			IO_set(LED, true);
			_delay_ms(100);
		}
	}
	LAYOUT_set_callback(&HID_set_scancode_state);

	MATRIX_init(5, rows, 14, cols, (const uint8_t*)matrix, &LAYOUT_set_key_state);

	HID_commit_state();
	for (int i = 0; i < 30; ++i) {
		IO_set(LED, false);
		_delay_ms(50);
		IO_set(LED, true);
		_delay_ms(50);
	}

	TCCR0A = 0x00;
	TCCR0B = 0x03; /* clk_io / 64 */
	TIMSK0 = _BV(TOIE0);
	while(1) {
		if (should_scan) {
			should_scan = false;
			bool changed = MATRIX_scan();
			if (changed)
				HID_commit_state();
			if (HID_get_leds() & 0x02)
				IO_set(LED, false);
			else
				IO_set(LED, true);
		}
		RAWHID_PROTOCOL_task();
	}

	while (1)
		;
}

void MAIN_handle_sof()
{
}

ISR(TIMER0_OVF_vect)
{
	static uint8_t num = 0;
	if (num > 16) {
		num = 0;
		should_scan = true;
	} else {
		++num;
	}
}
