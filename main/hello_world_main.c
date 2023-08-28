#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"

#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"


#include <driver/uart.h>


#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "string.h"




#define EXAMPLE_ESP_WIFI_SSID      "ASUS-FB9C"
#define EXAMPLE_ESP_WIFI_PASS      "00000000"
#define HOST_IP_ADDR               "10.82.0.86"
#define PORT                        (755)
#define EX_UART_NUM                 (UART_NUM_0)
#define EX_UART_BAUD                (115200L)
#define UART_BUF_SIZE               (1024)


static const char *UART_TAG = "UART";
static const char *WIFI_TAG = "WIFI";
static const char *TCP_CLIENT_TAG = "TCP_CLIENT";
nvs_handle  my_handle;
TaskHandle_t h_uart_task;
TaskHandle_t h_tcp_task;
//static char Wifi_SSID[20] = "ASUS-FB9C";
//static char Wifi_Pass[20] = "00000000";
//static char Host_IP[20]   = "10.82.0.86";
//static char Host_Port[20] = "755";


struct SMy_Wifi_Conf
{
	char Wifi_SSID[20];
	char Wifi_Pass[20];
	char Host_IP  [20];
	char Host_Port[20];
} My_Wifi_Conf;
enum E_CMD
{
	EC_Read_Config,
	EC_Write_Config,
	EC_Restart,
};


//wifi defs


//static EventGroupHandle_t s_wifi_event_group;

static QueueHandle_t uart0_queue;

int sock = 0;



