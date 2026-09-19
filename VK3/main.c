// Tavoittelen tehtävästä pistemäärää 2, sillä sain 2 pisteiden vaaditut tehtävät toimimaan,
// eli tässä tehtävässä kohdat: Ajoitukset liikennevaloihin, sekä Lisää debugtietoa.

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/timing/timing.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG

// Thread initializations
#define STACKSIZE 500
#define PRIORITY 5

// UART initialization
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

// Create dispatcher FIFO buffer
K_FIFO_DEFINE(dispatcher_fifo);
K_FIFO_DEFINE(red_fifo);
K_FIFO_DEFINE(green_fifo);
K_FIFO_DEFINE(yellow_fifo);
K_FIFO_DEFINE(data_fifo);

// Data received from UART
struct data_t {
	void *fifo_reserved;
	char msg[50];
};

// Data sent to LED threads 
struct led_data_t {
    void *fifo_reserved;
    int time;
};

// States
int tila = 0; // 0 = idle, 1 = red, 2 = yellow, 3 = green, 4 = pause, 5 = flashing yellow
int vanha_tila = 0;
int suunta = 0; // 0 = alas, 1 = ylös


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


// LED configurations
static const struct gpio_dt_spec red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);


// Function declarations
int init_led(void);
int init_button(void);
int init_uart(void);

// LED thread declarations
void red_led_task(void *, void *, void *);
void green_led_task(void *, void *, void *);
void yellow_led_task(void *, void *, void *);
void flashing_yellow_task(void *, void *, void *);

// Debug task declaration
void debug_task(void *, void *, void *);

// UART / dispatcher threads
static void uart_task(void *, void *, void *);
static void dispatcher_task(void *, void *, void *);


// Thread objects and stacks
K_THREAD_STACK_DEFINE(red_stack, STACKSIZE);
K_THREAD_STACK_DEFINE(green_stack, STACKSIZE);
K_THREAD_STACK_DEFINE(yellow_stack, STACKSIZE);
K_THREAD_STACK_DEFINE(flashing_yellow_stack, STACKSIZE);

K_THREAD_STACK_DEFINE(uart_stack, STACKSIZE);
K_THREAD_STACK_DEFINE(dispatcher_stack, STACKSIZE);
K_THREAD_DEFINE(debug_thread,STACKSIZE,debug_task,NULL,NULL,NULL,PRIORITY,0,0);


struct k_thread red_thread;
struct k_thread green_thread;
struct k_thread yellow_thread;

// Button 0 handler
void button_0_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Button 1 pressed\n");
	if (tila == 4) {
		// Un-pause, return
		tila = vanha_tila;

        printk("Sequence continued\n");
	}
	else if (tila >= 1 && tila <= 3) {
		// Pause sequence
		vanha_tila = tila;
		tila = 4;

        // Turn both LEDs off when paused
        gpio_pin_set_dt(&red, 0);
        gpio_pin_set_dt(&green, 0);

        printk("Sequence paused\n");
	}
}

// Button 1 handler
void button_1_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Button 2 pressed\n");
	// If paused, turn on red led
	if (tila == 4) {
		gpio_pin_toggle_dt(&red);
        printk("Manual red toggle\n");
	}
}

// Button 2 handler
void button_2_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Button 3 pressed\n");
	// If paused, turn on yellow led
	if (tila == 4) {
		gpio_pin_toggle_dt(&red);
		gpio_pin_toggle_dt(&green);
        printk("Manual yellow toggle\n");
	}
}

// Button 3 handler
void button_3_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Button 4 pressed\n");
	// If paused, turn on green led
	if (tila == 4) {
		gpio_pin_toggle_dt(&green);
        printk("Manual green toggle\n");
	}
}

// Button 4 handler
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
        gpio_pin_set_dt(&red, 0);
        gpio_pin_set_dt(&green, 0);
        printk("Flashing yellow started\n");
    }
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


// Init UART
int init_uart(void) {
	// UART initialization
	if (!device_is_ready(uart_dev)) {
		return 1;
	} 
	return 0;
}


// Main program
int main(void)
{
	timing_init();
	timing_start();
	timing_t start_time = timing_counter_get();

	int ret; 
	
	ret = init_led();
	if (ret != 0) {
        printk("LED initialization failed!\n");
        return ret;
    }

	k_msleep(100);

	ret = init_uart();
	if (ret != 0) {
		printk("UART initialization failed!\n");
		return ret;
	}

	k_msleep(100);

    ret = init_button();
    if (ret != 0) {
        printk("Button initialization failed!\n");
        return ret;
    }

	k_msleep(100);

    // Init state
    tila = 1;
    suunta = 0;

	//printk("System initialized\n");

	timing_t end_time = timing_counter_get();
	timing_stop();
    uint64_t timing_ns = timing_cycles_to_ns(timing_cycles_get(&start_time, &end_time));
	//printk("Initialization: %lld\n", timing_ns);

	while (true) {
		k_msleep(100);
	}

	return 0;
}


