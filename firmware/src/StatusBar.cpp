#include "StatusBar.h"

#include <math.h>

#include "Layout.h"

namespace {
// TFT_LIGHTGREY (211,211,211) hatte auf Weiss noch zu wenig Kontrast, auch
// nachdem die eigentliche Ursache (Farbinvertierung, siehe DisplayManager)
// behoben war. TFT_DARKGREY (128,128,128) ist deutlich lesbarer und noch
// als "grau" statt schwarz erkennbar.
constexpr uint16_t kIconColor = TFT_DARKGREY;
}

void StatusBar::drawGearIcon(TFT_eSPI &tft, int16_t cx, int16_t cy, int16_t r, uint16_t bgColor) const {
	// Gefuelltes Zahnrad statt duenner Umrisslinien (vorherige Version war
	// bei der kleinen Symbolgroesse zu grazil/schwer erkennbar) - massiver
	// Ring mit ausgeschnittener Mitte, Zaehne als kleine gefuellte Rechtecke.
	tft.fillCircle(cx, cy, r, kIconColor);
	tft.fillCircle(cx, cy, r - 5, bgColor);

	constexpr int8_t kTeeth = 8;
	constexpr int16_t kToothLen = 4;
	constexpr int16_t kToothHalfWidth = 2;
	for (int8_t i = 0; i < kTeeth; i++) {
		float angle = i * (2.0f * PI / kTeeth);
		float cosA = cosf(angle);
		float sinA = sinf(angle);
		float nx = -sinA;
		float ny = cosA;
		int16_t baseX = cx + static_cast<int16_t>(r * cosA);
		int16_t baseY = cy + static_cast<int16_t>(r * sinA);
		int16_t tipX = cx + static_cast<int16_t>((r + kToothLen) * cosA);
		int16_t tipY = cy + static_cast<int16_t>((r + kToothLen) * sinA);
		int16_t ox = static_cast<int16_t>(nx * kToothHalfWidth);
		int16_t oy = static_cast<int16_t>(ny * kToothHalfWidth);
		tft.fillTriangle(baseX + ox, baseY + oy, baseX - ox, baseY - oy, tipX, tipY, kIconColor);
	}
}

void StatusBar::drawWifiIcon(TFT_eSPI &tft, int16_t x, int16_t y, int8_t bars) const {
	// bars: -1 = kein WLAN (blinkt), 0-3 = Empfangsstaerke.
	if (bars < 0) {
		bool visible = (millis() / 500) % 2 == 0;
		if (!visible) {
			return;
		}
		bars = 3; // Umriss aller 3 Balken beim Blinken anzeigen
		for (int8_t b = 0; b < 3; b++) {
			int16_t bh = 5 + b * 5;
			tft.drawRect(x + b * 8, y + 16 - bh, 6, bh, kIconColor);
		}
		return;
	}
	for (int8_t b = 0; b < 3; b++) {
		int16_t bh = 5 + b * 5;
		int16_t by = y + 16 - bh;
		if (b < bars) {
			tft.fillRect(x + b * 8, by, 6, bh, kIconColor);
		} else {
			tft.drawRect(x + b * 8, by, 6, bh, kIconColor);
		}
	}
}

void StatusBar::drawInfoIcon(TFT_eSPI &tft, int16_t cx, int16_t cy, int16_t r, uint16_t bgColor) const {
	tft.drawCircle(cx, cy, r, kIconColor);
	tft.setTextColor(kIconColor, bgColor);
	tft.setTextDatum(MC_DATUM);
	tft.setTextFont(4);
	tft.drawString("i", cx, cy + 1);
	tft.setTextDatum(TL_DATUM);
}

