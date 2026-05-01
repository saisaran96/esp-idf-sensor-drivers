#ifndef RC522_H
#define RC522_H

#include <stdint.h>
#include <stdbool.h>
#include "hal_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===================== Status ===================== */

typedef enum {
    RC522_OK = 0,
    RC522_ERR,
    RC522_TIMEOUT,
    RC522_NO_TAG
} rc522_status_t;

/* ===================== Constants ===================== */

#define RC522_UID_MAX_LEN 10
#define RC522_BLOCK_SIZE  16
#define RC522_KEY_SIZE    6

/* ===================== Device Handle ===================== */

typedef struct {
    spi_dev_t *spi;
    int rst_pin;
} rc522_t;

/* ===================== Init ===================== */

/**
 * Initialize RC522 device
 */
rc522_status_t rc522_init(rc522_t *dev);

/**
 * Hardware reset (RST pin toggle)
 */
void rc522_reset(rc522_t *dev);

/* ===================== Low-level ===================== */

/**
 * Write register
 */
void rc522_write_reg(rc522_t *dev, uint8_t reg, uint8_t val);

/**
 * Read register
 */
uint8_t rc522_read_reg(rc522_t *dev, uint8_t reg);

/* ===================== Card Detection ===================== */

/**
 * Request card (REQA or WUPA)
 * atqa must be 2 bytes
 */
rc522_status_t rc522_request(rc522_t *dev, uint8_t *atqa);

/**
 * Wake up card (WUPA)
 */
rc522_status_t rc522_wakeup(rc522_t *dev, uint8_t *atqa);

/**
 * Read UID (supports cascade levels)
 */
rc522_status_t rc522_read_uid(
    rc522_t *dev,
    uint8_t *uid,
    uint8_t *uid_len,
    uint8_t *sak
);

/**
 * Halt card (HLTA)
 */
rc522_status_t rc522_halt(rc522_t *dev);

/* ===================== Crypto ===================== */

/**
 * Authenticate block using Key A or Key B
 */
rc522_status_t rc522_auth(
    rc522_t *dev,
    uint8_t auth_mode,     // MF_AUTH_KEY_A / MF_AUTH_KEY_B
    uint8_t block_addr,
    const uint8_t key[RC522_KEY_SIZE],
    const uint8_t uid[4]   // first 4 bytes of UID
);

/**
 * Stop encryption (Crypto1)
 */
void rc522_stop_crypto(rc522_t *dev);

/* ===================== MIFARE ===================== */

/**
 * Read 16-byte block
 */
rc522_status_t rc522_mifare_read(
    rc522_t *dev,
    uint8_t block,
    uint8_t out[RC522_BLOCK_SIZE],
    const uint8_t uid[4],
    const uint8_t key[RC522_KEY_SIZE]
);

/**
 * Write 16-byte block
 */
rc522_status_t rc522_mifare_write(
    rc522_t *dev,
    uint8_t block,
    const uint8_t data[RC522_BLOCK_SIZE],
    const uint8_t uid[4],
    const uint8_t key[RC522_KEY_SIZE]
);

/* ===================== Utility ===================== */

/**
 * Calculate CRC using RC522 hardware
 */
rc522_status_t rc522_crc(
    rc522_t *dev,
    const uint8_t *data,
    uint8_t len,
    uint8_t out[2]
);

/**
 * Enable antenna
 */
void rc522_antenna_on(rc522_t *dev);

/**
 * Disable antenna
 */
void rc522_antenna_off(rc522_t *dev);

/* ===================== PICC Commands ===================== */

#define PICC_REQA       0x26
#define PICC_WUPA       0x52
#define PICC_HLTA       0x50

#define PICC_ANTICOLL1  0x93
#define PICC_ANTICOLL2  0x95
#define PICC_ANTICOLL3  0x97

#define PICC_READ       0x30
#define PICC_WRITE      0xA0

#define MF_AUTH_KEY_A   0x60
#define MF_AUTH_KEY_B   0x61

/* ===================== Helpers ===================== */

/**
 * Check if block is sector trailer
 */
static inline bool rc522_is_trailer_block(uint8_t block) {
    return (block % 4) == 3;
}

/**
 * Check if block is manufacturer block
 */
static inline bool rc522_is_manufacturer_block(uint8_t block) {
    return block == 0;
}

#ifdef __cplusplus
}
#endif

#endif /* RC522_H */