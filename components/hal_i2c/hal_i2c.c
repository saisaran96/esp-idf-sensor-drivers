#include "hal_i2c.h"

#include <string.h>

#include "esp_log.h"

esp_err_t hal_i2c_bus_init(hal_i2c_bus_t *bus)
{
    if (!bus) {
        return ESP_ERR_INVALID_ARG;
    }

    if (bus->installed) {
        return ESP_OK;
    }

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = bus->sda_pin,
        .scl_io_num = bus->scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = bus->clock_hz,
        .clk_flags = 0,
    };

    esp_err_t err = i2c_param_config(bus->port, &conf);
    if (err != ESP_OK) return err;

    err = i2c_driver_install(bus->port, conf.mode, 0, 0, 0);
    if (err != ESP_OK) return err;

    bus->installed = true;
    return ESP_OK;
}

esp_err_t hal_i2c_bus_deinit(hal_i2c_bus_t *bus)
{
    if (!bus) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!bus->installed) {
        return ESP_OK;
    }

    esp_err_t err = i2c_driver_delete(bus->port);
    if (err == ESP_OK) {
        bus->installed = false;
    }
    return err;
}

esp_err_t hal_i2c_read(hal_i2c_bus_t *bus, uint8_t addr, uint8_t reg,
                       uint8_t *data, size_t len)
{
    if (!bus || !data || len == 0) return ESP_ERR_INVALID_ARG;

    return i2c_master_write_read_device(
        bus->port, addr, &reg, 1, data, len, pdMS_TO_TICKS(1000));
}

esp_err_t hal_i2c_write(hal_i2c_bus_t *bus, uint8_t addr, uint8_t reg,
                        const uint8_t *data, size_t len)
{
    if (!bus) return ESP_ERR_INVALID_ARG;

    uint8_t buf[32];
    if (len + 1 > sizeof(buf)) return ESP_ERR_INVALID_SIZE;

    buf[0] = reg;
    if (len > 0 && data) {
        memcpy(&buf[1], data, len);
    }

    return i2c_master_write_to_device(
        bus->port, addr, buf, len + 1, pdMS_TO_TICKS(1000));
}

esp_err_t hal_i2c_write_byte(hal_i2c_bus_t *bus, uint8_t addr, uint8_t reg, uint8_t value)
{
    return hal_i2c_write(bus, addr, reg, &value, 1);
}