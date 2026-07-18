# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(CMAKE_NINJA_FORCE_RESPONSE_FILE ON)

set(PROJECT_VER "1.4.3")
add_definitions(-DFIRMWARE_VERSION=\"${PROJECT_VER}\")

# Add this line to disable the specific warning
add_compile_options(-Wno-missing-field-initializers)

# Auto-load a local sdkconfig overlay (git-ignored) when present. Lets
# self-hosted / custom deployments pin CONFIG_STACKCHAN_SERVER_URL,
# CONFIG_OTA_URL, etc. without touching committed defaults.
# Manual equivalent: SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.local" idf.py build
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdkconfig.defaults.local")
    set(SDKCONFIG_DEFAULTS
        "${CMAKE_CURRENT_SOURCE_DIR}/sdkconfig.defaults;${CMAKE_CURRENT_SOURCE_DIR}/sdkconfig.defaults.local"
        CACHE STRING "ESP-IDF sdkconfig default files (auto-extended with local overlay)" FORCE)
    message(STATUS "StackChan: detected sdkconfig.defaults.local, applying overlay")
endif()

# Remove duplicate qrcodegen.c from esp_emote_gfx before any component globs it.
# Both esp_emote_gfx and LVGL (LV_USE_QRCODE=y) bundle the same Nayuki library;
# only LVGL's copy is needed (the uitk::lvgl_cpp::Qrcode wrapper and
# qrcode_wrapper.c both resolve against LVGL's symbols at link time).
set(_EMOTE_QRCODEGEN
    "${CMAKE_CURRENT_SOURCE_DIR}/managed_components/espressif2022__esp_emote_gfx/src/lib/qrcode/qrcodegen.c")
if(EXISTS "${_EMOTE_QRCODEGEN}")
    file(REMOVE "${_EMOTE_QRCODEGEN}")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(stack-chan)
