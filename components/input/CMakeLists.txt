idf_component_register(
    SRCS "input_touch.c" "chsc6x_parse.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2c esp_timer lvgl
)