void StatusBar::draw(DisplayManager &display, WlanManager &wlan, bool sensorValid, float tempC,
                      float humidityPct, const String &timeHHMM, const String &dateLine,
                      bool showBottomBar, uint16_t bgColor, const String &alertSource, bool alertBlue) {
	int8_t bars = wlan.signalBars();
	bool blinking = bars < 0; // Symbol animiert sich selbst, siehe drawWifiIcon()
	int tempRounded = sensorValid ? static_cast<int>(lroundf(tempC)) : 0;
	int humidityRounded = sensorValid ? static_cast<int>(lroundf(humidityPct)) : 0;

	bool changed = !everDrawn || bars != lastBars || sensorValid != lastSensorValid ||
	               tempRounded != lastTempRounded || humidityRounded != lastHumidityRounded ||
	               timeHHMM != lastTimeHHMM || dateLine != lastDateLine ||
	               showBottomBar != lastShowBottomBar || bgColor != lastBgColor ||
	               alertSource != lastAlertSource || alertBlue != lastAlertBlue;
	if (!changed && !blinking) {
		return;
	}
	everDrawn = true;
	lastBars = bars;
	lastSensorValid = sensorValid;
	lastTempRounded = tempRounded;
	lastHumidityRounded = humidityRounded;
	lastTimeHHMM = timeHHMM;
	lastDateLine = dateLine;
	lastShowBottomBar = showBottomBar;
	lastBgColor = bgColor;
	lastAlertSource = alertSource;
	lastAlertBlue = alertBlue;

	TFT_eSPI &tft = display.raw();

	// --- obere Leiste: Zahnrad, WLAN, DHT11 ---
	tft.fillRect(0, 0, DisplayManager::kScreenWidth, Layout::kStatusBarHeight, bgColor);
	tft.drawFastHLine(0, Layout::kStatusBarHeight - 1, DisplayManager::kScreenWidth, kIconColor);

	drawGearIcon(tft, 18, Layout::kStatusBarHeight / 2, 11, bgColor);
	drawWifiIcon(tft, 44, Layout::kStatusBarHeight / 2 - 8, bars);
	drawInfoIcon(tft, 92, Layout::kStatusBarHeight / 2, 11, bgColor);

	// Warnschwellwert-Quelle ("Intern"/"Sensormeter"/"Ping"): dauerhaft
	// (nicht blinkend) angezeigt, damit auch waehrend der weissen Phase des
	// blinkenden Alarm-Hintergrunds erkennbar bleibt, welche Quelle
	// betroffen ist. Farbe zeigt die Richtung (rot=Ueberschreitung/Ausfall,
	// blau=Unterschreitung), unabhaengig von der aktuellen Blink-Phase.
	if (!alertSource.isEmpty()) {
		tft.setTextColor(alertBlue ? TFT_BLUE : TFT_RED, bgColor);
		tft.setTextDatum(ML_DATUM);
		tft.setTextFont(2);
		tft.drawString(alertSource, 110, Layout::kStatusBarHeight / 2);
	}

	tft.setTextColor(kIconColor, bgColor);
	tft.setTextDatum(MR_DATUM);
	tft.setTextFont(2);
	String dhtText = sensorValid ? (String(tempRounded) + "C  " + String(humidityRounded) + "%")
	                              : String("--C  --%");
	tft.drawString(dhtText, DisplayManager::kScreenWidth - 8, Layout::kStatusBarHeight / 2);
	tft.setTextDatum(TL_DATUM);

	// --- untere Leiste: Uhrzeit/Datum (entfaellt im Static-Modus mit
	// Datenquelle "Uhrzeit" - die Ansicht nutzt diesen Platz dann selbst) ---
	if (!showBottomBar) {
		return;
	}
	int16_t barY = Layout::kContentBottom;
	tft.fillRect(0, barY, DisplayManager::kScreenWidth, Layout::kStatusBarHeight, bgColor);
	tft.drawFastHLine(0, barY, DisplayManager::kScreenWidth, kIconColor);

	tft.setTextColor(kIconColor, bgColor);
	tft.setTextDatum(ML_DATUM);
	tft.setTextFont(4);
	tft.drawString(timeHHMM.isEmpty() ? "--:--" : timeHHMM, 8, barY + Layout::kStatusBarHeight / 2);

	tft.setTextDatum(MR_DATUM);
	tft.setTextFont(2);
	tft.drawString(dateLine, DisplayManager::kScreenWidth - 8, barY + Layout::kStatusBarHeight / 2);
	tft.setTextDatum(TL_DATUM);
	tft.setTextColor(TFT_BLACK, bgColor);
}
