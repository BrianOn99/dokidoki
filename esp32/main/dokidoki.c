#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "driver/ledc.h"
#include "bluetooth.h"
#include "esp_log.h"
#include "esp_spp_api.h"
#include "takidoki.h"

#define TAG "DOKI"

#define htonl(ulIn)\
(\
( uint32_t )\
(\
( ( ( ( uint32_t ) ( ulIn ) )                ) << 24  ) | 	\
( ( ( ( uint32_t ) ( ulIn ) ) & 0x0000ff00UL ) <<  8  ) | 	\
( ( ( ( uint32_t ) ( ulIn ) ) & 0x00ff0000UL ) >>  8  ) | 	\
( ( ( ( uint32_t ) ( ulIn ) )                ) >> 24  )  	\
)\
)
#define ntohl(x) htonl(x)

#define htons(x) ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8))
#define ntohs(x) htons(x)

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
#define QUANTA_PER_REV 1171814
#define NA_QUANTA (QUANTA_PER_RAD * 100)
#define MOVE 1
#define ALIGN 2
#define GOTO 3
#define ZERO 4
#define GET_THETA_PHI 5
#define TRACK 6

static double SPEED_MAX = (1 << 7) - 1;

/*
 * each 0-2PI is mapped to 0-INT16_MAX
 */
struct align_cmd_star {
	uint32_t time;
	uint16_t phi;
	uint16_t theta;
	uint16_t asc;
	uint16_t dec;
};

#pragma pack(1)
struct align_cmd {
	uint8_t cmd;
	struct align_cmd_star star1;
	struct align_cmd_star star2;
};

#pragma pack(1)
struct goto_cmd {
	uint8_t cmd;
	uint32_t time;
	uint16_t asc;
	uint16_t dec;
};

struct position_reply {
	uint8_t reply;
	uint32_t time;
	uint16_t phi;
	uint16_t theta;
};

struct direction {
	//char name[2];
	gpio_num_t in_a;
	gpio_num_t in_b;
	gpio_num_t en;
	gpio_num_t rotory_sensor;
	gpio_num_t direction_sensor;
	ledc_channel_t ch;
	int quanta; // telescope elevation / horizontal angle
	int to_quanta; // GOTO target telescope elevation / horizontal angle
};

struct direction VERTICAL = {
	//.name = "V",
	.in_a = GPIO_NUM_2,
	.in_b = GPIO_NUM_15,
	.en = GPIO_NUM_4,
	.rotory_sensor = GPIO_NUM_23,
	.direction_sensor = GPIO_NUM_22,
	.ch = LEDC_CHANNEL_0,
};

