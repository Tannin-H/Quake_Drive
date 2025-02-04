#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "pico/stdlib.h"
#include <stdint.h>
#include <stdbool.h>

// Motor control pin definitions
#define DIR_PIN 18
#define STEP_PIN 19
#define EN_PIN 16
#define STATUS_LED_PIN 2

// Motor parameters
#define MAX_SPEED_HZ 13000
#define MIN_START_FREQ 1600
#define STEPS_PER_MM 80.0f    // Adjust based on your setup (steps/mm)
#define ACCEL_MMPS2 1500.0f    // Desired acceleration in mm/s²
#define NUM_STEPS 2000

void init_GPIO();
void generate_steps(float frequency, int steps, bool direction);
void leib_ramp(float start_freq, float target_freq, float ramp_time, bool direction);
void perform_movement(float start_freq, float max_speed, float ramp_time, int steps, bool direction);

#endif // MOTOR_CONTROL_H
