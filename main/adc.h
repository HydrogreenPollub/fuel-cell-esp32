#ifndef FCCU_ADC_H
#define FCCU_ADC_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <esp_adc/adc_continuous.h>

static adc_continuous_handle_t adc_handle;

void adc_task(void *pvParameter);
bool IRAM_ATTR on_adc_conversion_done(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata);
void configure_adc();

#endif