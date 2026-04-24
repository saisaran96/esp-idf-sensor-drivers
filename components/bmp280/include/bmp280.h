#pragma once

#include "hal_i2c.h"
#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    hal_i2c_bus_t *bus;
    uint8_t i2c_addr;
    uint8_t temp_oversampling;
    uint8_t press_oversampling;
    uint8_t filter_coeff;
    uint8_t standby_time;
} bmp280_config_t;

typedef struct bmp280_t *bmp280_handle_t;

esp_err_t bmp280_init(const bmp280_config_t *cfg, bmp280_handle_t *out_handle);
esp_err_t bmp280_read(bmp280_handle_t handle, float *temperature_c, float *pressure_hpa);
esp_err_t bmp280_deinit(bmp280_handle_t handle);

#ifdef __cplusplus
}
#endif