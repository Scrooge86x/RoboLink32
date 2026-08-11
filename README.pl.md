# RoboLink32

Bezprzewodowy system sterowania robotem mobilnym oparty o ESP-NOW.  
Składa się z kodu dla **kontrolera** (handheld z wyświetlaczem TFT) oraz **robota** (czujnik odległości VL53L5CX 8×8, MPU6050, napęd dwusilnikowy).

[English version](README.md)

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

Wszystkie pakiety zaczynają się od 1-bajtowego `Command` (wartości z `commands.h`):

| Wartość | Command              | Opis                                      |
|--------:|----------------------|-------------------------------------------|
| 0       | `forwardSlow`        | Jazda do przodu (wolno)                   |
| 1       | `forwardFast`        | Jazda do przodu (szybko)                  |
| 2       | `backwardSlow`       | Jazda do tyłu (wolno)                     |
| 3       | `backwardFast`       | Jazda do tyłu (szybko)                    |
| 4       | `leftSlow`           | Obrót w lewo (wolno)                      |
| 5       | `leftFast`           | Obrót w lewo (szybko)                     |
| 6       | `rightSlow`          | Obrót w prawo (wolno)                     |
| 7       | `rightFast`          | Obrót w prawo (szybko)                    |
| 8       | `broadcastPairing`   | Robot ogłasza swoją obecność + nazwę      |
| 9       | `pairRequest`        | Kontroler prosi o sparowanie              |
| 10      | `pairSuccess`        | Robot potwierdza + podaje kanał           |
| 11      | `requestDistanceData`| Żądanie siatki odległości                 |
| 12      | `distanceData`       | Odpowiedź: 8×8 × uint16 (mm)              |
| 13      | `requestMpuData`     | Żądanie danych orientacji                 |
| 14      | `mpuData`            | Odpowiedź: yaw, pitch, roll (float)       |

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
- MPU6050_6Axis_MotionApps20 (I2Cdevlib)
- ESP-NOW (wbudowane)

## Konfiguracja

Wszystkie parametry (timery, prędkości, progi, kanały, nazwa robota itd.) znajdują się w:

- `controller/config.h`
- `robot/config.h`

## Kompilacja i wgrywanie

1. Otwórz odpowiedni folder (`controller` lub `robot`) w Arduino IDE.
2. Wybierz płytkę ESP32.
3. Zainstaluj wymagane biblioteki.
4. Dla kontrolera upewnij się, że `User_Setup.h` jest poprawnie skonfigurowany pod Twój wyświetlacz.
5. Wgraj szkic.
