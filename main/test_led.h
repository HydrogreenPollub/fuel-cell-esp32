#ifndef TEST_LED_H
#define TEST_LED_H

void test_led_initialize();
void test_led_config_gpio();
void test_led_config_timer();

void test_led_timer_callback();
void test_led_thread(void* pvParameter);

#endif