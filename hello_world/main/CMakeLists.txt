idf_component_register(
    SRCS "main_app.c" "esp_now_comm.c" "config_wifi.c" "over_the_air_updates.c"
    INCLUDE_DIRS "."

    EMBED_TXTFILES
        "index.html"

    PRIV_REQUIRES
        spi_flash
        app_update
        esp_https_ota
        esp_http_server
        esp_http_client
        esp_event
        esp_netif
        nvs_flash
        esp_wifi
        log
)