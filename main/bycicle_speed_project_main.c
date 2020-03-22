#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "network_communication.h"

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"


#define GPIO_PIN_2 2
#define GPIO_PIN_SEL ((1ULL << GPIO_PIN_2))
#define ESP_INTR_FLAG_DEFAULT 0

static xQueueHandle gpio_event_queue = NULL;

//parametri legati de calculul vitezei
long double RADIUS_ROATA = 25;//in cm

void set_radius_roata(char *data){
	char *param = strchr(data, '?')+1;
	char needed_part[4];
	memcpy(needed_part, param, sizeof(needed_part)-1);
	needed_part[sizeof(needed_part)-1] = '\0';

	long double nr = atoi(needed_part);
	printf("am calculat noua valoare pt radius %Lf", nr);
	if(nr>0){
		printf("a venit noua valoare pt radius %Lf", nr);
		RADIUS_ROATA = nr;
	}
}

static void IRAM_ATTR gpio_isr_handler(void* arg)//adauga in queue cand e interrupt
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_event_queue, &gpio_num, NULL);
}

static void gpio_task(void *arg){//citeste din queue
	uint32_t io_num;
	uint32_t counter = 0;
	long double last_time = 0;//since last interrupt(adica o rotire completa a rotii)
	for(;;){
		if(xQueueReceive(gpio_event_queue, &io_num, portMAX_DELAY)){
			vTaskDelay(150 / portTICK_PERIOD_MS);//debouncing pentru buton
			if(gpio_get_level(io_num)==1){
				long double time_in_sec  = ((xTaskGetTickCount()-last_time)*portTICK_PERIOD_MS)/1000;
				if(time_in_sec>0){
					long double viteza_unghiulara = 2*3.14/time_in_sec;//unghi pe timp, rad/sec
					printf("viteza_unghiulara rad/sec %Lf \n", viteza_unghiulara);
					long double radius_in_km = RADIUS_ROATA/100000;//1km = 100000cm
					printf("radius_in_km %Lf \n", radius_in_km);
					long double viteza_liniara = viteza_unghiulara*radius_in_km;//va fi in km/sec
					printf("viteza_liniara km/sec %Lf \n", viteza_liniara);
					viteza_liniara = viteza_liniara*3600;//va fi in km/hour
					printf("%d time since last interrupt: %Lf\n", counter, time_in_sec);
					printf("km per hour %Lf \n", viteza_liniara);
					set_km_per_hour(viteza_liniara);
				}
				last_time = xTaskGetTickCount();
				//printf("%d: interrupt on GPIO[%d] = %d\n", counter, io_num, gpio_get_level(io_num));
				counter++;
			}
		}
	}
}

void app_main(){
	//configurare gpio
	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_POSEDGE;//interrupt of rising edge
	io_conf.pin_bit_mask = GPIO_PIN_SEL;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_down_en = 1;
	gpio_config(&io_conf);

	//queue pentru event de intrerupere
	gpio_event_queue = xQueueCreate(10 ,sizeof(uint32_t));
	xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);

	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	gpio_isr_handler_add(GPIO_PIN_2, gpio_isr_handler, (void*)GPIO_PIN_2);
	WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
	network_comm_main();
}
