idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES spi_flash
                       REQUIRES nvs_flash driver esp_timer yari)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -O3
    -ffast-math
    -fno-math-errno
)