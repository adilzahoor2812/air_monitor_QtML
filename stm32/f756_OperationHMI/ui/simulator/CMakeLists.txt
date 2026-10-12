cmake_minimum_required(VERSION 3.16)
project(operation_hmi_sim C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Prefer a local clone (avoids FetchContent git-hook issues on some Macs)
set(LVGL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lvgl" CACHE PATH "Path to LVGL sources")
if(NOT EXISTS "${LVGL_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "LVGL not found at ${LVGL_DIR}\n"
    "Run:\n"
    "  git clone --depth 1 --branch v9.2.2 https://github.com/lvgl/lvgl.git ${LVGL_DIR}")
endif()

set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(LV_BUILD_SET_CONFIG_OPTS ON CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)

add_subdirectory(${LVGL_DIR} lvgl_build)

find_package(SDL2 REQUIRED)
find_package(Threads REQUIRED)

# Homebrew ships headers under include/SDL2/, but LVGL includes <SDL2/SDL.h>
if(DEFINED SDL2_INCLUDE_DIRS)
  get_filename_component(_sdl2_inc_parent "${SDL2_INCLUDE_DIRS}" DIRECTORY)
  target_include_directories(lvgl PUBLIC ${_sdl2_inc_parent} ${SDL2_INCLUDE_DIRS})
endif()
if(TARGET SDL2::SDL2)
  target_link_libraries(lvgl PUBLIC SDL2::SDL2)
endif()

set(HMI_UI_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../lvgl/operation_hmi.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../fonts/roboto_regular_12.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../fonts/roboto_regular_14.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../fonts/roboto_medium_16.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../fonts/roboto_medium_20.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../fonts/roboto_medium_28.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../fonts/latin1_de.c
)

add_executable(operation_hmi_sim
  main.c
  ${HMI_UI_SOURCES}
)

add_executable(dump_hmi
  dump_hmi.c
  ${HMI_UI_SOURCES}
)

target_include_directories(operation_hmi_sim PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../lvgl
  ${CMAKE_CURRENT_SOURCE_DIR}/../fonts
)
target_include_directories(dump_hmi PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../lvgl
  ${CMAKE_CURRENT_SOURCE_DIR}/../fonts
)

target_compile_definitions(operation_hmi_sim PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)
target_compile_definitions(dump_hmi PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)

if(TARGET SDL2::SDL2)
  target_link_libraries(operation_hmi_sim PRIVATE lvgl SDL2::SDL2 Threads::Threads)
  target_link_libraries(dump_hmi PRIVATE lvgl SDL2::SDL2 Threads::Threads)
else()
  target_include_directories(operation_hmi_sim PRIVATE ${SDL2_INCLUDE_DIRS})
  target_include_directories(dump_hmi PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(operation_hmi_sim PRIVATE lvgl ${SDL2_LIBRARIES} Threads::Threads)
  target_link_libraries(dump_hmi PRIVATE lvgl ${SDL2_LIBRARIES} Threads::Threads)
endif()

if(APPLE)
  target_link_libraries(operation_hmi_sim PRIVATE "-framework Cocoa")
  target_link_libraries(dump_hmi PRIVATE "-framework Cocoa")
endif()

message(STATUS "Build then run: ./build/operation_hmi_sim [/dev/cu.usbmodemXXXX]")
