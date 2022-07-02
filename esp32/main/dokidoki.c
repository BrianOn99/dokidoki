#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "driver/ledc.h"
#include "bluetooth.h"

static void ledc_init(void)
{
	ledc_timer_config_t ledc_timer = {
		.duty_resolution = LEDC_TIMER_4_BIT, // resolution of PWM duty
		.freq_hz = 400,                      // frequency of PWM signal
		.speed_mode = LEDC_LOW_SPEED_MODE,           // timer mode
		.timer_num = LEDC_TIMER_1,            // timer index
		.clk_cfg = LEDC_AUTO_CLK,              // Auto select the source clock
	};

	ledc_channel_config_t ledc_channel = {
		.channel    = LEDC_CHANNEL_0,
		.duty       = 0,
		.gpio_num   = GPIO_NUM_18,
		.speed_mode = LEDC_LOW_SPEED_MODE,
		.hpoint     = 0,
		.timer_sel  = LEDC_TIMER_1
	};

	ledc_channel_config(&ledc_channel);
	ledc_timer_config(&ledc_timer);
	ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 4);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void command_handler(uint8_t *cmd, int len) {
	uint8_t c = cmd[0] -'a';
	if (c > 0 && c < 16) {
		ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, c);
		ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
	}
}

void app_main()
{
	gpio_pad_select_gpio(GPIO_NUM_4);
	gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);
	gpio_pad_select_gpio(GPIO_NUM_5);
	gpio_set_direction(GPIO_NUM_5, GPIO_MODE_OUTPUT);
	gpio_pad_select_gpio(GPIO_NUM_18);
	gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);

	ledc_init();
	bluetooth_init(command_handler);
        gpio_set_level(GPIO_NUM_4, 0);
        gpio_set_level(GPIO_NUM_5, 1);

	/*
	while(1) {
		vTaskDelay(100 / portTICK_PERIOD_MS);
		c = getchar();
		c = c-'a';
		if (c > 0 && c < 16) {
			ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, c);
			ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
		}
	}
	*/
}
