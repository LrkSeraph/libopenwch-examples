/*
 * This file is part of the libopenwch examples.
 *
 * Copyright (C) 2025 libopenwch contributors
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * A CH582M Bluetooth LE peripheral that advertises and accepts a connection.
 *
 * Scan for "libopenwch" from any phone.  The LED is off while advertising and
 * on while a central is connected.
 *
 * This is the smallest complete program on the ble_* layer, and it shows the
 * shape every one must have:
 *
 *   1. the application owns the stack's heap, which BLE_HEAP_DEFINE declares;
 *   2. ble_init() starts the stack, and the application calls it before
 *      anything else;
 *   3. the peripheral role is initialised, then a TMOS task is registered;
 *   4. advertising data and the advertising interval are set through
 *      ble_gap_role_set_param() and ble_gap_set_param();
 *   5. ble_gap_role_peripheral_start_device() hands the stack the callbacks
 *      and takes over from there;
 *   6. the main loop does nothing but call ble_tmos_process() forever.
 *
 * The MAC address has to be supplied here.  Reading the address burned into
 * the part needs WCH's libISP583.a, a second closed binary this project does
 * not vendor, so pick one - any value will do for a demo, but a product needs
 * a real, unique address.
 *
 * Build:   make LIBOPENWCH_BLE=1
 * Flash:   make LIBOPENWCH_BLE=1 flash
 */

#include <libopenwch/ble/ble.h>
#include <libopenwch/ch5xx58x/clk.h>
#include <libopenwch/ch5xx58x/gpio.h>

/*
 * Board wiring.  PB4 is broken out on most CH582/CH583 modules and the LED is
 * active low on the WCH boards.
 */
#define LED_PORT GPIOB
#define LED_PIN GPIO4

/*
 * The stack allocates from this and does not allocate it for you.  6 KiB is
 * what WCH's own examples use; BLE_HEAP_SIZE_MIN is the floor.
 */
BLE_HEAP_DEFINE(ble_heap, BLE_HEAP_SIZE_DEFAULT);

/*
 * This device's address, six bytes, little-endian.  The first byte is the
 * least significant octet, so this reads as 84:C2:E4:03:02:02 on the air.
 */
static const uint8_t device_mac[6] = {0x02, 0x02, 0x03, 0xe4, 0xc2, 0x84};

/*
 * Advertising data: the flags AD type (general discoverable, no BR/EDR) and
 * the complete local name.  Length byte, AD type byte, then the payload.
 */
static uint8_t advert_data[] = {
    0x02, 0x01, 0x06, 0x0b, 0x09, 'l', 'i', 'b',
    'o',  'p',	'e',  'n',  'w',  'c', 'h',
};

static ble_tmos_task_id_t app_task;

/*
 * The peripheral role reports its own state here.  Advertising restarts by
 * itself after a central disconnects, so there is nothing to do but show the
 * state.
 */
static void on_state_change(gapRole_States_t state, gapRoleEvent_t *event) {
	(void)event;

	switch (state) {
	case GAPROLE_CONNECTED:
	case GAPROLE_CONNECTED_ADV:
		gpio_set(LED_PORT, LED_PIN);
		break;
	case GAPROLE_ADVERTISING:
	case GAPROLE_WAITING:
		gpio_clear(LED_PORT, LED_PIN);
		break;
	default:
		break;
	}
}

static ble_gap_role_callbacks_t role_cbs = {
    .pfnStateChange = on_state_change,
    .pfnRssiRead = 0,
    .pfnParamUpdate = 0,
};

/*
 * The application's one event handler.  The stack calls it with event bits
 * and expects the consumed bits back.
 *
 * BLE_TMOS_EVENT_MSG means a message is waiting: connection changes arrive
 * that way, as a gapRoleEvent_t.  This example lets the role callbacks deal
 * with them and simply drains the queue, which is what every application must
 * do even when it has nothing to say about the contents.
 */
static ble_tmos_event_t app_event_handler(ble_tmos_task_id_t task,
					  ble_tmos_event_t events) {
	if ((events & BLE_TMOS_EVENT_MSG) != 0) {
		uint8_t *message = ble_tmos_message_receive(task);

		if (message != 0) {
			ble_tmos_message_free(message);
		}

		return (ble_tmos_event_t)(events ^ BLE_TMOS_EVENT_MSG);
	}

	return 0;
}

int main(void) {
	ble_config_t config;
	uint8_t advertising = 1;
	uint16_t interval = 160; /* 160 * 0.625 ms = 100 ms */

	/* 60 MHz from the PLL, the clock WCH's own BLE examples use. */
	clk_set_sys_clock(CLK_SOURCE_PLL_60MHZ);

	gpio_set_mode(LED_PORT, GPIO_MODE_OUTPUT_PP_5MA, LED_PIN);
	gpio_clear(LED_PORT, LED_PIN);

	ble_config_default(&config);
	config.heap = ble_heap;
	config.heap_size = sizeof(ble_heap);
	config.mac = device_mac;

	if (ble_init(&config) != BLE_INIT_OK) {
		for (;;) {
		}
	}

	ble_gap_role_peripheral_init();
	app_task = ble_tmos_task_register(app_event_handler);

	ble_gap_role_set_param(BLE_GAP_ROLE_PARAM_ADVERT_DATA,
			       (uint16_t)sizeof(advert_data), advert_data);
	ble_gap_set_param(BLE_GAP_PARAM_DISC_ADV_INT_MIN, interval);
	ble_gap_set_param(BLE_GAP_PARAM_DISC_ADV_INT_MAX, interval);
	ble_gap_role_set_param(BLE_GAP_ROLE_PARAM_ADVERT_ENABLED,
			       (uint16_t)sizeof(advertising), &advertising);

	/*
	 * No pairing callbacks: this example is an open peripheral.  A product
	 * that wants bonding passes a gapBondCBs_t here and needs somewhere to
	 * store the keys, which means flash.
	 */
	ble_gap_role_peripheral_start_device(app_task, 0, &role_cbs);

	for (;;) {
		ble_tmos_process();
	}

	return 0;
}
