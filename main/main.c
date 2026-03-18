/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdbool.h>
#include <string.h>

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

#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_client.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "esp_log.h"

#include "driver/i2c.h"
#include "ssd1306.h"  
#include "font8x8_basic.h"

//#define DISP_DEBUG

#include "cJSON.h"
#include "esp_crt_bundle.h"
#define MIN(a,b) (((a)<(b))?(a):(b))


#define tag "SSD1306"
#define MAX_WIDTH 14
#define MAX_LINES 4
#define MAX_HTTP_OUTPUT_BUFFER 10000

QueueHandle_t symbol_queue;


	int center, top, bottom;
	char lineChar[20];
	char my_buff[14];
	char api_buffer[5000];
	char responseBuffer[MAX_HTTP_OUTPUT_BUFFER];
	

#include "connectivity_config.h"
   

static EventGroupHandle_t s_wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;
static const char *TAG_HTTP = "HTTP";
static const char *TAG = "WIFI";

  



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

#define DISP_LINE_MAX 16

typedef struct {
    const char *symbol;
    char price_str[DISP_LINE_MAX + 1];
    char change_str[DISP_LINE_MAX + 1];
    bool has_data;
} StockInfo;

#define NUM_STOCKS 4
static StockInfo g_stocks[NUM_STOCKS] = {
    { "AAPL",  "", "", false },
    { "GOOGL", "", "", false },
    { "SOFI",  "", "", false },
    { "NVDA",  "", "", false },
};

#define FNG_API_URL  "https://api.alternative.me/fng/?limit=1&format=json"
static int g_fng_value = 0;           /* 0–100 */
static char g_fng_label[DISP_LINE_MAX + 1] = "";
static bool g_fng_has_data = false;
#define NUM_SLIDES  (NUM_STOCKS + 1)  /* 4 akcie + Fear & Greed */
static int g_current_slide = 0;

#define STOCK_REFRESH_MS  (2 * 60 * 1000)

static char disp_row_bufs[3][DISP_LINE_MAX + 1];

static void disp_line_safe(SSD1306_t *dev, int page, const char *text, int max_len)
{
	if (page < 0 || page > 2) return;
	char *buf = disp_row_bufs[page];
	int out = 0;
	if (!text) text = "-";
	for (int i = 0; text[i] != '\0' && out < DISP_LINE_MAX && (max_len <= 0 || i < max_len); i++) {
		unsigned char c = (unsigned char)text[i];
		buf[out++] = (c >= 0x20 && c <= 0x7E) ? (char)c : '?';
	}
	buf[out] = '\0';
	ssd1306_display_text(dev, page, buf, out ? out : 1, false);
}

static void disp_show_status(SSD1306_t *dev, const char *line1, const char *line2, const char *line3)
{
	ssd1306_clear_screen(dev, false);
	ssd1306_contrast(dev, 0xff);
	disp_line_safe(dev, 0, line1, DISP_LINE_MAX);
	disp_line_safe(dev, 1, (line2 && line2[0]) ? line2 : "", DISP_LINE_MAX);
	disp_line_safe(dev, 2, (line3 && line3[0]) ? line3 : "", DISP_LINE_MAX);
#if CONFIG_SSD1306_128x64
	/* 128x64: řádek 3 prázdný */
#endif
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
	spi_master_init(dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO);
#endif // CONFIG_SPI_INTERFACE

#if CONFIG_FLIP
	dev->_flip = true;
	ESP_LOGW(tag, "Flip upside down");
#endif

#if CONFIG_SSD1306_128x64
	ESP_LOGI(tag, "Panel is 128x64");
	ssd1306_init(dev, 128, 64);
	top = 2; center = 3; bottom = 8;
#endif // CONFIG_SSD1306_128x64
#if CONFIG_SSD1306_128x32
	ESP_LOGI(tag, "Panel is 128x32");
	ssd1306_init(dev, 128, 32);
	top = 1; center = 1; bottom = 4;
#endif // CONFIG_SSD1306_128x32

	ssd1306_clear_screen(dev, false);
	ssd1306_contrast(dev, 0xff);

    
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
	int page = (bottom < dev->_pages) ? bottom : 0;
	ssd1306_display_text(dev, page, "Scroll test! ---", 15, false);
	ssd1306_hardware_scroll(dev, SCROLL_LEFT);
	vTaskDelay(5000 / portTICK_PERIOD_MS);
	ssd1306_hardware_scroll(dev, SCROLL_STOP);
}

