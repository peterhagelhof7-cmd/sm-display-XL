#pragma once

#include "LGFX_XL.h"  // definiert LGFX_XL + Alias `using TFT_eSPI = LGFX_XL;`

// Kapselt die Display-Ansteuerung des ESP32-8048S070C (7" 800x480 RGB-Parallel,
// natives Querformat) ueber LovyanGFX. Die numerischen Fonts (Uhrzeit/Temperatur)
// nutzen weiterhin die eingebauten LovyanGFX-Fonts (Font 7 = 7-Segment-Stil,
// Font 4 fuer Text) - fuer die hoehere Aufloesung ueber setTextSize vergroessert
// (siehe Views).
class DisplayManager {
public:
	static constexpr int16_t kScreenWidth = 800;
	static constexpr int16_t kScreenHeight = 480;

	void begin();
	void drawBootScreen(const char *line1, const char *line2);

	TFT_eSPI &raw() { return tft; }

private:
	TFT_eSPI tft;
};
