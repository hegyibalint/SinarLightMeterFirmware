/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <logging/log.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#include <devicetree.h>
#include <drivers/gpio.h>

#include "sensor.h"
#include "comm.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

// ============================================================================
// Queues
// ============================================================================

K_MSGQ_DEFINE(sensor_queue, sizeof(struct sensor_data), 16, 4);

// ============================================================================
// Threads
// ============================================================================

#define SENSOR_THREAD_PRIO 5
K_THREAD_DEFINE(
	sensor_tid,
	256,
	sensor_thread_entry,
	NULL, NULL, NULL,
	SENSOR_THREAD_PRIO, 0, 0);

#define COMM_THREAD_PRIO 5
K_THREAD_DEFINE(
	comm_tid,
	1024,
	comm_thread_entry,
	NULL, NULL, NULL,
	COMM_THREAD_PRIO, 0, 0);

// ============================================================================
// GPIO devices
// ============================================================================

// Run LED --------------------------------------------------------------------

static const struct gpio_dt_spec run_led_dt = GPIO_DT_SPEC_GET(DT_NODELABEL(led0), gpios);


// ============================================================================
// Initialization
// ============================================================================

void main_init(void)
{
	int err;
	LOG_INF("Initializing main thread");

	// GPIOs ------------------------------------------------------------------

	err = gpio_pin_configure_dt(&run_led_dt, GPIO_OUTPUT_HIGH);
	if (err)
	{
		printk("Run LED configuring failed");
		k_fatal_halt(K_ERR_KERNEL_PANIC);
	}

	LOG_INF("Main initialization complete");
}

// ============================================================================
// Entry
// ============================================================================

void main(void)
{
	LOG_INF("Starting Light Meter...");
	main_init();

	for (int i = 0; i < 5; i++) {
		k_sleep(K_MSEC(100));
		gpio_pin_toggle(run_led_dt.port, run_led_dt.pin);
	}

	while (1)
	{
		k_sleep(K_SECONDS(1));
		gpio_pin_toggle(run_led_dt.port, run_led_dt.pin);
	}
}