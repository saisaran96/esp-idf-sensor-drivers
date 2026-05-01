#include "hal_spi.h"
#include <string.h>

#include "esp_err.h"
#include "driver/spi_master.h"

esp_err_t spi_bus_init(
    spi_host_device_t host,
    int miso,
    int mosi,
    int sclk
) {
    spi_bus_config_t buscfg = {
        .miso_io_num = miso,
        .mosi_io_num = mosi,
        .sclk_io_num = sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    return spi_bus_initialize(host, &buscfg, SPI_DMA_CH_AUTO);
}

esp_err_t spi_dev_add(
    spi_host_device_t host,
    spi_dev_t *dev,
    int cs_pin,
    int clock_hz,
    int mode
) {
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = clock_hz,
        .mode = mode,
        .spics_io_num = cs_pin,
        .queue_size = 1,
    };

    return spi_bus_add_device(host, &devcfg, &dev->handle);
}

esp_err_t spi_dev_transfer(
    spi_dev_t *dev,
    const uint8_t *tx,
    uint8_t *rx,
    size_t len_bytes
) {
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    t.length = len_bytes * 8;
    t.tx_buffer = tx;
    t.rx_buffer = rx;

    return spi_device_transmit(dev->handle, &t);
}

esp_err_t spi_dev_write(
    spi_dev_t *dev,
    const uint8_t *tx,
    size_t len_bytes
) {
    return spi_dev_transfer(dev, tx, NULL, len_bytes);
}

esp_err_t spi_dev_read(
    spi_dev_t *dev,
    uint8_t *rx,
    size_t len_bytes
) {
    uint8_t dummy[len_bytes];
    memset(dummy, 0, len_bytes);

    return spi_dev_transfer(dev, dummy, rx, len_bytes);
}