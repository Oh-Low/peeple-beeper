cmake_minimum_required(VERSION 3.16)
project(peeple-beeper C)

# Host tests and the SDL bench. The Feather images are `make feather` and `make bringup`.

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT MSVC)
  add_compile_options(-Wall -Wextra -Werror)
endif()

file(GLOB FW_SRC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/firmware/src/*.c")
set(HOST_SRC "${CMAKE_SOURCE_DIR}/bench/mesh.c")
set(ART_SRC
  "${CMAKE_SOURCE_DIR}/firmware/art/art.c"
  "${CMAKE_SOURCE_DIR}/firmware/art/beeple.c")
set(HAL_RAM_SRC "${CMAKE_SOURCE_DIR}/hal/ram/emu_hal.c")
set(HAL_FEATHER_SRC "${CMAKE_SOURCE_DIR}/hal/feather/feather_hal.c")
set(FW_LINK ${FW_SRC} ${ART_SRC} ${HOST_SRC} ${HAL_RAM_SRC})
set(FW_INC "${CMAKE_SOURCE_DIR}/firmware/include")
set(HAL_INC "${CMAKE_SOURCE_DIR}/hal")
set(HAL_RAM_INC "${CMAKE_SOURCE_DIR}/hal/ram")

function(peeple_test name)
  add_executable(${name} ${ARGN})
  target_include_directories(${name} PRIVATE ${FW_INC} ${HAL_INC} ${HAL_RAM_INC})
endfunction()

peeple_test(message_test firmware/tests/message_test.c ${FW_LINK})
peeple_test(relay_test firmware/tests/relay_test.c ${FW_LINK})
peeple_test(store_test firmware/tests/store_test.c ${FW_LINK})
peeple_test(screen_test firmware/tests/screen_test.c ${FW_LINK})
peeple_test(mesh_test firmware/tests/mesh_test.c ${FW_LINK})

peeple_test(seed_test firmware/tests/seed_test.c bench/seed.c ${FW_LINK})
target_include_directories(seed_test PRIVATE "${CMAKE_SOURCE_DIR}/bench")

peeple_test(pair_test firmware/tests/pair_test.c bench/seed.c ${FW_LINK})
target_include_directories(pair_test PRIVATE "${CMAKE_SOURCE_DIR}/bench")

peeple_test(gps_test firmware/tests/gps_test.c ${FW_LINK} ${HAL_FEATHER_SRC})
target_include_directories(gps_test PRIVATE "${CMAKE_SOURCE_DIR}/hal/feather")

peeple_test(heading_test firmware/tests/heading_test.c ${FW_LINK})
peeple_test(receiver_test firmware/tests/receiver_test.c ${FW_LINK})
peeple_test(locate_test firmware/tests/locate_test.c ${FW_LINK})

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_executable(peeple-emu bench/main.c bench/font.c bench/seed.c ${FW_LINK})
target_include_directories(peeple-emu PRIVATE ${FW_INC} ${HAL_INC} ${HAL_RAM_INC} "${CMAKE_SOURCE_DIR}/bench")
target_link_libraries(peeple-emu PRIVATE PkgConfig::SDL2)

add_custom_command(
  TARGET peeple-emu POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/bench/third_party/tiny5/Tiny5-Regular.ttf"
          "$<TARGET_FILE_DIR:peeple-emu>/Tiny5-Regular.ttf")
