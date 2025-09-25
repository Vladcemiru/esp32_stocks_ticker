# Embed CA, certificate & key directly into binary
idf_component_register(SRCS "wifi_eap_fast_main.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_wifi wpa_supplicant nvs_flash driver esp_http_client cJSON
                    )
