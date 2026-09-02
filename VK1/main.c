// Tavoittelen tehtävästä pistemäärää 3, sillä sain kaikkien pisteiden vaaditut tehtävät toimimaan, 
// sekä keltaisen flash jälkeen "liikennevalosarja" alkaa uudestaan, sen sijaan, että menisi jumiin, jonka huomasin olevan ongelma.
// Toki tällä ohjelmalla keltaista ei voi flashata jos pause on painettu, joka mielestäni on syytä kuitenkin tässä mainita.
// Tähän ei kuitenkaan ohjeissa ollut kirjattua vaatimusta, joten oletan sen olevan ihan okei.

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

int tila = 0; // 0 = idle, 1 = red, 2 = yellow, 3 = green, 4 = pause, 5 = flashing yellow
int vanha_tila = 0;
int suunta = 0; // 0 = alas, 1 = ylös

// Led pin configurations
static const struct gpio_dt_spec red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

// Button configurations
#define BUTTON_0 DT_ALIAS(sw0)
static const struct gpio_dt_spec button_0 = GPIO_DT_SPEC_GET_OR(BUTTON_0, gpios, {0});
static struct gpio_callback button_0_data;
#define BUTTON_1 DT_ALIAS(sw1)
static const struct gpio_dt_spec button_1 = GPIO_DT_SPEC_GET_OR(BUTTON_1, gpios, {0});
static struct gpio_callback button_1_data;
#define BUTTON_2 DT_ALIAS(sw2)
static const struct gpio_dt_spec button_2 = GPIO_DT_SPEC_GET_OR(BUTTON_2, gpios, {0});
static struct gpio_callback button_2_data;
#define BUTTON_3 DT_ALIAS(sw3)
static const struct gpio_dt_spec button_3 = GPIO_DT_SPEC_GET_OR(BUTTON_3, gpios, {0});
static struct gpio_callback button_3_data;
#define BUTTON_4 DT_ALIAS(sw4)
static const struct gpio_dt_spec button_4 = GPIO_DT_SPEC_GET_OR(BUTTON_4, gpios, {0});
static struct gpio_callback button_4_data;

// Red led thread initialization
#define STACKSIZE 500
#define PRIORITY 5
void red_led_task(void *, void *, void*);
K_THREAD_DEFINE(red_thread,STACKSIZE,red_led_task,NULL,NULL,NULL,PRIORITY,0,0);

// Green led thread initialization
void green_led_task(void *, void *, void*);
K_THREAD_DEFINE(green_thread,STACKSIZE,green_led_task,NULL,NULL,NULL,PRIORITY,0,0);

// Yellow led thread initialization
void yellow_led_task(void *, void *, void*);
K_THREAD_DEFINE(yellow_thread,STACKSIZE,yellow_led_task,NULL,NULL,NULL,PRIORITY,0,0);

// Flashing yellow initialization
void flashing_yellow_task(void *, void *, void*);
K_THREAD_DEFINE(flashing_yellow_thread,STACKSIZE,flashing_yellow_task,NULL,NULL,NULL,PRIORITY,0,0);

// Flashing yellow led
void flashing_yellow_task(void *, void *, void*) {
    printk("Flashing yellow thread started\n");
    while (true) {
        if (tila == 5) {
            // Yellow ON = red + green ON
            gpio_pin_set_dt(&red, 1);
            gpio_pin_set_dt(&green, 1);
            printk("Flashing yellow ON\n");
            k_sleep(K_SECONDS(1));
            // Yellow OFF
            gpio_pin_set_dt(&red, 0);
            gpio_pin_set_dt(&green, 0);
            printk("Flashing yellow OFF\n");
            k_sleep(K_SECONDS(1));
        }
        k_yield();
    }
}

// Button 0 interrupt handler
void button_0_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Button 1 pressed\n");
	if (tila == 4) {
		// Un-pause, return
		tila = vanha_tila;
	}
	else if (tila >= 1 && tila <= 3) {
		// Pause sequence
		vanha_tila = tila;
		tila = 4;
	}
}

// Button 1 interrupt handler
void button_1_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Button 2 pressed\n");
	// If paused, turn on red led
	if (tila == 4) {
		gpio_pin_toggle_dt(&red);
	}
}

// Button 2 interrupt handler
void button_2_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Button 3 pressed\n");
	// If paused, turn on yellow led
	if (tila == 4) {
		gpio_pin_toggle_dt(&red);
		gpio_pin_toggle_dt(&green);
	}
}

// Button 3 interrupt handler
void button_3_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Button 4 pressed\n");
	// If paused, turn on green led
	if (tila == 4) {
		gpio_pin_toggle_dt(&green);
	}
}

// Button 4 interrupt handler
void button_4_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Button 5 pressed\n");
	if (tila == 5) {
        // Stop flashing yellow
        gpio_pin_set_dt(&red, 0);
        gpio_pin_set_dt(&green, 0);
        // Start normal sequence again from red
        tila = 1;
        suunta = 0;
        printk("Flashing yellow stopped\n");
    }
    else if (tila >= 1 && tila <= 3) {
        // Start flashing yellow
        tila = 5;
        printk("Flashing yellow started\n");
    }
}

// Main program
int main(void)
{
	init_led();

	int ret = init_button();
	if (ret < 0) {
		return 0;
	}

	// init state
	tila = 1;

	return 0;
}

