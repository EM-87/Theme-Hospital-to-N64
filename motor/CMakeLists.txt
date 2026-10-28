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

# La vista del mapa y el rasterizador de referencia: portables, sin libdragon.
add_library(th_vista STATIC vista/vista.c vista/raster.c vista/objetos.c)
target_include_directories(th_vista PUBLIC vista)
target_link_libraries(th_vista PUBLIC th_core)

# La interfaz: ventanas, texto y el panel inferior, también portables.
add_library(th_ui STATIC ui/ui.c ui/texto.c ui/panel_inferior.c ui/puntero.c)
target_include_directories(th_ui PUBLIC ui)
target_link_libraries(th_ui PUBLIC th_vista m)

add_library(th_core_tests STATIC tests/test_core.c tests/test_map.c tests/test_humanoid.c tests/test_rules.c)
target_include_directories(th_core_tests PUBLIC tests)
target_link_libraries(th_core_tests PUBLIC th_core)

add_executable(test_core tests/main_pc.c)
target_link_libraries(test_core th_core_tests)

add_executable(th64sim platform/pc/th64sim.c platform/pc/hito4.c)
target_link_libraries(th64sim th_core_tests th_vista th_ui)

enable_testing()
add_test(NAME nucleo COMMAND test_core)
