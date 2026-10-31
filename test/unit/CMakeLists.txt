# NT-3 Light unit tests (GoogleTest + fff), run on the PC.
#
#   cmake -S test/unit -B test/unit/build -G "MinGW Makefiles"
#   cmake --build test/unit/build
#   ctest --test-dir test/unit/build --output-on-failure
#
# or, from the project folder:   make test      (see the top-level Makefile)
#
# Each test builds the module(s) under test as C, from Light/, against the
# stubs in stubs/ (the SDK and the modules that are not under test).  The
# test fakes (fff) every function the module calls outside itself.
cmake_minimum_required(VERSION 3.14)
project(nt3_unit_tests LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Same GoogleTest as the Z3Switch project.  To build offline, point CMake at an
# already downloaded copy:  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<path>
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)
enable_testing()

set(ROOT  ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(LIGHT ${ROOT}/Light)

# NT_3_app.h is force-included into every firmware file; do the same here.
add_compile_options(-include ${ROOT}/NT_3_app.h -Wall)
# The target is 32-bit; on a 64-bit PC, printing a pointer as uint32_t warns.
add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-pointer-to-int-cast>)

# Stubs first, so they replace the SDK and the modules that are not under test.
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/stubs)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${LIGHT}/tx ${LIGHT}/rx ${LIGHT}/utils ${LIGHT}/time
                    ${LIGHT}/buttons ${LIGHT}/leds ${LIGHT}/hal)

function(nt3_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} gtest_main)
  gtest_discover_tests(${name})
endfunction()

# rx / tx
nt3_test(test_zcl_header      test_zcl_header.cpp      ${LIGHT}/rx/zcl_header.c ${LIGHT}/utils/byte_utils.c)
nt3_test(test_rx_zcl_handler  test_rx_zcl_handler.cpp  ${LIGHT}/rx/rx_zcl_handler.c ${LIGHT}/rx/zcl_header.c
                                                       ${LIGHT}/utils/byte_utils.c)
nt3_test(test_rx_zdo_handler  test_rx_zdo_handler.cpp  ${LIGHT}/rx/rx_zdo_handler.c ${LIGHT}/utils/byte_utils.c)
nt3_test(test_tx_queue        test_tx_queue.cpp        ${LIGHT}/tx/tx_queue.c)

# buttons
nt3_test(test_hal_buttons     test_hal_buttons.cpp     ${LIGHT}/hal/hal_buttons.c)
nt3_test(test_app_button      test_app_button.cpp      ${LIGHT}/buttons/app_button.c)
nt3_test(test_button_actions  test_button_actions.cpp  ${LIGHT}/buttons/button_actions.c)

# LEDs
nt3_test(test_hal_leds        test_hal_leds.cpp        ${LIGHT}/hal/hal_leds.c)
nt3_test(test_app_leds        test_app_leds.cpp        ${LIGHT}/leds/app_leds.c reference/network_status_led_ref.c)
