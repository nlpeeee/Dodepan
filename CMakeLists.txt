cmake_minimum_required(VERSION 3.13)
 
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

if (${PICO_BOARD} STREQUAL "pico2")
project(Dodepan_RP2350 C CXX ASM)
add_definitions(-DBOARD_IS_PICO2)
else()
project(Dodepan_RP2040 C CXX ASM)
add_definitions(-DBOARD_IS_PICO)
endif ()
 
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
 
if (NOT PICO_BOARD STREQUAL "pico2")
    message(FATAL_ERROR "This firmware requires Pico 2/RP2350 with 4 MiB flash")
endif()
set(PICO_FLASH_SIZE_BYTES "4 * 1024 * 1024 - 8192")
option(DODEPAN_ENGINE_218 "PRA32-U2 v2.18.2 native 48 kHz evaluation" OFF)
set(DODEPAN_ENGINE_PATH "${CMAKE_CURRENT_LIST_DIR}/lib/digital-synth-pra32-u2/Digital-Synth-PRA32-U2")
if(DODEPAN_ENGINE_218)
    set(DODEPAN_ENGINE_PATH "${CMAKE_CURRENT_LIST_DIR}/lib/pra32-u2-v2.18.2/Digital-Synth-PRA32-U2")
endif()
pico_sdk_init()

add_executable(${PROJECT_NAME}
        main.cpp
        synth_service.cpp
        pop_capture.cpp
        settings_store.c
        note_router.c
)

add_subdirectory(lib/pico-mpr121/lib mpr121)
add_subdirectory(lib/RP2040-Button button)
add_subdirectory(lib/rpi-pico-mpu6050 haw_mpu6050)
add_subdirectory(lib/RP2040-Battery-Check battery_check)
add_subdirectory(lib/RP2040-Rotary-Encoder)
add_subdirectory(lib/sound_i2s)

target_sources(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/imu.c
        ${CMAKE_CURRENT_LIST_DIR}/state.c
        ${CMAKE_CURRENT_LIST_DIR}/touch.c
        ${CMAKE_CURRENT_LIST_DIR}/looper.c
        ${CMAKE_CURRENT_LIST_DIR}/arpeggiator.c
        ${CMAKE_CURRENT_LIST_DIR}/demo_player.c
        ${CMAKE_CURRENT_LIST_DIR}/i2c_mutex.c
        ${CMAKE_CURRENT_LIST_DIR}/display/display.c
        ${CMAKE_CURRENT_LIST_DIR}/lib/pico-ssd1306/ssd1306.c
        ${CMAKE_CURRENT_LIST_DIR}/inc/tinyusb-midi/usb_descriptors.c
        )

target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/lib/rpi-pico-mpu6050/include/haw
        ${CMAKE_CURRENT_LIST_DIR}/lib/pico-ssd1306
        ${DODEPAN_ENGINE_PATH}
        ${CMAKE_CURRENT_LIST_DIR}/inc/tinyusb-midi
        )

target_link_libraries(${PROJECT_NAME} PRIVATE
        pico_stdlib
        hardware_i2c
        hardware_dma
        hardware_adc
        hardware_flash
        pico_flash
        pico-mpr121
        pico_multicore
        sound_i2s_16bits
        tinyusb_device
        tinyusb_board
        pico_unique_id
        button
        haw_mpu6050
        battery_check
        pico_rotary_encoder
        )

pico_add_extra_outputs(${PROJECT_NAME})

pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

target_link_options(${PROJECT_NAME} PRIVATE "LINKER:-T,${CMAKE_CURRENT_LIST_DIR}/storage_reservation.ld")

target_compile_definitions(${PROJECT_NAME} PRIVATE PICO_STACK_SIZE=4096 PICO_CORE1_STACK_SIZE=4096)

