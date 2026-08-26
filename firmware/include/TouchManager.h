#pragma once

#include <Arduino.h>

class DisplayManager;

// Kapazitiver GT911-Touch des ESP32-8048S070C. Gelesen ueber LovyanGFX
// (display.raw().getTouch) - kapazitiv ist werksseitig kalibriert, daher keine
// Kalibrierroutine noetig: isCalibrated() ist immer true, runCalibration() ein
// No-Op. Schnittstelle bleibt kompatibel zur resistiven 2,8"-Variante, damit
// die UI-Views (read/isCalibrated) unveraendert bleiben.
class TouchManager {
public:
	void begin(DisplayManager &display);

	bool isCalibrated() const { return true; }
	void runCalibration(DisplayManager &) {}

	// true, wenn aktuell beruehrt; screenX/screenY in Bildschirmkoordinaten
	// (0..799 x, 0..479 y).
	bool read(int16_t &screenX, int16_t &screenY);

private:
	DisplayManager *_display = nullptr;
};
