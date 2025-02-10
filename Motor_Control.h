#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "pico/stdlib.h"
#include <stdint.h>
#include <stdbool.h>

// Motor control pin definitions
#define DIR_PIN 18
#define STEP_PIN 19
#define EN_PIN 16

// Motor parameters
#define MAX_SPEED_HZ 13000       // Maximum motor speed in Hz
#define MIN_START_FREQ 1600      // Minimum starting frequency in Hz
#define STEPS_PER_MM 80.0f       // Steps per millimeter (adjust based on your setup)
#define ACCEL_MMPS2 1500.0f      // Desired acceleration in mm/s²

// Data structure for movement parameters
typedef struct {
    uint16_t speed;
    uint16_t acceleration;
    int32_t steps;
    bool direction;
} movement_t;

/**
 * @brief Initializes GPIO pins for motor control.
 */
void init_motor_GPIO();

/**
 * @brief Generates a specified number of steps at a given frequency.
 * 
 * @param frequency Frequency of steps in Hz.
 * @param steps Number of steps to generate.
 * @param direction Direction of movement (true = forward, false = backward).
 * @param stop_requested Pointer to a flag indicating if movement should be interrupted.
 */
void generate_steps(float frequency, int steps, bool direction, volatile bool* stop_requested);

/**
 * @brief Generates a linear frequency ramp for smooth acceleration/deceleration.
 * 
 * @param start_freq Starting frequency in Hz.
 * @param target_freq Target frequency in Hz.
 * @param ramp_time Time for the ramp in seconds.
 * @param direction Direction of movement (true = forward, false = backward).
 * @param stop_requested Pointer to a flag indicating if movement should be interrupted.
 */
void leib_ramp(float start_freq, float target_freq, float ramp_time, bool direction, volatile bool* stop_requested);

/**
 * @brief Performs a complete movement with acceleration, constant speed, and deceleration.
 * 
 * @param start_freq Starting frequency in Hz.
 * @param max_speed Maximum speed in Hz.
 * @param ramp_time Time for acceleration/deceleration in seconds.
 * @param steps Number of steps at constant speed.
 * @param direction Direction of movement (true = forward, false = backward).
 * @param stop_requested Pointer to a flag indicating if movement should be interrupted.
 */
void perform_movement(movement_t, volatile bool* stop_requested);

#endif // MOTOR_CONTROL_H