void disp_horizontal_scroll_long(SSD1306_t *dev, const char *text, int delay_ms, int duration_ms)
{
	int len = (int)strlen(text);
	if (len == 0) return;
	ssd1306_clear_screen(dev, false);
	ssd1306_contrast(dev, 0xff);
	int page = (bottom < dev->_pages) ? bottom : 0;
	int max_offset = (len > 16) ? len - 16 : 0;
	int offset = 0;
	int elapsed = 0;
	while (elapsed < duration_ms) {
		int n = (len - offset) > 16 ? 16 : (len - offset);
		if (n > 0) {
			ssd1306_clear_line(dev, page, false);
			ssd1306_display_text(dev, page, text + offset, n, false);
		}
		vTaskDelay(pdMS_TO_TICKS(delay_ms));
		elapsed += delay_ms;
		offset++;
		if (offset > max_offset) offset = 0;
	}
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



static size_t s_response_len;

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (evt->user_data && evt->data_len > 0) {
                size_t copy = evt->data_len;
                if (s_response_len + copy >= MAX_HTTP_OUTPUT_BUFFER)
                    copy = MAX_HTTP_OUTPUT_BUFFER - s_response_len;
                if (copy) {
                    memcpy((char *)evt->user_data + s_response_len, evt->data, copy);
                    s_response_len += copy;
                }
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

/* Jednoduchý helper: GET na danou URL, tělo do responseBuffer */
static esp_err_t http_get_to_buffer(const char *url, int *out_status)
{
    if (!url || !out_status) return ESP_ERR_INVALID_ARG;

    memset(responseBuffer, 0, sizeof(responseBuffer));
    s_response_len = 0;

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = _http_event_handler,
        .user_data = responseBuffer,
        .disable_auto_redirect = false,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
        .buffer_size = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG_HTTP, "http client init failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG_HTTP, "HTTP GET %s", url);
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (s_response_len >= MAX_HTTP_OUTPUT_BUFFER)
        s_response_len = MAX_HTTP_OUTPUT_BUFFER - 1;
    responseBuffer[s_response_len] = '\0';

    *out_status = status;
    ESP_LOGI(TAG_HTTP, "status=%d len=%zu", status, (size_t)s_response_len);

    return err;
}

/* RapidAPI helper: GET s x-rapidapi-key + x-rapidapi-host */
static esp_err_t http_get_to_buffer_rapidapi(const char *url, int *out_status)
{
    if (!url || !out_status) return ESP_ERR_INVALID_ARG;

    memset(responseBuffer, 0, sizeof(responseBuffer));
    s_response_len = 0;

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = _http_event_handler,
        .user_data = responseBuffer,
        .disable_auto_redirect = false,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
        .buffer_size = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG_HTTP, "http client init failed");
        return ESP_FAIL;
    }

    /* RapidAPI auth + content type */
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "x-rapidapi-host", RAPIDAPI_FNG_API_HOST);
    esp_http_client_set_header(client, "x-rapidapi-key", RAPIDAPI_FNG_API_KEY);

    ESP_LOGI(TAG_HTTP, "HTTP GET %s (RapidAPI)", url);
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (s_response_len >= MAX_HTTP_OUTPUT_BUFFER)
        s_response_len = MAX_HTTP_OUTPUT_BUFFER - 1;
    responseBuffer[s_response_len] = '\0';

    *out_status = status;
    ESP_LOGI(TAG_HTTP, "status=%d len=%zu", status, (size_t)s_response_len);

    return err;
}

