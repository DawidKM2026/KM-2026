idf_component_register(
    SRCS
        "main_app.c"
        "STATE/system_state.c"
        "BOOT/system_boot.c"

        "ESP_NOW/esp_now_comm.c"

        "OTA/config_wifi.c"
        "OTA/over_the_air_updates.c"

        "GPIO/gpio_config.c"

        "Move_XY/encoders.c"
        "Move_XY/stepper_motor.c"
        "Move_XY/bresenham.c"

        "SPM/spm_motors.c"
        "SPM/spm_ik.c"

        "ARDUINO_THRUSTERS_GYRO/uart_comm.c"
        
    INCLUDE_DIRS
        "."
        "STATE"
        "BOOT"
        "ESP_NOW"
        "OTA"
        "GPIO"
        "Move_XY"
        "SPM"
        "ARDUINO_THRUSTERS_GYRO"

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