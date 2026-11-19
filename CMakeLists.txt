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
set(CMAKE_PROJECT_NAME GalaxyBoot)

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

# ---------------------------------------------------------------------------
# GalaxyBoot code (App/) and shared code (../common). CubeMX owns the rest.
# ---------------------------------------------------------------------------
set(COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)

# Bootloader: 256 KB at 0x08000000 (sectors 0-5).
string(REPLACE "STM32F405XX_FLASH.ld" "GalaxyBoot_boot.ld" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/App/*.c)
set(COMMON_SOURCES
    ${COMMON_DIR}/util/crc32.c
    ${COMMON_DIR}/galaxy/oled.c
    ${COMMON_DIR}/galaxy/eeprom.c)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${APP_SOURCES} ${COMMON_SOURCES})
set_source_files_properties(${APP_SOURCES} ${COMMON_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wextra")

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Boot
    ${COMMON_DIR}/util
    ${COMMON_DIR}/galaxy
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
