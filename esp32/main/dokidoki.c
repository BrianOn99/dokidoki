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

#define htonll( ulIn )\
(\
( uint64_t )\
(\
( ( ( ( uint64_t ) ( ulIn ) )                         ) << 56  ) | 	\
( ( ( ( uint64_t ) ( ulIn ) ) & 0x000000000000ff00ULL ) <<  40  ) | 	\
( ( ( ( uint64_t ) ( ulIn ) ) & 0x00ff000000000000ULL ) >>  40  ) | 	\
( ( ( ( uint64_t ) ( ulIn ) ) & 0x0000000000ff0000ULL ) <<  24  ) | 	\
( ( ( ( uint64_t ) ( ulIn ) ) & 0x0000ff0000000000ULL ) >>  24  ) | 	\
( ( ( ( uint64_t ) ( ulIn ) ) & 0x00000000ff000000ULL ) <<  8  ) | 	\
( ( ( ( uint64_t ) ( ulIn ) ) & 0x000000ff00000000ULL ) >>  8  ) | 	\
( ( ( ( uint64_t ) ( ulIn ) )                         ) >> 56  )  	\
)\
)

#define ntohll( x ) htonll( x )

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
	ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 4);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void command_handler(uint8_t *cmd, int len) {
	if (cmd[0] == MOV) {
		unsigned char *x = cmd+1;
		uint64_t l = ntohll(*((uint64_t *)x));
		double j = *((double *)&l);
		unsigned int strength = 4 + j * (10-4);
		ESP_LOGI(TAG, "strength %d", strength);
		if (strength >= 0 && strength < 16) {
			ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, strength);
			ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
		}
	} else {
		ESP_LOGI(TAG, "Unknown cmd");
	}

	/*
	uint8_t c = cmd[0] -'a';
	if (c > 0 && c < 16) {
		ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, c);
		ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
	}
	*/
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
