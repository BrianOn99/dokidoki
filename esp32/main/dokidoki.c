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

#define ESP_INTR_FLAG_DEFAULT 0
#define SPEED_RESOLUTION 7
/*
 * From experiment, number of triggers per 7 full rotations of motor is
 * 7412611 +- 1000 for vertical motor
 * 7408141 +- 1000 for horizontal motor
 * which is close enough to assume the same number:
 * (7412611+7408141)/2/7/(2*PI) ~ 168500
 * There is only 0.8 trigger per arc second, so precision to arc second is meaningless
 */
#define QUANTA_PER_RAD 168500
#define PI 3.141592654

enum _state
{
      STATE_IDLE=0, STATE_GOTO=1,
};

static enum _state state = STATE_IDLE;
static double SPEED_MAX = (1 << 7) - 1;
static uint8_t MOV = 1;
static int theta_quanta = 0;  // telescope elevation
static int phi_quanta = 0;  // telescope horizontal angle
static int to_theta_quanta = 0;  // GOTO target telescope elevation
static int to_phi_quanta = 0;  // GOTO target horizontal angle


struct direction {
	gpio_num_t in_a;
	gpio_num_t in_b;
	gpio_num_t en;
	gpio_num_t rotory_sensor;
	gpio_num_t direction_sensor;
	ledc_channel_t ch;
};

struct direction VERTICAL = {
	.in_a = GPIO_NUM_2,
	.in_b = GPIO_NUM_15,
	.en = GPIO_NUM_4,
	.rotory_sensor = GPIO_NUM_23,
	.direction_sensor = GPIO_NUM_22,
	.ch = LEDC_CHANNEL_0,
};

struct direction HORIZONTAL = {
	.in_a = GPIO_NUM_27,
	.in_b = GPIO_NUM_14,
	.en = GPIO_NUM_13,
	.rotory_sensor = GPIO_NUM_36,
	.direction_sensor = GPIO_NUM_39,
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

/*
 * Up direction is 1, Down is -1
 * Right direction is 1, LEFT is -1
 */
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

static void set_pins_rotation(signed char plus_minus, uint32_t strength, struct direction *d)
{
	if (plus_minus > 0) {
		gpio_set_level(d->in_a, 0);
		gpio_set_level(d->in_b, 1);
	} else {
		gpio_set_level(d->in_a, 1);
		gpio_set_level(d->in_b, 0);
	}
	ledc_set_duty(LEDC_LOW_SPEED_MODE, d->ch, strength);
	ledc_update_duty(LEDC_LOW_SPEED_MODE, d->ch);
}

static void command_handler(uint8_t *cmd, int len)
{
	ESP_LOGI(TAG, "command_handler");
	if (cmd[0] == MOV) {
		uint32_t strength;
		signed char plus_minus;
		//assert(len == sizeof(uint8_t) + sizeof(int32_t) * 2);

		normalize_strength((int32_t *)(cmd+1), &strength, &plus_minus);
		ESP_LOGI(TAG, "strength %d plus_minus %d", strength, plus_minus);
		set_pins_rotation(plus_minus, strength, &HORIZONTAL);

		normalize_strength((int32_t *)(cmd+5), &strength, &plus_minus);
		ESP_LOGI(TAG, "strength %d direction %d", strength, plus_minus);
		set_pins_rotation(plus_minus, strength, &VERTICAL);
	} else {
		ESP_LOGI(TAG, "Unknown cmd");
	}
}

static void IRAM_ATTR rotory_isr_handler(void* arg)
{
	phi_quanta += 1;
	if (state == STATE_GOTO) {
		if (phi_quanta >= to_phi_quanta) {
			set_pins_rotation(0, 0, &HORIZONTAL);
			state = STATE_IDLE;
		}
	}
}

/* params are radian*/
static void rotate(double to_theta, double to_phi)
{
	state = STATE_GOTO;
	to_phi_quanta = to_phi * QUANTA_PER_RAD;
	set_pins_rotation(1, SPEED_MAX, &HORIZONTAL);
}

void app_main()
{
	gpio_config_t io_conf = {};
	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pin_bit_mask = (\
		(1ULL<<VERTICAL.in_a) | (1ULL<<VERTICAL.in_b) | (1ULL<<VERTICAL.en) |\
		(1ULL<<HORIZONTAL.in_a) | (1ULL<<HORIZONTAL.in_b) | (1ULL<<HORIZONTAL.en));
	io_conf.pull_down_en = 0;
	io_conf.pull_up_en = 0;
	gpio_config(&io_conf);

	io_conf.pin_bit_mask = ((1ULL<<VERTICAL.rotory_sensor) | (1ULL<<HORIZONTAL.rotory_sensor));
	io_conf.intr_type = GPIO_INTR_POSEDGE;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_down_en = 1;
	gpio_config(&io_conf);

	io_conf.pin_bit_mask = ((1ULL<<VERTICAL.direction_sensor) | (1ULL<<HORIZONTAL.direction_sensor));
	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_down_en = 1;
	gpio_config(&io_conf);

	bluetooth_init(command_handler);
	ledc_init();
	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	gpio_isr_handler_add(VERTICAL.rotory_sensor, rotory_isr_handler, (void*) &VERTICAL);
	gpio_isr_handler_add(HORIZONTAL.rotory_sensor, rotory_isr_handler, (void*) &HORIZONTAL);

	gpio_set_level(VERTICAL.en, 0);
	gpio_set_level(HORIZONTAL.en, 0);

	//rotate(PI*2, PI*2);
}