static void fetch_one_quote_finnhub(int idx)
{
    if (idx < 0 || idx >= NUM_STOCKS || FINNHUB_API_TOKEN[0] == '\0') return;

    char url[200];
    snprintf(url, sizeof(url), FINNHUB_API_BASE "?symbol=%s&token=%s",
             g_stocks[idx].symbol, FINNHUB_API_TOKEN);

    int status = 0;
    esp_err_t err = http_get_to_buffer(url, &status);
    if (err != ESP_OK || status != 200) {
        g_stocks[idx].has_data = false;
        return;
    }

    cJSON *root = cJSON_Parse(responseBuffer);
    if (!root || !cJSON_IsObject(root)) {
        if (root) cJSON_Delete(root);
        g_stocks[idx].has_data = false;
        return;
    }

    cJSON *c_j = cJSON_GetObjectItemCaseSensitive(root, "c");
    cJSON *pc_j = cJSON_GetObjectItemCaseSensitive(root, "pc");
    double price = (c_j && cJSON_IsNumber(c_j)) ? c_j->valuedouble : 0.0;
    double prev_close = (pc_j && cJSON_IsNumber(pc_j)) ? pc_j->valuedouble : 0.0;

    cJSON_Delete(root);

    if (price <= 0.0) {
        g_stocks[idx].has_data = false;
        return;
    }

    double chg_pct = (prev_close > 0.0) ? ((price - prev_close) / prev_close * 100.0) : 0.0;
    snprintf(g_stocks[idx].price_str, sizeof(g_stocks[idx].price_str), "%.2f USD", (float)price);
    snprintf(g_stocks[idx].change_str, sizeof(g_stocks[idx].change_str), "%+.2f%%", (float)chg_pct);
    g_stocks[idx].has_data = true;
    ESP_LOGI(TAG_HTTP, "%s %.2f USD %+.2f%%", g_stocks[idx].symbol, (float)price, (float)chg_pct);
}

static void fetch_all_stocks_finnhub(void)
{
    if (FINNHUB_API_TOKEN[0] == '\0') {
        ESP_LOGW(TAG_HTTP, "FINNHUB_API_TOKEN není nastavený");
        for (int i = 0; i < NUM_STOCKS; ++i) g_stocks[i].has_data = false;
        return;
    }

    for (int i = 0; i < NUM_STOCKS; ++i) {
        fetch_one_quote_finnhub(i);
        vTaskDelay(pdMS_TO_TICKS(400));
    }
}

