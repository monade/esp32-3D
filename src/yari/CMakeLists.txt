idf_component_register(
    SRCS
        "yari.c"
        "physics.c"
        "renderer_common.c"
        "yari_utils.c"
        "ht.c"
        "platform/esp32/inputs.c"
        "platform/esp32/renderer.c"
        "platform/esp32/multithread.c"
    INCLUDE_DIRS
        "./"
    REQUIRES
        driver
        esp_timer
        esp_adc
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -O3
    -ffast-math
    -fno-math-errno
    -fjump-tables
    -ftree-switch-conversion
    -funroll-loops
)
