#include "ClockView.h"

#include "TimeSync.h"

void ClockView::draw(DisplayManager &display, int16_t contentTop, int16_t contentBottom,
                      uint16_t bgColor) {
	TFT_eSPI &tft = display.raw();
	int16_t h = contentBottom - contentTop;

	tft.fillRect(0, contentTop, DisplayManager::kScreenWidth, h, bgColor);
	tft.setTextColor(TFT_BLACK, bgColor);
	tft.setTextDatum(MC_DATUM);

	String time = TimeSync::formatTime();
	String date = TimeSync::formatDate();

	int16_t timeAreaH = (h * 2) / 3;
	int16_t timeMidY = contentTop + timeAreaH / 2;
	tft.setTextFont(7);
	tft.drawString(time.isEmpty() ? "--:--" : time, DisplayManager::kScreenWidth / 2, timeMidY);

	int16_t dateMidY = contentTop + timeAreaH + (h - timeAreaH) / 2;
	tft.setTextFont(4);
	tft.drawString(date.isEmpty() ? "Kein NTP-Sync" : date, DisplayManager::kScreenWidth / 2, dateMidY);

	tft.setTextDatum(TL_DATUM);
}
