#include "bmp280.h"
#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"

#define BMP280_REG_CHIP_ID 0xD0
#define BMP280_REG_CTRL_MEAS 0xF4
#define BMP280_REG_CONFIG 0xF5
#define BMP280_REG_PRESSURE 0xF7
#define BMP280_REG_TEMPERATURE 0xFA
#define BMP280_REG_CALIB_START 0x88
#define BMP280_REG_RESET 0xE0

#define BMP280_MODE_NORMAL 0x03
#define BMP280_RESET_CMD 0xB6
#define BMP280_CHIP_ID_VALUE 0X58

typedef struct bmp280_t {
    bmp280_config_t cfg;
    struct {
        uint16_t dig_t1;
        int16_t dig_t2;
        int16_t dig_t3;
        uint16_t dig_p1;
        int16_t dig_p2;
        int16_t dig_p3;
        int16_t dig_p4;
        int16_t dig_p5;
        int16_t dig_p6;
        int16_t dig_p7;
        int16_t dig_p8;
        int16_t dig_p9;
    } calib;
    int32_t t_fine;
} bmp280_t;

static esp_err_t bmp280_read_reg(bmp280_handle_t handle, const uint8_t reg, uint8_t *buf, const size_t len) {
    return hal_i2c_read(handle->cfg.bus,
                            handle->cfg.i2c_addr,
                            reg,
                            buf,
                            len);
}

static esp_err_t bmp280_write_reg(bmp280_handle_t handle, const uint8_t reg, const uint8_t val) {
    return hal_i2c_write_byte(handle->cfg.bus,
                              handle->cfg.i2c_addr,
                              reg,
                              val);
}

static esp_err_t bmp280_read_calib(bmp280_handle_t handle) {
    uint8_t data[24];
    const esp_err_t err = bmp280_read_reg(handle, BMP280_REG_CALIB_START, data, 24);
    if (err != ESP_OK) return err;

    handle->calib.dig_t1 = (uint16_t)((data[1] << 8) | data[0]);
    handle->calib.dig_t2 = (int16_t)((data[3] << 8) | data[2]);
    handle->calib.dig_t3 = (int16_t)((data[5] << 8) | data[4]);

    handle->calib.dig_p1 = (uint16_t)((data[7] << 8) | data[6]);
    handle->calib.dig_p2 = (int16_t)((data[9] << 8) | data[8]);
    handle->calib.dig_p3 = (int16_t)((data[11] << 8) | data[10]);
    handle->calib.dig_p4 = (int16_t)((data[13] << 8) | data[12]);
    handle->calib.dig_p5 = (int16_t)((data[15] << 8) | data[14]);
    handle->calib.dig_p6 = (int16_t)((data[17] << 8) | data[16]);
    handle->calib.dig_p7 = (int16_t)((data[19] << 8) | data[18]);
    handle->calib.dig_p8 = (int16_t)((data[21] << 8) | data[20]);
    handle->calib.dig_p9 = (int16_t)((data[23] << 8) | data[22]);

    return ESP_OK;
}

