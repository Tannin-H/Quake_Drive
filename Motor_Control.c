#include "Motor_Control.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "stepper.pio.h"

// PIO state machine and program initialization
PIO pio = pio0;
uint sm = 0;
uint offset;

// Initialize PIO for stepper motor control
void stepper_pio_init(PIO pio, uint sm, uint offset, uint step_pin) {
    pio_gpio_init(pio, step_pin);  // Initialize the step pin for PIO
    pio_sm_set_consecutive_pindirs(pio, sm, step_pin, 1, true);  // Set step pin as output

    // Load the PIO program into the PIO memory
    pio_sm_config c = stepper_program_get_default_config(offset);
    sm_config_set_set_pins(&c, step_pin, 1);  // Set the step pin in the PIO program

    // Set the clock divider for the desired frequency (default 1 kHz)
    float div = clock_get_hz(clk_sys) / (1000.0f * 2);  // Default frequency (1 kHz)
    sm_config_set_clkdiv(&c, div);

    // Initialize the state machine
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

void init_motor_GPIO() {
    gpio_init(DIR_PIN);
    gpio_init(STEP_PIN);
    gpio_init(EN_PIN);
    gpio_set_dir(DIR_PIN, GPIO_OUT);
    gpio_set_dir(STEP_PIN, GPIO_OUT);
    gpio_set_dir(EN_PIN, GPIO_OUT);
    gpio_put(EN_PIN, 0); // Enable motor driver

    // Initialize PIO for stepper motor control
    offset = pio_add_program(pio, &stepper_program);
    stepper_pio_init(pio, sm, offset, STEP_PIN);
}

// Generate steps using PIO
void generate_steps(float frequency, int steps, bool direction) {
    if (frequency <= 0) return;

    // Set direction pin
    gpio_put(DIR_PIN, direction);

    // Adjust the clock divider for the desired frequency
    float div = clock_get_hz(clk_sys) / (frequency * 2);  // Calculate divider for frequency
    pio_sm_set_clkdiv(pio, sm, div);

    // Send the number of steps to the PIO state machine
    pio_sm_put_blocking(pio, sm, steps);

    // Wait for the state machine to finish (optional, if you need to block)
    while (!pio_sm_is_tx_fifo_empty(pio, sm)) {
        tight_loop_contents();
    }
}

void leib_ramp(float start_freq, float target_freq, float ramp_time, bool direction) {
    if (start_freq <= 0 || target_freq <= 0 || ramp_time <= 0) return;

    gpio_put(DIR_PIN, direction);
    uint64_t start_time = time_us_64();
    uint64_t ramp_duration_us = (uint64_t)(ramp_time * 1e6);

    float freq_diff = target_freq - start_freq;
    float freq_slope = freq_diff / ramp_time; // Frequency change per second

    while (time_us_64() - start_time < ramp_duration_us) {
        float elapsed = (float)(time_us_64() - start_time) / 1e6;
        float current_freq = start_freq + (freq_slope * elapsed);

        if ((freq_slope > 0 && current_freq > target_freq) || 
            (freq_slope < 0 && current_freq < target_freq)) {
            current_freq = target_freq;
        }

        generate_steps(current_freq, 1, direction);
    }

    generate_steps(target_freq, 1, direction);
}

void perform_movement(float start_freq, float max_speed, float ramp_time, int steps, bool direction) {
    leib_ramp(start_freq, max_speed, ramp_time, direction);
    generate_steps(max_speed, steps, direction);
    leib_ramp(max_speed, start_freq, ramp_time, direction);
}