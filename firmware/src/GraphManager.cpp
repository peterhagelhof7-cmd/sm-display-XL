#include "GraphManager.h"

#include <LittleFS.h>
#include <time.h>

#include "Layout.h"
#include "SettingsManager.h"
#include "TimeSync.h"

namespace {
constexpr const char *kHistoryFile = "/history.csv";

// Gestrichelte horizontale Linie fuer Warnschwellwerte im Graph -
// unterscheidet sie optisch von den durchgezogenen Messwert-Polylinien.
void drawDashedHLine(TFT_eSPI &tft, int16_t x0, int16_t x1, int16_t y, uint16_t color) {
	constexpr int16_t kDashLen = 4, kGapLen = 3;
	for (int16_t x = x0; x < x1; x += kDashLen + kGapLen) {
		int16_t segEnd = min(static_cast<int16_t>(x + kDashLen), x1);
		tft.drawLine(x, y, segEnd, y, color);
	}
}
}

void GraphManager::begin() {
	mutex_ = xSemaphoreCreateMutex();
	if (!LittleFS.begin(true)) {
		Serial.println("LittleFS-Mount fehlgeschlagen (auch nach Formatierungsversuch)");
		return;
	}
	load(); // unlocked: laeuft in begin(), bevor ein zweiter Task existieren kann
}

void GraphManager::load() {
	fs::File f = LittleFS.open(kHistoryFile, "r");
	if (!f) {
		return;
	}
	count = 0;
	while (f.available() && count < kMaxEntries) {
		String line = f.readStringUntil('\n');
		line.trim();
		if (line.isEmpty()) {
			continue;
		}
		int firstComma = line.indexOf(',');
		int secondComma = line.indexOf(',', firstComma + 1);
		if (firstComma < 0 || secondComma < 0) {
			continue;
		}
		Entry e;
		e.ts = static_cast<time_t>(line.substring(0, firstComma).toInt());
		e.tempC = static_cast<int16_t>(line.substring(firstComma + 1, secondComma).toInt());
		e.humidityPct = static_cast<int16_t>(line.substring(secondComma + 1).toInt());
		entries[count++] = e;
	}
	f.close();
	if (count > 0) {
		lastRecordTs = entries[count - 1].ts;
	}
}

void GraphManager::save() const {
	fs::File f = LittleFS.open(kHistoryFile, "w");
	if (!f) {
		Serial.println("history.csv konnte nicht geschrieben werden");
		return;
	}
	for (size_t i = 0; i < count; i++) {
		f.printf("%ld,%d,%d\n", static_cast<long>(entries[i].ts), entries[i].tempC,
		         entries[i].humidityPct);
	}
	f.close();
}

void GraphManager::appendEntry(time_t ts, int16_t tempC, int16_t humidityPct) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	if (count < kMaxEntries) {
		entries[count++] = {ts, tempC, humidityPct};
	} else {
		for (size_t i = 1; i < kMaxEntries; i++) {
			entries[i - 1] = entries[i];
		}
		entries[kMaxEntries - 1] = {ts, tempC, humidityPct};
	}
	lastRecordTs = ts;
	save(); // haelt selbst keine eigene Sperre, siehe Kommentar dort
	xSemaphoreGive(mutex_);
}

size_t GraphManager::entryCount() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	size_t v = count;
	xSemaphoreGive(mutex_);
	return v;
}

time_t GraphManager::entryTs(size_t i) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	time_t v = (i < count) ? entries[i].ts : 0;
	xSemaphoreGive(mutex_);
	return v;
}

int16_t GraphManager::entryTempC(size_t i) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	int16_t v = (i < count) ? entries[i].tempC : 0;
	xSemaphoreGive(mutex_);
	return v;
}

void GraphManager::reset() {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	count = 0;
	lastRecordTs = 0;
	LittleFS.remove(kHistoryFile);
	everDrawn = false; // erzwingt Neuzeichnen mit leerem Verlauf
	xSemaphoreGive(mutex_);
}

int16_t GraphManager::entryHumidityPct(size_t i) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	int16_t v = (i < count) ? entries[i].humidityPct : 0;
	xSemaphoreGive(mutex_);
	return v;
}

void GraphManager::maybeRecord(float tempC, float humidityPct) {
	if (!TimeSync::isValid()) {
		return;
	}
	time_t now = time(nullptr);
	if (lastRecordTs != 0 && (now - lastRecordTs) < static_cast<time_t>(kRecordIntervalSec)) {
		return;
	}
	appendEntry(now, static_cast<int16_t>(lroundf(tempC)), static_cast<int16_t>(lroundf(humidityPct)));
}

