SET(SOURCES screens/ui_loading_screen.c
    screens/ui_app_screen.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c)

add_library(ui ${SOURCES})
