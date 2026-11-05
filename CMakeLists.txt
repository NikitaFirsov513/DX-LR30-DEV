cmake_minimum_required(VERSION 3.24)
project(dx_lr30_pj26 LANGUAGES C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PJ26_TARGET "Radio" CACHE STRING "Radio or UART")
set_property(CACHE PJ26_TARGET PROPERTY STRINGS Radio UART)
if(NOT PJ26_TARGET MATCHES "^(Radio|UART)$")
    message(FATAL_ERROR "PJ26_TARGET must be Radio or UART")
endif()
file(READ "${CMAKE_SOURCE_DIR}/project.json" manifest)
foreach(key source_dirs include_dirs defines)
    string(JSON count LENGTH "${manifest}" ${key})
    math(EXPR last "${count} - 1")
    foreach(index RANGE ${last})
        string(JSON value GET "${manifest}" ${key} ${index})
        list(APPEND ${key} "${value}")
    endforeach()
endforeach()
foreach(directory IN LISTS source_dirs)
    file(GLOB_RECURSE found CONFIGURE_DEPENDS "${directory}/*.c" "${directory}/*.s")
    list(APPEND sources ${found})
endforeach()
list(SORT sources)
add_executable(${PROJECT_NAME} ${sources})
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")
target_include_directories(${PROJECT_NAME} PRIVATE ${include_dirs})
target_compile_definitions(${PROJECT_NAME} PRIVATE ${defines})
if(PJ26_TARGET STREQUAL "UART")
    target_compile_definitions(${PROJECT_NAME} PRIVATE UART_ONLY=1)
endif()
target_compile_options(${PROJECT_NAME} PRIVATE
    -mcpu=cortex-m3 -mthumb -mfloat-abi=soft -Os -g3
    -ffunction-sections -fdata-sections -fno-common
    $<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra;-Werror;-Wno-unused-parameter>)
string(JSON linker GET "${manifest}" linker_script)
target_link_options(${PROJECT_NAME} PRIVATE
    -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
    --specs=nano.specs --specs=nosys.specs
    "-T${CMAKE_SOURCE_DIR}/${linker}" -Wl,--gc-sections
    "-Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map" -Wl,--print-memory-usage)
set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS "${CMAKE_SOURCE_DIR}/${linker}")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
    VERBATIM)
