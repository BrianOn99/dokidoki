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

#define SPEED_RESOLUTION 7

static double SPEED_MAX = (1 << 7) - 1;
static uint8_t MOV = 1;

struct direction {
	gpio_num_t in_a;
	gpio_num_t in_b;
	gpio_num_t en;
	ledc_channel_t ch;
};

struct direction VERTICAL = {
	.in_a = GPIO_NUM_2,
	.in_b = GPIO_NUM_15,
	.en = GPIO_NUM_4,
	.ch = LEDC_CHANNEL_0,
};

struct direction HORIZONTAL = {
	.in_a = GPIO_NUM_27,
	.in_b = GPIO_NUM_14,
	.en = GPIO_NUM_13,
	.ch = LEDC_CHANNEL_1,
};

static void ledc_init(void)
{
	ledc_channel_config_t ledc_channel = {
		.channel    = VERTICAL.ch,
		.duty       = 0,
		.gpio_num   = VERTICAL.en,
		.speed_mode = LEDC_LOW_SPEED_MODE,
		.hpoint     = 0,
		.timer_sel  = LEDC_TIMER_1
	};
	ledc_channel_config(&ledc_channel);

	ledc_channel.channel = HORIZONTAL.ch;
	ledc_channel.gpio_num = HORIZONTAL.en;
	ledc_channel_config(&ledc_channel);

	ledc_timer_config_t ledc_timer = {
		.duty_resolution = SPEED_RESOLUTION, // resolution of PWM duty
		.freq_hz = 100,                      // frequency of PWM signal
		.speed_mode = LEDC_LOW_SPEED_MODE,           // timer mode
		.timer_num = LEDC_TIMER_1,            // timer index
		.clk_cfg = LEDC_AUTO_CLK,              // Auto select the source clock
	};
	ledc_timer_config(&ledc_timer);

	ledc_set_duty(LEDC_LOW_SPEED_MODE, VERTICAL.ch, 0);
	ledc_update_duty(LEDC_LOW_SPEED_MODE, VERTICAL.ch);
	ledc_set_duty(LEDC_LOW_SPEED_MODE, HORIZONTAL.ch, 0);
	ledc_update_duty(LEDC_LOW_SPEED_MODE, HORIZONTAL.ch);
}

static void normalize_strength(int32_t *x, uint32_t *strength, signed char *direction)
{
	uint32_t l;
	int32_t k = (int32_t) ntohl(*x);
	if (k < 0) {
		l = -k;
		*direction = -1;
	} else {
		l = k;
		*direction = 1;
	}
	*strength = (uint32_t)(SPEED_MAX * l / INT32_MAX);
}

static void set_direction(signed char direction, struct direction *d)
{
	if (direction > 0) {
		ESP_LOGI(TAG, "PIN %d LEVEL 0, PIN %d LEVEL 1", d->in_a, d->in_b);
		gpio_set_level(d->in_a, 0);
		gpio_set_level(d->in_b, 1);
	} else {
		ESP_LOGI(TAG, "PIN %d LEVEL 1, PIN %d LEVEL 0", d->in_a, d->in_b);
		gpio_set_level(d->in_a, 1);
		gpio_set_level(d->in_b, 0);
	}
}

static void command_handler(uint8_t *cmd, int len)
{
	ESP_LOGI(TAG, "command_handler");
	if (cmd[0] == MOV) {
		uint32_t strength;
		signed char direction;
		//assert(len == sizeof(uint8_t) + sizeof(int32_t) * 2);

		normalize_strength((int32_t *)(cmd+1), &strength, &direction);
		ESP_LOGI(TAG, "strength %d direction %d", strength, direction);
		set_direction(direction, &HORIZONTAL);
		ledc_set_duty(LEDC_LOW_SPEED_MODE, HORIZONTAL.ch, strength);
		ledc_update_duty(LEDC_LOW_SPEED_MODE, HORIZONTAL.ch);

		normalize_strength((int32_t *)(cmd+5), &strength, &direction);
		ESP_LOGI(TAG, "strength %d direction %d", strength, direction);
		set_direction(direction, &VERTICAL);
		ledc_set_duty(LEDC_LOW_SPEED_MODE, VERTICAL.ch, strength);
		ledc_update_duty(LEDC_LOW_SPEED_MODE, VERTICAL.ch);
	} else {
		ESP_LOGI(TAG, "Unknown cmd");
	}
}

void app_main()
{
	gpio_num_t out_pins[6] = {
		VERTICAL.in_a,
		VERTICAL.in_b,
		VERTICAL.en,
		HORIZONTAL.in_a,
		HORIZONTAL.in_b,
		HORIZONTAL.en
	};

	for (int i=0; i<sizeof(out_pins)/sizeof(gpio_num_t); i++) {
		gpio_pad_select_gpio(out_pins[i]);
		gpio_set_direction(out_pins[i], GPIO_MODE_OUTPUT);
	}

	bluetooth_init(command_handler);
	ledc_init();
	gpio_set_level(VERTICAL.en, 0);
	gpio_set_level(HORIZONTAL.en, 0);
}
