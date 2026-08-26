#pragma once
// ESP32-8048S070C. Die Display-/Touch-Pins (RGB-Parallel + GT911-I2C) stehen in
// LGFX_XL.h und werden von LovyanGFX verwaltet. Hier nur die uebrigen Peripherie-
// Pins:
#define BACKLIGHT_PIN 2       // BCKL (aktiv HIGH), per LEDC-PWM vom BacklightManager
#define DHT11_PIN 18          // DHT11 am P4-Extension-Anschluss
#define DHT11_TYPE DHT11
