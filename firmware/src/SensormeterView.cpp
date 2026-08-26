#include "SensormeterView.h"

namespace {
struct SlideRef {
	size_t targetIndex;
	uint8_t sensorIndex;
};
} // namespace

void SensormeterView::draw(DisplayManager &display, const SensormeterManager &manager, int16_t contentTop,
                            int16_t contentBottom, uint16_t bgColor, int overrideTargetIndex,
                            uint8_t overrideSensorIndex) {
	TFT_eSPI &tft = display.raw();
	int16_t h = contentBottom - contentTop;
	tft.fillRect(0, contentTop, DisplayManager::kScreenWidth, h, bgColor);
	tft.setTextColor(TFT_BLACK, bgColor);
	tft.setTextDatum(MC_DATUM);

	int16_t midY = contentTop + h / 2;

	if (manager.targetCount() == 0) {
		tft.setTextFont(4);
		tft.drawString("Kein Sensormeter-Ziel konfiguriert", DisplayManager::kScreenWidth / 2, midY);
		tft.setTextDatum(TL_DATUM);
		return;
	}

	SlideRef slide;
	if (overrideTargetIndex >= 0) {
		// Slide-Modus: main.cpp hat diesen konkreten Slide bereits als
		// aufgeloest verifiziert (siehe buildSlideEntries()) - keine eigene
		// Rotation, kein Neuaufbau von slides[] noetig.
		slide = {static_cast<size_t>(overrideTargetIndex), overrideSensorIndex};
	} else {
		SlideRef slides[SensormeterManager::kMaxTargets * 2];
		size_t slideCount = 0;
		for (size_t i = 0; i < manager.targetCount(); i++) {
			if (!manager.isResolved(i)) continue;
			slides[slideCount++] = {i, 0};
			if (manager.isPro(i)) {
				slides[slideCount++] = {i, 1};
			}
		}

		if (slideCount == 0) {
			tft.setTextFont(4);
			tft.drawString("Sensormeter --", DisplayManager::kScreenWidth / 2, midY);
			tft.setTextDatum(TL_DATUM);
			return;
		}

		uint32_t now = millis();
		if (now - lastRotateMs_ >= kSubSlideIntervalMs) {
			lastRotateMs_ = now;
			currentSlide_ = (currentSlide_ + 1) % slideCount;
		}
		if (currentSlide_ >= slideCount) {
			currentSlide_ = 0;
		}
		slide = slides[currentSlide_];
	}

	String title = manager.systemName(slide.targetIndex);
	String sensorName = manager.sensorName(slide.targetIndex, slide.sensorIndex);
	if (!sensorName.isEmpty()) {
		title += " (" + sensorName + ")";
	}
	tft.setTextFont(2);
	tft.drawString(title, DisplayManager::kScreenWidth / 2, contentTop + 14);

	if (!manager.sensorValid(slide.targetIndex, slide.sensorIndex)) {
		tft.setTextFont(4);
		tft.drawString("--", DisplayManager::kScreenWidth / 2, midY + 6);
		tft.setTextDatum(TL_DATUM);
		return;
	}

	char tempBuf[8];
	char humBuf[8];
	snprintf(tempBuf, sizeof(tempBuf), "%.1f", manager.sensorTempC(slide.targetIndex, slide.sensorIndex));
	snprintf(humBuf, sizeof(humBuf), "%.1f", manager.sensorHumidityPct(slide.targetIndex, slide.sensorIndex));

	tft.setTextDatum(MR_DATUM);
	tft.setTextFont(7);
	tft.drawString(tempBuf, DisplayManager::kScreenWidth / 2 - 6, midY + 10);
	int16_t degX = DisplayManager::kScreenWidth / 2 + 2;
	tft.drawCircle(degX, midY - 12, 3, TFT_BLACK);
	tft.setTextFont(2);
	tft.setTextDatum(ML_DATUM);
	tft.drawString("C", degX + 6, midY);

	tft.setTextFont(7);
	tft.setTextDatum(MR_DATUM);
	tft.drawString(humBuf, DisplayManager::kScreenWidth - 30, midY + 10);
	tft.setTextFont(2);
	tft.setTextDatum(ML_DATUM);
	tft.drawString("%", DisplayManager::kScreenWidth - 26, midY);

	tft.setTextDatum(TL_DATUM);
}

