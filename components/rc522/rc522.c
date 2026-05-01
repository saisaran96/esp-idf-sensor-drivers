/* ===================== Includes ===================== */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "hal_spi.h"
#include "rc522.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ===================== Pins ===================== */
#define RC522_SPI_HOST  SPI2_HOST
#define PIN_NUM_MISO    19
#define PIN_NUM_MOSI    11
#define PIN_NUM_CLK     18
#define PIN_NUM_CS      5
#define PIN_NUM_RST     21

spi_device_handle_t rc522_spi;

/* ===================== RC522 Commands ===================== */
#define PCD_IDLE        0x00
#define PCD_TRANSCEIVE  0x0C
#define PCD_SOFTRESET   0x0F
#define PCD_CALCCRC     0x03

/* ===================== Registers ===================== */
#define CommandReg      0x01
#define ComIrqReg       0x04
#define DivIrqReg       0x05
#define ErrorReg        0x06
#define FIFODataReg     0x09
#define FIFOLevelReg    0x0A
#define ControlReg      0x0C
#define BitFramingReg   0x0D
#define CollReg         0x0E
#define ModeReg         0x11
#define TxControlReg    0x14
#define TxASKReg        0x15
#define TModReg         0x2A
#define TPrescalerReg   0x2B
#define TReloadRegH     0x2C
#define TReloadRegL     0x2D
#define CRCResultRegH   0x21
#define CRCResultRegL   0x22
#define VersionReg      0x37

/* ===================== PICC Commands ===================== */
#define PICC_REQIDL     0x26
#define PICC_CT         0x88
#define PICC_ANTICOLL1  0x93
#define PICC_ANTICOLL2  0x95
#define PICC_ANTICOLL3  0x97

/* ===================== Status ===================== */
#define RC522_OK        0
#define RC522_NOTAG     1
#define RC522_ERR       2

#define CRCIRQ          (1 << 2)

#define PCD_AUTHENT   0x0E
#define PCD_TRANSCEIVE 0x0C

#define PICC_READ     0x30
#define PICC_WRITE    0xA0
#define MF_AUTH_KEY_A 0x60
#define MF_AUTH_KEY_B 0x61

#define Status2Reg    0x08
#define MFCrypto1On   (1 << 3)

/* ===================== Low-Level Helpers ===================== */

static inline uint32_t tick_ms() {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}


void rc522_write(rc522_t *dev, uint8_t reg, uint8_t val) {
    uint8_t tx[2] = {
        (uint8_t) ((reg << 1) & 0x7E),
        val
    };

    spi_dev_write(dev->spi, tx, 2);
}

uint8_t rc522_read(rc522_t *dev, uint8_t reg) {
    uint8_t tx[2] = {
        (uint8_t) (((reg << 1) & 0x7E) | 0x80), // read (MSB = 1)
        0x00
    };

    uint8_t rx[2] = {0};

    spi_dev_transfer(dev->spi, tx, rx, 2);

    return rx[1];
}

static inline void set_bits(rc522_t *dev, uint8_t reg, uint8_t mask) {
    rc522_write(dev, reg, rc522_read(dev, reg) | mask);
}

static inline void clr_bits(rc522_t *dev, uint8_t reg, uint8_t mask) {
    rc522_write(dev, reg, rc522_read(dev, reg) & ~mask);
}

/* ===================== Core RC522 ===================== */

