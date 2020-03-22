/*
 * network_communication.c
 *
 *  Created on: Mar 2, 2020
 *      Author: vadim
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/apps/sntp.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "string.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include <sys/param.h>
#include <esp_http_server.h>

#include "bycicle_speed_project_main.h"

static QueueHandle_t client_queue;
const static int client_queue_size = 10;

#define EXAMPLE_ESP_WIFI_SSID      "MyESP32"
#define EXAMPLE_ESP_WIFI_PASS      "12345678"
#define EXAMPLE_MAX_STA_CONN       2

/* FreeRTOS event group to signal when we are connected*/

static const char *TAG = "wifi softAP";
long double km_per_hour = 0;

void set_km_per_hour(long double kms){
	km_per_hour = kms;
}

static void http_serve(struct netconn *conn)
{
	const static char *TAG = "http_server";
	const static char HTML_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
	const static char ICO_HEADER[] = "HTTP/1.1 200 OK\nContent-type: image/x-icon\n\n";
	const static char MISSING_PAGE[] = "<br><h3>Unknown page or invalid privileges";
	struct netbuf *inbuf;
	static char *buf;
	static uint16_t buflen;
	static err_t err;

	// header
	//extern const uint8_t header_html_start[] asm("_binary_src_html_header_html_start");
	//extern const uint8_t header_html_end[] asm("_binary_src_html_header_html_end");
	//const uint32_t header_html_len = header_html_end - header_html_start;

	// input.html
	//extern const uint8_t input_html_start[] asm("_binary_src_html_input_html_start");
	//extern const uint8_t input_html_end[] asm("_binary_src_html_input_html_end");
	//const uint32_t input_html_len = input_html_end - input_html_start;

	// favicon.ico
	//extern const uint8_t favicon_ico_start[] asm("_binary_src_html_favicon_ico_start");
	//extern const uint8_t favicon_ico_end[] asm("_binary_src_html_favicon_ico_end");
	//const uint32_t favicon_ico_len = favicon_ico_end - favicon_ico_start;

	netconn_set_recvtimeout(conn, 1000); // allow a connection timeout of 1 second
	ESP_LOGI(TAG, "reading from client...");
	err = netconn_recv(conn, &inbuf);
	ESP_LOGI(TAG, "read from client");
	if (err == ERR_OK)
	{
		netbuf_data(inbuf, (void **)&buf, &buflen);
		if (buf)
		{
			// default page
			if (strstr(buf, "GET / "))
			{
				ESP_LOGI(TAG, "Sending /");
				netconn_write(conn, HTML_HEADER, sizeof(HTML_HEADER) - 1, NETCONN_NOCOPY);
				//netconn_write(conn, header_html_start, header_html_len, NETCONN_NOCOPY);
				//netconn_write(conn, input_html_start, input_html_len, NETCONN_NOCOPY);

				netconn_close(conn);
				netconn_delete(conn);
				netbuf_delete(inbuf);
			}
			else if (strstr(buf, "GET /input.html "))
			{
				ESP_LOGI(TAG, "Sending /input.html");
				netconn_write(conn, HTML_HEADER, sizeof(HTML_HEADER) - 1, NETCONN_NOCOPY);
				//(conn, header_html_start, header_html_len, NETCONN_NOCOPY);
				char speed_as_str[6];
				snprintf(speed_as_str, 6, "%Lf", km_per_hour);
				netconn_write(conn, speed_as_str, sizeof(speed_as_str), NETCONN_NOCOPY);
				netconn_close(conn);
				netconn_delete(conn);
				netbuf_delete(inbuf);
			}
			else if (strstr(buf, "GET /favicon.ico "))
			{
				ESP_LOGI(TAG, "Sending favicon.ico");
				netconn_write(conn, ICO_HEADER, sizeof(ICO_HEADER) - 1, NETCONN_NOCOPY);
				//netconn_write(conn, favicon_ico_start, favicon_ico_len, NETCONN_NOCOPY);
				netconn_close(conn);
				netconn_delete(conn);
				netbuf_delete(inbuf);
			}
			else
			{
				ESP_LOGI(TAG, "Unknown request");
				ESP_LOGI(TAG, "%s", buf);
				set_radius_roata(buf);
				netconn_write(conn, HTML_HEADER, sizeof(HTML_HEADER) - 1, NETCONN_NOCOPY);
				//netconn_write(conn, admin_info, admin_info_len, NETCONN_NOCOPY);
				//netconn_write(conn, header_html_start, header_html_len, NETCONN_NOCOPY);
				netconn_write(conn, MISSING_PAGE, sizeof(MISSING_PAGE) - 1, NETCONN_NOCOPY);
				netconn_close(conn);
				netconn_delete(conn);
				netbuf_delete(inbuf);
			}
		}
		else
		{
			ESP_LOGI(TAG, "Unknown request (empty?...)");
			netconn_close(conn);
			netconn_delete(conn);
			netbuf_delete(inbuf);
		}
	}
	else
	{ // if err==ERR_OK
		ESP_LOGI(TAG, "error %d on read, closing connection", err);
		netconn_close(conn);
		netconn_delete(conn);
		netbuf_delete(inbuf);
	}
}

// handles clients when they first connect. passes to a queue
void server_task(void *pvParameters)
{
	const static char *TAG = "server_task";
	struct netconn *conn, *newconn;
	static err_t err;
	client_queue = xQueueCreate(client_queue_size, sizeof(struct netconn *));
	conn = netconn_new(NETCONN_TCP);
	netconn_bind(conn, NULL, 80);
	netconn_listen(conn);
	ESP_LOGI(TAG, "server listening");
	do
	{
		err = netconn_accept(conn, &newconn);
		ESP_LOGI(TAG, "new client");
		if (err == ERR_OK)
		{

			xQueueSendToBack(client_queue, &newconn, portMAX_DELAY);

		}
	} while (err == ERR_OK);
	netconn_close(conn);
	netconn_delete(conn);
	ESP_LOGE(TAG, "task ending, rebooting board");
	esp_restart();
}

void server_handle_task(void *pvParameters)
{
	const static char *TAG = "server_handle_task";
	struct netconn *conn;
	ESP_LOGI(TAG, "task starting");
	for (;;)
	{
		xQueueReceive(client_queue, &conn, portMAX_DELAY);
		if (!conn)
			continue;
		http_serve(conn);
	}
	vTaskDelete(NULL);
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "station:"MACSTR" join, AID=%d",
                 MAC2STR(event->event_info.sta_connected.mac),
                 event->event_info.sta_connected.aid);
        break;
    case SYSTEM_EVENT_AP_STAIPASSIGNED:
        ESP_LOGI(TAG, "SYSTEM_EVENT_AP_STAIPASSIGNED");
        ESP_LOGI(TAG, "Got IP: '%s'",
                ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        /* Start the web server */
        xTaskCreate(&server_task, "server_task", 20000, NULL, 5, NULL);
        xTaskCreate(&server_handle_task, "server_handle_task", 2048, NULL, 5, NULL);
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "station:"MACSTR"leave, AID=%d",
                 MAC2STR(event->event_info.sta_disconnected.mac),
                 event->event_info.sta_disconnected.aid);
        /* Stop the web server */

        break;
    default:
        break;
    }
    return ESP_OK;
}

void wifi_init_softap()
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished.SSID:%s password:%s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}

void network_comm_main()
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();

}
