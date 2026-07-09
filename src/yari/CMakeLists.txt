idf_component_register(
    SRCS
        "yari.c"
        "colors.c"
        "physics.c"
        "renderer_common.c"
        "yari_utils.c"
        "ht.c"
        "platform/esp32/inputs.c"
        "platform/esp32/renderer.c"
    INCLUDE_DIRS
        "./"
    REQUIRES
        esp_lcd
        driver
        esp_timer
        esp_driver_gpio
        esp_adc
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -O3
    -ffast-math
    -fno-math-errno
    -fjump-tables
    -ftree-switch-conversion
)
