idf_component_register(
    SRCS "ui_render.c" "ui_rooms.c" "sprites_data.c"
    INCLUDE_DIRS "."
    REQUIRES pet_core lvgl esp_lcd_gc9a01 esp_lcd esp_driver_spi esp_driver_gpio esp_timer
)
