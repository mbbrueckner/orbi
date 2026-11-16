idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES pet_core ui input storage esp_driver_i2c
)
