#include "fans.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/adc_types.h"
#include "hal/gpio_types.h"
#include <stdio.h>
#include "driver/pulse_cnt.h"
#include "sdkconfig.h"

pcnt_unit_handle_t pcnt_handle;
pcnt_channel_handle_t pcnt_channel_handle;
esp_timer_handle_t timer_handle;
uint64_t timer_last_time = 0;

adc_cali_handle_t adc_cali_handle;
adc_oneshot_unit_handle_t adc_unit;

void fans_initialize()
{
    ////////////////////////////////////////////////////////////////////////////////////

    // Fans on/off output GPIO
    gpio_config_t fan_on_off_config = {
        .pin_bit_mask = 1ULL << CONFIG_IO_1_GPIO_NUM, // TODO replace with FANS_ON_OFF_GPIO_NUM
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&fan_on_off_config);

    ////////////////////////////////////////////////////////////////////////////////////

    // Fans PWM timer configuration
    ledc_timer_config_t ledc_timer = {

        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = CONFIG_FAN_PWM_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {

        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = CONFIG_IO_2_GPIO_NUM, // TODO replace with FAN_PWM_OUT_GPIO_NUM,
        .duty = 0,
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ////////////////////////////////////////////////////////////////////////////////////
    // TODO Move to new file
    // Fuel Cell temperatue ADC config
    adc_oneshot_unit_init_cfg_t adc_config = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_DIGI_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    adc_oneshot_new_unit(&adc_config, &adc_unit);

    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_11, // ADC_ATTEN_DB_11 na 0 podaje 3,3V przy 0,97V
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    adc_oneshot_config_channel(adc_unit, CONFIG_FUEL_CELL_TEMP_ADC_CHANNEL, &channel_config);

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1, // TODO
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    int result = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);
    ESP_LOGI("ADC", "Cali result %d", result);

    ////////////////////////////////////////////////////////////////////////////////////
    // TODO set two pcnt_handle & channel
    // Left fan PCNT config
    pcnt_unit_config_t pcnt_config = {
        .low_limit = -1,
        .high_limit = 32768 - 1,
        .intr_priority = 0,
    };
    pcnt_new_unit(&pcnt_config, &pcnt_handle);

    pcnt_chan_config_t pcnt_channel_config = {
        .edge_gpio_num = CONFIG_FAN_LEFT_TACHO_GPIO_NUM, 
        .level_gpio_num = -1,
        .flags = {
            .invert_edge_input = 1, 
        },
    };
    pcnt_new_channel(pcnt_handle, &pcnt_channel_config, &pcnt_channel_handle);

    pcnt_glitch_filter_config_t pcnt_glitch_config = {
        .max_glitch_ns = 12000,
    };
    pcnt_unit_set_glitch_filter(pcnt_handle, &pcnt_glitch_config);

    pcnt_channel_set_edge_action(pcnt_channel_handle, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD);
    pcnt_unit_enable(pcnt_handle);
    pcnt_unit_start(pcnt_handle);

    // Right fan PCNT config

    pcnt_new_unit(&pcnt_config, &pcnt_handle); /////

    pcnt_chan_config_t pcnt_right_channel_config = {
        .edge_gpio_num = CONFIG_FAN_RIGHT_TACHO_GPIO_NUM,
        .level_gpio_num = -1,
        .flags = {
            .invert_edge_input = 1, 
        },
    };
    pcnt_new_channel(pcnt_handle, &pcnt_channel_config, &pcnt_channel_handle);

    pcnt_unit_set_glitch_filter(pcnt_handle, &pcnt_glitch_config);

    pcnt_channel_set_edge_action(pcnt_channel_handle, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD);
    pcnt_unit_enable(pcnt_handle);
    pcnt_unit_start(pcnt_handle);
}

void fans_start(fan_type_t fan_type) { }

void fans_stop(fan_type_t fan_type) { }

void fans_set_speed(fan_type_t fan_type, float speed_rpm) { }
