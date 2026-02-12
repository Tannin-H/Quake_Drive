# Shake Table - Pico Firmware

Stepper motor control firmware for Raspberry Pi Pico implemented trapezoidal motion profiles and limit switch safety.
All movements commands are sent via wired serial from the [control application](https://github.com/Tannin-H/Quake_Table) and processed in a FIFO order with the exception of the STOP and RESET commands that are processed as soon as recieved.

## Hardware

**Pin Configuration:**
- DIR: GPIO 18, STEP: GPIO 19, EN: GPIO 16
- Front Limit: GPIO 7, Back Limit: GPIO 8
- Status LED: GPIO 2

## Commands Format

```bash
MANUAL <speed> <accel> <steps> <dir> <speed> <accel> <steps> <dir>  # Continuous motion
MOVE <speed> <accel> <steps> <dir>                                    # Single movement
STOP                                                                   # Emergency stop
RESET                                                                  # Re-home table
```

Parameters: speed (steps/s), acceleration (steps/s²), steps (int), direction (0/1)

## Building

Built using CMake with the Pico SDK and Ninja. Installing the Pico SDK extension for vscode is the easiest way to manage the builds.

```bash
# Set up build directory
mkdir build && cd build

# Configure
cmake -G Ninja ..

# Build
ninja

# Flash to Pico (hold BOOTSEL button while connecting USB)
# Copy Quake_Drive.uf2 to the mounted Pico drive
```