static void tcp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];

    while (1)
    {
        struct sockaddr_in destAddr;
        //destAddr.sin_addr.s_addr = inet_addr(My_Wifi_Conf.Host_IP);
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        destAddr.sin_addr.s_addr = inet_addr("10.82.0.86");
        destAddr.sin_family = AF_INET;
        //destAddr.sin_port = htons(My_Wifi_Conf.Host_Port);
        destAddr.sin_port = htons(755);

        inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

        sock =  socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (sock < 0)
        {
            ESP_LOGE(TCP_CLIENT_TAG, "Unable to create socket: errno %d", errno);
            vTaskDelay(200 /portTICK_PERIOD_MS );
            break;
        }
        ESP_LOGI(TCP_CLIENT_TAG, "Socket created");

        int err = connect(sock, (struct sockaddr *)&destAddr, sizeof(destAddr));
        if (err != 0)
        {
            ESP_LOGE(TCP_CLIENT_TAG, "Socket unable to connect: errno %d", errno);
            close(sock);
            vTaskDelay(200 /portTICK_PERIOD_MS );
            continue;
        }
        ESP_LOGI(TCP_CLIENT_TAG, "Successfully connected");

        while (1) {


            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            // Error occured during receiving
            if (len < 0)
            {
                ESP_LOGE(TCP_CLIENT_TAG, "recv failed: errno %d", errno);
                break;
            }
            // Data received
            else
            {
                //rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TCP_CLIENT_TAG, "Received %d bytes from %s:", len, addr_str);
				uart_write_bytes(EX_UART_NUM, (const char *) rx_buffer, len);
            }


        }

        if (sock != -1) {
            ESP_LOGE(TCP_CLIENT_TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}




static void process_cmd(uint8_t *p_data)
{
	static uint8_t tx_buffer[128];
	uint8_t *p_tx_buffer = tx_buffer;
	switch(p_data[0])
	{
		case EC_Read_Config:
		{
			uart_tx_chars(UART_NUM_0, (const char *)"Read Config \n",  sizeof("Read Config \n"));
			p_tx_buffer[0] = EC_Read_Config;
			p_tx_buffer++;
			strcpy((char *)p_tx_buffer, (const char *)My_Wifi_Conf.Wifi_SSID);
			p_tx_buffer += strlen(My_Wifi_Conf.Wifi_SSID) + 1;

			strcpy((char *)p_tx_buffer, (const char *)My_Wifi_Conf.Wifi_Pass);
			p_tx_buffer += strlen(My_Wifi_Conf.Wifi_Pass) + 1;

			strcpy((char *)p_tx_buffer, (const char *)My_Wifi_Conf.Host_IP);
			p_tx_buffer += strlen(My_Wifi_Conf.Host_IP) + 1;

			strcpy((char *)p_tx_buffer, (const char *)My_Wifi_Conf.Host_Port);


			uart_tx_chars(UART_NUM_0, (const char *)tx_buffer,  sizeof(tx_buffer));
		}
		break;

		case EC_Write_Config:
		{
			uart_tx_chars(UART_NUM_0, (const char *)"Write Config \n",  sizeof("Write Config \n"));
			p_data++; //FIRST IS COMMAND BYTE
			strcpy(My_Wifi_Conf.Wifi_SSID, (const char *)p_data);
			p_data += strlen((const char*)p_data) + 1;

			strcpy(My_Wifi_Conf.Wifi_Pass, (const char *)p_data);
			p_data += strlen((const char*)p_data) + 1;

			strcpy(My_Wifi_Conf.Host_IP, (const char *)p_data);
			p_data += strlen((const char*)p_data) + 1;

			strcpy(My_Wifi_Conf.Host_Port, (const char *)p_data);


			ESP_ERROR_CHECK(nvs_set_blob (my_handle, "wifi_config", (const void*)&My_Wifi_Conf, sizeof(My_Wifi_Conf) ));
			ESP_ERROR_CHECK(nvs_commit(my_handle));

			tx_buffer[0] = EC_Write_Config;
			uart_tx_chars(UART_NUM_0, (const char *)tx_buffer,  1);
		}

		break;

		case EC_Restart:
			uart_tx_chars(UART_NUM_0, (const char *)"ESP Restart",  sizeof("ESP Restart"));
			esp_restart();
			break;

	}
}


static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t *dtmp = (uint8_t *) malloc(UART_BUF_SIZE);
    bzero(dtmp, UART_BUF_SIZE);
    for (;;)
    {
        // Waiting for UART event.
        if (xQueueReceive(uart0_queue, (void *)&event, (portTickType)portMAX_DELAY))
        {
            switch (event.type)
            {
                case UART_DATA:
                {
                	uint16_t i;
					static uint16_t j = 0;
					static uint8_t mode = 0;
					ESP_LOGI(UART_TAG, "[UART DATA]: %d", event.size);
					uart_read_bytes(EX_UART_NUM, dtmp, event.size, 50 / portMAX_DELAY); //проверка буфера

					if(mode == 0)//Normal mode
					{
						static uint8_t connect_seq[] = "go to config mode";
						for(i = 0; i < event.size; i++)
						{
							if(dtmp[i] == connect_seq[j])
								j++;
							else
								j = 0;
						}


						if(j == (sizeof(connect_seq) - 1) )
						{
							static const uint8_t answer[] = "im in config mode";
							mode = 1;
							j = 0;

							uart_write_bytes(EX_UART_NUM, (const char *) answer, sizeof(answer));
							vTaskSuspend(h_tcp_task);

						}
						else
						{
							int err = send(sock, dtmp, event.size, 0);
							if (err < 0)
							{
								ESP_LOGE(TCP_CLIENT_TAG, "Error occured during sending: errno %d", errno);
							}
						}
					}
					else//config mode
					{
						process_cmd(dtmp);//передача указателя на буфер полученных байт
					}
					break;
                }


                    // Event of HW FIFO overflow detected
					case UART_FIFO_OVF:
						ESP_LOGI(UART_TAG, "hw fifo overflow");
						// If fifo overflow happened, you should consider adding flow control for your application.
						// The ISR has already reset the rx FIFO,
						// As an example, we directly flush the rx buffer here in order to read more data.
						uart_flush_input(EX_UART_NUM);
						xQueueReset(uart0_queue);
						break;

					// Event of UART ring buffer full
					case UART_BUFFER_FULL:
						ESP_LOGI(UART_TAG, "ring buffer full");
						// If buffer full happened, you should consider encreasing your buffer size
						// As an example, we directly flush the rx buffer here in order to read more data.
						uart_flush_input(EX_UART_NUM);
						xQueueReset(uart0_queue);
						break;

					case UART_PARITY_ERR:
						ESP_LOGI(UART_TAG, "uart parity error");
						break;

					// Event of UART frame error
					case UART_FRAME_ERR:
						ESP_LOGI(UART_TAG, "uart frame error");
						break;

					// Others
					default:
						ESP_LOGI(UART_TAG, "uart event type: %d", event.type);
						break;
            }
        }
    }

    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}


static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if(event_base == WIFI_EVENT)
    {
    	if(event_id == WIFI_EVENT_STA_START)
    	{
    		esp_wifi_connect();
    	}
    	else if(event_id == WIFI_EVENT_STA_DISCONNECTED)
    	{
    		esp_wifi_connect();
			ESP_LOGI(WIFI_TAG, "retry to connect to the AP");
    	}
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "connected to ap SSID:%s password:%s", My_Wifi_Conf.Wifi_SSID, My_Wifi_Conf.Wifi_Pass);
        ESP_LOGI(WIFI_TAG, "got ip:%s", ip4addr_ntoa(&event->ip_info.ip));

    }
}