/* Načte Fear & Greed Index (RapidAPI preferred). */
static void fetch_fear_greed(void)
{
    g_fng_has_data = false;

    int status = 0;
    esp_err_t err;

    /* 1) RapidAPI endpoint */
    if (RAPIDAPI_FNG_API_KEY[0] != '\0') {
        err = http_get_to_buffer_rapidapi(RAPIDAPI_FNG_API_URL, &status);
        if (err == ESP_OK && status == 200) {
            cJSON *root = cJSON_Parse(responseBuffer);
            if (!root || !cJSON_IsObject(root)) {
                if (root) cJSON_Delete(root);
                ESP_LOGW(TAG_HTTP, "FNG RapidAPI parse failed (root). len=%zu first80=%.80s",
                         s_response_len, responseBuffer);
            } else {
                cJSON *fgi = cJSON_GetObjectItemCaseSensitive(root, "fgi");
                cJSON *now = (fgi && cJSON_IsObject(fgi)) ? cJSON_GetObjectItemCaseSensitive(fgi, "now") : NULL;
                cJSON *val_j = (now && cJSON_IsObject(now)) ? cJSON_GetObjectItemCaseSensitive(now, "value") : NULL;
                cJSON *valueText_j = (now && cJSON_IsObject(now)) ? cJSON_GetObjectItemCaseSensitive(now, "valueText") : NULL;

                int value_parsed = -1;
                if (val_j && cJSON_IsNumber(val_j)) {
                    value_parsed = (int)val_j->valuedouble;
                } else if (val_j && cJSON_IsString(val_j) && val_j->valuestring) {
                    value_parsed = atoi(val_j->valuestring);
                }

                const char *valueText = cJSON_GetStringValue(valueText_j);

                if (value_parsed >= 0 && value_parsed <= 100) {
                    g_fng_value = value_parsed;
                    if (valueText && valueText[0] != '\0') {
                        snprintf(g_fng_label, sizeof(g_fng_label), "%s", valueText);
                    } else {
                        /* Fallback: popisky dle rozsahu */
                        if (g_fng_value <= 24)
                            snprintf(g_fng_label, sizeof(g_fng_label), "Extreme Fear");
                        else if (g_fng_value <= 44)
                            snprintf(g_fng_label, sizeof(g_fng_label), "Fear");
                        else if (g_fng_value <= 55)
                            snprintf(g_fng_label, sizeof(g_fng_label), "Neutral");
                        else if (g_fng_value <= 75)
                            snprintf(g_fng_label, sizeof(g_fng_label), "Greed");
                        else
                            snprintf(g_fng_label, sizeof(g_fng_label), "Extreme Greed");
                    }

                    g_fng_has_data = true;
                    ESP_LOGI(TAG_HTTP, "Fear&Greed RapidAPI %d %s", g_fng_value, g_fng_label);
                } else {
                    ESP_LOGW(TAG_HTTP, "FNG RapidAPI parse failed (value). len=%zu first80=%.80s",
                             s_response_len, responseBuffer);
                }

                cJSON_Delete(root);
            }
            if (g_fng_has_data) return;
        } else {
            ESP_LOGW(TAG_HTTP, "FNG RapidAPI request failed err=%d status=%d", (int)err, status);
        }
    } else {
        ESP_LOGW(TAG_HTTP, "RAPIDAPI_FNG_API_KEY není nastavený");
    }

    /* 2) Fallback: alternative.me */
    err = http_get_to_buffer(FNG_API_URL, &status);
    if (err != ESP_OK || status != 200) {
        ESP_LOGW(TAG_HTTP, "FNG alternative.me request failed err=%d status=%d", (int)err, status);
        return;
    }

    cJSON *root = cJSON_Parse(responseBuffer);
    if (!root || !cJSON_IsObject(root)) {
        if (root) cJSON_Delete(root);
        ESP_LOGW(TAG_HTTP, "FNG alternative.me parse failed (root). len=%zu first80=%.80s",
                 s_response_len, responseBuffer);
        return;
    }

    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!data || !cJSON_IsArray(data) || cJSON_GetArraySize(data) < 1) {
        ESP_LOGW(TAG_HTTP, "FNG alternative.me no data array or empty. len=%zu first80=%.80s",
                 s_response_len, responseBuffer);
        cJSON_Delete(root);
        return;
    }

    cJSON *first = cJSON_GetArrayItem(data, 0);
    cJSON *val_j = first ? cJSON_GetObjectItemCaseSensitive(first, "value") : NULL;

    int value_parsed = -1;
    if (val_j && cJSON_IsString(val_j) && val_j->valuestring) {
        value_parsed = atoi(val_j->valuestring);
    } else if (val_j && cJSON_IsNumber(val_j)) {
        value_parsed = (int)val_j->valuedouble;
    }

    cJSON_Delete(root);

    if (value_parsed < 0 || value_parsed > 100) return;
    g_fng_value = value_parsed;

    /* Popisky dle rozsahu (ASCII) */
    if (g_fng_value <= 24)
        snprintf(g_fng_label, sizeof(g_fng_label), "Extreme Fear");
    else if (g_fng_value <= 44)
        snprintf(g_fng_label, sizeof(g_fng_label), "Fear");
    else if (g_fng_value <= 55)
        snprintf(g_fng_label, sizeof(g_fng_label), "Neutral");
    else if (g_fng_value <= 75)
        snprintf(g_fng_label, sizeof(g_fng_label), "Greed");
    else
        snprintf(g_fng_label, sizeof(g_fng_label), "Extreme Greed");

    g_fng_has_data = true;
    ESP_LOGI(TAG_HTTP, "Fear&Greed alternative.me %d %s", g_fng_value, g_fng_label);
}

