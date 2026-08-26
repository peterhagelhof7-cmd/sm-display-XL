#include "DisplayManager.h"

void DisplayManager::begin() {
	tft.init();
	// 800x480 ist natives Querformat -> Rotation 0. (Keine ST7789-INVON-
	// Kompensation noetig wie beim 2,8"-SPI-Panel; das RGB-Panel stellt
	// Weiss/Schwarz/Farben direkt korrekt dar.)
	tft.setRotation(0);
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
}

void DisplayManager::drawBootScreen(const char *line1, const char *line2) {
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	tft.setTextDatum(MC_DATUM);
	tft.setFreeFont(nullptr);
	tft.setTextFont(4);
	tft.setTextSize(2);
	tft.drawString(line1, kScreenWidth / 2, kScreenHeight / 2 - 28);
	tft.setTextFont(2);
	tft.drawString(line2, kScreenWidth / 2, kScreenHeight / 2 + 28);
	tft.setTextSize(1);
}