namespace {
// "3d 04:15" bzw. "04:15:33" ohne Tagesanteil - Sekunden werden ab einem Tag
// Laufzeit weggelassen (Zeile muss in die schmale Uebersichtstabelle
// passen, siehe drawOverview()), unter einem Tag bleiben sie sichtbar.
String formatUptime(uint32_t totalSeconds) {
	uint32_t days = totalSeconds / 86400;
	uint32_t rem = totalSeconds % 86400;
	uint32_t hh = rem / 3600;
	uint32_t mm = (rem % 3600) / 60;
	uint32_t ss = rem % 60;
	char buf[24];
	if (days > 0) {
		snprintf(buf, sizeof(buf), "%ud %02u:%02u", static_cast<unsigned>(days), static_cast<unsigned>(hh),
		         static_cast<unsigned>(mm));
	} else {
		snprintf(buf, sizeof(buf), "%02u:%02u:%02u", static_cast<unsigned>(hh), static_cast<unsigned>(mm),
		         static_cast<unsigned>(ss));
	}
	return String(buf);
}
// Gemeinsame Zeilen-Geometrie der Uebersicht - von drawOverview() (Zeichnen)
// und overviewHitTest() (Tap) genutzt, damit beide garantiert dieselben
// Zeilenhoehen/-positionen verwenden. targetCount muss > 0 sein.
void overviewRowGeometry(int16_t contentTop, int16_t contentBottom, size_t targetCount, int16_t &rowTop,
                          int16_t &rowH) {
	int16_t h = contentBottom - contentTop;
	rowTop = contentTop + 24;
	int16_t rh = static_cast<int16_t>((h - 24) / static_cast<int16_t>(targetCount));
	if (rh > 32) rh = 32;
	if (rh < 16) rh = 16;
	rowH = rh;
}
} // namespace

int SensormeterView::overviewHitTest(const SensormeterManager &manager, int16_t contentTop,
                                      int16_t contentBottom, int16_t x, int16_t y) const {
	(void)x;  // ganze Zeilenbreite ist antippbar - x nicht eingeschraenkt
	size_t count = manager.targetCount();
	if (count == 0) return -1;
	int16_t rowTop, rowH;
	overviewRowGeometry(contentTop, contentBottom, count, rowTop, rowH);
	for (size_t i = 0; i < count; i++) {
		int16_t rowY = rowTop + static_cast<int16_t>(i) * rowH;
		if (y >= rowY && y < rowY + rowH) return static_cast<int>(i);
	}
	return -1;
}

void SensormeterView::drawOverview(DisplayManager &display, const SensormeterManager &manager, int16_t contentTop,
                                    int16_t contentBottom, uint16_t bgColor) {
	TFT_eSPI &tft = display.raw();
	int16_t h = contentBottom - contentTop;
	tft.fillRect(0, contentTop, DisplayManager::kScreenWidth, h, bgColor);
	tft.setTextColor(TFT_BLACK, bgColor);

	if (manager.targetCount() == 0) {
		tft.setTextDatum(MC_DATUM);
		tft.setTextFont(4);
		tft.drawString("Kein Sensormeter-Ziel konfiguriert", DisplayManager::kScreenWidth / 2,
		               contentTop + h / 2);
		tft.setTextDatum(TL_DATUM);
		return;
	}

	tft.setTextFont(2);
	tft.setTextDatum(TL_DATUM);
	tft.drawString("Sensormeter - Uebersicht", 8, contentTop + 4);

	int16_t rowTop, rowH;
	overviewRowGeometry(contentTop, contentBottom, manager.targetCount(), rowTop, rowH);

	for (size_t i = 0; i < manager.targetCount(); i++) {
		int16_t y = rowTop + static_cast<int16_t>(i) * rowH;
		String name = manager.systemName(i);
		if (name.isEmpty()) name = "(Ziel " + String(i + 1) + ")";

		tft.setTextDatum(ML_DATUM);
		tft.setTextFont(2);
		tft.drawString(name, 12, y + rowH / 2);

		String uptimeText = "--";
		if (manager.isResolved(i) && manager.uptimeValid(i)) {
			uptimeText = formatUptime(manager.uptimeSeconds(i));
		} else if (!manager.isResolved(i)) {
			uptimeText = "nicht erreichbar";
		}
		tft.setTextDatum(MR_DATUM);
		tft.drawString(uptimeText, DisplayManager::kScreenWidth - 12, y + rowH / 2);

		if (i + 1 < manager.targetCount()) {
			tft.drawFastHLine(8, y + rowH, DisplayManager::kScreenWidth - 16, TFT_LIGHTGREY);
		}
	}

	tft.setTextDatum(TL_DATUM);
}
