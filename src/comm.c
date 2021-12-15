#include "comm.h"

#include <zephyr.h>
#include <logging/log.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#include <devicetree.h>
#include <drivers/gpio.h>

LOG_MODULE_REGISTER(comm, LOG_LEVEL_DBG);

// ============================================================================
// Condition variables
// ============================================================================

K_CONDVAR_DEFINE(indicating_condvar);
K_MUTEX_DEFINE(indicating_mutex);

// ============================================================================
// Queues
// ============================================================================

extern struct k_msgq sensor_queue;

// ============================================================================
// GPIO devices
// ============================================================================

static const struct gpio_dt_spec con_led_dt = GPIO_DT_SPEC_GET(DT_NODELABEL(led1), gpios);

// ============================================================================
// Bluetooth connection tracking
// ============================================================================

static void bt_connected(struct bt_conn *conn, uint8_t err)
{
	if (err)
	{
		LOG_INF("Connection failed (err 0x%02x)", err);
	}
	else
	{
		LOG_INF("Connected");
		gpio_pin_set(con_led_dt.port, con_led_dt.pin, 1);
	}
}

static void bt_disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason 0x%02x)", reason);
	gpio_pin_set(con_led_dt.port, con_led_dt.pin, 0);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = bt_connected,
	.disconnected = bt_disconnected,
};

// ============================================================================
// Bluetooth read/writes
// ============================================================================

// Sensor CCC write  ----------------------------------------------------------

/**
 * @brief storing if the indication is turned on
 */
static uint8_t sensor_indicate = 0U;

static void write_sensor_ccc(const struct bt_gatt_attr *attr, uint16_t value)
{
	sensor_indicate = (value == BT_GATT_CCC_INDICATE) ? 1 : 0;
	LOG_INF("Sensor indication is set to: %d", value);
}

// Sensor data indication -----------------------------------------------------

/**
 * @brief storing if an indication is in progress
 */
static struct bt_gatt_indicate_params sensor_indicate_params;

static void sensor_indicate_start(
	struct bt_conn *conn,
	struct bt_gatt_indicate_params *params,
	uint8_t err)
{
	LOG_INF("Indication %s", err != 0U ? "fail" : "success");
}

static void sensor_indicate_end(struct bt_gatt_indicate_params *params)
{
	LOG_INF("Indication complete");
	k_condvar_signal(&indicating_condvar);
}

// ============================================================================
// Bluetooth attribute definitions
// ============================================================================

#define VND_UUID BT_UUID_128_ENCODE(0xf28e76d6, 0x40f5, 0x43a3, 0xb2a4, 0x8deac278fb30)
static struct bt_uuid_128 sensor_service_uuid = BT_UUID_INIT_128(VND_UUID);

#define POS_UUID BT_UUID_128_ENCODE(0xf28e76d6, 0x40f5, 0x43a3, 0xb2a4, 0x8deac278fb31)
static struct bt_uuid_128 sensor_attr_uuid = BT_UUID_INIT_128(POS_UUID);

BT_GATT_SERVICE_DEFINE(sensor_service,
					   BT_GATT_PRIMARY_SERVICE(&sensor_service_uuid),
					   BT_GATT_CHARACTERISTIC(
						   &sensor_attr_uuid.uuid,
						   BT_GATT_CHRC_INDICATE,
						   BT_GATT_PERM_READ,
						   NULL, NULL, NULL),
					   BT_GATT_CCC(
						   write_sensor_ccc,
						   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));

// ============================================================================
// Bluetooth initialization
// ============================================================================

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, VND_UUID),
};

static void bt_ready(int err)
{
	if (err)
	{
		LOG_INF("Bluetooth init failed (err %d)", err);
		return;
	}
	LOG_INF("Bluetooth initialized");

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err)
	{
		LOG_INF("Advertising failed to start (err %d)", err);
		return;
	}
}

// ============================================================================
// Entry
// ============================================================================

void comm_init(void)
{
	int err;
	struct bt_gatt_attr *sensor_attr;

	LOG_INF("Initializing communication thread");

	// Bluetooth ------------------------------------------------------------------

	err = bt_enable(bt_ready);
	if (err)
	{
		LOG_ERR("Bluetooth init failed (err %d)", err);
		k_fatal_halt(K_ERR_KERNEL_PANIC);
	}

	// GPIOs ------------------------------------------------------------------

	err = gpio_pin_configure_dt(&con_led_dt, GPIO_OUTPUT_LOW);
	if (err)
	{
		LOG_ERR("Connection LED configuring failed");
		k_fatal_halt(K_ERR_KERNEL_PANIC);
	}

	// BT indication ----------------------------------------------------------
	sensor_attr = bt_gatt_find_by_uuid(
		sensor_service.attrs, 
		sensor_service.attr_count,
		&sensor_attr_uuid.uuid);
	if (sensor_attr == NULL) {
		LOG_ERR("Cannot get sensor attribute");
		k_fatal_halt(K_ERR_KERNEL_PANIC);
	}	

	sensor_indicate_params.attr = sensor_attr;
	sensor_indicate_params.func = sensor_indicate_start;
	sensor_indicate_params.destroy = sensor_indicate_end;

	LOG_INF("Communication thread initialized");
}

void comm_thread_entry(void *param1, void *param2, void *param3)
{
	struct sensor_data data;

	comm_init();

	while (1)
	{
		k_msgq_get(&sensor_queue, &data, K_FOREVER);
		LOG_INF("Sensor data received: %d", data.lumens);

		if (sensor_indicate) {
			LOG_INF("Indicating new value");

			sensor_indicate_params.data = &data;
			sensor_indicate_params.len = sizeof(data);

			k_mutex_lock(&indicating_mutex, K_FOREVER);
			if (bt_gatt_indicate(NULL, &sensor_indicate_params) == 0) {
				LOG_INF("Start waiting on indication condition");
				k_condvar_wait(&indicating_condvar, &indicating_mutex, K_FOREVER);
				LOG_INF("Indication condition set");
			} else {
				LOG_ERR("Cannot complete sensor indication");
			}
			k_mutex_unlock(&indicating_mutex);
			
		} else {
			LOG_INF("No indication set up, data dropped");
		}
	}
}