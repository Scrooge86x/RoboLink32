# RoboLink32

- [English](#en)
- [Polski](#pl)

## EN

**Table of contents**
- [Features](#features)
- [Communication (ESP-NOW)](#communication-esp-now)
- [Controls (controller)](#controls-controller)
- [Hardware requirements](#hardware-requirements)
- [Libraries (Arduino)](#libraries-arduino)
- [Configuration](#configuration)
- [Building and uploading](#building-and-uploading)

Wireless mobile robot control system based on ESP-NOW.  
Consists of a **controller** (handheld with TFT display) and a **robot** equipped with a VL53L5CX distance sensor (8×8), MPU6050 and dual-motor drive.

```
├── controller/          # Controller code (ESP32 + TFT)
│   ├── controller.ino
│   ├── commands.h
│   ├── comms.*
│   ├── config.h
│   ├── controls.*
│   ├── display.*
│   ├── pairing.*
│   └── User_Setup.h     # TFT_eSPI configuration
│
└── robot/               # Robot code (ESP32)
    ├── robot.ino
    ├── commands.h
    ├── config.h
    ├── distance-sensor.*
    ├── motor.*
    ├── mpu-sensor.*
    └── pairing.*
```

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

Every packet starts with a 1-byte `Command`:

| Command              | Description                               |
|----------------------|-------------------------------------------|
| `forwardSlow/Fast`   | Drive forward                             |
| `backwardSlow/Fast`  | Drive backward                            |
| `leftSlow/Fast`      | Turn left                                 |
| `rightSlow/Fast`     | Turn right                                |
| `broadcastPairing`   | Robot announces presence + name           |
| `pairRequest`        | Controller requests pairing               |
| `pairSuccess`        | Robot confirms + sends channel            |
| `requestDistanceData`| Request distance grid                     |
| `distanceData`       | Response: 8×8 × uint16 (mm)               |
| `requestMpuData`     | Request orientation data                  |
| `mpuData`            | Response: yaw, pitch, roll (float)        |

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
- MPU6050 (Electronic Cats)
- ESP-NOW (built-in)

## Configuration

### Controller
- `message::DISTANCE_REQUEST_INTERVAL` – how often distance data is requested (default 100 ms)
- `message::MPU_REQUEST_INTERVAL` – how often MPU data is requested (500 ms)
- `message::TIMEOUT_MS` – connection loss timeout (2000 ms)
- Heatmap range: `heatmap::MIN_DISTANCE` / `MAX_DISTANCE` (20–800 mm)

### Robot
- `control::OBSTACLE_THRESHOLD_MM` – obstacle threshold (150 mm)
- `control::MAX_BLOCKED_PIXELS` – number of close pixels that block forward movement
- `control::SLOW_SPEED` / `FAST_SPEED`
- `control::TIMEOUT_MS` – automatic motor stop (100 ms)
- Robot name: `pairing::robotName`

## Building and uploading

1. Open the appropriate folder (`controller` or `robot`) in Arduino IDE / PlatformIO.
2. Select an ESP32 board.
3. Install the required libraries.
4. For the controller, make sure `User_Setup.h` matches your display.
5. Upload the sketch.

## PL

**Spis treści**
- [Funkcje](#funkcje)
- [Komunikacja (ESP-NOW)](#komunikacja-esp-now)
- [Sterowanie (kontroler)](#sterowanie-kontroler)
- [Wymagania sprzętowe](#wymagania-sprzętowe)
- [Biblioteki (Arduino)](#biblioteki-arduino)
- [Konfiguracja](#konfiguracja)
- [Kompilacja i wgrywanie](#kompilacja-i-wgrywanie)

Bezprzewodowy system sterowania robotem mobilnym oparty o ESP-NOW.  
Składa się z **kontrolera** (handheld z wyświetlaczem TFT) oraz **robota** z czujnikiem odległości VL53L5CX (8×8), MPU6050 i napędem dwusilnikowym.

```
├── controller/          # Kod kontrolera (ESP32 + TFT)
│   ├── controller.ino
│   ├── commands.h
│   ├── comms.*
│   ├── config.h
│   ├── controls.*
│   ├── display.*
│   ├── pairing.*
│   └── User_Setup.h     # Konfiguracja TFT_eSPI
│
└── robot/               # Kod robota (ESP32)
    ├── robot.ino
    ├── commands.h
    ├── config.h
    ├── distance-sensor.*
    ├── motor.*
    ├── mpu-sensor.*
    └── pairing.*
```

## Funkcje

### Kontroler
- Joystick + przyciski do sterowania ruchem (wolno / szybko)
- Tryb parowania z listą wykrytych robotów
- Mapa ciepła (heatmap) z danych czujnika odległości 8×8
- Opcjonalny overlay danych MPU (yaw / pitch / roll)
- Zapamiętywanie sparowanego MAC + kanału w flash (Preferences)
- Timeout – po utracie łączności wyświetla „No robot paired”

### Robot
- Czujnik ToF VL53L5CX (siatka 8×8, częstotliwość 15 Hz)
- MPU6050 z DMP (orientacja)
- Unikanie przeszkód – blokuje jazdę do przodu gdy zbyt wiele pikseli jest blisko
- Automatyczne zatrzymanie silników po utracie komend (timeout)
- Parowanie przytrzymaniem przycisku (~2,5 s)
- Broadcast nazwy `"RoboLink32"` w trybie parowania

## Komunikacja (ESP-NOW)

Wszystkie pakiety zaczynają się od 1-bajtowego `Command`:

| Command              | Opis                                      |
|----------------------|-------------------------------------------|
| `forwardSlow/Fast`   | Jazda do przodu                           |
| `backwardSlow/Fast`  | Jazda do tyłu                             |
| `leftSlow/Fast`      | Obrót w lewo                              |
| `rightSlow/Fast`     | Obrót w prawo                             |
| `broadcastPairing`   | Robot ogłasza swoją obecność + nazwę      |
| `pairRequest`        | Kontroler prosi o sparowanie              |
| `pairSuccess`        | Robot potwierdza + podaje kanał           |
| `requestDistanceData`| Żądanie siatki odległości                 |
| `distanceData`       | Odpowiedź: 8×8 × uint16 (mm)              |
| `requestMpuData`     | Żądanie danych orientacji                 |
| `mpuData`            | Odpowiedź: yaw, pitch, roll (float)       |

### Przepływ parowania
1. Robot: przytrzymaj przycisk PAIR → wchodzi w tryb parowania, co 1 s wysyła `broadcastPairing` + nazwę na kanale 1.
2. Kontroler: Y+B → menu parowania, skanuje broadcasty, wybiera robota joystickiem, A → `pairRequest`.
3. Robot odpowiada `pairSuccess` + nowy kanał (domyślnie 2), obie strony przechodzą na ten kanał i zapisują MAC.
4. Po sparowaniu komunikacja odbywa się tylko między sparowanymi urządzeniami.

## Sterowanie (kontroler)

| Wejście              | Akcja                                      |
|----------------------|--------------------------------------------|
| Joystick             | Ruch (forward / back / left / right)       |
| Przycisk A (trzymany)| Tryb szybki                                |
| Y + B                | Wejście / wyjście z trybu parowania        |
| B (gdy sparowany)    | Włącz / wyłącz overlay MPU                 |
| X (w menu parowania) | Anuluj / wyjdź                             |

## Wymagania sprzętowe

### Kontroler
- ESP32 (zalecany ESP32-S3-Pico)
- Wyświetlacz ST7789 240×240 (zalecany Waveshare Pico LCD 1.3)
- Joystick (5 kierunków)
- 4 przyciski (A, B, X, Y)

Piny zdefiniowane w `controller/config.h` i `User_Setup.h`.

### Robot
- ESP32 (zalecany ESP32-C3-Zero)
- VL53L5CX (I²C)
- MPU6050 (I²C + INT)
- Sterownik silników (AIN1/2, BIN1/2, PWMA/B) – np. TB6612FNG
- Przycisk PAIR (domyślnie pin 9, współdzielony z RST czujnika. Dla ESP32-C3-Zero jest to przycisk BOOT)

Piny w `robot/config.h`.

## Biblioteki (Arduino)

**Kontroler**
- TFT_eSPI (skonfigurowany przez `User_Setup.h`)
- ESP-NOW (wbudowane)

**Robot**
- SparkFun VL53L5CX Arduino Library
- MPU6050 (Electronic Cats)
- ESP-NOW (wbudowane)

## Konfiguracja

### Kontroler
- `message::DISTANCE_REQUEST_INTERVAL` – jak często prosi o heatmapę (domyślnie 100 ms)
- `message::MPU_REQUEST_INTERVAL` – jak często prosi o MPU (500 ms)
- `message::TIMEOUT_MS` – po jakim czasie uznaje utratę łączności (2000 ms)
- Zakres heatmapy: `heatmap::MIN_DISTANCE` / `MAX_DISTANCE` (20–800 mm)

### Robot
- `control::OBSTACLE_THRESHOLD_MM` – próg przeszkody (150 mm)
- `control::MAX_BLOCKED_PIXELS` – ile pikseli może być „zajętych” zanim zablokuje jazdę do przodu
- `control::SLOW_SPEED` / `FAST_SPEED`
- `control::TIMEOUT_MS` – automatyczny stop silników (100 ms)
- Nazwa robota: `pairing::robotName`

## Kompilacja i wgrywanie

1. Otwórz odpowiedni folder (`controller` lub `robot`) w Arduino IDE / PlatformIO.
2. Wybierz płytkę ESP32.
3. Zainstaluj wymagane biblioteki.
4. Dla kontrolera upewnij się, że `User_Setup.h` jest poprawnie skonfigurowany pod Twój wyświetlacz.
5. Wgraj szkic.