void app_main()
{
	//esp_log_level_set("*", ESP_LOG_NONE); //working methods

	//инициализация UART_____________________________________________________________________________________________
	// communication pins and install the driver
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	uart_config_t uart_config =
	{
		.baud_rate = EX_UART_BAUD,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE
	};
	uart_param_config(EX_UART_NUM, &uart_config);
	uart_driver_install(EX_UART_NUM, UART_BUF_SIZE * 2, UART_BUF_SIZE * 2, 100, &uart0_queue, 0); // Install UART driver, and get the queue.






	//innit nvs flash
	esp_err_t nvs_err = nvs_flash_init();
	 if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES)
	 {
	        ESP_ERROR_CHECK(nvs_flash_erase());
	        nvs_err = nvs_flash_init();
	 }
	 ESP_ERROR_CHECK( nvs_err );


	 size_t par_length = 100;
	 ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));

//	 // Read value

	 size_t wifi_config_size = sizeof(My_Wifi_Conf);
	 nvs_err = nvs_get_blob (my_handle, "wifi_config", (const void *)&My_Wifi_Conf, &wifi_config_size);
	 if(nvs_err == ESP_ERR_NVS_NOT_FOUND)
	 {
	 		 strcpy(My_Wifi_Conf.Wifi_SSID, "Not Init");
	 		 strcpy(My_Wifi_Conf.Wifi_Pass, "Not Init");
	 		 strcpy(My_Wifi_Conf.Host_IP,   "Not Init");
	 		 strcpy(My_Wifi_Conf.Host_Port, "Not Init");

	 		 ESP_ERROR_CHECK(nvs_set_blob (my_handle, "wifi_config", (const void *)&My_Wifi_Conf, sizeof(My_Wifi_Conf) ));
	 		 ESP_ERROR_CHECK(nvs_commit(my_handle));
	 }
	 //nvs_close(my_handle); после работы с хэндлом его надо закрыть



	//ns flash init end














	//инициализация wifi_____________________________________________________________________________________________


	tcpip_adapter_init();
	tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA); // Don't run a DHCP client
	tcpip_adapter_ip_info_t ipInfo;
	inet_pton(AF_INET, "10.82.1.243",       &ipInfo.ip);
	inet_pton(AF_INET, "10.82.0.10",        &ipInfo.gw);
	inet_pton(AF_INET, "255.255.254.0" ,    &ipInfo.netmask);
	tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg) );
	ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA));
	wifi_config_t wifi_config = {0};
	memset((void *) &wifi_config, 0, (size_t)sizeof(wifi_config));
	strcpy((char *)wifi_config.sta.ssid, (const char *)My_Wifi_Conf.Wifi_SSID);
	strcpy((char *)wifi_config.sta.password, (const char *)My_Wifi_Conf.Wifi_Pass);
	if (strlen((char *)wifi_config.sta.password))
	{
		wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
	}
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_ERROR_CHECK(esp_wifi_connect());


//#define ENABLE_STATIC_IP 1
//#if ENABLE_STATIC_IP
//	tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
//	tcpip_adapter_dns_info_t dns_info = {0};
//	tcpip_adapter_ip_info_t ip_info = {0};
//	IP4_ADDR(&ip_info.ip, 	   10,82,1,243);
//	IP4_ADDR(&ip_info.gw, 	   10,82,0,10);
//	IP4_ADDR(&ip_info.netmask, 255,255,254,0);
//	//IP4_ADDR(&dns_info.ip, 	   10,82,0,10);
//	//ESP_ERROR_CHECK(tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_STA, TCPIP_ADAPTER_DNS_FALLBACK, &dns_info));
//	ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
//#endif
//
//    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
//	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
//    ESP_ERROR_CHECK(esp_wifi_start());
//    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(-128))// max power  17.5dBm
//    ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");
//    //инициализация wifi_____________________________________________________________________________________________



    // Create a task to handler UART event from ISR
    xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, 1, &h_uart_task);
    xTaskCreate(tcp_client_task, "tcp_client", 4096, NULL, 1, &h_tcp_task);
//

}