set(DODEPAN_CLOCK_KHZ 288000 CACHE STRING "Experimental system clock in kHz")
set(DODEPAN_BUFFER_FRAMES 128 CACHE STRING "Audio frames per buffer (1..256)")
if(DODEPAN_BUFFER_FRAMES LESS 1 OR DODEPAN_BUFFER_FRAMES GREATER 256)
    message(FATAL_ERROR "Audio buffer must contain 1..256 frames")
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE SYSTEM_CLOCK_KHZ=${DODEPAN_CLOCK_KHZ} AUDIO_BUFFER_LENGTH=${DODEPAN_BUFFER_FRAMES})
foreach(feature DISPLAY IMU PERSISTENCE)
    option(DODEPAN_DISABLE_${feature} "Disable ${feature} for controlled diagnostics" OFF)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DODEPAN_DISABLE_${feature}=$<BOOL:${DODEPAN_DISABLE_${feature}}>)
endforeach()
option(DODEPAN_UART_DIAGNOSTICS "Emit bounded diagnostics on UART1 GPIO8/9" OFF)
target_compile_definitions(${PROJECT_NAME} PRIVATE DODEPAN_UART_DIAGNOSTICS=$<BOOL:${DODEPAN_UART_DIAGNOSTICS}>)

file(GLOB_RECURSE identity_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.c" "${CMAKE_CURRENT_LIST_DIR}/*.cpp" "${CMAKE_CURRENT_LIST_DIR}/*.h" "${CMAKE_CURRENT_LIST_DIR}/*.pio" "${CMAKE_CURRENT_LIST_DIR}/*.ld")
list(FILTER identity_files EXCLUDE REGEX "/(build[^/]*|tools|\\.git)/")
list(SORT identity_files)
set(identity "")
foreach(source IN LISTS identity_files)
    file(SHA256 "${source}" digest)
    string(APPEND identity "${digest}")
endforeach()
string(SHA256 identity "${identity}")
string(SUBSTRING "${identity}" 0 16 identity)
target_compile_definitions(${PROJECT_NAME} PRIVATE DODEPAN_SOURCE_ID="${identity}")

file(SHA256 "${DODEPAN_ENGINE_PATH}/pra32-u2-synth.h" engine_identity)
string(SUBSTRING "${engine_identity}" 0 16 engine_identity)
target_compile_definitions(${PROJECT_NAME} PRIVATE DODEPAN_ENGINE_ID="${engine_identity}")

option(DODEPAN_POP_CAPTURE "Diagnostic pre-I2S capture over USB CDC" OFF)
if(DODEPAN_POP_CAPTURE AND NOT DODEPAN_DISABLE_PERSISTENCE)
    message(FATAL_ERROR "Pop capture requires saves disabled")
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE DODEPAN_POP_CAPTURE=$<BOOL:${DODEPAN_POP_CAPTURE}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -DNM=${CMAKE_NM} -DELF=$<TARGET_FILE:${PROJECT_NAME}>
        -P ${CMAKE_CURRENT_LIST_DIR}/tools/pop-audit/check_sram.cmake
    VERBATIM)

option(DODEPAN_TRANSPORT_48K "Diagnostic 48 kHz I2S with 24 kHz synth sample repetition" OFF)
if(DODEPAN_TRANSPORT_48K)
    if(NOT DODEPAN_DISABLE_PERSISTENCE OR DODEPAN_POP_CAPTURE)
        message(FATAL_ERROR "48 kHz transport test requires saves disabled and PCM capture OFF")
    endif()
    if(NOT DODEPAN_BUFFER_FRAMES EQUAL 128 OR NOT DODEPAN_CLOCK_KHZ EQUAL 288000)
        message(FATAL_ERROR "48 kHz transport test requires 128 synth frames and 288 MHz")
    endif()
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE DODEPAN_TRANSPORT_48K=$<BOOL:${DODEPAN_TRANSPORT_48K}>)

target_compile_definitions(${PROJECT_NAME} PRIVATE DODEPAN_ENGINE_218=$<BOOL:${DODEPAN_ENGINE_218}>)
if(DODEPAN_ENGINE_218)
    if(DODEPAN_TRANSPORT_48K OR DODEPAN_POP_CAPTURE)
        message(FATAL_ERROR "Engine candidate requires native transport and capture OFF")
    endif()
    if(NOT DODEPAN_BUFFER_FRAMES EQUAL 256)
        message(FATAL_ERROR "Engine candidate requires 256 native frames for the 5.333ms deadline")
    endif()
    target_compile_definitions(${PROJECT_NAME} PRIVATE PRA32_U2_ENABLE_POLY_ON_1_CORE)
endif()
