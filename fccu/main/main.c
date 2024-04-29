#include <stdio.h>
#include <esp_adc/adc_continuous.h>

void adc_task(void *pvParameter)
{
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

void app_main(void)
{

}