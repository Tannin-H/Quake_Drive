#include "timing.h"
#include <stdio.h>
#include "pico/time.h"

void timing_start_profile(int total_steps, float max_speed, float acceleration) {
    printf("\n=== Motion Profile Timing ===\n");
    printf("Total steps: %d, Max speed: %.1f steps/s, Accel: %.1f steps/s²\n", 
           total_steps, max_speed, acceleration);
}

void timing_record_phase(const char* phase_name, int steps, uint64_t start_time) {
    uint64_t end_time = time_us_64();
    float duration_ms = (float)(end_time - start_time) / 1000.0f;
    
    if (steps == 0 && strstr(phase_name, "Const") != NULL) {
        printf("%s  %d steps (triangular profile)\n", phase_name, steps);
    } else {
        printf("%s  %d steps, %.2f ms\n", phase_name, steps, duration_ms);
    }
}

void timing_end_profile(void) {
    printf("=============================\n\n");
}