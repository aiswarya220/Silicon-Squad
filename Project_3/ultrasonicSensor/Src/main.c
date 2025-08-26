#include <stdint.h>

/* ---------------- RCC base ---------------- */
#define RCC_BASE        0x40023800
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x40))

/* ---------------- GPIO base ---------------- */
#define GPIOA_BASE      0x40020000
#define GPIOB_BASE      0x40020400
#define GPIOC_BASE      0x40020800

#define GPIO_MODER(port)   (*(volatile uint32_t *)((port) + 0x00))
#define GPIO_IDR(port)     (*(volatile uint32_t *)((port) + 0x10))
#define GPIO_ODR(port)     (*(volatile uint32_t *)((port) + 0x14))

/* ---------------- TIM2 ---------------- */
#define TIM2_BASE       0x40000000
#define TIM2_CR1        (*(volatile uint32_t *)(TIM2_BASE + 0x00))
#define TIM2_CNT        (*(volatile uint32_t *)(TIM2_BASE + 0x24))
#define TIM2_PSC        (*(volatile uint32_t *)(TIM2_BASE + 0x28))
#define TIM2_ARR        (*(volatile uint32_t *)(TIM2_BASE + 0x2C))

/* ---------------- Ports ---------------- */
#define PORTA GPIOA_BASE
#define PORTB GPIOB_BASE
#define PORTC GPIOC_BASE

#define DISTANCE_THRESHOLD 10  // Object detection threshold in cm
#define MAX_GREEN_TIME 6000    // Maximum green light duration in ms
#define YELLOW_TIME 2000       // Yellow light duration in ms
#define CYCLE_TIME 10000       // Total cycle time for each lane in ms

/* ---------------- Ultrasonic struct ---------------- */
typedef struct {
	uint32_t trig_port;
	uint8_t trig_pin;
	uint32_t echo_port;
	uint8_t echo_pin;
} HCSR04_t;

HCSR04_t sensors[4] = { { PORTA, 5, PORTA, 6 },   // Sensor 1
		{ PORTA, 7, PORTC, 0 },   // Sensor 2
		{ PORTC, 1, PORTC, 2 },   // Sensor 3
		{ PORTC, 3, PORTA, 4 }    // Sensor 4
};

uint32_t distances[4];

/* ---------------- Traffic Lights ---------------- */
typedef struct {
	uint32_t port;
	uint8_t red, yellow, green;
} Traffic_t;

Traffic_t lanes[4] = { { PORTA, 2, 3, 8 },   // Lane 1
		{ PORTA, 9, 10, 11 }, // Lane 2
		{ PORTB, 2, 3, 4 },   // Lane 3
		{ PORTB, 5, 6, 7 }    // Lane 4
};

/* ---------------- Delay using TIM2 ---------------- */
void delay_us(uint32_t us) {
	TIM2_CNT = 0;
	while (TIM2_CNT < us)
		;
}
void delay_ms(uint32_t ms) {
	while (ms--)
		delay_us(1000);
}

/* ---------------- Init GPIO ---------------- */
void gpio_init(void) {
	RCC_AHB1ENR |= (1 << 0) | (1 << 1) | (1 << 2);

	for (int i = 0; i < 4; i++) {
		GPIO_MODER(sensors[i].trig_port) &= ~(3 << (sensors[i].trig_pin * 2));
		GPIO_MODER(sensors[i].trig_port) |= (1 << (sensors[i].trig_pin * 2));

		GPIO_MODER(sensors[i].echo_port) &= ~(3 << (sensors[i].echo_pin * 2));
	}

	for (int i = 0; i < 4; i++) {
		GPIO_MODER(lanes[i].port) &= ~(3 << (lanes[i].red * 2));
		GPIO_MODER(lanes[i].port) |= (1 << (lanes[i].red * 2));

		GPIO_MODER(lanes[i].port) &= ~(3 << (lanes[i].yellow * 2));
		GPIO_MODER(lanes[i].port) |= (1 << (lanes[i].yellow * 2));

		GPIO_MODER(lanes[i].port) &= ~(3 << (lanes[i].green * 2));
		GPIO_MODER(lanes[i].port) |= (1 << (lanes[i].green * 2));
	}
}

