cmake_minimum_required(VERSION 3.12)

find_package(Perl)
if(NOT PERL_FOUND)
    message(FATAL_ERROR "Perl is needed for generating the fsdata.c file")
endif()

set(MAKE_FS_DATA_SCRIPT /pico-sdk/lib/lwip/src/apps/http/makefsdata/makefsdata)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconcepts-ts -fno-exceptions -fno-rtti")

execute_process(COMMAND
        perl ${MAKE_FS_DATA_SCRIPT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        ECHO_OUTPUT_VARIABLE
        ECHO_ERROR_VARIABLE
        )
file(RENAME fsdata.c my_fsdata.c)

set(PICO_BOARD pico_w)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

project(pico_examples C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

pico_sdk_init()

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        -Wno-maybe-uninitialized
	-Werror
        )

add_executable(blink
        blink.cpp
	tmc2209.cpp
	ENNPinImpl.cpp
	StepPinSqWaveImpl.cpp
	UARTImpl.cpp
	picow_ntp_client.cpp
	dhcp.c
	UrlDecode.cpp
	Persistence.cpp
	Server.cpp
	ParseConfig.cpp
	Utils.cpp
        )

# pull in common dependencies
target_link_libraries(blink
	pico_stdlib
	hardware_pwm
	hardware_rtc
	pico_multicore
	pico_cyw43_arch_lwip_threadsafe_background
	pico_lwip_http
	#pico_cyw43_arch_lwip_poll
	)

target_include_directories(blink PRIVATE . ${CMAKE_CURRENT_LIST_DIR})

pico_enable_stdio_uart(blink 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(blink)