struct direction HORIZONTAL = {
	//.name = "H",
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

static void set_pins_rotation(signed char plus_minus, uint32_t strength, struct direction *axis)
{
	if (strength == 0)
		axis->to_quanta = NA_QUANTA;

	if (plus_minus > 0) {
		gpio_set_level(axis->in_a, 0);
		gpio_set_level(axis->in_b, 1);
	} else if (plus_minus <0) {
		gpio_set_level(axis->in_a, 1);
		gpio_set_level(axis->in_b, 0);
	} else {
		gpio_set_level(axis->in_a, 0);
		gpio_set_level(axis->in_b, 0);
	}
	gpio_set_level(HORIZONTAL.en, 1);
	gpio_set_level(VERTICAL.en, 1);

	ledc_set_duty(LEDC_LOW_SPEED_MODE, axis->ch, strength);
	ledc_update_duty(LEDC_LOW_SPEED_MODE, axis->ch);
}

static double u16r(uint16_t x)
{
	return ((double) ntohs(x)) / UINT16_MAX * 2 * M_PI;
}

static double qu16(int x)
{
	double y = ((double) x) / QUANTA_PER_REV * UINT16_MAX;
	return htons((uint16_t) y);
}

/* params are radian*/
static void rotate(double to_theta, double to_phi)
{
	struct direction * axises[2] = {&HORIZONTAL, &VERTICAL};

	HORIZONTAL.to_quanta = to_phi * QUANTA_PER_RAD;
	VERTICAL.to_quanta = to_theta * QUANTA_PER_RAD;
	printf("HORIZONTAL quanta %d VERTICAL quanta %d\n", HORIZONTAL.quanta, VERTICAL.quanta);
	printf("HORIZONTAL to %d VERTICAL to %d\n", HORIZONTAL.to_quanta, VERTICAL.to_quanta);
	for (int i=0; i<2;i++) {
		int d;
		if (axises[i]->to_quanta > axises[i]->quanta) {
			d = 1;
		} else if (axises[i]->to_quanta < axises[i]->quanta) {
			d = -1;
		} else {
			continue;
		}
		if (i == 0)
			d = d * -1;

		set_pins_rotation(d, SPEED_MAX, axises[i]);
	}
}

static void command_handler(esp_spp_cb_param_t *param)
{
	uint32_t strength;
	signed char plus_minus;
	uint8_t *cmd = param->data_ind.data;
	int len = param->data_ind.len;

	switch (cmd[0]) {
	case MOVE:
		if (len < 9) {
			break;
		}
		//assert(len == sizeof(uint8_t) + sizeof(int32_t) * 2);

		normalize_strength((int32_t *)(cmd+1), &strength, &plus_minus);
		set_pins_rotation(plus_minus, strength, &HORIZONTAL);

		normalize_strength((int32_t *)(cmd+5), &strength, &plus_minus);
		set_pins_rotation(plus_minus, strength, &VERTICAL);
		break;
	case ALIGN:
		if (len < sizeof(struct align_cmd)) {
			break;
		}
		struct align_cmd *c = (struct align_cmd *) cmd;
		struct align_cmd_star *s1 = &c->star1;
		struct align_cmd_star *s2 = &c->star2;
		struct ref_star stars[2] = {
			{ntohl(s1->time), u16r(s1->phi), u16r(s1->theta), u16r(s1->asc), u16r(s1->dec)},
			{ntohl(s2->time), u16r(s2->phi), u16r(s2->theta), u16r(s2->asc), u16r(s2->dec)},
		};
		align(stars);
		break;
	case GOTO:
		if (len < sizeof(struct goto_cmd)) {
			break;
		}
		struct goto_cmd *d = (struct goto_cmd *) cmd;
		double phi, theta;
		struct celestial_star target_star = {
			.asc = u16r(d->asc),
			.dec = u16r(d->dec),
			.time = ntohl(d->time),
		};
		eq2tel(&phi, &theta, &target_star);
		printf("TELESCOPE DIRECTION (RAD): %'.5f\n", phi);
		printf("TELESCOPE ELEVATION (RAD): %'.5f\n", theta);
		if (isnan(theta) || isnan(phi))
			break;
		rotate(theta, phi);
		break;
	case ZERO:
		HORIZONTAL.quanta = 0;
		VERTICAL.quanta = QUANTA_PER_REV / 4;
		break;
	case GET_THETA_PHI:
		;
		int64_t microsec = esp_timer_get_time();
		struct position_reply reply = {
			.reply = GET_THETA_PHI,
			.time = htonl((int)(microsec / 1000000)),
			.phi = qu16(HORIZONTAL.quanta),
			.theta = qu16(VERTICAL.quanta),
		};
		esp_spp_write(param->open.handle, sizeof(struct position_reply), (uint8_t*) &reply);
		break;
	default:
		ESP_LOGI(TAG, "Unknown cmd");
	}
}

static void IRAM_ATTR rotory_isr_handler(struct direction *axis)
{
	int level = gpio_get_level(axis->direction_sensor);
	if (axis == &VERTICAL)
		axis->quanta += level * 2 - 1;
	else
		axis->quanta -= level * 2 - 1;

	if (axis->to_quanta != NA_QUANTA && axis->quanta == axis->to_quanta) {
		set_pins_rotation(0, 0, axis);
	}
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
	gpio_isr_handler_add(
		VERTICAL.rotory_sensor, (void (*)(void *)) rotory_isr_handler, (void*) &VERTICAL);
	gpio_isr_handler_add(
		HORIZONTAL.rotory_sensor, (void (*)(void *)) rotory_isr_handler, (void*) &HORIZONTAL);

	gpio_set_level(VERTICAL.en, 0);
	gpio_set_level(HORIZONTAL.en, 0);

	//rotate(-M_PI/10, 0);

	//struct align_cmd_star s1 = {2352, 7264, 7264, 14412, 8374};
	//struct align_cmd_star s2 = {2418, 17221, 6590, 6910, 16250};
	//struct align_cmd cmd = {'\x02', s1, s2};
	//command_handler(&cmd, 99);
}
