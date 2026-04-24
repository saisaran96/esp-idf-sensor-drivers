#ifndef GPIO_DRIVER_DRIVER_H
#define GPIO_DRIVER_DRIVER_H

#include <stdbool.h>
#include "driver/gpio.h"


typedef enum {GPIO_DIR_INPUT = 0, GPIO_DIR_OUTPUT = 1} gpio_dir_t;

typedef enum {GPIO_PULL_NONE =0, GPIO_PULL_UP, GPIO_PULL_DOWN} gpio_pull_t;

typedef struct { gpio_num_t pin; gpio_pull_t pull; gpio_dir_t direction; bool initial_state; gpio_int_type_t intr_type; void (*isr_handler)(void*);void *args;    } gpio_cfg_t;

esp_err_t gpio_drv_init(const gpio_cfg_t *cfg);
void gpio_drv_set(gpio_num_t pin);
void gpio_drv_clear(gpio_num_t pin);
void gpio_drv_toggle(gpio_num_t pin);
esp_err_t gpio_drv_read(gpio_num_t pin, bool *value);
esp_err_t gpio_drv_deinit(gpio_num_t pin);

#endif
