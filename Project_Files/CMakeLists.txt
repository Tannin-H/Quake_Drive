# Add executable. Default name is the project name, version 0.1
add_executable(Quake_Drive Quake_Drive.c Queue.c Motor_Control.c)

target_link_libraries(Quake_Drive pico_stdlib freertos hardware_pio)


# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(Quake_Drive 0)
pico_enable_stdio_usb(Quake_Drive 1)

pico_add_extra_outputs(Quake_Drive)

