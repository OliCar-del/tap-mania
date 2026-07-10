cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME buttonswinterrupts2)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
   ${CMAKE_SOURCE_DIR}/Core/Src/ARGB.c
   ${CMAKE_SOURCE_DIR}/Core/Src/beat_event.c
   ${CMAKE_SOURCE_DIR}/Core/Src/button_debounce.c
   ${CMAKE_SOURCE_DIR}/Core/Src/button_handler.c
   ${CMAKE_SOURCE_DIR}/Core/Src/command_handler.c
   ${CMAKE_SOURCE_DIR}/Core/Src/config_input.c
   ${CMAKE_SOURCE_DIR}/Core/Src/config_storage.c
   ${CMAKE_SOURCE_DIR}/Core/Src/game_state.c
   ${CMAKE_SOURCE_DIR}/Core/Src/led_control.c
   ${CMAKE_SOURCE_DIR}/Core/Src/led_display.c
   ${CMAKE_SOURCE_DIR}/Core/Src/scoring.c
   ${CMAKE_SOURCE_DIR}/Core/Src/sd_card_detect.c
   ${CMAKE_SOURCE_DIR}/Core/Src/sd_card_utils.c
   ${CMAKE_SOURCE_DIR}/Core/Src/sequence.c
   ${CMAKE_SOURCE_DIR}/Core/Src/test_sequences.c
   ${CMAKE_SOURCE_DIR}/Core/Src/tsq_parser.c
   ${CMAKE_SOURCE_DIR}/Core/Src/uart_debug.c
   ${CMAKE_SOURCE_DIR}/Core/Src/fonts.c
   ${CMAKE_SOURCE_DIR}/Core/Src/SH1106.c
   # ${CMAKE_SOURCE_DIR}/Core/Src/audio.c
   # (optional, for IDE visibility only)
   ${CMAKE_SOURCE_DIR}/Core/Inc/main.h
   ${CMAKE_SOURCE_DIR}/Core/Inc/ARGB.h
   ${CMAKE_SOURCE_DIR}/Core/Inc/button_debounce.h
   ${CMAKE_SOURCE_DIR}/Core/Inc/button_handler.h
   ${CMAKE_SOURCE_DIR}/Core/Inc/command_handler.h
   ${CMAKE_SOURCE_DIR}/Core/Inc/config_input.h
   ${CMAKE_SOURCE_DIR}/Core/Inc/config_storage.h
   ${CMAKE_SOURCE_DIR}/Core/Inc/game_state.h
   ${CMAKE_SOURCE_DIR}/Core/Inc/led_control.h
   ${CMAKE_SOURCE_DIR}/Core/Inc/led_display.h
   ${CMAKE_SOURCE_DIR}/Core/Inc/scoring.h
   ${CMAKE_SOURCE_DIR}/Core/Inc/sd_card_detect.h
   ${CMAKE_SOURCE_DIR}/Core/Inc/sd_card_utils.h
   ${CMAKE_SOURCE_DIR}/Core/Inc/sequence.h
   ${CMAKE_SOURCE_DIR}/Core/Inc/test_sequences.h
   ${CMAKE_SOURCE_DIR}/Core/Inc/tsq_parser.h
   ${CMAKE_SOURCE_DIR}/Core/Inc/uart_debug.h
   ${CMAKE_SOURCE_DIR}/Core/Inc/fonts.h
   ${CMAKE_SOURCE_DIR}/Core/Inc/SH1106.h
   # ${CMAKE_SOURCE_DIR}/Core/Inc/audio.h

   # ${CMAKE_SOURCE_DIR}/Core/Inc/libs.h
   ${CMAKE_SOURCE_DIR}/FATFS/Target/user_diskio_spi.c
   ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FatFs/src/ff.h
   # ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FatFs/src/ff_gen_drv.c
   # ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FatFs/src/diskio.c
   # ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FatFs/src/option/ccsbcs.c 
   ${CMAKE_SOURCE_DIR}/Core/Inc/button_debounce.h
   ${CMAKE_SOURCE_DIR}/Core/Src/button_debounce.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