// UART task
static void uart_task(void *unused1, void *unused2, void *unused3)
{
	// Received character from UART
	char rc=0;

	// Message from UART
	char uart_msg[50];
	memset(uart_msg,0,sizeof(uart_msg));
	int uart_msg_cnt = 0;

	while (true) {
		// Ask UART if data available
		if (uart_poll_in(uart_dev,&rc) == 0) {

			// If character is not newline, add to UART message buffer
			if (rc != '\r') {
			//Prevent buffer overflow
                if (uart_msg_cnt < (sizeof(uart_msg) - 1)) { 
                    uart_msg[uart_msg_cnt] = rc; 
                    uart_msg_cnt++; 
                    uart_msg[uart_msg_cnt] = '\0'; 
                }
			// Character is newline, copy dispatcher data and put to FIFO buffer
			} else {
				//printk("UART msg: %s\n", uart_msg);
                
				struct data_t *buf = k_malloc(sizeof(struct data_t));

				if (buf == NULL) {
					//printk("Memory allocation failed!\n");

                    uart_msg_cnt = 0;
                    memset(uart_msg, 0, sizeof(uart_msg));

                    continue;
				}

				// Copy UART message to dispatcher data
				snprintf(buf->msg, 50, "%s", uart_msg);

				// Put dispatcher data to FIFO buffer
				k_fifo_put(&dispatcher_fifo, buf);

				// Clear UART receive buffer
				uart_msg_cnt = 0;
				memset(uart_msg,0,sizeof(uart_msg));

				// Clear UART message buffer
				uart_msg_cnt = 0;
				memset(uart_msg,0,sizeof(uart_msg));
			}
		}
		k_msleep(10);
	}
}

// Task to handle red led
void red_led_task(void *, void *, void*) 
{
	timing_start();
	timing_t red_start_time = timing_counter_get();

	struct led_data_t *data;

    //printk("Red led thread started\n");

    // Wait to get data from red FIFO
    data = k_fifo_get(&red_fifo, K_FOREVER);

    // Turn red LED on
    gpio_pin_set_dt(&red, 1);
    //printk("Red on\n");

    // Keep LED on for requested time
    k_sleep(K_MSEC(data->time));

    // Turn red LED off
    gpio_pin_set_dt(&red, 0);
    //printk("Red off\n");

	k_sleep(K_SECONDS(1));

	// measure task duration
	timing_t red_end_time = timing_counter_get();
	timing_stop();
    uint64_t diff = timing_cycles_to_ns(timing_cycles_get(&red_start_time, &red_end_time)/1000);
	
	data->time = diff;
	k_fifo_put(&data_fifo, data);

	//uint64_t timing_ms = timing_ns / 1000;
	//printk("Red task: %lld\n", timing_ms);

    // Free FIFO data
    k_free(data);
    //printk("Red led thread finished\n");
	k_yield();
}

// Task to handle green led
void green_led_task(void *, void *, void*) 
{
	timing_start();
	timing_t green_start_time = timing_counter_get();

	struct led_data_t *data;

    //printk("Green led thread started\n");

    // Wait to get data from green FIFO
    data = k_fifo_get(&green_fifo, K_FOREVER);

    // Turn green LED on
    gpio_pin_set_dt(&green, 1);
    //printk("Green on\n");

    // Keep LED on for requested time
    k_sleep(K_MSEC(data->time));

    // Turn green LED off
    gpio_pin_set_dt(&green, 0);
    //printk("Green off\n");

	k_sleep(K_SECONDS(1));

	// measure task duration
	timing_t green_end_time = timing_counter_get();
	timing_stop();
    uint64_t diff = timing_cycles_to_ns(timing_cycles_get(&green_start_time, &green_end_time)/1000);

	data->time = diff;
	k_fifo_put(&data_fifo, data);
	
	
	//uint64_t timing_ms = timing_ns / 1000;
	//printk("Green task: %lld\n", timing_ms);

    // Free FIFO data
    k_free(data);
    //printk("Green led thread finished\n");
	k_yield();
}

