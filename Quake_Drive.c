#include "pico/stdlib.h"
#include "Queue.h"
#include "Motor_Control.h"
#include <stdio.h>
#include <math.h>
#include <tusb.h>
#include <string.h>

#define MAX_LINE_LENGTH 64
#define STATUS_LED_PIN 2
#define FRONT_LIMIT_SWITCH_PIN 7
#define BACK_LIMIT_SWITCH_PIN 8

void init_peripherals_GPIO() {
    gpio_init(STATUS_LED_PIN);
    gpio_init(FRONT_LIMIT_SWITCH_PIN);
    gpio_init(BACK_LIMIT_SWITCH_PIN);
    
    gpio_set_dir(STATUS_LED_PIN, GPIO_OUT);
    gpio_set_dir(FRONT_LIMIT_SWITCH_PIN, GPIO_IN);
    gpio_set_dir(BACK_LIMIT_SWITCH_PIN, GPIO_IN);

    gpio_pull_up(FRONT_LIMIT_SWITCH_PIN);
    gpio_pull_up(BACK_LIMIT_SWITCH_PIN);
}


bool reset_position() {
    int step_count = 0;

    // Move backward until the BACK limit switch is triggered
    while (gpio_get(BACK_LIMIT_SWITCH_PIN) == 1) {  
        perform_movement(MIN_START_FREQ, 500, 0, 1, true);
        step_count++;
    }

    // Reset step count
    step_count = 0;

    // Move forward until the FRONT limit switch is triggered
    while (gpio_get(FRONT_LIMIT_SWITCH_PIN) == 1) {  
        perform_movement(MIN_START_FREQ, 500, 0, 1, false);
        step_count++;
    }

    // Move to the middle (halfway between both limit switches)
    int middle_steps = step_count / 2;
    perform_movement(500, MIN_START_FREQ, 0, middle_steps, true); 

    return true;
}

void process_text(const char *line, queue_t *movement_queue) {
    char cmd[10];
    int speed = 0, acceleration = 0, steps = 0;

    int parsed = sscanf(line, "%9s %d %d %d", cmd, &speed, &acceleration, &steps);

    if (strcmp(cmd, "MOVE") == 0 && parsed == 4) {
        movement_t movement = {speed, acceleration, steps};
        enqueue(movement_queue, movement);
    } else if (strcmp(cmd, "RESET") == 0 && parsed == 1) {
        printf("[PICO] Reset command received\n");
        reset_position();
    } else {
        printf("Unknown or malformed command: %s\n", line);
    }
}

void read_line(char *buffer, int buffer_size) {
    int index = 0;
    while (index < buffer_size - 1) {
        int c = getchar();
        if (c == '\n' || c == '\r') {
            buffer[index] = '\0';
            break;
        }
        buffer[index++] = (char)c;
    }
    buffer[buffer_size - 1] = '\0';
}

void start_serial_connection() {
    while (!tud_cdc_connected()) {
        sleep_ms(100);
    }

    char buffer[6];
    while (true) {
        printf("OK\n");

        if (tud_cdc_available()) {
            uint32_t bytes_read = tud_cdc_read(buffer, sizeof(buffer) - 1);
            buffer[bytes_read] = '\0';

            if (strncmp(buffer, "CONF\n", 4) == 0) {
                gpio_put(STATUS_LED_PIN, 1);
                break;
            }
        }

        sleep_ms(300);
    }
}

bool check_for_commands(queue_t *movement_queue) {
    if (tud_cdc_available()) {
        char line[MAX_LINE_LENGTH];
        read_line(line, sizeof(line));
        printf("[PICO] Received: %s\n", line);
        if (strlen(line) > 0) {
            process_text(line, movement_queue);
            return true;
        }
    }
    return false;
}

int main() {
    stdio_init_all();
    init_motor_GPIO();
    init_peripherals_GPIO();
    start_serial_connection();

    sleep_ms(1000);

    reset_position();  
    
    queue_t* movement_queue = create_queue();
    bool commands_pending = false;
    while (commands_pending == false)
    {   
        printf("[PICO] Waiting for commands\n");
        commands_pending = check_for_commands(movement_queue);
        sleep_ms(100);
    }
    

    movement_t movement = dequeue(movement_queue);
    const float delta_f = movement.speed - MIN_START_FREQ;
    const float ramp_time = delta_f / (movement.acceleration * STEPS_PER_MM);
    perform_movement(MIN_START_FREQ, movement.speed, ramp_time, movement.steps, true);
    printf("[PICO] Movement completed\n");

    return 0;
}