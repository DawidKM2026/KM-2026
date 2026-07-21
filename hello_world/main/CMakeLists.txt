idf_component_register(
    SRCS 
    "Move_XY/encoders.c"
    "SPM/spherical_parallel_manipulator.c" 
    "main_app.c" 
    "ESP_NOW/esp_now_comm.c" 
    "OTA/config_wifi.c" 
    "OTA/over_the_air_updates.c" 
    "GPIO/gpio_config.c" 
    "Move_XY/stepper_motor.c" 
    "Move_XY/bresenham.c"
    
    
    INCLUDE_DIRS
            "."
            "Move_XY"
            "SPM"
            "ESP_NOW"
            "OTA"
            "GPIO"


    EMBED_TXTFILES
        "OTA/index.html"

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
        esp_driver_gpio
        esp_driver_gptimer
        esp_driver_uart
        esp_driver_pcnt
        log
)