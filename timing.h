#ifndef TIMING_H
#define TIMING_H

#include <stdint.h>
#include <string.h>
#include "pico/time.h"

// Timing instrumentation functions
void timing_start_profile(int total_steps, float max_speed, float acceleration);
void timing_record_phase(const char* phase_name, int steps, uint64_t start_time);
void timing_end_profile(void);

#endif // TIMING_H