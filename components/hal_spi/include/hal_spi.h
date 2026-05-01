#ifndef SPI_DEVICE_WRAPPER_H
#define SPI_DEVICE_WRAPPER_H

#include <stdint.h>
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    spi_device_handle_t handle;
} spi_dev_t;

/**
 * Initialize SPI bus (call once globally)
 */
esp_err_t spi_bus_init(
    spi_host_device_t host,
    int miso,
    int mosi,
    int sclk
);

/**
 * Add a device to SPI bus
 */
esp_err_t spi_dev_add(
    spi_host_device_t host,
    spi_dev_t *dev,
    int cs_pin,
    int clock_hz,
    int mode
);

/**
 * Transmit full-duplex data
 */
esp_err_t spi_dev_transfer(
    spi_dev_t *dev,
    const uint8_t *tx,
    uint8_t *rx,
    size_t len_bytes
);

/**
 * Transmit only (no read)
 */
esp_err_t spi_dev_write(
    spi_dev_t *dev,
    const uint8_t *tx,
    size_t len_bytes
);

/**
 * Receive only (clock dummy bytes)
 */
esp_err_t spi_dev_read(
    spi_dev_t *dev,
    uint8_t *rx,
    size_t len_bytes
);

#ifdef __cplusplus
}
#endif

#endif