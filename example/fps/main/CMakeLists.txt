idf_component_register(SRCS "main.c")

target_compile_options(${COMPONENT_LIB} PRIVATE
    -O3
    -ffast-math
    -fno-math-errno
    -fjump-tables
    -ftree-switch-conversion
    -funroll-loops
)
