#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/adc_types.h"
#include "hal/gpio_types.h"
#include "sdkconfig.h"
#include <inttypes.h>
#include <stdio.h>
#include "driver/pulse_cnt.h"

#include "can.h"
#include "fans.h"
#include "test_led.h"

/*  digital output main valve
    digtal output purge valve
    PWM fan     (100%)
    pulse coutner tacho x2 (50%)
    ADC fuel cell temp sensor (100%)  */

bool fan_flash = 0;
// bool flash = 0;
// bool signal_change = 0;

pcnt_unit_handle_t pcnt_handle;
pcnt_channel_handle_t pcnt_channel_handle;
// esp_timer_handle_t timer_handle;
uint64_t timer_last_time = 0;

adc_cali_handle_t adc_cali_handle;
adc_oneshot_unit_handle_t adc_unit;

float map(float x, float in_min, float in_max, float out_min, float out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void config_gpios()
{
    // OUTPUTS
    // LED Test output gpio
    // gpio_config_t LED = {
    //     .pin_bit_mask = 1ULL << GPIO_NUM_33,
    //     .mode = GPIO_MODE_OUTPUT,
    // };
    // gpio_config(&LED);

    // Main valve output gpio
    gpio_config_t MAIN_VALVE_MCU = {
        .pin_bit_mask = 1ULL << GPIO_NUM_37,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&MAIN_VALVE_MCU);

    // Purge valve output gpio
    gpio_config_t PURGE_VALVE_MCU = {
        .pin_bit_mask = 1ULL << GPIO_NUM_27, // gpio is occupied, need to change documentation
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&PURGE_VALVE_MCU);

    // // Fan output gpio
    // gpio_config_t FAN_ON_MCU = {
    //     .pin_bit_mask = 1ULL << GPIO_NUM_28, // gpio is occupied, need to change documentation
    //     .mode = GPIO_MODE_OUTPUT,
    // };
    // gpio_config(&FAN_ON_MCU);

    // // MCU LED output gpio
    // gpio_config_t LED_MCU_STATUS = {
    //     .pin_bit_mask = 1ULL << GPIO_NUM_32, // gpio is occupied, need to change documentation
    //     .mode = GPIO_MODE_OUTPUT,
    // };
    // gpio_config(&LED_MCU_STATUS);

    // CAN LED output gpio
    gpio_config_t LED_CAN_STATUS = {
        .pin_bit_mask = 1ULL << GPIO_NUM_33,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&LED_CAN_STATUS);

    // INPUTS
    // Test input gpio
    gpio_config_t TEST = {
        .pin_bit_mask = 1ULL << GPIO_NUM_1,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&TEST);

    // Temp MCU input gpio
    gpio_config_t Temp_MCU = {
        .pin_bit_mask = 1ULL << GPIO_NUM_2,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&Temp_MCU);

    // Analog MCU input gpio
    gpio_config_t Analog_MCU = {
        .pin_bit_mask = 1ULL << GPIO_NUM_3,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&Analog_MCU);

    // MCU FC voltage input gpio
    gpio_config_t FC_VOLTAGE_MCU = {
        .pin_bit_mask = 1ULL << GPIO_NUM_4,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&FC_VOLTAGE_MCU);

    // Fan PWM input gpio
    gpio_config_t FAN_IN_PWM_MCU = {
        .pin_bit_mask = 1ULL << GPIO_NUM_45,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&FAN_IN_PWM_MCU);
}

// void IRAM_ATTR gpio_isr_handler()
// {
//     uint64_t timer_time = esp_timer_get_time();
//     if (timer_time - timer_last_time > 50 * 1000)
//     {
//         if (signal_change)
//         {
//             esp_timer_start_periodic(timer_handle, 1000 * 100);
//         }
//         else
//         {
//             if (esp_timer_is_active(timer_handle))
//             {
//                 esp_timer_stop(timer_handle);
//             };
//             gpio_set_level(GPIO_NUM_33, 0);
//         }
//         signal_change = !signal_change;
//     }
//     timer_last_time = timer_time;
// };

// void config_interrupt_handler()
// {
//     gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);

//     gpio_isr_handler_add(GPIO_NUM_1, gpio_isr_handler, NULL);
// }

// void timer_function()
// {
//     gpio_set_level(GPIO_NUM_33, flash);
//     flash = !flash;
// }

// void config_timer()
// {
//     esp_timer_create_args_t timer_args = {
//         .callback = timer_function,
//         .dispatch_method = ESP_TIMER_TASK,
//     };
//     esp_timer_create(&timer_args, &timer_handle);
// }

void adc_init()
{
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

    adc_oneshot_config_channel(adc_unit, ADC_CHANNEL_1, &channel_config);

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    int result = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);
    ESP_LOGI("ADC", "Cali result %d", result);
}

void pwm_init()
{
    ledc_timer_config_t ledc_timer = {

        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 10, // Set output frequency at 4 kHz
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {

        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = GPIO_NUM_37,
        .duty = 0, // Set duty to 0%
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void pulse_counter_init()
{
    pcnt_unit_config_t pcnt_config = {
        .low_limit = -1,
        .high_limit = 32768 - 1,
        .intr_priority = 0,
    };
    pcnt_new_unit(&pcnt_config, &pcnt_handle);

    pcnt_chan_config_t pcnt_channel_config = {.edge_gpio_num = GPIO_NUM_8, // should be GPIO_NUM_29
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
}

void app_main(void)
{
    // config_gpios();
    // config_timer();
    // config_interrupt_handler();
    // adc_init();
    // pwm_init();
    // pulse_counter_init();

    // int adc_output = 0;
    // int pcnt_value = 0;
    // int fan_rpm_value = 0;
    can_initialize();
    test_led_initialize();

    while (1)
    {
        // can_send();
        can_recieve();
        // adc_oneshot_read(adc_unit, ADC_CHANNEL_1, &adc_output);
        // float voltage = map(adc_output, 0, 4095, 0, 3.3);
        // uint32_t duty = (uint32_t) map(adc_output, 0, 4095, 0, 8000);
        // ESP_LOGI("ADC", "%.2f V, (value: %d, duty: %lu/8000)", voltage, adc_output, duty);

        // ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        // ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        // pcnt_unit_get_count(pcnt_handle, &pcnt_value);
        // pcnt_unit_clear_count(pcnt_handle);
        // fan_rpm_value = pcnt_value * 60 / 2; // rps -> rpm (2 pulses per revolution)
        // ESP_LOGI("PCNT", "%d rpm, (%d pulses/s)", fan_rpm_value, pcnt_value);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}
