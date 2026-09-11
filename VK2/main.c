// Tavoittelen tehtävästä pistemäärää 3, sillä sain kaikkien pisteiden vaaditut tehtävät toimimaan,
// eli tässä tehtävässä kohdat: Sekvenssin vastaanotto sarjaportista, Lisää sekvenssiin ajastus sekä Refaktoroi ohjelmaa

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

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

// FIFO dispatcher data type
struct data_t {
	void *fifo_reserved;
	char msg[20];
};

struct led_data_t {
    void *fifo_reserved;
    int time;
};

 // Init UART
int init_uart(void) {
	// UART initialization
	if (!device_is_ready(uart_dev)) {
		return 1;
	} 
	return 0;
}

// Led pin configurations
static const struct gpio_dt_spec red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

// LED thread declarations
void red_led_task(void *, void *, void *);
void green_led_task(void *, void *, void *);
void yellow_led_task(void *, void *, void *);

// Thread objects and stacks
K_THREAD_STACK_DEFINE(red_stack, STACKSIZE);
K_THREAD_STACK_DEFINE(green_stack, STACKSIZE);
K_THREAD_STACK_DEFINE(yellow_stack, STACKSIZE);

struct k_thread red_thread;
struct k_thread green_thread;
struct k_thread yellow_thread;

int init_led(void);
int init_uart(void);

static void uart_task(void *, void *, void *);
static void dispatcher_task(void *, void *, void *);

// Main program
int main(void)
{
	int ret; 
	
	ret = init_led();

	if (ret != 0) {
        printk("LED initialization failed!\n");
        return ret;
    }

	ret = init_uart();

	if (ret != 0) {
		printk("UART initialization failed!\n");
		return ret;
	}
	
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

// UART task
static void uart_task(void *unused1, void *unused2, void *unused3)
{
	// Received character from UART
	char rc=0;
	// Message from UART
	char uart_msg[20];
	memset(uart_msg,0,20);
	int uart_msg_cnt = 0;

	while (true) {
		// Ask UART if data available
		if (uart_poll_in(uart_dev,&rc) == 0) {

			// If character is not newline, add to UART message buffer
			if (rc != '\r') {
				uart_msg[uart_msg_cnt] = rc;
				uart_msg_cnt++;
				uart_msg[uart_msg_cnt] = '\0';
			// Character is newline, copy dispatcher data and put to FIFO buffer
			} else {
				printk("UART msg: %s\n", uart_msg);
                
				struct data_t *buf = k_malloc(sizeof(struct data_t));
				if (buf == NULL) {
					return;
				}

				// Copy UART message to dispatcher data
				snprintf(buf->msg, 20, "%s", uart_msg);

				// Put dispatcher data to FIFO buffer
				k_fifo_put(&dispatcher_fifo, buf);

				// Clear UART receive buffer
				uart_msg_cnt = 0;
				memset(uart_msg,0,20);

				// Clear UART message buffer
				uart_msg_cnt = 0;
				memset(uart_msg,0,20);
			}
		}
		k_msleep(10);
	}
	return 0;
}

// Task to handle red led
void red_led_task(void *, void *, void*) 
{
	struct led_data_t *data;
    printk("Red led thread started\n");

    // Get data from FIFO
    data = k_fifo_get(&red_fifo, K_FOREVER);

    // Turn red LED on
    gpio_pin_set_dt(&red, 1);
    printk("Red on\n");

    // Keep LED on for requested time
    k_sleep(K_MSEC(data->time));

    // Turn red LED off
    gpio_pin_set_dt(&red, 0);
    printk("Red off\n");

    // Free FIFO data
    k_free(data);
    printk("Red led thread finished\n");
}

// Task to handle green led
void green_led_task(void *, void *, void*) 
{
	struct led_data_t *data;
    printk("Green led thread started\n");

    // Get data from FIFO
    data = k_fifo_get(&green_fifo, K_FOREVER);

    // Turn green LED on
    gpio_pin_set_dt(&green, 1);
    printk("Green on\n");

    // Keep LED on for requested time
    k_sleep(K_MSEC(data->time));

    // Turn green LED off
    gpio_pin_set_dt(&green, 0);
    printk("Green off\n");

    // Free FIFO data
    k_free(data);
    printk("Green led thread finished\n");
}

// Task to handle yellow led
void yellow_led_task(void *, void *, void*) 
{	
	struct led_data_t *data;
    printk("Yellow led thread started\n");

    // Get data from FIFO
    data = k_fifo_get(&yellow_fifo, K_FOREVER);

    // Yellow = red + green
    gpio_pin_set_dt(&red, 1);
    gpio_pin_set_dt(&green, 1);
    printk("Yellow on\n");

    // Keep LED on for requested time
    k_sleep(K_MSEC(data->time));

    // Turn yellow LED off
    gpio_pin_set_dt(&red, 0);
    gpio_pin_set_dt(&green, 0);
    printk("Yellow off\n");

    // Free FIFO data
    k_free(data);
    printk("Yellow led thread finished\n");
}

// Dispatcher task 
static void dispatcher_task(void *unused1, void *unused2, void *unused3)
{
	while (true) {
		// Receive dispatcher data from uart_task fifo
		struct data_t *rec_item = k_fifo_get(&dispatcher_fifo, K_FOREVER);
		char sequence[20];
		memcpy(sequence,rec_item->msg,20);
		k_free(rec_item);

		printk("Dispatcher: %s\n", sequence);

		// Parse color and time
        char color = sequence[0];
        int time = atoi(sequence + 2);
        printk("Data: %c %d\n", color, time);

        // Create data for LED task
        struct led_data_t *data = k_malloc(sizeof(struct led_data_t));

        if (data == NULL) {
            printk("Memory allocation failed!\n");
            continue;
        }
        data->time = time;

        // Send data to correct LED FIFO and create a corresponding thread
        if (color == 'R') {
            printk("Sending data to RED FIFO\n");
            k_fifo_put(&red_fifo, data);
            k_thread_create(&red_thread,red_stack,K_THREAD_STACK_SIZEOF(red_stack),red_led_task,NULL,NULL,NULL,PRIORITY,0,K_NO_WAIT);

            // Wait until red thread has finished
            k_thread_join(&red_thread, K_FOREVER);

        } else if (color == 'Y') {
            printk("Sending data to YELLOW FIFO\n");
            k_fifo_put(&yellow_fifo, data);
            k_thread_create(&yellow_thread,yellow_stack,K_THREAD_STACK_SIZEOF(yellow_stack),yellow_led_task,NULL,NULL,NULL,PRIORITY,0,K_NO_WAIT);
            
			// Wait until yellow thread has finished
            k_thread_join(&yellow_thread, K_FOREVER);

        } else if (color == 'G') {
            printk("Sending data to GREEN FIFO\n");
            k_fifo_put(&green_fifo, data);
            k_thread_create(&green_thread,green_stack,K_THREAD_STACK_SIZEOF(green_stack),green_led_task,NULL,NULL,NULL,PRIORITY,0,K_NO_WAIT);

            // Wait until green thread has finished
            k_thread_join(&green_thread, K_FOREVER);

        } else {
            printk("Unknown color: %c\n", color);
            k_free(data);
        }
    }
}

K_THREAD_DEFINE(uart_thread,STACKSIZE,uart_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(dis_thread,STACKSIZE,dispatcher_task,NULL,NULL,NULL,PRIORITY,0,0);