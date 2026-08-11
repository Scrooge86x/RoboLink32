# RoboLink32

Wireless mobile robot control system based on ESP-NOW.  
Consists of code for the **controller** (handheld with TFT display) and the **robot** (VL53L5CX 8×8 distance sensor, MPU6050, dual-motor drive).

[Polska wersja](README.pl.md)

## Features

### Controller
- Joystick + buttons for movement control (slow / fast)
- Pairing mode with a list of discovered robots
- Heatmap from 8×8 distance sensor data
- Optional MPU overlay (yaw / pitch / roll)
- Paired MAC + channel stored in flash (Preferences)
- Timeout – shows “No robot paired” after losing connection

### Robot
- VL53L5CX ToF sensor (8×8 grid, 15 Hz)
- MPU6050 with DMP (orientation)
- Obstacle avoidance – blocks forward movement when too many pixels are close
- Automatic motor stop after command timeout
- Pairing by holding the button (~2.5 s)
- Broadcasts the name `"RoboLink32"` while in pairing mode

## Communication (ESP-NOW)

Every packet starts with a 1-byte `Command` (values from `commands.h`):

| Value | Command              | Description                               |
|------:|----------------------|-------------------------------------------|
| 0     | `forwardSlow`        | Drive forward (slow)                      |
| 1     | `forwardFast`        | Drive forward (fast)                      |
| 2     | `backwardSlow`       | Drive backward (slow)                     |
| 3     | `backwardFast`       | Drive backward (fast)                     |
| 4     | `leftSlow`           | Turn left (slow)                          |
| 5     | `leftFast`           | Turn left (fast)                          |
| 6     | `rightSlow`          | Turn right (slow)                         |
| 7     | `rightFast`          | Turn right (fast)                         |
| 8     | `broadcastPairing`   | Robot announces presence + name           |
| 9     | `pairRequest`        | Controller requests pairing               |
| 10    | `pairSuccess`        | Robot confirms + sends channel            |
| 11    | `requestDistanceData`| Request distance grid                     |
| 12    | `distanceData`       | Response: 8×8 × uint16 (mm)               |
| 13    | `requestMpuData`     | Request orientation data                  |
| 14    | `mpuData`            | Response: yaw, pitch, roll (float)        |

### Pairing flow
1. Robot: hold the PAIR button → enters pairing mode and sends `broadcastPairing` + name every 1 s on channel 1.
2. Controller: Y+B → pairing menu, scans broadcasts, select robot with joystick, A → `pairRequest`.
3. Robot replies with `pairSuccess` + new channel (default 2). Both sides switch to that channel and store the MAC.
4. After pairing, communication occurs only between the paired devices.

## Controls (controller)

| Input                | Action                                     |
|----------------------|--------------------------------------------|
| Joystick             | Movement (forward / back / left / right)   |
| Button A (held)      | Fast mode                                  |
| Y + B                | Enter / exit pairing mode                  |
| B (when paired)      | Toggle MPU overlay                         |
| X (in pairing menu)  | Cancel / exit                              |

## Hardware requirements

### Controller
- ESP32 (recommended ESP32-S3-Pico)
- ST7789 240×240 display (recommended Waveshare Pico LCD 1.3)
- 5-way joystick
- 4 buttons (A, B, X, Y)

Pins are defined in `controller/config.h` and `User_Setup.h`.

### Robot
- ESP32 (recommended ESP32-C3-Zero)
- VL53L5CX (I²C)
- MPU6050 (I²C + INT)
- Motor driver (AIN1/2, BIN1/2, PWMA/B) – e.g. TB6612FNG
- PAIR button (default pin 9, shared with sensor RST. On ESP32-C3-Zero this is the BOOT button)

Pins are defined in `robot/config.h`.

## Libraries (Arduino)

**Controller**
- TFT_eSPI (configured via `User_Setup.h`)
- ESP-NOW (built-in)

**Robot**
- SparkFun VL53L5CX Arduino Library
- MPU6050_6Axis_MotionApps20 (I2Cdevlib)
- ESP-NOW (built-in)

## Configuration

All tunable parameters (timings, speeds, thresholds, channels, robot name, etc.) are defined in:

- `controller/config.h`
- `robot/config.h`

## Building and uploading

1. Open the appropriate folder (`controller` or `robot`) in Arduino IDE.
2. Select an ESP32 board.
3. Install the required libraries.
4. For the controller, make sure `User_Setup.h` matches your display.
5. Upload the sketch.