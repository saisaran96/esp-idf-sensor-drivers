#pragma once

#include "esp_err.h"
#include "driver/i2c.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_port_t port;
    gpio_num_t sda_pin;
    gpio_num_t scl_pin;
    uint32_t clock_hz;
    bool installed;
} hal_i2c_bus_t;

esp_err_t hal_i2c_bus_init(hal_i2c_bus_t *bus);
esp_err_t hal_i2c_bus_deinit(hal_i2c_bus_t *bus);

esp_err_t hal_i2c_read(hal_i2c_bus_t *bus, uint8_t addr, uint8_t reg,
                       uint8_t *data, size_t len);
esp_err_t hal_i2c_write(hal_i2c_bus_t *bus, uint8_t addr, uint8_t reg,
                        const uint8_t *data, size_t len);
esp_err_t hal_i2c_write_byte(hal_i2c_bus_t *bus, uint8_t addr, uint8_t reg, uint8_t value);

#ifdef __cplusplus
}
#endif