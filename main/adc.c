#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <esp_adc/adc_continuous.h>
#include "adc.h"

/**
*   This is the implementation used in Energy Flow module.
*   In case of an error - modification is required.
*
*/

void adc_task(void *pvParameter){
    adc_task_handle = xTaskGetCurrentTaskHandle();

    uint8_t result[CONVERSION_SAMPLES_LENGTH];
    memset(result, 0xcc, CONVERSION_SAMPLES_LENGTH);
    uint32_t length = 0;

    while (1)
    {
        /**
         * This is to show you the way to use the ADC continuous mode driver event callback.
         * This `ulTaskNotifyTake` will block when the data processing in the task is fast.
         * However in this example, the data processing (print) is slow, so you barely block here.
         *
         * Without using this event callback (to notify this task), you can still just call
         * `adc_continuous_read()` here in a loop, with/without a certain block timeout.
         */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (1)
        {
            esp_err_t ret = adc_continuous_read(adc_handle, result, CONVERSION_SAMPLES_LENGTH, &length, 0);
            if (ret == ESP_OK)
            {
                uint32_t sum = 0;
                for (int i = 0; i < length; i += SOC_ADC_DIGI_RESULT_BYTES * 2) // Without * 2 data is invalid?
                {
                    adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result[i];
                    uint32_t chan_num = p->type2.channel;
                    uint32_t data = p->type2.data;
                    /* Check the channel number validation, the data is invalid if the channel num exceed the maximum channel */
                    if (chan_num < SOC_ADC_CHANNEL_NUM(p->type2.unit))
                    {
                        sum += data;
                    }
                    else
                    {
                        ESP_LOGW(TAG, "Invalid data [%lu_%lu]", chan_num, data);
                    }
                }
                OD_RAM.x6400_vehicleSpeedFromMCU = map((float)sum / (length / 2), 0, 4096, 0, 3.3);
                /**
                 * Because printing is slow, so every time you call `ulTaskNotifyTake`, it will immediately return.
                 * To avoid a task watchdog timeout, add a delay here. When you replace the way you process the data,
                 * usually you don't need this delay (as this task will block for a while).
                 */
                vTaskDelay(1);
            }
            else if (ret == ESP_ERR_TIMEOUT)
            {
                // We try to read `EXAMPLE_READ_LEN` until API returns timeout, which means there's no available data
                break;
            }
        }
    }
}

static bool IRAM_ATTR on_adc_conversion_done(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata){
    BaseType_t mustYield = pdFALSE;

    // Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(adc_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}

static void configure_adc(){
    adc_continuous_handle_cfg_t adc_handle_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = CONVERSION_SAMPLES_LENGTH,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_handle_config, &adc_handle));

    adc_continuous_config_t adc_config = {
        .sample_freq_hz = 1 * 1000,
        .conv_mode = ADC_CONV_BOTH_UNIT,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2, // For ESP32-S3
    };

    uint8_t channels_unit_one[10] = {};
    uint8_t channels_unit_two[10] = {};
    uint8_t unit_one_size = 1; // sizeof(channels_unit_one) / sizeof(channels_unit_one[0]);
    uint8_t unit_two_size = 0; // sizeof(channels_unit_two) / sizeof(channels_unit_two[0])

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX];
    adc_config.pattern_num = unit_one_size + unit_two_size;

    for (int i = 0; i < unit_one_size; i++)
    {
        adc_pattern[i].atten = ADC_ATTEN_DB_0;
        adc_pattern[i].channel = channels_unit_one[i];
        adc_pattern[i].unit = ADC_UNIT_1;
        adc_pattern[i].bit_width = ADC_BITWIDTH_12;

        ESP_LOGI(TAG, "adc1_pattern[%d].atten is :%" PRIx8, i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc1_pattern[%d].channel is :%" PRIx8, i, adc_pattern[i].channel);
        ESP_LOGI(TAG, "adc1_pattern[%d].unit is :%" PRIx8, i, adc_pattern[i].unit);
    }

    for (int i = 0; i < unit_two_size; i++)
    {
        uint8_t pattern_number = i + unit_one_size;
        adc_pattern[pattern_number].atten = ADC_ATTEN_DB_0;
        adc_pattern[pattern_number].channel = channels_unit_two[i];
        adc_pattern[pattern_number].unit = ADC_UNIT_2;
        adc_pattern[pattern_number].bit_width = ADC_BITWIDTH_12;

        ESP_LOGI(TAG, "adc2_pattern[%d].atten is :%" PRIx8, pattern_number, adc_pattern[pattern_number].atten);
        ESP_LOGI(TAG, "adc2_pattern[%d].channel is :%" PRIx8, pattern_number, adc_pattern[pattern_number].channel);
        ESP_LOGI(TAG, "adc2_pattern[%d].unit is :%" PRIx8, pattern_number, adc_pattern[pattern_number].unit);
    }

    adc_config.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &adc_config));

    adc_continuous_evt_cbs_t callbacks = {
        .on_conv_done = on_adc_conversion_done,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(adc_handle, &callbacks, NULL));

    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
}