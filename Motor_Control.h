#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "pico/stdlib.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

// Motor control pin definitions
#define DIR_PIN 18
#define STEP_PIN 19
#define EN_PIN 16

// Motor parameters
#define MIN_START_SPEED 1600      // Minimum starting frequency in Hz (steps/s)

// Data structure for movement parameters
typedef struct {
    uint32_t speed;        // Maximum speed in steps/s
    uint32_t acceleration; // Acceleration in steps/s²
    int32_t steps;         // Total steps to move
    bool direction;        // Direction: 0 or 1
} movement_t;

/**
 * @brief Initializes GPIO pins for motor control.
 */
void init_motor_GPIO();

/**
 * @brief Generates a specified number of steps at a given frequency.
 * 
 * @param frequency Frequency of steps in Hz (steps/s).
 * @param steps Number of steps to generate.
 * @param direction Direction of movement (0 or 1).
 * @param stop_requested Pointer to a flag indicating if movement should be interrupted.
 */
void generate_steps(float frequency, int steps, bool direction, volatile bool* stop_requested);

/**
 * @brief Generates a linear frequency ramp for smooth acceleration/deceleration.
 * 
 * @param start_freq Starting frequency in Hz (steps/s).
 * @param target_freq Target frequency in Hz (steps/s).
 * @param ramp_time Time for the ramp in seconds.
 * @param direction Direction of movement (0 or 1).
 * @param stop_requested Pointer to a flag indicating if movement should be interrupted.
 */
void leib_ramp(float start_freq, float target_freq, float ramp_time, bool direction, volatile bool* stop_requested);

/**
 * @brief Performs a complete trapezoidal movement profile with acceleration, 
 *        constant speed, and deceleration phases.
 * 
 * The function automatically calculates the step distribution across three phases:
 * 1. Acceleration from MIN_START_SPEED to movement.speed
 * 2. Constant velocity at movement.speed
 * 3. Deceleration from movement.speed to MIN_START_SPEED
 * 
 * If insufficient steps are available for a full trapezoid, a error is returned saying it is not possible
 * 
 * @param movement Movement parameters containing:
 *                 - speed: Maximum/target speed in steps/s
 *                 - acceleration: Acceleration rate in steps/s²
 *                 - steps: Total steps for the entire move
 *                 - direction: Movement direction (0 or 1)
 * @param stop_requested Pointer to a flag indicating if movement should be interrupted.
 */
void perform_movement(movement_t movement, volatile bool* stop_requested);

#endif // MOTOR_CONTROL_H