void GraphManager::drawGraph(DisplayManager &display, int16_t x, int16_t y, int16_t w, int16_t h, int16_t tempMinThreshold,
                              int16_t tempMaxThreshold, int16_t humMinThreshold, int16_t humMaxThreshold) const {
	TFT_eSPI &tft = display.raw();
	tft.drawRect(x, y, w, h, TFT_BLACK);

	if (count < 2) {
		tft.setTextDatum(MC_DATUM);
		tft.setTextFont(2);
		tft.drawString("Noch nicht genug Messwerte", x + w / 2, y + h / 2);
		tft.setTextDatum(TL_DATUM);
		return;
	}

	int16_t tempMin = entries[0].tempC, tempMax = entries[0].tempC;
	int16_t humMin = entries[0].humidityPct, humMax = entries[0].humidityPct;
	for (size_t i = 1; i < count; i++) {
		tempMin = min(tempMin, entries[i].tempC);
		tempMax = max(tempMax, entries[i].tempC);
		humMin = min(humMin, entries[i].humidityPct);
		humMax = max(humMax, entries[i].humidityPct);
	}
	if (tempMax == tempMin) tempMax++;
	if (humMax == humMin) humMax++;

	constexpr int16_t kMargin = 26; // Platz fuer Achsenbeschriftung links/rechts
	constexpr int16_t kBottomMargin = 10; // Platz fuer Zeitachsenbeschriftung unten
	int16_t plotX = x + kMargin;
	int16_t plotW = w - 2 * kMargin;
	int16_t plotY = y + 4;
	int16_t plotH = h - 8 - kBottomMargin;

	auto plotPointX = [&](size_t i) -> int16_t {
		if (count == 1) return plotX;
		return plotX + static_cast<int16_t>(static_cast<int32_t>(i) * plotW / (static_cast<int32_t>(count) - 1));
	};
	auto tempPointY = [&](int16_t v) -> int16_t {
		return plotY + plotH - static_cast<int16_t>(static_cast<int32_t>(v - tempMin) * plotH / (tempMax - tempMin));
	};
	auto humPointY = [&](int16_t v) -> int16_t {
		return plotY + plotH - static_cast<int16_t>(static_cast<int32_t>(v - humMin) * plotH / (humMax - humMin));
	};

	for (size_t i = 1; i < count; i++) {
		tft.drawLine(plotPointX(i - 1), tempPointY(entries[i - 1].tempC), plotPointX(i),
		             tempPointY(entries[i].tempC), TFT_RED);
		tft.drawLine(plotPointX(i - 1), humPointY(entries[i - 1].humidityPct), plotPointX(i),
		             humPointY(entries[i].humidityPct), TFT_BLUE);
	}

	// Warnschwellwerte als gestrichelte Referenzlinien - auf den Plotbereich
	// geklemmt, falls der Schwellwert ausserhalb des aktuell sichtbaren
	// Wertebereichs liegt (dann erscheint die Linie am oberen/unteren Rand
	// statt gar nicht, als Hinweis "noch nicht erreicht").
	auto clampToPlot = [&](int16_t v) -> int16_t {
		if (v < plotY) return plotY;
		if (v > plotY + plotH) return plotY + plotH;
		return v;
	};
	if (tempMinThreshold != SettingsManager::kThresholdDisabled) {
		drawDashedHLine(tft, plotX, plotX + plotW, clampToPlot(tempPointY(tempMinThreshold)), TFT_RED);
	}
	if (tempMaxThreshold != SettingsManager::kThresholdDisabled) {
		drawDashedHLine(tft, plotX, plotX + plotW, clampToPlot(tempPointY(tempMaxThreshold)), TFT_RED);
	}
	if (humMinThreshold != SettingsManager::kThresholdDisabled) {
		drawDashedHLine(tft, plotX, plotX + plotW, clampToPlot(humPointY(humMinThreshold)), TFT_BLUE);
	}
	if (humMaxThreshold != SettingsManager::kThresholdDisabled) {
		drawDashedHLine(tft, plotX, plotX + plotW, clampToPlot(humPointY(humMaxThreshold)), TFT_BLUE);
	}

	tft.setTextFont(1);
	tft.setTextDatum(TR_DATUM);
	tft.setTextColor(TFT_RED, TFT_WHITE);
	tft.drawString(String(tempMax) + "C", x + kMargin - 2, plotY);
	tft.drawString(String(tempMin) + "C", x + kMargin - 2, plotY + plotH - 8);

	tft.setTextDatum(TL_DATUM);
	tft.setTextColor(TFT_BLUE, TFT_WHITE);
	tft.drawString(String(humMax) + "%", x + w - kMargin + 2, plotY);
	tft.drawString(String(humMin) + "%", x + w - kMargin + 2, plotY + plotH - 8);

	// Zeitachse: aeltester und neuester Messpunkt als Uhrzeit, damit
	// erkennbar ist, ueber welchen Zeitraum der Verlauf geht (Ringpuffer
	// deckt bis zu 12h ab, siehe GraphManager.h).
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	int16_t axisY = y + h - 1;
	if (TimeSync::isValid()) {
		struct tm tmStart, tmEnd;
		localtime_r(&entries[0].ts, &tmStart);
		localtime_r(&entries[count - 1].ts, &tmEnd);
		char startBuf[6], endBuf[6];
		snprintf(startBuf, sizeof(startBuf), "%02d:%02d", tmStart.tm_hour, tmStart.tm_min);
		snprintf(endBuf, sizeof(endBuf), "%02d:%02d", tmEnd.tm_hour, tmEnd.tm_min);
		tft.setTextDatum(BL_DATUM);
		tft.drawString(startBuf, plotX, axisY);
		tft.setTextDatum(BR_DATUM);
		tft.drawString(endBuf, plotX + plotW, axisY);
	}

	tft.setTextDatum(TL_DATUM);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
}

