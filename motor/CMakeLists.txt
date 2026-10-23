# El núcleo del motor en el PC: biblioteca, pruebas y th64sim.
# La ROM se compila aparte con el Makefile (n64.mk de libdragon).
# Uso: cmake -S motor -B motor/build/pc -G Ninja && cmake --build motor/build/pc && ctest --test-dir motor/build/pc
cmake_minimum_required(VERSION 3.20)
project(th64_motor C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_EXTENSIONS ON)          # gnu17, como n64.mk
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
option(TH_SANITIZE "ASan + UBSan" OFF)
add_compile_options(-Wall -Wextra -Werror)
if(TH_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
  add_link_options(-fsanitize=address,undefined)
endif()

file(GLOB TH_CORE_SOURCES CONFIGURE_DEPENDS core/src/*.c)
add_library(th_core STATIC ${TH_CORE_SOURCES})
target_include_directories(th_core PUBLIC core/include)

add_library(th_core_tests STATIC tests/test_core.c)
target_include_directories(th_core_tests PUBLIC tests)
target_link_libraries(th_core_tests PUBLIC th_core)

add_executable(test_core tests/main_pc.c)
target_link_libraries(test_core th_core_tests)

add_executable(th64sim platform/pc/th64sim.c)
target_link_libraries(th64sim th_core_tests)

enable_testing()
add_test(NAME nucleo COMMAND test_core)