// Task to handle yellow led
void yellow_led_task(void *, void *, void*) 
{	
	timing_start();
	timing_t yellow_start_time = timing_counter_get();

	struct led_data_t *data;

    //printk("Yellow led thread started\n");

    // Wait to get data from yellow FIFO
    data = k_fifo_get(&yellow_fifo, K_FOREVER);

    // Yellow = red + green
    gpio_pin_set_dt(&red, 1);
    gpio_pin_set_dt(&green, 1);
    //printk("Yellow on\n");

    // Keep LED on for requested time
    k_sleep(K_MSEC(data->time));

    // Turn yellow LED off
    gpio_pin_set_dt(&red, 0);
    gpio_pin_set_dt(&green, 0);
    //printk("Yellow off\n");

	k_sleep(K_SECONDS(1));

	// measure task duration
	timing_t yellow_end_time = timing_counter_get();

	timing_stop();
    uint64_t diff = timing_cycles_to_ns(timing_cycles_get(&yellow_start_time, &yellow_end_time)/1000);
	
	data->time = diff;
	k_fifo_put(&data_fifo, data); 

	// uint64_t timing_ms = timing_ns / 1000;
	// printk("Yellow task: %lld\n", timing_ms);

    // Free FIFO data
    k_free(data);
    //printk("Yellow led thread finished\n");
	k_yield();
}

// Task to handle flashing yellow led
void flashing_yellow_task(void *, void *, void *)
{
    printk("Flashing yellow thread started\n");

    // Yellow on
    gpio_pin_set_dt(&red, 1);
    gpio_pin_set_dt(&green, 1);

    printk("Flashing yellow ON\n");

    k_sleep(K_SECONDS(1));

    // Yellow off
    gpio_pin_set_dt(&red, 0);
    gpio_pin_set_dt(&green, 0);

    printk("Flashing yellow OFF\n");

    printk("Flashing yellow thread finished\n");
}

// Dispatcher task 
static void dispatcher_task(void *unused1, void *unused2, void *unused3)
{
	while (true) {
		// Receive dispatcher data from uart_task fifo
		struct data_t *rec_item = k_fifo_get(&dispatcher_fifo, K_FOREVER);

        if (rec_item == NULL) {
            continue;
        }

		char sequence[20];
		memcpy(sequence,rec_item->msg,sizeof(sequence));
        sequence[sizeof(sequence) - 1] = '\0';

		k_free(rec_item);

		//printk("Dispatcher: %s\n", sequence);

		char *token = strtok(sequence, ",");

        while (token != NULL) {
            char color = token[0];

            // Get time
            token = strtok(NULL, ",");

            if (token == NULL) {
                printk("Missing time\n");
                break;
            }

            int time = atoi(token);

            //printk("Data: %c %d\n",color,time);

            // Allocate LED data
            struct led_data_t *data = k_malloc(sizeof(struct led_data_t));

            if (data == NULL) {
                printk("Memory allocation failed!\n");
                break;
            }

            data->time = time;

            // Red
            if (color == 'R') {
                //printk("Sending data to RED FIFO\n");
                k_fifo_put(&red_fifo,data);

                // Create red thread
                k_thread_create(&red_thread,red_stack,K_THREAD_STACK_SIZEOF(red_stack),red_led_task,NULL,NULL,NULL,PRIORITY,0,K_NO_WAIT);

                // Wait until red thread finishes
                k_thread_join(&red_thread,K_FOREVER);
            }

            // Yellow
            else if (color == 'Y') {
                //printk("Sending data to YELLOW FIFO\n");
                k_fifo_put(&yellow_fifo,data);

                // Create yellow thread
                k_thread_create(&yellow_thread,yellow_stack,K_THREAD_STACK_SIZEOF(yellow_stack),yellow_led_task,NULL,NULL,NULL,PRIORITY,0,K_NO_WAIT);

                // Wait until yellow thread finishes
                k_thread_join(&yellow_thread,K_FOREVER);
            }

            // Green
            else if (color == 'G') {
                //printk("Sending data to GREEN FIFO\n");
                k_fifo_put(&green_fifo,data);

                // Create green thread
                k_thread_create(&green_thread,green_stack,K_THREAD_STACK_SIZEOF(green_stack),green_led_task,NULL,NULL,NULL,PRIORITY,0,K_NO_WAIT);

                // Wait until green thread finishes
                k_thread_join(&green_thread, K_FOREVER);
            }

            // UNKNOWN COLOR
            else {
                //printk("Unknown color: %c\n", color);
                k_free(data);
            }

            // Get next color from sequence
            token = strtok(NULL, ",");
        }
    }
}

// Debug task
void debug_task(void *, void *, void*) 
{
	// Store received data
	struct led_data_t *received;

	while (true) {
		received = k_fifo_get(&data_fifo, K_FOREVER);
		printk("Debug received: %ld\n", received->time);
		k_free(received);

		k_yield();
	}
}

K_THREAD_DEFINE(uart_thread,STACKSIZE,uart_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(dis_thread,STACKSIZE,dispatcher_task,NULL,NULL,NULL,PRIORITY,0,0);