void GraphManager::drawFullScreen(DisplayManager &display, float currentTempC, float currentHumidityPct,
                                   bool sensorValid, int16_t tempMinC, int16_t tempMaxC, int16_t humMinPct,
                                   int16_t humMaxPct, uint16_t bgColor) {
	int tempRounded = sensorValid ? static_cast<int>(lroundf(currentTempC)) : 0;
	int humidityRounded = sensorValid ? static_cast<int>(lroundf(currentHumidityPct)) : 0;
	bool changed = !everDrawn || sensorValid != lastSensorValid || tempRounded != lastTempRounded ||
	               humidityRounded != lastHumidityRounded || bgColor != lastBgColor || count != lastCount ||
	               tempMinC != lastTempMinC || tempMaxC != lastTempMaxC || humMinPct != lastHumMinPct ||
	               humMaxPct != lastHumMaxPct;
	if (!changed) {
		return;
	}
	everDrawn = true;
	lastSensorValid = sensorValid;
	lastTempRounded = tempRounded;
	lastHumidityRounded = humidityRounded;
	lastBgColor = bgColor;
	lastCount = count;
	lastTempMinC = tempMinC;
	lastTempMaxC = tempMaxC;
	lastHumMinPct = humMinPct;
	lastHumMaxPct = humMaxPct;

	TFT_eSPI &tft = display.raw();
	tft.fillRect(0, Layout::kContentTop, DisplayManager::kScreenWidth, Layout::kContentHeight, bgColor);

	int16_t valueAreaH = Layout::kContentHeight / 3;
	int16_t valueAreaY = Layout::kContentTop;

	tft.setTextColor(TFT_BLACK, bgColor);
	if (!sensorValid) {
		tft.setTextDatum(MC_DATUM);
		tft.setTextFont(4);
		tft.drawString("Sensor --", DisplayManager::kScreenWidth / 2, valueAreaY + valueAreaH / 2);
		tft.setTextDatum(TL_DATUM);
	} else {
		char tempBuf[8];
		char humBuf[8];
		snprintf(tempBuf, sizeof(tempBuf), "%d", static_cast<int>(lroundf(currentTempC)));
		snprintf(humBuf, sizeof(humBuf), "%d", static_cast<int>(lroundf(currentHumidityPct)));

		tft.setTextDatum(MR_DATUM);
		tft.setTextFont(7);
		int16_t midY = valueAreaY + valueAreaH / 2;
		tft.drawString(tempBuf, DisplayManager::kScreenWidth / 2 - 6, midY);
		// Eingebaute Fonts kennen kein "°" (nur ASCII 32-127) - stattdessen
		// ein kleiner Kreis von Hand gezeichnet, gefolgt von "C".
		int16_t degX = DisplayManager::kScreenWidth / 2 + 2;
		int16_t degY = midY - 22;
		tft.drawCircle(degX, degY, 3, TFT_BLACK);
		tft.setTextFont(2);
		tft.setTextDatum(ML_DATUM);
		tft.drawString("C", degX + 6, midY - 10);

		tft.setTextFont(7);
		tft.setTextDatum(MR_DATUM);
		tft.drawString(humBuf, DisplayManager::kScreenWidth - 30, midY);
		tft.setTextFont(2);
		tft.setTextDatum(ML_DATUM);
		tft.drawString("%", DisplayManager::kScreenWidth - 26, midY - 10);

		tft.setTextDatum(TL_DATUM);
	}

	int16_t graphY = valueAreaY + valueAreaH;
	int16_t graphH = Layout::kContentHeight - valueAreaH;
	drawGraph(display, 4, graphY, DisplayManager::kScreenWidth - 8, graphH, tempMinC, tempMaxC, humMinPct, humMaxPct);
}
