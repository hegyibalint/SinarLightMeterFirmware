#pragma once

#include <stdint.h>

/**
 * @brief Sensor data returned
 */
struct sensor_data {
    uint32_t lumens;
};

/**
 * @brief Entry point of the sensor thread
 */
void sensor_thread_entry(void *, void *, void *);