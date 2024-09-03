#include <stdio.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/adc_types.h"
#include "hal/gpio_types.h"

#include "can.h"
#include "test_led.h"
#include "global.h"

bool is_led_output_enabled = 0;
volatile bool previous_button_state = 0;
volatile bool current_button_state = 0;
volatile bool is_blinking_enabled = 0;

esp_timer_handle_t timer_handle;

void test_led_initialize()
{
    test_led_config_gpio();
    test_led_config_timer();

    xTaskCreate(&test_led_thread, "test_led", 2048, NULL, 0, NULL);
}

void test_led_config_gpio()
{
    // Test input gpio
    gpio_config_t TEST = {
        .pin_bit_mask = 1ULL << GPIO_NUM_1,
        .mode = GPIO_MODE_INPUT,
    };
    gpio_config(&TEST);

    // LED Test output gpio
    gpio_config_t LED = {
        .pin_bit_mask = 1ULL << GPIO_NUM_33,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&LED);
}

void test_led_config_timer()
{
    esp_timer_create_args_t timer_args = {
        .callback = test_led_timer_callback,
        .dispatch_method = ESP_TIMER_TASK,
    };
    esp_timer_create(&timer_args, &timer_handle);
}

void test_led_timer_callback()
{
    gpio_set_level(GPIO_NUM_33, is_led_output_enabled);
    is_led_output_enabled = !is_led_output_enabled;
}

void test_led_thread(void* pvParameter)
{
    while (true)
    {
        current_button_state = gpio_get_level(GPIO_NUM_1);
        if (current_button_state == false && previous_button_state == true)
        {
            vTaskDelay(20 / portTICK_PERIOD_MS);
            current_button_state = gpio_get_level(GPIO_NUM_1);

            if (current_button_state == false)
            {
                // is_blinking_enabled = !is_blinking_enabled;
                can_send();
            }
        }

        if (is_blinking_enabled == true)
        {
            esp_timer_start_periodic(timer_handle, 1000 * 100);
        }
        else
        {
            if (esp_timer_is_active(timer_handle))
            {
                esp_timer_stop(timer_handle);
            };
            gpio_set_level(GPIO_NUM_33, 0);
        }
        previous_button_state = current_button_state;
        // vTaskDelay(1 / portTICK_PERIOD_MS);       //if xTaskCreate priority > 0
    }
}
