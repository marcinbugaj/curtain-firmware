add_executable(blink
        blink.cpp
        )

# pull in common dependencies
target_link_libraries(blink pico_stdlib hardware_pwm)

pico_enable_stdio_uart(blink 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(blink)

# add url via pico_set_program_url
example_auto_set_url(blink)
