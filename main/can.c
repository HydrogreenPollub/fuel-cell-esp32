#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/twai.h"

#include "global.h"
#include "can.h"

void can_initialize()
{
    // Initialize configuration structures using macro initializers
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_39, GPIO_NUM_40, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Install TWAI driver
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK)
    {
        printf("Driver installed\n");
    }
    else
    {
        printf("Failed to install driver\n");
        return;
    }

    // Start TWAI driver
    if (twai_start() == ESP_OK)
    {
        printf("Driver started\n");
    }
    else
    {
        printf("Failed to start driver\n");
        return;
    }
}

void can_send()
{ // Configure message to transmit
    twai_message_t message = {
        // Message type and format settings
        .extd = 0,         // Standard vs extended format
        .rtr = 0,          // Data vs RTR frame
        .ss = 0,           // Whether the message is single shot (i.e., does not repeat on error)
        .self = 0,         // Whether the message is a self reception request (loopback)
        .dlc_non_comp = 0, // DLC is less than 8
        // Message ID and payload
        .identifier = 0xAAAA,
        .data_length_code = 4,
        .data = { 0, 1, 2, 3 },
    };

    // Queue message for transmission
    if (twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK)
    {
        printf("Message queued for transmission\n");
    }
    else
    {
        printf("Failed to queue message for transmission\n");
    }
}

void can_recieve()
{ // Wait for message to be received
    twai_message_t message;
    if (twai_receive(&message, pdMS_TO_TICKS(10000)) == ESP_OK)
    {
        printf("Message received\n");
    }
    else
    {
        printf("Failed to receive message\n");
        return;
    }

    // Process received message
    if (message.extd)
    {
        printf("Message is in Extended Format\n");
    }
    else
    {
        printf("Message is in Standard Format\n");
    }
    printf("ID is %lu\n", message.identifier);
    if (!(message.rtr))
    {
        for (int i = 0; i < message.data_length_code; i++)
        {
            printf("Data byte %d = %d\n", i, message.data[i]);
        }
    }

    is_blinking_enabled = !is_blinking_enabled;
}
