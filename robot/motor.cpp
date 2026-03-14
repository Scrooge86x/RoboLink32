#include <Arduino.h>
#include "motor.h"
#include "config.h"

namespace motor {

void setup() {
  pinMode(pins::AIN1, OUTPUT);
  pinMode(pins::AIN2, OUTPUT);
  pinMode(pins::PWMA, OUTPUT);

  pinMode(pins::BIN1, OUTPUT);
  pinMode(pins::BIN2, OUTPUT);
  pinMode(pins::PWMB, OUTPUT);
}

void forward(const int speed) {
  digitalWrite(pins::AIN1, LOW);
  digitalWrite(pins::AIN2, HIGH);
  analogWrite(pins::PWMA, speed);

  digitalWrite(pins::BIN1, HIGH);
  digitalWrite(pins::BIN2, LOW);
  analogWrite(pins::PWMB, speed);
}

void reverse(const int speed) {
  digitalWrite(pins::AIN1, HIGH);
  digitalWrite(pins::AIN2, LOW);
  analogWrite(pins::PWMA, speed);

  digitalWrite(pins::BIN1, LOW);
  digitalWrite(pins::BIN2, HIGH);
  analogWrite(pins::PWMB, speed);
}

void left(const int speed) {
  digitalWrite(pins::AIN1, HIGH);
  digitalWrite(pins::AIN2, LOW);
  analogWrite(pins::PWMA, speed);

  digitalWrite(pins::BIN1, HIGH);
  digitalWrite(pins::BIN2, LOW);
  analogWrite(pins::PWMB, speed);
}

void right(const int speed) {
  digitalWrite(pins::AIN1, LOW);
  digitalWrite(pins::AIN2, HIGH);
  analogWrite(pins::PWMA, speed);

  digitalWrite(pins::BIN1, LOW);
  digitalWrite(pins::BIN2, HIGH);
  analogWrite(pins::PWMB, speed);
}

void stop() {
  digitalWrite(pins::AIN1, LOW);
  digitalWrite(pins::AIN2, LOW);
  analogWrite(pins::PWMA, 255);

  digitalWrite(pins::BIN1, LOW);
  digitalWrite(pins::BIN2, LOW);
  analogWrite(pins::PWMB, 255);
}

} // motor
