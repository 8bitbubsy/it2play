cmake_minimum_required(VERSION 3.7)

project(it2play C)

find_package(SDL2 REQUIRED)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${it2play_SOURCE_DIR}/it2play/release/other/")

file(GLOB it2play_SRC
    "${it2play_SOURCE_DIR}/audiodrivers/sdl/*.c"
    "${it2play_SOURCE_DIR}/loaders/mmcmp/*.c"
    "${it2play_SOURCE_DIR}/loaders/*.c"
    "${it2play_SOURCE_DIR}/it2drivers/*.c"
    "${it2play_SOURCE_DIR}/*.c"
    "${it2play_SOURCE_DIR}/it2play/src/*.c"
)

add_executable(it2play ${it2play_SRC})

target_include_directories(it2play SYSTEM
    PRIVATE ${SDL2_INCLUDE_DIRS})

if("${SDL2_LIBRARIES}" STREQUAL "")
    message(WARNING "SDL2_LIBRARIES wasn't set, manually setting to SDL2::SDL2")
    set(SDL2_LIBRARIES "SDL2::SDL2")
endif()

target_link_libraries(it2play
    PRIVATE m pthread ${SDL2_LIBRARIES})

target_compile_definitions(it2play
    PRIVATE AUDIODRIVER_SDL)

install(TARGETS it2play
    RUNTIME DESTINATION bin)