// Initialize leds
int init_led() {

	// Red led pin initialization
	int ret = gpio_pin_configure_dt(&red, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Error: Led configure failed\n");		
		return ret;
	}
	// set led off
	gpio_pin_set_dt(&red,0);

	// Green led pin initialization
	ret = gpio_pin_configure_dt(&green, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Error: Led configure failed\n");		
		return ret;
	}
	// set led off
	gpio_pin_set_dt(&green,0);

	printk("Leds initialized ok\n");
	
	return 0;
}

// Button initialization
int init_button() {

	int ret;
	// BUTTON 0
	if (!gpio_is_ready_dt(&button_0)) {
		printk("Error: button 0 is not ready\n");
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_0, GPIO_INPUT);
	if (ret != 0) {
		printk("Error: failed to configure pin\n");
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button_0, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error: failed to configure interrupt on pin\n");
		return -1;
	}

	gpio_init_callback(&button_0_data, button_0_handler, BIT(button_0.pin));
	gpio_add_callback(button_0.port, &button_0_data);
	printk("Set up button 0 ok\n");

	// BUTTON 1
	if (!gpio_is_ready_dt(&button_1)) {
		printk("Error: button 1 is not ready\n");
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_1, GPIO_INPUT);
	if (ret != 0) {
		printk("Error: failed to configure pin\n");
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button_1, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error: failed to configure interrupt on pin\n");
		return -1;
	}

	gpio_init_callback(&button_1_data, button_1_handler, BIT(button_1.pin));
	gpio_add_callback(button_1.port, &button_1_data);
	printk("Set up button 1 ok\n");

	// BUTTON 2
	if (!gpio_is_ready_dt(&button_2)) {
		printk("Error: button 2 is not ready\n");
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_2, GPIO_INPUT);
	if (ret != 0) {
		printk("Error: failed to configure pin\n");
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button_2, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error: failed to configure interrupt on pin\n");
		return -1;
	}

	gpio_init_callback(&button_2_data, button_2_handler, BIT(button_2.pin));
	gpio_add_callback(button_2.port, &button_2_data);
	printk("Set up button 2 ok\n");

	// BUTTON 3
	if (!gpio_is_ready_dt(&button_3)) {
		printk("Error: button 3 is not ready\n");
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_3, GPIO_INPUT);
	if (ret != 0) {
		printk("Error: failed to configure pin\n");
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button_3, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error: failed to configure interrupt on pin\n");
		return -1;
	}

	gpio_init_callback(&button_3_data, button_3_handler, BIT(button_3.pin));
	gpio_add_callback(button_3.port, &button_3_data);
	printk("Set up button 3 ok\n");

	// BUTTON 4
	if (!gpio_is_ready_dt(&button_4)) {
		printk("Error: button 4 is not ready\n");
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_4, GPIO_INPUT);
	if (ret != 0) {
		printk("Error: failed to configure pin\n");
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button_4, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error: failed to configure interrupt on pin\n");
		return -1;
	}

	gpio_init_callback(&button_4_data, button_4_handler, BIT(button_4.pin));
	gpio_add_callback(button_4.port, &button_4_data);
	printk("Set up button 4 ok\n");
	
	return 0;
}

// Task to handle red led
void red_led_task(void *, void *, void*) {
	
	printk("Red led thread started\n");
	while (true) {
		if (tila == 1) { // 1 = red

			// 1. set led on 
			gpio_pin_set_dt(&red,1);
			printk("Red on\n");
			// 2. sleep for 2 seconds
			k_sleep(K_SECONDS(1));
			// Red off if we are still in green state
            if (tila == 1) {
                gpio_pin_set_dt(&red, 0);
                printk("Red off\n");

                k_sleep(K_SECONDS(1));
            }
			if (tila == 1) {
				suunta = 0;
				tila = 2;
			}
		}
		k_yield();
	} 
}

// Task to handle green led
void green_led_task(void *, void *, void*) {
	
	printk("Green led thread started\n");
	while (true) {
		if (tila == 3) { // 3 = green

			// 1. set led on 
			gpio_pin_set_dt(&green,1);
			printk("Green on\n");
			// 2. sleep for 2 seconds
			k_sleep(K_SECONDS(1));

			// Green off if we are still in green state
            if (tila == 3) {
                gpio_pin_set_dt(&green, 0);
                printk("Green off\n");

                k_sleep(K_SECONDS(1));
            }
			if (tila == 3) {
				suunta = 1;
				tila = 2;
			}
		}
		k_yield();
	}
}

// Task to handle yellow led
void yellow_led_task(void *, void *, void*) 
{	
	printk("Yellow led thread started\n");
	while (true) {
		if (tila == 2) { // 2 = yellow

			// 1. set led on 
			gpio_pin_set_dt(&red, 1);
			gpio_pin_set_dt(&green, 1);
			printk("Yellow on\n");
			// 2. sleep for 2 seconds
			k_sleep(K_SECONDS(1));
			// Yellow off if we are still in green state
            if (tila == 2) {
				gpio_pin_set_dt(&red, 0);
                gpio_pin_set_dt(&green, 0);
                printk("Yellow off\n");

                k_sleep(K_SECONDS(1));
            }
			if (tila == 2) {
        		if (suunta == 0) {
            		tila = 3;
        		}
        		else {
            		tila = 1;
				}
			}
		}
		k_yield();
	}
}
