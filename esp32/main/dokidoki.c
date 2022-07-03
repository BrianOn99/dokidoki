#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "driver/ledc.h"
#include "bluetooth.h"
#include "esp_log.h"

#define TAG "DOKI"

#define htonl( ulIn )\
(\
( uint32_t )\
(\
( ( ( ( uint32_t ) ( ulIn ) )                ) << 24  ) | 	\
( ( ( ( uint32_t ) ( ulIn ) ) & 0x0000ff00UL ) <<  8  ) | 	\
( ( ( ( uint32_t ) ( ulIn ) ) & 0x00ff0000UL ) >>  8  ) | 	\
( ( ( ( uint32_t ) ( ulIn ) )                ) >> 24  )  	\
)\
)
#define ntohl( x ) htonl( x )

static uint8_t MOV = 1;

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
	ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void normalize_strength(int32_t *x, uint32_t *strength, signed char *direction) {
	uint32_t l;
	int32_t k = (int32_t) ntohl(*x);
	if (k < 0) {
		l = -k;
		*direction = -1;
	} else {
		l = k;
		*direction = 1;
	}
	printf("T %d %d", l, INT32_MAX);
	*strength = (uint32_t)((15.0) * l / INT32_MAX);
	if (*strength < 4) {
		*strength = 0;
	}
}

static void set_direction(signed char direction, gpio_num_t a, gpio_num_t b) {
	if (direction > 0) {
		gpio_set_level(GPIO_NUM_4, 0);
		gpio_set_level(GPIO_NUM_5, 1);
	} else {
		gpio_set_level(GPIO_NUM_4, 1);
		gpio_set_level(GPIO_NUM_5, 0);
	}
}

static void command_handler(uint8_t *cmd, int len) {
	ESP_LOGI(TAG, "command_handler");
	if (cmd[0] == MOV) {
		uint32_t strength;
		signed char direction;
		normalize_strength((int32_t *)(cmd+1), &strength, &direction);

		ESP_LOGI(TAG, "strength %d direction %d", strength, direction);
		set_direction(direction, GPIO_NUM_4, GPIO_NUM_5);
		if (strength < 16) {
			ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, strength);
			ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
		}
	} else {
		ESP_LOGI(TAG, "Unknown cmd");
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

	bluetooth_init(command_handler);
	ledc_init();
        gpio_set_level(GPIO_NUM_4, 0);
        gpio_set_level(GPIO_NUM_5, 0);

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
