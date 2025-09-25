/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_eap_client.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "driver/gpio.h"


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_client.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "esp_log.h"

#include "driver/i2c.h"
#include "ssd1306.h"  // nebo jiná OLED knihovna pro ESP-IDF
#include "font8x8_basic.h"

#include "cJSON.h"


#define tag "SSD1306"
#define MAX_WIDTH 14
#define MAX_LINES 4


QueueHandle_t symbol_queue;


	int center, top, bottom;
	char lineChar[20];
	char my_buff[14];




#define WIFI_SSID      "***"    
#define WIFI_PASS      "****"     

static EventGroupHandle_t s_wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;
static const char *TAG = "empty";

void http_get_task(void *pvParameters)
{
    esp_http_client_config_t config = {
        .url = "http://www.alphavantage.co/query?function=TIME_SERIES_DAILY&symbol=IBM&apikey=*****", 
        .transport_type = HTTP_TRANSPORT_OVER_TCP, // vynutí čistý TCP, bez TLS
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        int len = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d", status, len);

        char buffer[30000];
        int read_len = esp_http_client_read_response(client, buffer, sizeof(buffer)-1);
        if (read_len >= 0) {
            buffer[read_len] = '\0'; // ukončení stringu
            ESP_LOGI(TAG, "Data: %s", buffer);

			cJSON *root = cJSON_Parse(buffer);
			if (root == NULL) {
    		ESP_LOGE(TAG, "JSON root parse error!");
				}
			cJSON *format = cJSON_GetObjectItem(root,"Meta Data");
			if (format == NULL) {
    		ESP_LOGE(TAG, "JSON format parse error!");
				} else {
			const char *myLine = cJSON_GetObjectItem(format,"symbol")->valuestring;

			printf("Symbol: %s\n", myLine);
				}

				const char *myLine = "empty";
    		xQueueSend(symbol_queue, myLine, portMAX_DELAY);

        }
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }


    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}






void check_server(const char *host, const char *port)
{
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;

    int err = getaddrinfo(host, port, &hints, &res);
    if(err != 0 || res == NULL) {
        ESP_LOGE(TAG, "DNS lookup failed");
        return;
    }

    int s = socket(res->ai_family, res->ai_socktype, 0);
    if(s < 0) {
        ESP_LOGE(TAG, "socket failed");
        freeaddrinfo(res);
        return;
    }

    err = connect(s, res->ai_addr, res->ai_addrlen);
    if(err != 0) {
        ESP_LOGE(TAG, "connect failed");
    } else {
        ESP_LOGI(TAG, "connected to %s:%s", host, port);
    }

    close(s);
    freeaddrinfo(res);
}
void wifi_test(){
    printf("testing wifi....\n");

    check_server("www.google.com", "80");
}

void disp_static_text(SSD1306_t *dev){

	ssd1306_contrast(dev, 0xff);
	ssd1306_display_text_x3(dev, 0, "Hello!", 6, false);
	vTaskDelay(3000 / portTICK_PERIOD_MS);
}

