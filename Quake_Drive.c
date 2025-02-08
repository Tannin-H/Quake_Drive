#include "Motor_Control.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include <math.h>
#include <tusb.h>
#include <string.h>

#define MAX_LINE_LENGTH 64
#define STATUS_LED_PIN 2
#define FRONT_LIMIT_SWITCH_PIN 7
#define BACK_LIMIT_SWITCH_PIN 8

typedef enum {
    CMD_MOVE,
    CMD_RESET,
    CMD_STOP
} CommandType;

typedef struct {
    CommandType type;
    movement_t movement;
} Command;

queue_t command_queue;
volatile bool stop_requested = false;

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
    stop_requested = false;

    // Move backward until BACK limit switch is triggered or stopped
    while (gpio_get(BACK_LIMIT_SWITCH_PIN) == 1 && !stop_requested) {
        movement_t back_movement = { .speed = 500, .acceleration = 0, .steps = 1, .direction = true };
        perform_movement(back_movement, &stop_requested);
        step_count++;
    }
    if (stop_requested) return false;

    step_count = 0;

    // Move forward until FRONT limit switch is triggered or stopped
    while (gpio_get(FRONT_LIMIT_SWITCH_PIN) == 1 && !stop_requested) {
        movement_t for_movement = { .speed = 500, .acceleration = 0, .steps = 1, .direction = false };
        perform_movement(for_movement, &stop_requested);
        step_count++;
    }
    if (stop_requested) return false;

    // Move to the middle
    int middle_steps = step_count / 2;
    movement_t home_movement = { .speed = 1200, .acceleration = 0, .steps = middle_steps, .direction = true };
    perform_movement(home_movement, &stop_requested);

    return !stop_requested;
}

void process_text(const char *line, queue_t *command_queue) {
    char cmd[11]; // Increased to 11 to hold "BATCH_SIZE" (10 chars + null)
    int speed = 0, acceleration = 0, steps = 0, direction = 0;

    // Step 1: Read the full command name (up to 10 chars)
    if (sscanf(line, "%10s", cmd) == 1) { // Now uses %10s to read 10 characters
        if (strcmp(cmd, "BATCH_SIZE") == 0) {
            // Parse BATCH_SIZE (expect 1 argument)
            int batch_size;
            if (sscanf(line, "%10s %d", cmd, &batch_size) == 2) {
                queue_free(command_queue);
                queue_init(command_queue, sizeof(Command), batch_size);
                printf("[PICO] Queue resized to %d\n", batch_size);
            } else {
                printf("Invalid BATCH_SIZE command\n");
            }
        } else if (strcmp(cmd, "MOVE") == 0) {
            // Parse MOVE (4 arguments: speed, acceleration, steps, direction)
            if (sscanf(line, "%10s %d %d %d %d", cmd, &speed, &acceleration, &steps, &direction) == 5) {
                Command command = { 
                    .type = CMD_MOVE, 
                    .movement = {speed, acceleration, steps, direction}
                };
                queue_add_blocking(command_queue, &command);
            } else {
                printf("Invalid MOVE command\n");
            }
        } else if (strcmp(cmd, "STOP") == 0) {
            multicore_fifo_push_blocking(CMD_STOP);
        } else if (strcmp(cmd, "RESET") == 0) {
            multicore_fifo_push_blocking(CMD_RESET);
        } else {
            printf("Unknown command: %s\n", line);
        }
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
    while (!tud_cdc_connected()) sleep_ms(100);
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

bool check_for_commands(queue_t *command_queue) {
    if (tud_cdc_available()) {
        char line[MAX_LINE_LENGTH];
        read_line(line, sizeof(line));
        if (strlen(line) > 0) {
            printf("[PICO] Received: %s\n", line);
            process_text(line, command_queue);
            return true;
        }
    }
    return false;
}

void core1_entry() {
    while (true) {
        // Check for STOP/RESET commands via FIFO
        if (multicore_fifo_rvalid()) {
            uint32_t cmd = multicore_fifo_pop_blocking();
            if (cmd == CMD_STOP) {
                stop_requested = true;
                printf("[PICO] STOP command received\n");
            } else if (cmd == CMD_RESET) {
                stop_requested = true;
                reset_position();
                printf("[PICO] RESET command received\n");
            }
        }

        // Process movement commands
        Command cmd;
        if (queue_try_remove(&command_queue, &cmd)) {
            switch (cmd.type) {
                case CMD_MOVE:
                    perform_movement(cmd.movement, &stop_requested);
                    break;
                default:
                    break;
            }
        } else {
            tight_loop_contents(); // Minimize latency
        }
    }
}

int main() {
    stdio_init_all();
    init_motor_GPIO();
    init_peripherals_GPIO();
    start_serial_connection();
    sleep_ms(1000);

    reset_position();

    multicore_launch_core1(core1_entry);

    while (true) {
        check_for_commands(&command_queue);
        sleep_ms(10); // Reduce CPU usage
    }

    return 0;
}