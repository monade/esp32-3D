#include <assert.h>
#include <stdbool.h>
#include "inputs.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "soc/soc_caps.h"

#define YR_ESP_MAX_JOYSTICKS 8
#define YR_JOYSTICK_AXIS_COUNT 2
#define YR_JOYSTICK_ADC_MAX 4095
#define YR_JOYSTICK_ADC_CENTER 2048
#define YR_JOYSTICK_CALIBRATION_SAMPLES 16
#define YR_JOYSTICK_DEADZONE 32

struct joystick_config_t {
  adc_oneshot_unit_handle_t adc_handle;
  adc_unit_t adc_unit;
  adc_channel_t adc_channel;
  int pin;
  int center;
};

#ifndef YR_PERF_VAR_ATTR
#if defined(ESP32) && !defined(ESP32_NO_PERF)
#define YR_PERF_VAR_ATTR __attribute__((section(".iram1"))) 
#else
#define YR_PERF_VAR_ATTR
#endif
#endif

static bool key_state[YR_KEYS_COUNT] = {0};
YR_PERF_VAR_ATTR static int key_maps[YR_KEYS_COUNT] = {0};
static adc_oneshot_unit_handle_t adc_unit_handles[SOC_ADC_PERIPH_NUM] = {0};
static struct joystick_config_t joysticks[YR_ESP_MAX_JOYSTICKS][YR_JOYSTICK_AXIS_COUNT] = {0};
static int joystick_count = 0;

void yr_inputs_init() {}

static inline void validate_pin(int pin) {
    assert(pin >= 0 && pin < GPIO_NUM_MAX);
}

static int get_or_create_adc_unit(adc_unit_t unit, adc_oneshot_unit_handle_t *handle) {
    if (unit >= SOC_ADC_PERIPH_NUM) {
        return ESP_ERR_INVALID_ARG;
    }

    if (adc_unit_handles[unit] == NULL) {
        adc_oneshot_unit_init_cfg_t unit_cfg = {
            .unit_id = unit,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        int ret = adc_oneshot_new_unit(&unit_cfg, &adc_unit_handles[unit]);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    *handle = adc_unit_handles[unit];
    return ESP_OK;
}

static int configure_joystick_axis(struct joystick_config_t *axis, int pin) {
    axis->pin = pin;

    int ret = adc_oneshot_io_to_channel(pin, &axis->adc_unit, &axis->adc_channel);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = get_or_create_adc_unit(axis->adc_unit, &axis->adc_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    return adc_oneshot_config_channel(axis->adc_handle, axis->adc_channel, &chan_cfg);
}

static int calibrate_joystick_axis(struct joystick_config_t *axis) {
    int sum = 0;
    int samples = 0;

    for (int i = 0; i < YR_JOYSTICK_CALIBRATION_SAMPLES; i++) {
        int value = 0;
        if (adc_oneshot_read(axis->adc_handle, axis->adc_channel, &value) == ESP_OK) {
            sum += value;
            samples++;
        }
    }

    if (samples == 0) {
        return YR_JOYSTICK_ADC_CENTER;
    }

    return sum / samples;
}

static float normalize_joystick_axis(int value, int center) {
    int delta = value - center;
    if (delta > -YR_JOYSTICK_DEADZONE && delta < YR_JOYSTICK_DEADZONE) {
        return 0.0f;
    }

    float range = delta > 0 ? (float)(YR_JOYSTICK_ADC_MAX - center) : (float)center;
    if (range <= 0.0f) {
        return 0.0f;
    }

    float normalized = (float)delta / range;
    if (normalized > 1.0f) {
        normalized = 1.0f;
    } else if (normalized < -1.0f) {
        normalized = -1.0f;
    }

    return -normalized;
}

void yr_esp_key_init(int pin, int key) {
    validate_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
    key_maps[key] = pin;
}

int yr_esp_joystick_init(int joystick_pin_x, int joystick_pin_y) {
    validate_pin(joystick_pin_x);
    validate_pin(joystick_pin_y);

    if (joystick_count >= YR_ESP_MAX_JOYSTICKS) {
        return -1;
    }

    struct joystick_config_t axes[YR_JOYSTICK_AXIS_COUNT] = {0};

    if (configure_joystick_axis(&axes[YR_X_AXIS], joystick_pin_x) != ESP_OK) {
        return -1;
    }
    if (configure_joystick_axis(&axes[YR_Y_AXIS], joystick_pin_y) != ESP_OK) {
        return -1;
    }

    axes[YR_X_AXIS].center = calibrate_joystick_axis(&axes[YR_X_AXIS]);
    axes[YR_Y_AXIS].center = calibrate_joystick_axis(&axes[YR_Y_AXIS]);

    int joystick_id = joystick_count++;
    joysticks[joystick_id][YR_X_AXIS] = axes[YR_X_AXIS];
    joysticks[joystick_id][YR_Y_AXIS] = axes[YR_Y_AXIS];

    return joystick_id;
}

float yr_esp_joystick_get_axis(int joystick_id, int axis) {
    if (joystick_id < 0 || joystick_id >= joystick_count || axis < 0 || axis >= YR_JOYSTICK_AXIS_COUNT) {
        return 0.0f;
    }

    struct joystick_config_t *axis_cfg = &joysticks[joystick_id][axis];
    int value = 0;
    int ret = adc_oneshot_read(axis_cfg->adc_handle, axis_cfg->adc_channel, &value);
    if (ret != ESP_OK) {
        return 0.0f;
    }

    return normalize_joystick_axis(value, axis_cfg->center);
}

bool yr_is_key_down(int key) {
    int pin = key_maps[key];
    if (pin == 0) return false;
    return !gpio_get_level(pin);
}

bool yr_is_key_up(int key) {
    return !yr_is_key_down(key);
}

bool yr_is_key_pressed(int key) {
    bool prev = key_state[key];
    bool current = yr_is_key_down(key);

    key_state[key] = current;

    return current && !prev;
}
