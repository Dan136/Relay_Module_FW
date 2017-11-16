#include <FreeRTOS.h>
#include <task.h>
#include <platform_stdlib.h>
#include <semphr.h>
#include "sensor_wifi.h"
#include "wifi_constants.h"
#include "wifi_structures.h"
#include "lwip_netconf.h"
#include "wifi_conf.h"
#include "netif.h"
#include <lwip/sockets.h>
//#include <osdep_api.h>
#include <osdep_service.h>
//Includes for LED
#include "device.h"
#include "gpio_api.h"

#define GPIO_LED_PIN       PC_3

static SemaphoreHandle_t wifiBufSem = 0;
static char wifiBuf[50];
static char wifiBufSendFlag;

char* ssid;
char* password;

static void wifi_socket_thread(void *param)
{
	int loops = 0;
	while (1)
	{
		//printf("WiFi loop: %d\n",loops);
		loops++;
		vTaskDelay(10000);
	}
}

void start_sensor_wifi()
{
	connect_to_network();
	wifiBufSem = xSemaphoreCreateMutex(); // initialize semaphore
	if(xTaskCreate(wifi_socket_thread, ((const char*)"WiFi Socket Thread"), 2048, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
			printf("\n\r%s xTaskCreate(wifi_socket_thread) failed", __FUNCTION__);
	vTaskDelete(NULL);
}

int connect_to_network()
{
	gpio_t gpio_led;
	gpio_init(&gpio_led, GPIO_LED_PIN);
	gpio_dir(&gpio_led, PIN_OUTPUT);    // Direction: Output
	gpio_mode(&gpio_led, PullNone);     // No pull
	gpio_write(&gpio_led, 0);
	wifi_on(RTW_MODE_STA); //Resets wifi so we dont get errors
	wifi_off(); //Resets wifi to avoid errors due to warm start
	if(wifi_on(RTW_MODE_STA) < 0){
		printf("\n\r[WIFI] ERROR: wifi_on failed\n");
		return 0;
	}
	vTaskDelay(5000);
	// Set the auto-reconnect mode with retry 1 time(limit is 2) and timeout 5 seconds.
	// This command need to be set before invoke wifi_connect() to make reconnection work.
	wifi_config_autoreconnect(1, 2, 5);
	// Connect to AP with PSK-WPA2-AES.
	ssid = "WLAN_Test";
	password = "testtest";
	if(wifi_connect(ssid, RTW_SECURITY_WPA2_AES_PSK, password, strlen(ssid), strlen(password), -1, NULL) == RTW_SUCCESS)
	{
		printf("WiFi Connected!");
		LwIP_DHCP(0, DHCP_START);
		gpio_write(&gpio_led, 1);
		vTaskDelay(2000);
	}
	else {
		printf("Error connecting to WIFI!");
		gpio_write(&gpio_led, 0);
		return 0;
	}
	return 1;
}

BaseType_t write_wifi_buffer(const char * dataStart){
	BaseType_t retValue = pdFAIL;
	if(wifiBufSem){ // only run with initialized semaphore
		retValue = xSemaphoreTake(wifiBufSem, 0);
		if(retValue == pdPASS){ // copy data and return semaphore if semaphore was taken
			strcpy(wifiBuf, dataStart);
			wifiBufSendFlag = 1;
			xSemaphoreGive(wifiBufSem);
		}
	}
	return retValue;

}


#define SERVER_PORT     80
#define LISTEN_QLEN     2

static int tx_exit = 0;
//static _Sema tcp_tx_rx_sema;
static _sema tcp_tx_rx_sema;

static void tx_thread(void *param)
{
	int client_fd = * (int *) param;
	printf("\n%s start\n", __FUNCTION__);
	while(1) {
		int ret = 0;

		//RtlDownSema(&tcp_tx_rx_sema);
		rtw_down_sema(&tcp_tx_rx_sema);
		if(wifiBufSem){ // only run with initialized semaphore
			if(xSemaphoreTake(wifiBufSem, 0) == pdPASS){ // send data and return semaphore if semaphore was taken
				if(wifiBufSendFlag){
					ret = send(client_fd, wifiBuf, strlen(wifiBuf), 0);
				}
				xSemaphoreGive(wifiBufSem);
			}
		}

		//RtlUpSema(&tcp_tx_rx_sema);
		rtw_up_sema(&tcp_tx_rx_sema);
		if(ret <= 0){
			goto exit;
		}
		vTaskDelay(1000);
	}

exit:
	printf("\n%s exit\n", __FUNCTION__);
	tx_exit = 1;
	vTaskDelete(NULL);
}



static void example_socket_tcp_trx_thread(void *param)
{
	int server_fd = -1, client_fd = -1;
	struct sockaddr_in server_addr, client_addr;
	size_t client_addr_size;

	// Delay to wait for IP by DHCP
	vTaskDelay(10000);
	//printf("\nExample: socket tx/rx 1\n");

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	//printf("\nExample: socket tx/rx place 2\n");

	if(bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
		printf("ERROR: bind\n");
		goto exit;
	}

	if(listen(server_fd, LISTEN_QLEN) != 0) {
		printf("ERROR: listen\n");
		goto exit;
	}
	while(1) {
		client_addr_size = sizeof(client_addr);
		client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_size);
		if(client_fd >= 0) {
			printf("EXAMPLE SOCKET OUTER LOOP");
			tx_exit = 1;
			//RtlInitSema(&tcp_tx_rx_sema, 1);
			rtw_init_sema(&tcp_tx_rx_sema, 1);

			if(xTaskCreate(tx_thread, ((const char*)"tx_thread"), 512, &client_fd, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
				printf("\n\r%s xTaskCreate(tx_thread) failed", __FUNCTION__);
			else
				tx_exit = 0;
			vTaskDelay(10);

			//if(xTaskCreate(rx_thread, ((const char*)"rx_thread"), 512, &client_fd, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
			//	printf("\n\r%s xTaskCreate(rx_thread) failed", __FUNCTION__);

			while(1) {
				if(tx_exit) {
					close(client_fd);
					break;
				}
				else
					vTaskDelay(1000);
			}
			printf("\nExample: socket tx/rx place loop\n");
			//RtlFreeSema(&tcp_tx_rx_sema);
			rtw_free_sema(&tcp_tx_rx_sema);
		}
	}

exit:
	close(server_fd);
	vTaskDelete(NULL);
}


void example_socket_tcp_trx_1(void)
{
	if(xTaskCreate(example_socket_tcp_trx_thread, ((const char*)"example_socket_tcp_trx_thread"), 1024, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
		printf("\n\r%s xTaskCreate(example_socket_tcp_trx_thread) failed", __FUNCTION__);
}
