# Embed CA, certificate & key directly into binary
idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "." "../include"
                    EMBED_FILES "tickers.txt"
                    PRIV_REQUIRES log esp_wifi wpa_supplicant nvs_flash driver esp_http_client mbedtls cJSON ssd1306
                    )
