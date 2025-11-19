# Automatically include all LVGL UI generated .c files
file(GLOB_RECURSE UI_SOURCES "lvgl_ui_export/*.c")

idf_component_register(
    SRCS
        remote_display_tradfri_controller.c
        controller_queue.c
	remote_queue.c
        ${UI_SOURCES}
    INCLUDE_DIRS
        "."
        "lvgl_ui_export"
)
