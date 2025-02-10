//Motor_Control.c
#include "Motor_Control.h"

void init_motor_GPIO() {
    gpio_init(DIR_PIN);
    gpio_init(STEP_PIN);
    gpio_init(EN_PIN);
    gpio_set_dir(DIR_PIN, GPIO_OUT);
    gpio_set_dir(STEP_PIN, GPIO_OUT);
    gpio_set_dir(EN_PIN, GPIO_OUT);
    gpio_put(EN_PIN, 0); // Enable motor driver
}

void generate_steps(float frequency, int steps, bool direction, volatile bool* stop_requested) {
    if (frequency <= 0 || *stop_requested) return;

    gpio_put(DIR_PIN, direction);
    uint32_t delay_us = (uint32_t)(500000.0f / frequency); // Half-period in µs

    for (int i = 0; i < steps && !(*stop_requested); ++i) {
        gpio_put(STEP_PIN, 1);
        sleep_us(delay_us);
        gpio_put(STEP_PIN, 0);
        sleep_us(delay_us);
    }
}

void leib_ramp(float start_freq, float target_freq, float ramp_time, bool direction, volatile bool* stop_requested) {
    if (start_freq <= 0 || target_freq <= 0 || ramp_time <= 0 || *stop_requested) return;

    gpio_put(DIR_PIN, direction);
    uint64_t start_time = time_us_64();
    uint64_t ramp_duration_us = (uint64_t)(ramp_time * 1e6);

    float freq_diff = target_freq - start_freq;
    float freq_slope = freq_diff / ramp_time; // Frequency change per second

    while ((time_us_64() - start_time < ramp_duration_us) && !(*stop_requested)) {
        float elapsed = (float)(time_us_64() - start_time) / 1e6;
        float current_freq = start_freq + (freq_slope * elapsed);

        if ((freq_slope > 0 && current_freq > target_freq) || 
            (freq_slope < 0 && current_freq < target_freq)) {
            current_freq = target_freq;
        }

        generate_steps(current_freq, 1, direction, stop_requested);
    }
}

void perform_movement(movement_t movement, volatile bool* stop_requested) {
    if (*stop_requested) return;
    float start_freq = MIN_START_FREQ;
    uint max_speed = movement.speed;

    const float delta_f = movement.speed - MIN_START_FREQ;
    const float ramp_time = delta_f / (movement.acceleration * STEPS_PER_MM);
    
    int steps = movement.steps;
    bool direction = movement.direction;
    
    // Acceleration phase
    leib_ramp(start_freq, max_speed, ramp_time, direction, stop_requested);
    
    // Constant speed phase
    generate_steps(max_speed, steps, direction, stop_requested);
    
    // Deceleration phase (only if not interrupted)
    if (!*stop_requested) {
        leib_ramp(max_speed, start_freq, ramp_time, direction, stop_requested);
    }
}