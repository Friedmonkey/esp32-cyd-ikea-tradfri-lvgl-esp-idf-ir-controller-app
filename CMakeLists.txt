# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# ---------------------------
# Define your personal libraries
# ---------------------------
set(MY_LIBS_FOLDER "C:/Users/marti/Documents/esp-projects/.libraries")

set(MY_COMPONENTS
    ${MY_LIBS_FOLDER}/fried_esp_cyd_screen_lvgl8
    ${MY_LIBS_FOLDER}/fried_tradfri_client
    ${MY_LIBS_FOLDER}/fried_coaps_client
    ${MY_LIBS_FOLDER}/fdata
)

# ---------------------------
# Extra ESP-IDF components
# ---------------------------
set(EXTRA_COMPONENT_DIRS
    $ENV{IDF_PATH}/examples/common_components/protocol_examples_common
)

list(APPEND EXTRA_COMPONENT_DIRS ${MY_COMPONENTS})

# ---------------------------
# Include project CMake boilerplate
# ---------------------------
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(remote_display_tradfri_controller)
