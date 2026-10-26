cmake_minimum_required(VERSION 3.16)
project(peeple-beeper C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT MSVC)
  add_compile_options(-Wall -Wextra -Werror)
endif()

file(GLOB FW_SRC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/firmware/src/*.c")
set(FW_INC "${CMAKE_SOURCE_DIR}/firmware/include")

function(peeple_test name)
  add_executable(${name} ${ARGN})
  target_include_directories(${name} PRIVATE ${FW_INC})
endfunction()

peeple_test(page_test firmware/tests/page_test.c ${FW_SRC})
peeple_test(store_test firmware/tests/store_test.c ${FW_SRC})

peeple_test(mesh_test firmware/tests/mesh_test.c emu/seed.c ${FW_SRC})
target_include_directories(mesh_test PRIVATE "${CMAKE_SOURCE_DIR}/emu")

peeple_test(locate_test firmware/tests/locate_test.c ${FW_SRC} hal/nrf/nrf_hal.c)
target_include_directories(locate_test PRIVATE "${CMAKE_SOURCE_DIR}/hal/nrf")

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_executable(peeple-emu emu/main.c emu/font.c emu/seed.c ${FW_SRC})
target_include_directories(peeple-emu PRIVATE ${FW_INC} "${CMAKE_SOURCE_DIR}/emu")
target_link_libraries(peeple-emu PRIVATE PkgConfig::SDL2)

add_custom_command(
  TARGET peeple-emu POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/emu/third_party/roboto/Roboto-Regular.ttf"
          "$<TARGET_FILE_DIR:peeple-emu>/Roboto-Regular.ttf")
