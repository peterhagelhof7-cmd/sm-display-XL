#include "UiHelpers.h"

bool UiHelpers::hitRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
	return x >= rx && x < (rx + rw) && y >= ry && y < (ry + rh);
}

void UiHelpers::waitForTapEvent(TouchManager &touch, int16_t &x, int16_t &y) {
	int16_t tx, ty;
	while (!touch.read(tx, ty)) {
		delay(15);
	}
	x = tx;
	y = ty;
	delay(150); // Entprellen
	while (touch.read(tx, ty)) {
		delay(15);
	}
}

void UiHelpers::drawCloseButton(TFT_eSPI &tft, int16_t x, int16_t y, int16_t w, int16_t h) {
	tft.drawRect(x, y, w, h, TFT_BLACK);
	tft.setTextDatum(MC_DATUM);
	tft.setTextFont(4);
	tft.drawString("X", x + w / 2, y + h / 2);
	tft.setTextDatum(TL_DATUM);
}
