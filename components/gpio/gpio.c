
#include "gpio.h"
#include "esp_log.h"
#include "soc/gpio_struct.h"
#

static const char *TAG = "gpio_drv";


esp_err_t gpio_drv_init(const gpio_cfg_t *cfg) {

    if (!cfg || cfg->pin < 0 || cfg->pin >= GPIO_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    gpio_config_t io = {
        .pin_bit_mask =  (1ULL << cfg->pin),
        .mode = (cfg->direction == GPIO_DIR_INPUT ? GPIO_MODE_INPUT : GPIO_MODE_OUTPUT),
        .pull_up_en = (cfg->pull == GPIO_PULL_UP) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = (cfg->pull == GPIO_PULL_DOWN) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = cfg->intr_type,
    };

    esp_err_t ret = gpio_config(&io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %d", ret);
        return ret;
    }

    if (cfg->direction == GPIO_DIR_OUTPUT) {
        if (cfg->initial_state) {
            gpio_drv_set(cfg->pin);
        } else {
            gpio_drv_clear(cfg->pin);
        }
    }

    if (cfg->intr_type != GPIO_INTR_DISABLE && cfg->isr_handler) {

        static bool isr_service_installed = false;

        if (!isr_service_installed) {
            gpio_install_isr_service(0);
            isr_service_installed = true;
        }

        gpio_isr_handler_add(cfg->pin, cfg->isr_handler, cfg->args);
    }

    ESP_LOGI(TAG, "gpio driver initialized");
    return ESP_OK;
}


void gpio_drv_set(const gpio_num_t pin) {

    if (pin < 32) {
       GPIO.out_w1ts = (1U << pin);
    } else {
        GPIO.out1_w1ts.val = (1U << (pin - 32));
    }
}

void gpio_drv_clear(gpio_num_t pin)
{
    if (pin < 32) {
        GPIO.out_w1tc = (1U << pin);
    } else {
        GPIO.out1_w1tc.val = (1U << (pin - 32));
    }
}

void gpio_drv_toggle(gpio_num_t pin) {
    uint32_t current;

    if (pin < 32) {
        current = (GPIO.out >> pin) & 1;
    } else {
        current = (GPIO.out1.val >> (pin -32)) & 1;
    }

    if (current) {
        gpio_drv_clear(pin);
    } else {
        gpio_drv_set(pin);
    }
}



esp_err_t gpio_drv_read(gpio_num_t pin, bool *value) {
    if (!value || pin < 0 || pin >= GPIO_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    if (pin < 32) {
        *value = (GPIO.in >> pin) & 1;
    } else {
        *value = (GPIO.in1.val >> (pin -32)) & 1;
    }

    return ESP_OK;
}


esp_err_t gpio_drv_deinit(gpio_num_t pin) {
    if (pin < 0 || pin >= GPIO_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    gpio_config_t io = {
        .pin_bit_mask =  (1ULL << pin),
        .mode = GPIO_MODE_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&io);
    gpio_isr_handler_remove(pin);
    gpio_intr_disable(pin);

    return ESP_OK;
}