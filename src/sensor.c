#include "sensor.h"

#include <zephyr.h>
#include <logging/log.h>
#include <logging/log_ctrl.h>

#include <devicetree.h>
#include <drivers/gpio.h>

LOG_MODULE_REGISTER(sensor, LOG_LEVEL_DBG);

// ============================================================================
// GPIO devices
// ============================================================================

// Connection LED -------------------------------------------------------------

#define MEAS_BUTTON_NODE DT_NODELABEL(button0)
static const struct gpio_dt_spec meas_button_dt = GPIO_DT_SPEC_GET(MEAS_BUTTON_NODE, gpios);
static struct gpio_callback meas_button_cb;

// ============================================================================
// Queues
// ============================================================================

extern struct k_msgq sensor_queue;

// ============================================================================
// Interrupts
// ============================================================================

// TEST: value used for test data
static uint32_t counter = 0x12345600;

void meas_button_interrupt(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    const struct sensor_data data = {
        counter++
    };

    k_msgq_put(&sensor_queue, &data, K_NO_WAIT);
    LOG_INF("Test data sent to queue");
}

// ============================================================================
// Initialization
// ============================================================================

void sensor_init(void)
{
    int err;
    LOG_INF("Initializing sensor thread");

    // GPIO initialization
    err = gpio_pin_configure_dt(&meas_button_dt, GPIO_INPUT | GPIO_INT_DEBOUNCE);
    if (err != 0) {
		LOG_ERR("Send button configuration failed");
        LOG_PANIC();
		k_fatal_halt(K_ERR_KERNEL_PANIC);
    }

    err = gpio_pin_interrupt_configure_dt(&meas_button_dt, GPIO_INT_EDGE_TO_ACTIVE);
    if (err != 0) {
		LOG_ERR("Send button interrupt configuration failed");
        LOG_PANIC();
		k_fatal_halt(K_ERR_KERNEL_PANIC);
    }

    gpio_init_callback(&meas_button_cb, meas_button_interrupt, BIT(meas_button_dt.pin));
    err = gpio_add_callback(meas_button_dt.port, &meas_button_cb);
    if (err != 0) {
		LOG_ERR("Send button callback registration failed");
        LOG_PANIC();
		k_fatal_halt(K_ERR_KERNEL_PANIC);
    }

    LOG_INF("Sensor thread initialized");
}

// ============================================================================
// Entry
// ============================================================================

void sensor_thread_entry(void *param1, void *param2, void *param3)
{
    sensor_init();

    while (1)
    {
        k_sleep(K_FOREVER);
    }
}