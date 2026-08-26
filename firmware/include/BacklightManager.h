#pragma once

#include <Arduino.h>

#include "pins.h"

// Helligkeitsregelung ueber LEDC-PWM auf BACKLIGHT_PIN (siehe pins.h).
// TFT_eSPI schaltet den Pin beim Init nur digital HIGH/LOW; ledcAttach()
// uebernimmt ihn danach fuer PWM - daher muss begin() erst NACH
// DisplayManager::begin() aufgerufen werden.
class BacklightManager {
public:
	static constexpr uint32_t kPwmFreqHz = 5000;
	static constexpr uint8_t kPwmResolutionBits = 8;
	static constexpr uint8_t kPwmChannel = 4; // 0-3 oft von anderen Peripherien belegt

	void begin(uint8_t initialPercent);
	void setPercent(uint8_t percent);
	uint8_t percent() const { return currentPercent; }

private:
	uint8_t currentPercent = 60;
};
