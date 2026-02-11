#include "Motor_Control.h"
#include "timing.h"
#include <stdio.h>

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
    
    float start_speed = MIN_START_SPEED;  // steps/s
    float max_speed = movement.speed;     // steps/s
    float acceleration = movement.acceleration; // steps/s²
    int total_steps = movement.steps;
    bool direction = movement.direction;
    
    // If max_speed is below MIN_START_SPEED, use max_speed as both start and target
    if (max_speed < MIN_START_SPEED) {
        start_speed = max_speed;
    }

    // Now check if we even need to accelerate
    if (max_speed <= start_speed) {
        // Run at constant speed
        timing_start_profile(total_steps, start_speed, acceleration);
        uint64_t phase_start = time_us_64();
        generate_steps(start_speed, total_steps, direction, stop_requested);
        timing_record_phase("Const phase:", total_steps, phase_start);
        timing_end_profile();
        return;
    }
    
    // Calculate ramp time: t = Δv / a
    float delta_v = max_speed - start_speed;
    float ramp_time = delta_v / acceleration;  // seconds
    
    // Calculate steps during acceleration phase
    // Using: distance = v0*t + 0.5*a*t²
    float accel_steps_f = start_speed * ramp_time + 0.5f * acceleration * ramp_time * ramp_time;
    int accel_steps = (int)(accel_steps_f + 0.5f);
    int decel_steps = accel_steps;
    
    // Calculate remaining steps for constant velocity phase
    int constant_steps = total_steps - accel_steps - decel_steps;
    
    // Check if we have enough steps for a full trapezoidal profile
    if (constant_steps < 0) {
        // Not enough distance for the requested speed/acceleration profile
        printf("[ERROR] Motion profile impossible: %d steps required for accel+decel, but only %d steps available\n", 
            accel_steps + decel_steps, total_steps);
        printf("[ERROR] Reduce speed from %.1f steps/s or increase distance from %d steps\n", 
            max_speed, total_steps);
        return;
    }
    
    // Start timing instrumentation
    timing_start_profile(total_steps, max_speed, acceleration);
    uint64_t phase_start;
    
    // Execute the motion profile
    
    // 1. Acceleration phase
    if (!*stop_requested) {
        phase_start = time_us_64();
        leib_ramp(start_speed, max_speed, ramp_time, direction, stop_requested);
        timing_record_phase("Accel phase:", accel_steps, phase_start);
    }
    
    // 2. Constant velocity phase
    if (!*stop_requested && constant_steps > 0) {
        phase_start = time_us_64();
        generate_steps(max_speed, constant_steps, direction, stop_requested);
        timing_record_phase("Const phase:", constant_steps, phase_start);
    } else if (constant_steps == 0) {
        timing_record_phase("Const phase:", 0, 0);
    }
    
    // 3. Deceleration phase
    if (!*stop_requested) {
        phase_start = time_us_64();
        leib_ramp(max_speed, start_speed, ramp_time, direction, stop_requested);
        timing_record_phase("Decel phase:", decel_steps, phase_start);
    }
    
    timing_end_profile();
}