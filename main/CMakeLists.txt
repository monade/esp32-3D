idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES spi_flash
                       INCLUDE_DIRS "libs"
                       REQUIRES nvs_flash esp_lcd driver esp_timer)
