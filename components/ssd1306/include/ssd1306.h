#pragma once

#include "hal_i2c.h"
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SSD1306_WIDTH     128
#define SSD1306_HEIGHT     64
#define SSD1306_PAGES      (SSD1306_HEIGHT / 8)
#define SSD1306_BUF_SIZE   (SSD1306_WIDTH * SSD1306_PAGES)

#define SSD1306_ADDR_DEFAULT   0x3C
#define SSD1306_ADDR_ALT       0x3D

typedef struct {
    hal_i2c_bus_t *bus;
    uint8_t        addr;
    bool           initialized;
    uint8_t        framebuf[SSD1306_BUF_SIZE];
} ssd1306_t;

esp_err_t ssd1306_init(ssd1306_t *dev, hal_i2c_bus_t *bus, uint8_t addr);
esp_err_t ssd1306_deinit(ssd1306_t *dev);
esp_err_t ssd1306_display_on(ssd1306_t *dev, bool on);
esp_err_t ssd1306_set_contrast(ssd1306_t *dev, uint8_t contrast);
esp_err_t ssd1306_invert(ssd1306_t *dev, bool invert);
void ssd1306_clear(ssd1306_t *dev);
void ssd1306_fill(ssd1306_t *dev);
void ssd1306_set_pixel(ssd1306_t *dev, uint8_t x, uint8_t y);
bool ssd1306_get_pixel(const ssd1306_t *dev, uint8_t x, uint8_t y);
void ssd1306_draw_hline(ssd1306_t *dev, uint8_t x, uint8_t y,
                        uint8_t width);
void ssd1306_draw_vline(ssd1306_t *dev, uint8_t x, uint8_t y,
                        uint8_t height);
void ssd1306_draw_rect(ssd1306_t *dev, uint8_t x, uint8_t y,
                       uint8_t w, uint8_t h);
void ssd1306_fill_rect(ssd1306_t *dev, uint8_t x, uint8_t y,
                       uint8_t w, uint8_t h);
void ssd1306_draw_string(ssd1306_t *dev, uint8_t x, uint8_t y, const char *str);
esp_err_t ssd1306_flush(ssd1306_t *dev);

#ifdef __cplusplus
}
#endif