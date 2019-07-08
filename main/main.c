#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <stdio.h>
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "esp_timer.h"
#include <driver/adc.h>


#define EXAMPLE_ESP_WIFI_SSID      "ESP32"
#define EXAMPLE_ESP_WIFI_PASS      "121235RA"
#define EXAMPLE_ESP_MAXIMUM_RETRY  3

#define HOST_IP_ADDR 				"192.168.4.1"
#define PORT 						8080

#define DC_OFFSET 				0
#define AUDIO_BUFFER_MAX		2000
#define SAMPLE_RATE				8000

int16_t audioBuffer[AUDIO_BUFFER_MAX];

static EventGroupHandle_t s_wifi_event_group;

const int WIFI_CONNECTED_BIT = BIT0;

static int s_retry_num = 0;

int j=0;

int time_before = 0;

static void send_all(int sock, const void *vbuf, size_t size_buf)
{
	const char *buf = (char*)vbuf;
	int send_size;
	size_t size_left;
	const int flags = 0;

	size_left = size_buf;

	while(size_left > 0 )
	{
		if((send_size = send(sock, buf,size_left,flags)) == -1)
		{
			printf("Erro ao enviar\n");
			break;
		}
		if(send_size == 0)
		{
			printf("envio completp\n");
			break;
		}

		size_left -= send_size;
		buf +=send_size;
	}
	return;
}

static void tcp_client_task(void *pvParameters)
{
    int addr_family;
    int ip_protocol;

    while(1){
        struct sockaddr_in destAddr;
        destAddr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;

        int sock = socket(addr_family, SOCK_STREAM, ip_protocol);
   		connect(sock, (struct sockaddr *)&destAddr, sizeof(destAddr));

		send_all(sock,(void*)&audioBuffer, sizeof(audioBuffer));

   		shutdown(sock, 0);
        close(sock);
        vTaskDelete(NULL);
    }
}


static void periodic_timer_callback(void * arg)
{
	int readValue = adc1_get_raw(ADC1_CHANNEL_0);
	audioBuffer[j] = readValue - DC_OFFSET;
	j++;
	if(j>=AUDIO_BUFFER_MAX){
	j = 0;
	//printf("time: %lld\n",(esp_timer_get_time()-time_before)/1000);
	xTaskCreatePinnedToCore(tcp_client_task, "tcp_client", 4096, NULL, 5, NULL,1);
	}

}


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        gpio_set_level(GPIO_NUM_2, 1);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        {
        	gpio_set_level(GPIO_NUM_2, 0);
            if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
                esp_wifi_connect();
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                s_retry_num++;

            }

            break;
        }
    default:
        break;
    }
    return ESP_OK;
}


void wifi_init_sta()
{
    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

}


void app_main()
{

    const esp_timer_create_args_t periodic_timer_args = {
            .callback = &periodic_timer_callback,
            .name = "periodic"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    time_before = esp_timer_get_time();
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, (1000000/SAMPLE_RATE)));

    gpio_pad_select_gpio(GPIO_NUM_2);
	gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);

	adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_DB_0);

	nvs_flash_init();
	wifi_init_sta();


}
