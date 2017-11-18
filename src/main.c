#include "FreeRTOS.h"
#include "task.h"
#include "diag.h"
#include "main.h"
#include "wifi_constants.h"
#include "wifi_structures.h"
#include "lwip_netconf.h"
#include "wifi_conf.h"
#include "netif.h"
#include <platform/platform_stdlib.h>
#include "relay_wifi.h"

#define STACKSIZE                   (512 + 768)
#if CONFIG_LWIP_LAYER
extern struct netif xnetif[NET_IF_NUM];
#endif

extern void wlan_netowrk(void);
extern void console_init(void);
/**
  * @brief  Main program.
  * @param  None
  * @retval None
  */


static rtw_result_t scan_result_handler(rtw_scan_handler_result_t* malloced_scan_result);


void main(void)
{
	printf("\nInitializing Relay Board Firmware...\n\n");
	//Initialize console
	console_init();
	//Initialize LwIP
	LwIP_Init();
	//Initialize wifi manager
	wifi_manager_init();
	if(xTaskCreate(start_relay_wifi, ((const char*)"Sensor WiFI"), STACKSIZE, NULL, tskIDLE_PRIORITY + 3 + PRIORITIE_OFFSET, NULL) != pdPASS)
		printf("\n\r%s xTaskCreate(start_sensor_wifi) failed", __FUNCTION__);
	printf("Starting TCP Thread");
	example_socket_tcp_trx_1();


    //Start task scheduler
#if defined(CONFIG_KERNEL) && !TASK_SCHEDULER_DISABLED
	#ifdef PLATFORM_FREERTOS
	vTaskStartScheduler();
	#endif
#else
	RtlConsolTaskRom(NULL);
#endif

}