void rc522_reset(rc522_t *dev) {
    gpio_set_level(dev->rst_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(dev->rst_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

rc522_status_t rc522_init(rc522_t *dev) {
    gpio_set_direction(dev->rst_pin, GPIO_MODE_OUTPUT);

    rc522_reset(dev);

    rc522_write(dev, CommandReg, PCD_SOFTRESET);
    vTaskDelay(pdMS_TO_TICKS(50));

    rc522_write(dev, TModReg, 0x8D);
    rc522_write(dev,TPrescalerReg, 0x3E);
    rc522_write(dev,TReloadRegL, 0x1E);
    rc522_write(dev,TReloadRegH, 0);

    rc522_write(dev,TxASKReg, 0x40);
    rc522_write(dev,ModeReg, 0x3D);

    set_bits(dev, TxControlReg, 0x03);

    printf("RC522 Version: 0x%02X\n", rc522_read(dev,VersionReg));
    return RC522_OK;
}

/* ===================== FIFO Helper ===================== */

static void fifo_write(rc522_t *dev, const uint8_t *data, uint8_t len) {
    set_bits(dev, FIFOLevelReg, 0x80);
    for (int i = 0; i < len; i++)
        rc522_write(dev,FIFODataReg, data[i]);
}

/* ===================== Transceive ===================== */

static int rc522_transceive(
    rc522_t *dev,
    const uint8_t *send, uint8_t send_len,
    uint8_t *recv, uint8_t *recv_len,
    uint8_t tx_bits,
    uint8_t *rx_bits
) {
    rc522_write(dev,CommandReg, PCD_IDLE);
    rc522_write(dev,ComIrqReg, 0x7F);

    fifo_write(dev, send, send_len);

    rc522_write(dev,BitFramingReg, tx_bits);
    rc522_write(dev,CommandReg, PCD_TRANSCEIVE);
    set_bits(dev, BitFramingReg, 0x80);

    uint16_t timeout = 2000;
    uint8_t irq = 0;

    while (timeout--) {
        irq = rc522_read(dev,ComIrqReg);
        if (irq & 0x30) break;
        if (irq & 0x01) return RC522_NOTAG;
        vTaskDelay(1);
    }

    clr_bits(dev, BitFramingReg, 0x80);

    if (!(irq & 0x20)) return RC522_NOTAG;
    if (rc522_read(dev,ErrorReg) & 0x13) return RC522_ERR;

    uint8_t n = rc522_read(dev,FIFOLevelReg);

    if (recv_len) {
        n = (n < *recv_len) ? n : *recv_len;
        *recv_len = n;
    }

    for (int i = 0; i < n; i++)
        recv[i] = rc522_read(dev,FIFODataReg);

    if (rx_bits)
        *rx_bits = rc522_read(dev,ControlReg) & 0x07;

    return RC522_OK;
}

/* ===================== CRC ===================== */

rc522_status_t rc522_crc(rc522_t *dev, const uint8_t *data, uint8_t len, uint8_t out[2]) {
    rc522_write(dev,CommandReg, PCD_IDLE);
    clr_bits(dev,DivIrqReg, CRCIRQ);

    fifo_write(dev, data, len);

    rc522_write(dev,CommandReg, PCD_CALCCRC);

    uint32_t deadline = tick_ms() + 100;

    while (!(rc522_read(dev,DivIrqReg) & CRCIRQ)) {
        if (tick_ms() > deadline)
            return RC522_ERR;
    }

    rc522_write(dev,CommandReg, PCD_IDLE);

    out[0] = rc522_read(dev,CRCResultRegL);
    out[1] = rc522_read(dev,CRCResultRegH);

    return RC522_OK;
}

/* ===================== Request ===================== */

rc522_status_t rc522_request(rc522_t *dev, uint8_t *atqa) {
    uint8_t cmd = PICC_REQIDL;
    uint8_t len = 2;

    clr_bits(dev,CollReg, 0x80);

    int ret = rc522_transceive(dev, &cmd, 1, atqa, &len, 7, NULL);

    return (ret == RC522_OK && len == 2) ? RC522_OK : RC522_NOTAG;
}

static int anticoll_level(rc522_t *dev, uint8_t cmd, uint8_t buf[4], uint8_t *sak) {
    uint8_t req[2] = {cmd, 0x20};
    uint8_t rx[5];
    uint8_t len = 5;

    clr_bits(dev,CollReg, 0x80);

    int ret = rc522_transceive(dev, req, 2, rx, &len, 0, NULL);
    if (ret != RC522_OK || len != 5) return RC522_ERR;

    if ((rx[0] ^ rx[1] ^ rx[2] ^ rx[3]) != rx[4]) return RC522_ERR;

    memcpy(buf, rx, 4);

    uint8_t sel[9];
    sel[0] = cmd;
    sel[1] = 0x70;
    memcpy(&sel[2], rx, 5);

    if (rc522_crc(dev, sel, 7, &sel[7]) != RC522_OK)
        return RC522_ERR;

    uint8_t sak_buf[3];
    uint8_t sak_len = 3;

    ret = rc522_transceive(dev, sel, 9, sak_buf, &sak_len, 0, NULL);
    if (ret != RC522_OK || sak_len < 1) return RC522_ERR;

    *sak = sak_buf[0];
    return RC522_OK;
}

rc522_status_t rc522_read_uid(rc522_t *dev, uint8_t *uid, uint8_t *uid_len, uint8_t *sak) {
    const uint8_t cmds[3] = {
        PICC_ANTICOLL1,
        PICC_ANTICOLL2,
        PICC_ANTICOLL3
    };

    uint8_t tmp[10];
    uint8_t idx = 0;

    for (int i = 0; i < 3; i++) {
        uint8_t buf[4];
        uint8_t level_sak;

        int ret = anticoll_level(dev, cmds[i], buf, &level_sak);
        if (ret != RC522_OK) return ret;

        bool cascade = (buf[0] == PICC_CT);

        uint8_t start = cascade ? 1 : 0;
        memcpy(&tmp[idx], &buf[start], 4 - start);
        idx += (4 - start);

        *sak = level_sak;

        if (!(level_sak & 0x04)) break;
        if (!cascade) return RC522_ERR;
    }

    memcpy(uid, tmp, idx);
    *uid_len = idx;

    return RC522_OK;
}


rc522_status_t rc522_auth(rc522_t *dev, uint8_t auth_mode, uint8_t block_addr,
                          const uint8_t key[6], const uint8_t uid4[4]) {
    uint8_t buf[12];
    buf[0] = auth_mode;
    buf[1] = block_addr;
    memcpy(&buf[2], key, 6);
    memcpy(&buf[8], uid4, 4);

    rc522_write(dev,CommandReg, PCD_IDLE);
    rc522_write(dev,ComIrqReg, 0x7F);

    set_bits(dev,FIFOLevelReg, 0x80);
    for (int i = 0; i < 12; i++) {
        rc522_write(dev,FIFODataReg, buf[i]);
    }

    rc522_write(dev,CommandReg, PCD_AUTHENT);
    set_bits(dev,BitFramingReg, 0x80);

    uint32_t deadline = tick_ms() + 100;
    while (!(rc522_read(dev,ComIrqReg) & 0x30)) {
        if (tick_ms() > deadline) {
            return RC522_ERR;
        }
    }

    if (rc522_read(dev,ErrorReg) & 0x13) return RC522_ERR;
    if (!(rc522_read(dev,Status2Reg) & MFCrypto1On)) return RC522_ERR;

    return RC522_OK;
}

void rc522_stop_crypto(rc522_t *dev) {
    clr_bits(dev,Status2Reg, MFCrypto1On);
    rc522_write(dev,CommandReg, PCD_IDLE);
}

rc522_status_t rc522_mifare_read(rc522_t *dev, uint8_t block, uint8_t out[16],
                                 const uint8_t uid4[4], const uint8_t key[6]) {
    uint8_t cmd[4] = {PICC_READ, block, 0, 0};

    if (rc522_auth(dev, MF_AUTH_KEY_A, block, key, uid4) != RC522_OK) {
        return RC522_ERR;
    }

    if (rc522_crc(dev, cmd, 2, &cmd[2]) != RC522_OK) {
        return RC522_ERR;
    }

    uint8_t rx[18];
    uint8_t rx_len = sizeof(rx);

    int ret = rc522_transceive(dev, cmd, 4, rx, &rx_len, 0, NULL);
    if (ret != RC522_OK || rx_len < 16) {
        return RC522_ERR;
    }

    memcpy(out, rx, 16);
    return RC522_OK;
}

rc522_status_t rc522_mifare_write(rc522_t *dev, uint8_t block, const uint8_t data[16],
                                  const uint8_t uid4[4], const uint8_t key[6]) {
    uint8_t cmd[4] = {PICC_WRITE, block, 0, 0};

    if (rc522_auth(dev,MF_AUTH_KEY_A, block, key, uid4) != RC522_OK) {
        return RC522_ERR;
    }

    if (rc522_crc(dev, cmd, 2, &cmd[2]) != RC522_OK) {
        return RC522_ERR;
    }

    uint8_t ack[2];
    uint8_t ack_len = sizeof(ack);

    int ret = rc522_transceive(dev, cmd, 4, ack, &ack_len, 0, NULL);
    if (ret != RC522_OK || ack_len < 1 || (ack[0] & 0x0F) != 0x0A) {
        return RC522_ERR;
    }

    uint8_t frame[18];
    memcpy(frame, data, 16);
    if (rc522_crc(dev, frame, 16, &frame[16]) != RC522_OK) {
        return RC522_ERR;
    }

    ack_len = sizeof(ack);
    ret = rc522_transceive(dev, frame, 18, ack, &ack_len, 0, NULL);
    if (ret != RC522_OK || ack_len < 1 || (ack[0] & 0x0F) != 0x0A) {
        return RC522_ERR;
    }

    return RC522_OK;
}

