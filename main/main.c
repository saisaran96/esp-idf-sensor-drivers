#include <string.h>
#include "hal_spi.h"
#include "rc522.h"
#include "driver/gpio.h"

#define RC522_SPI_HOST  SPI2_HOST
#define PIN_NUM_MISO    19
#define PIN_NUM_MOSI    11
#define PIN_NUM_CLK     18
#define PIN_NUM_CS      5
#define PIN_NUM_RST     21


void app_main(void) {

    spi_dev_t spi;
    rc522_t rc522;
    // SPI init
    spi_bus_init(SPI2_HOST, PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK);
    spi_dev_add(SPI2_HOST, &spi, PIN_NUM_CS, 1 * 1000 * 1000, 0);

    // RC522 context setup
    rc522.spi = &spi;
    rc522.rst_pin = PIN_NUM_RST;


    rc522_init(&rc522);

    uint8_t atqa[2];
    uint8_t uid[10];
    uint8_t uid_len;
    uint8_t sak;

    uint8_t last_uid[10] = {0};
    uint8_t last_uid_len = 0;

    uint8_t keyA[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t block = 4;
    uint8_t data[16];

    int miss_count = 0;
    const int MISS_THRESHOLD = 5;

    while (1) {
        bool card_present = false;

        if (rc522_request(&rc522, atqa) == RC522_OK) {
            if (rc522_read_uid(&rc522, uid, &uid_len, &sak) == RC522_OK) {
                card_present = true;
                miss_count = 0;

                bool same_card =
                        (uid_len == last_uid_len) &&
                        (memcmp(uid, last_uid, uid_len) == 0);

                if (!same_card) {
                    printf("New card detected\nUID (%d): ", uid_len);
                    for (int i = 0; i < uid_len; i++)
                        printf("%02X ", uid[i]);

                    printf("| SAK: 0x%02X\n", sak);

                    memcpy(last_uid, uid, uid_len);
                    last_uid_len = uid_len;

                    if (rc522_is_trailer_block(block) ||
                        rc522_is_manufacturer_block(block)) {

                        printf("Do not write this block; it is manufacturer/trailer data\n");

                    } else {
                        if (rc522_mifare_read(&rc522, block, data, uid, keyA) == RC522_OK) {
                            printf("Read block %d: ", block);
                            for (int i = 0; i < 16; i++) printf("%02X ", data[i]);
                            printf("\n");
                        }

                        rc522_stop_crypto(&rc522);
                    }
                }
            }
        }

        if (!card_present) {
            miss_count++;

            if (miss_count >= MISS_THRESHOLD && last_uid_len != 0) {
                printf("Card removed\n");

                last_uid_len = 0;
                memset(last_uid, 0, sizeof(last_uid));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