/* Task: fetch every 2 minuty Finnhub quote + Fear & Greed */
static void stocks_task(void *pvParameters)
{
    (void)pvParameters;

    for (int wait = 0; wait < 30; wait++) {
        if (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT) break;
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    while (1) {
        fetch_all_stocks_finnhub();
        fetch_fear_greed();
        vTaskDelay(pdMS_TO_TICKS(STOCK_REFRESH_MS));
    }
}

void app_main(void)
{
    snprintf(my_buff, sizeof(my_buff), "no_data");

//display---------------------
    SSD1306_t sc1;
    disp_init(&sc1);
    /* Hned rozsvítit displej – uvidíš že běží */
    disp_static_text(&sc1);

    /* Stav: Start */
    disp_show_status(&sc1, "STOCK MONITOR", "Starting...", "");

 #ifdef DISP_DEBUG   
    disp_fill_with_text(&sc1);
    disp_countdown(&sc1);
    disp_scrollup(&sc1);
    //disp_scrolldown(&sc1, 16); TODO modified scrolldown for my purpose but does not work for no data input then
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

    /* WiFi – na displeji uvidíš průběh */
    disp_show_status(&sc1, "STOCK MONITOR", "WiFi...", "");
    wifi_init_sta();
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    if (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT) {
        disp_show_status(&sc1, "STOCK MONITOR", "WiFi OK", "");
    } else {
        disp_show_status(&sc1, "STOCK MONITOR", "WiFi FAIL", "");
    }
    vTaskDelay(1500 / portTICK_PERIOD_MS);

    wifi_test();

    /* Task: každé 2 minuty Finnhub REST /quote pro všechny tickery */
    xTaskCreate(stocks_task, "stocks_task", 6144, NULL, 5, NULL);

    gpio_reset_pin(GPIO_NUM_2);
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
    

    /* Hlavní smyčka – rotace: 4 akcie + Fear & Greed */
    while (1) {
        bool wifi_ok = (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT) != 0;

        if (g_current_slide < NUM_STOCKS) {
            StockInfo *info = &g_stocks[g_current_slide];
            if (info->has_data) {
                char line1[DISP_LINE_MAX + 1];
                snprintf(line1, sizeof(line1), "%s", info->symbol);
                disp_show_status(&sc1, line1, info->price_str, info->change_str);
            } else {
                char line3[DISP_LINE_MAX + 1];
                snprintf(line3, sizeof(line3), "%s ...", info->symbol);
                disp_show_status(&sc1, "STOCK MONITOR", wifi_ok ? "WiFi OK" : "WiFi --", line3);
            }
        } else {
            static char fng_index_line[DISP_LINE_MAX + 1];
            if (g_fng_has_data) {
                snprintf(fng_index_line, sizeof(fng_index_line), "Index: %d", (int)g_fng_value);
                disp_show_status(&sc1, "FEAR & GREED", fng_index_line, g_fng_label);
            } else {
                disp_show_status(&sc1, "FEAR & GREED", "...", "");
            }
        }

        gpio_set_level(GPIO_NUM_2, 1);
        vTaskDelay(pdMS_TO_TICKS(5000));
        gpio_set_level(GPIO_NUM_2, 0);

        g_current_slide = (g_current_slide + 1) % NUM_SLIDES;
    }

}