esp_err_t bmp280_init(const bmp280_config_t *cfg, bmp280_handle_t *out_handle) {
    if (!cfg || !out_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_handle = NULL;

    bmp280_handle_t dev = calloc(1, sizeof(bmp280_t));
    if (!dev) {
        return ESP_ERR_NO_MEM;
    }
    dev->cfg = *cfg;

    uint8_t id = 0;
    esp_err_t err = hal_i2c_read(cfg->bus, cfg->i2c_addr,
                             BMP280_REG_CHIP_ID,
                             &id, 1);

    if (err != ESP_OK || id != BMP280_CHIP_ID_VALUE) {
        free(dev);
        return ESP_ERR_INVALID_RESPONSE;
    }

    err = bmp280_write_reg(dev, BMP280_REG_RESET, BMP280_RESET_CMD);
    if (err != ESP_OK) {
        free(dev);
        return ESP_ERR_INVALID_RESPONSE;
    }

    vTaskDelay(pdMS_TO_TICKS(10));


    uint8_t config = (cfg->standby_time << 5) | (cfg->filter_coeff << 2);
    err = bmp280_write_reg(dev, BMP280_REG_CONFIG, config);
    if (err != ESP_OK) {
        free(dev);
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint8_t ctrl = (cfg->temp_oversampling << 5) |
                   (cfg->press_oversampling << 2) | BMP280_MODE_NORMAL;

    err = bmp280_write_reg(dev, BMP280_REG_CTRL_MEAS, ctrl);

    if (err != ESP_OK) {
        free(dev);
        return ESP_ERR_INVALID_RESPONSE;
    }

    err = bmp280_read_calib(dev);
    if (err != ESP_OK) {
        free(dev);
        return err;
    }

    dev->t_fine = 0;

    *out_handle = dev;
    return ESP_OK;
}

static esp_err_t bmp280_read_raw(bmp280_handle_t handle, int32_t *raw_temp, int32_t *raw_press) {
    uint8_t data[6];
    esp_err_t err = bmp280_read_reg(handle, BMP280_REG_PRESSURE, data, 6);

    if (err != ESP_OK) return err;

    *raw_press = ((int32_t)data[0] << 12) |
            ((int32_t)data[1] << 4)  |
            ((int32_t)data[2] >> 4);

    *raw_temp = ((int32_t)data[3] << 12) |
            ((int32_t)data[4] << 4)  |
            ((int32_t)data[5] >> 4);
    return ESP_OK;

}

static int32_t bmp280_compensate_temp(bmp280_handle_t out_handle, const int32_t raw_temp) {
    const int32_t var1 = ((((raw_temp >> 3) - ((int32_t) out_handle->calib.dig_t1 << 1))) * ((int32_t) out_handle->calib.
                        dig_t2)) >> 11;
    int32_t var2 = (((((raw_temp >> 4) - ((int32_t) out_handle->calib.dig_t1)) * (
                          (raw_temp >> 4) - ((int32_t) out_handle->calib.dig_t1))) >> 12) * ((int32_t) out_handle->
                        calib.dig_t3)) >> 14;
    out_handle->t_fine =  var1 + var2;
    int32_t t = (out_handle->t_fine * 5 + 128) >> 8;
    return t;
}


static uint32_t bmp280_compensate_press(bmp280_handle_t out_handle, const int32_t raw_press) {
    int64_t var1 = ((int64_t) out_handle->t_fine) - 128000;
    int64_t var2 = var1 * var1 * (int64_t) out_handle->calib.dig_p6;
    var2 = var2 + ((var1 * (int64_t)out_handle->calib.dig_p5) << 17);
    var2 = var2 + (((int64_t)out_handle->calib.dig_p4) << 35);
    var1 = ((var1 * var1 * (int64_t)out_handle->calib.dig_p3) >> 8) + ((var1 * (int64_t)out_handle->calib.dig_p2) << 12);
    var1 = ((((int64_t)1) << 47) + var1) * (int64_t)out_handle->calib.dig_p1 >> 33;
    if (var1 == 0) {
        return 0;
    }
    int64_t p = 1048576 - raw_press;
    p = (((p<<31) - var2) * 3125) / var1;
    var1 = (((int64_t)out_handle->calib.dig_p9) * (p >>13) * (p>>13)) >> 25;
    var2 = (((int64_t)out_handle->calib.dig_p8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)out_handle->calib.dig_p7) << 4);

    return (uint32_t)p;
}

esp_err_t bmp280_read(bmp280_handle_t handle, float *temperature_c, float *pressure_hpa) {
    if (!handle || !temperature_c || !pressure_hpa) {
        return ESP_ERR_INVALID_ARG;
    }

    int32_t raw_temp = 0;
    int32_t raw_press = 0;

    esp_err_t err = bmp280_read_raw(handle, &raw_temp, &raw_press);
    if (err != ESP_OK) {
        return err;
    }

    int32_t temp = bmp280_compensate_temp(handle, raw_temp);
    uint32_t press = bmp280_compensate_press(handle, raw_press);

    *temperature_c = (float)temp / 100.0f;

    float press_pa = (float)press / 256.0f;
    *pressure_hpa = press_pa / 100.0f;

    return ESP_OK;
}

esp_err_t bmp280_deinit(bmp280_handle_t handle)
{
    if (!handle) return ESP_ERR_INVALID_ARG;

    free(handle);
    return ESP_OK;
}