void disp_init(SSD1306_t *dev){
	


#if CONFIG_I2C_INTERFACE
	ESP_LOGI(tag, "INTERFACE is i2c");
	ESP_LOGI(tag, "CONFIG_SDA_GPIO=%d",CONFIG_SDA_GPIO);
	ESP_LOGI(tag, "CONFIG_SCL_GPIO=%d",CONFIG_SCL_GPIO);
	ESP_LOGI(tag, "CONFIG_RESET_GPIO=%d",CONFIG_RESET_GPIO);
	i2c_master_init(dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
#endif // CONFIG_I2C_INTERFACE

#if CONFIG_SPI_INTERFACE
	ESP_LOGI(tag, "INTERFACE is SPI");
	ESP_LOGI(tag, "CONFIG_MOSI_GPIO=%d",CONFIG_MOSI_GPIO);
	ESP_LOGI(tag, "CONFIG_SCLK_GPIO=%d",CONFIG_SCLK_GPIO);
	ESP_LOGI(tag, "CONFIG_CS_GPIO=%d",CONFIG_CS_GPIO);
	ESP_LOGI(tag, "CONFIG_DC_GPIO=%d",CONFIG_DC_GPIO);
	ESP_LOGI(tag, "CONFIG_RESET_GPIO=%d",CONFIG_RESET_GPIO);
	spi_master_init(&dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO);
#endif // CONFIG_SPI_INTERFACE

#if CONFIG_FLIP
	dev._flip = true;
	ESP_LOGW(tag, "Flip upside down");
#endif

#if CONFIG_SSD1306_128x64
	ESP_LOGI(tag, "Panel is 128x64");
	ssd1306_init(&dev, 128, 64);
#endif // CONFIG_SSD1306_128x64
#if CONFIG_SSD1306_128x32
	ESP_LOGI(tag, "Panel is 128x32");
	ssd1306_init(dev, 128, 32);
#endif // CONFIG_SSD1306_128x32

	ssd1306_clear_screen(dev, false);

    
}

void disp_fill_with_text(SSD1306_t *dev){
    #if CONFIG_SSD1306_128x64
	top = 2;
	center = 3;
	bottom = 8;
	ssd1306_display_text(dev, 0, "SSD1306 128x64", 14, false);
	ssd1306_display_text(dev, 1, "ABCDEFGHIJKLMNOP", 16, false);
	ssd1306_display_text(dev, 2, "abcdefghijklmnop",16, false);
	ssd1306_display_text(dev, 3, "Hello World!!", 13, false);
	//ssd1306_clear_line(dev, 4, true);
	//ssd1306_clear_line(dev, 5, true);
	//ssd1306_clear_line(dev, 6, true);
	//ssd1306_clear_line(dev, 7, true);
	ssd1306_display_text(dev, 4, "SSD1306 128x64", 14, true);
	ssd1306_display_text(dev, 5, "ABCDEFGHIJKLMNOP", 16, true);
	ssd1306_display_text(dev, 6, "abcdefghijklmnop",16, true);
	ssd1306_display_text(dev, 7, "Hello World!!", 13, true);
#endif // CONFIG_SSD1306_128x64

#if CONFIG_SSD1306_128x32
	top = 1;
	center = 1;
	bottom = 4;
	ssd1306_display_text(dev, 0, "SSD1306 128x32", 14, false);
	ssd1306_display_text(dev, 1, "Hello World", 13, false);
	//ssd1306_clear_line(dev, 2, true);
	//ssd1306_clear_line(dev, 3, true);
	ssd1306_display_text(dev, 2, "SSD1306 128x32", 14, true);


#endif // CONFIG_SSD1306_128x32
	vTaskDelay(3000 / portTICK_PERIOD_MS);
}

void disp_countdown(SSD1306_t *dev){
    	// Display Count Down
	uint8_t image[24];
	memset(image, 0, sizeof(image));
	ssd1306_display_image(dev, top, (6*8-1), image, sizeof(image));
	ssd1306_display_image(dev, top+1, (6*8-1), image, sizeof(image));
	ssd1306_display_image(dev, top+2, (6*8-1), image, sizeof(image));
	for(int font=0x39;font>0x30;font--) {
		memset(image, 0, sizeof(image));
		ssd1306_display_image(dev, top+1, (7*8-1), image, 8);
		memcpy(image, font8x8_basic_tr[font], 8);
		if (dev->_flip) ssd1306_flip(image, 8);
		ssd1306_display_image(dev, top+1, (7*8-1), image, 8);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

void disp_scrollup(SSD1306_t *dev){
	// Scroll Up
	ssd1306_clear_screen(dev, false);
	ssd1306_contrast(dev, 0xff);
	ssd1306_display_text(dev, 0, "---Scroll  UP---", 16, true);
	//ssd1306_software_scroll(dev, 7, 1);
	ssd1306_software_scroll(dev, (dev->_pages - 1), 1);
	for (int line=0;line<bottom+10;line++) {
		lineChar[0] = 0x01;
		sprintf(&lineChar[1], " Line %02d", line);
		ssd1306_scroll_text(dev, lineChar, strlen(lineChar), false);
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
	vTaskDelay(3000 / portTICK_PERIOD_MS);
    }

void disp_scrolldown(SSD1306_t *dev, char *ln){
	ssd1306_clear_screen(dev, false);
	ssd1306_contrast(dev, 0xff);
	ssd1306_display_text(dev, 0, "--STOCK MARKET--", 16, true);
	//ssd1306_software_scroll(dev, 1, 7);
	ssd1306_software_scroll(dev, 1, (dev->_pages - 1) );
	for (int line=0;line<bottom+10;line++) {
		lineChar[0] = 0x02;
		sprintf(&lineChar[1], "%s %02d",ln, line);
		ssd1306_scroll_text(dev, lineChar, strlen(lineChar), false);
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
	vTaskDelay(3000 / portTICK_PERIOD_MS);

    }

void disp_fadeout(SSD1306_t *dev){
	ssd1306_fadeout(dev);
}

void disp_page_down(SSD1306_t *dev){
	ssd1306_clear_screen(dev, false);
	ssd1306_contrast(dev, 0xff);
	ssd1306_display_text(dev, 0, "---Page	DOWN---", 16, true);
	ssd1306_software_scroll(dev, 1, (dev->_pages-1) );
	for (int line=0;line<bottom+10;line++) {
		//if ( (line % 7) == 0) ssd1306_scroll_clear(dev);
		if ( (line % (dev->_pages-1)) == 0) ssd1306_scroll_clear(dev);
		lineChar[0] = 0x02;
		sprintf(&lineChar[1], " Line %02d", line);
		ssd1306_scroll_text(dev, lineChar, strlen(lineChar), false);
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
	vTaskDelay(3000 / portTICK_PERIOD_MS);
}

void disp_horizontal_scroll(SSD1306_t *dev){
	ssd1306_clear_screen(dev, false);
	ssd1306_contrast(dev, 0xff);
	ssd1306_display_text(dev, bottom, "Stock_Market_is_going_crazy!", 29, false);
	ssd1306_hardware_scroll(dev, SCROLL_LEFT);
	vTaskDelay(5000 / portTICK_PERIOD_MS);
	ssd1306_hardware_scroll(dev, SCROLL_LEFT);
	vTaskDelay(5000 / portTICK_PERIOD_MS);
	ssd1306_hardware_scroll(dev, SCROLL_STOP);
}

void disp_vertical_scroll(SSD1306_t *dev){
	ssd1306_clear_screen(dev, false);
	ssd1306_contrast(dev, 0xff);
	ssd1306_display_text(dev, center, "Vertical", 8, false);
	ssd1306_hardware_scroll(dev, SCROLL_DOWN);
	vTaskDelay(5000 / portTICK_PERIOD_MS);
	ssd1306_hardware_scroll(dev, SCROLL_UP);
	vTaskDelay(5000 / portTICK_PERIOD_MS);
	ssd1306_hardware_scroll(dev, SCROLL_STOP);
}

void disp_invert(SSD1306_t *dev){
	ssd1306_clear_screen(dev, true);
	ssd1306_contrast(dev, 0xff);
	ssd1306_display_text(dev, center, "  Good Bye!!", 12, true);
	vTaskDelay(5000 / portTICK_PERIOD_MS);

}


void app_main(void)
{
	symbol_queue = xQueueCreate(5, sizeof(char[32]));
 	char extern_line[10];
	char t = 'b';
	snprintf(my_buff, 9, "no_data");

//display---------------------

    SSD1306_t sc1;
    disp_init(&sc1);


 #ifdef DISP_DEBUG   
    disp_fill_with_text(&sc1);
    disp_countdown(&sc1);
    disp_scrollup(&sc1);
    disp_scrolldown(&sc1);
    disp_page_down(&sc1);
    disp_horizontal_scroll(&sc1);
    disp_vertical_scroll(&sc1);
    disp_invert(&sc1);
    disp_fadeout(&sc1);
#endif

	
#if 0
	// Fade Out
	for(int contrast=0xff;contrast>0;contrast=contrast-0x20) {
		ssd1306_contrast(dev, contrast);
		vTaskDelay(40);
	}
#endif

	// Restart module
	// esp_restart();

//     //wifi---------------------------------
//     // init NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    // init wifi
    wifi_init_sta();
    vTaskDelay(1000 / portTICK_PERIOD_MS); // čeká 1 sekundu

    wifi_test();

    xTaskCreate(&http_get_task, "http_get_task", 4096, NULL, 5, NULL);


    gpio_reset_pin(GPIO_NUM_2);
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
    

    //main loop
    while(1){
	// if (xQueueReceive(symbol_queue, extern_line, pdMS_TO_TICKS(1000))) {
 
	// } else {
   disp_scrolldown(&sc1, my_buff);
	
    // disp_horizontal_scroll(&sc1);
    // disp_horizontal_scroll(&sc1);



    gpio_set_level(GPIO_NUM_2, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS); // čeká 1 sekundu
  gpio_set_level(GPIO_NUM_2, 0);
          vTaskDelay(1000 / portTICK_PERIOD_MS); // čeká 1 sekundu

    }

}