/* ---------------- Init TIM2 ---------------- */
void tim2_init(void) {
	RCC_APB1ENR |= (1 << 0);
	TIM2_PSC = 16 - 1;  // 1MHz tick
	TIM2_ARR = 0xFFFF;
	TIM2_CR1 |= 1;
}

/* ---------------- Ultrasonic read ---------------- */
uint32_t hcsr04_read(HCSR04_t *sensor) {
	uint32_t start_time, end_time;

	GPIO_ODR(sensor->trig_port) &= ~(1 << sensor->trig_pin);
	delay_us(2);
	GPIO_ODR(sensor->trig_port) |= (1 << sensor->trig_pin);
	delay_us(10);
	GPIO_ODR(sensor->trig_port) &= ~(1 << sensor->trig_pin);

	while (!(GPIO_IDR(sensor->echo_port) & (1 << sensor->echo_pin)))
		;
	start_time = TIM2_CNT;

	while (GPIO_IDR(sensor->echo_port) & (1 << sensor->echo_pin))
		;
	end_time = TIM2_CNT;

	uint32_t pulse =
			(end_time >= start_time) ?
					(end_time - start_time) : (0xFFFF - start_time + end_time);
	return pulse / 58;
}

/* ---------------- Traffic light control ---------------- */
void set_light(int lane, int r, int y, int g) {
	if (r)
		GPIO_ODR(lanes[lane].port) |= (1 << lanes[lane].red);
	else
		GPIO_ODR(lanes[lane].port) &= ~(1 << lanes[lane].red);

	if (y)
		GPIO_ODR(lanes[lane].port) |= (1 << lanes[lane].yellow);
	else
		GPIO_ODR(lanes[lane].port) &= ~(1 << lanes[lane].yellow);

	if (g)
		GPIO_ODR(lanes[lane].port) |= (1 << lanes[lane].green);
	else
		GPIO_ODR(lanes[lane].port) &= ~(1 << lanes[lane].green);
}

/* ---------------- Global State ---------------- */
int current_lane = 0;
uint32_t last_served_time = 0;

/* ---------------- Smart Traffic Control ---------------- */
void run_traffic_cycle(void) {
	int priority_lane = -1;
	int next_priority_lane = -1;
	uint32_t current_time = TIM2_CNT;

	// Check for any detected vehicle
	for (int i = 0; i < 4; i++) {
		if (distances[i] <= DISTANCE_THRESHOLD) {
			if (priority_lane == -1
					|| distances[i] < distances[priority_lane]) {
				next_priority_lane = priority_lane; // Store the previous priority lane
				priority_lane = i;
			} else if (next_priority_lane == -1
					|| distances[i] < distances[next_priority_lane]) {
				next_priority_lane = i; // Update the next priority lane
			}
		}
	}

	// Choose lane to serve: either detected vehicle or normal sequence
	int lane_to_serve = (priority_lane != -1) ? priority_lane : current_lane;

	// Check if the current lane has served for too long
	if (current_time - last_served_time >= CYCLE_TIME) {
		lane_to_serve = (current_lane + 1) % 4; // Move to the next lane in sequence
	}

	// Set lights: selected lane green, others red
	for (int i = 0; i < 4; i++) {
		if (i == lane_to_serve)
			set_light(i, 0, 0, 1);
		else
			set_light(i, 1, 0, 0);
	}

	delay_ms(MAX_GREEN_TIME);
	last_served_time = TIM2_CNT; // Update the last served time

	// Yellow for served lane
	set_light(lane_to_serve, 0, 1, 0);
	delay_ms(YELLOW_TIME);
	set_light(lane_to_serve, 1, 0, 0);

	// Update next lane in normal cyclic order
	current_lane = (lane_to_serve + 1) % 4;
}

/* ---------------- Main ---------------- */
int main(void) {
	gpio_init();
	tim2_init();

	// Initial power-up: all lanes red
	for (int i = 0; i < 4; i++)
		set_light(i, 1, 0, 0);
	delay_ms(1000);

	while (1) {
		for (int i = 0; i < 4; i++) {
			distances[i] = hcsr04_read(&sensors[i]);
			delay_ms(50);
		}
		run_traffic_cycle();
	}
}
