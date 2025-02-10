//Quake Drive - Pico Firmware
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
    CMD_STOP,
    CMD_MANUAL
} CommandType;

typedef struct {
    CommandType type;
    union {
        movement_t movement;
        struct {
            movement_t forward;
            movement_t backward;
        } manual;
    };
} Command;

queue_t command_queue;
volatile bool stop_requested = false;
volatile bool manual_mode = false;
movement_t manual_forward;
movement_t manual_backward;

void front_limit_isr(uint gpio, uint32_t events) {
    if (!stop_requested) { // Ignore during reset
        stop_requested = true;
        printf("[PICO] Front limit switch triggered, stopping motor.\n");
    }
}

void back_limit_isr(uint gpio, uint32_t events) {
    if (!stop_requested) { // Ignore during reset
        stop_requested = true;
        printf("[PICO] Back limit switch triggered, stopping motor.\n");
    }
}

void init_peripherals_GPIO() {
    gpio_init(STATUS_LED_PIN);
    gpio_init(FRONT_LIMIT_SWITCH_PIN);
    gpio_init(BACK_LIMIT_SWITCH_PIN);

    gpio_set_dir(STATUS_LED_PIN, GPIO_OUT);
    gpio_set_dir(FRONT_LIMIT_SWITCH_PIN, GPIO_IN);
    gpio_set_dir(BACK_LIMIT_SWITCH_PIN, GPIO_IN);

    gpio_pull_up(FRONT_LIMIT_SWITCH_PIN);
    gpio_pull_up(BACK_LIMIT_SWITCH_PIN);

    // Attach interrupts for limit switches
    gpio_set_irq_enabled_with_callback(FRONT_LIMIT_SWITCH_PIN, GPIO_IRQ_EDGE_FALL, true, &front_limit_isr);
    gpio_set_irq_enabled_with_callback(BACK_LIMIT_SWITCH_PIN, GPIO_IRQ_EDGE_FALL, true, &back_limit_isr);
}


bool reset_position() {
    int step_count = 0;
    stop_requested = false;

    // Disable limit switch interrupts during reset
    gpio_set_irq_enabled(FRONT_LIMIT_SWITCH_PIN, GPIO_IRQ_EDGE_FALL, false);
    gpio_set_irq_enabled(BACK_LIMIT_SWITCH_PIN, GPIO_IRQ_EDGE_FALL, false);

    while (gpio_get(BACK_LIMIT_SWITCH_PIN) == 1 && !stop_requested) {
        movement_t back_movement = { .speed = 1200, .acceleration = 100, .steps = 1, .direction = true };
        perform_movement(back_movement, &stop_requested);
        step_count++;
    }
    if (stop_requested) return false;

    step_count = 0;

    while (gpio_get(FRONT_LIMIT_SWITCH_PIN) == 1 && !stop_requested) {
        movement_t for_movement = { .speed = 1200, .acceleration = 100, .steps = 1, .direction = false };
        perform_movement(for_movement, &stop_requested);
        step_count++;
    }
    if (stop_requested) return false;

    int middle_steps = step_count / 2;
    printf("[PICO] Middle steps: %d\n", middle_steps);
    movement_t home_movement = { .speed = 1600, .acceleration = 100, .steps = middle_steps, .direction = true };
    perform_movement(home_movement, &stop_requested);

    // Re-enable limit switch interrupts after reset
    gpio_set_irq_enabled(FRONT_LIMIT_SWITCH_PIN, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BACK_LIMIT_SWITCH_PIN, GPIO_IRQ_EDGE_FALL, true);

    return !stop_requested;
}


void process_text(const char *line, queue_t *command_queue) {
    char cmd[11];
    int params[8];

    if (sscanf(line, "%10s", cmd) == 1) {
        if (strcmp(cmd, "BATCH_SIZE") == 0) {
            stop_requested = false;
            int batch_size;
            if (sscanf(line, "%10s %d", cmd, &batch_size) == 2) {
                queue_free(command_queue);
                queue_init(command_queue, sizeof(Command), batch_size);
                printf("[PICO] Queue resized to %d\n", batch_size);
            }
        }
        else if (strcmp(cmd, "MOVE") == 0) {
            if (sscanf(line, "%10s %d %d %d %d", cmd, 
                     &params[0], &params[1], &params[2], &params[3]) == 5) {
                Command command = { 
                    .type = CMD_MOVE,
                    .movement = {params[0], params[1], params[2], params[3]}
                };
                queue_add_blocking(command_queue, &command);
            }
        }
        else if (strcmp(cmd, "MANUAL") == 0) {
            if (sscanf(line, "%10s %d %d %d %d %d %d %d %d", cmd,
                     &params[0], &params[1], &params[2], &params[3],
                     &params[4], &params[5], &params[6], &params[7]) == 9) {
                Command command = {
                    .type = CMD_MANUAL,
                    .manual.forward = {params[0], params[1], params[2], params[3]},
                    .manual.backward = {params[4], params[5], params[6], params[7]}
                };
                queue_add_blocking(command_queue, &command);
            }
        }
        else if (strcmp(cmd, "STOP") == 0) {
            stop_requested = true;
            multicore_fifo_push_blocking(CMD_STOP);
        }
        else if (strcmp(cmd, "RESET") == 0) {
            multicore_fifo_push_blocking(CMD_RESET);
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
        if (multicore_fifo_rvalid()) {
            uint32_t cmd = multicore_fifo_pop_blocking();
            if (cmd == CMD_STOP) {
                printf("[PICO] STOP command received\n");
                // Clear the command queue
                Command discarded;
                while (queue_try_remove(&command_queue, &discarded)) {
                    // Discard all queued commands
                }
            }
            else if (cmd == CMD_RESET) {
                stop_requested = true;
                reset_position();
                printf("[PICO] RESET command received\n");
            }
        }

        if (manual_mode) {
            stop_requested = false;
            while (!stop_requested) {
                perform_movement(manual_forward, &stop_requested);
                if (stop_requested) break;
                perform_movement(manual_backward, &stop_requested);
                if (stop_requested) break;

                if (multicore_fifo_rvalid()) {
                    uint32_t fifo_cmd = multicore_fifo_pop_blocking();
                    if (fifo_cmd == CMD_STOP) {
                        stop_requested = true;
                        printf("[PICO] STOP received in manual mode\n");
                        break;
                    }
                    else if (fifo_cmd == CMD_RESET) {
                        stop_requested = true;
                        reset_position();
                        printf("[PICO] RESET received in manual mode\n");
                        break;
                    }
                }
            }
            manual_mode = false;
        }
        else {
            Command cmd;
            if (queue_try_remove(&command_queue, &cmd)) {
                switch (cmd.type) {
                    case CMD_MOVE:
                        perform_movement(cmd.movement, &stop_requested);
                        break;
                    case CMD_MANUAL:
                        manual_forward = cmd.manual.forward;
                        manual_backward = cmd.manual.backward;
                        manual_mode = true;
                        break;
                    default:
                        break;
                }
            }
        }
        tight_loop_contents();
    }
}

int main() {
    stdio_init_all();
    init_motor_GPIO();
    init_peripherals_GPIO();
    start_serial_connection();
    sleep_ms(1000);

    reset_position();

    queue_init(&command_queue, sizeof(Command), 10);
    multicore_launch_core1(core1_entry);

    while (true) {
        check_for_commands(&command_queue);
        sleep_ms(10);
    }

    return 0;
}