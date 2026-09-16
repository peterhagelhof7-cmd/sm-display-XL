#include "NumericKeypad.h"

#include "UiHelpers.h"

using UiHelpers::hitRect;
using UiHelpers::waitForTapEvent;

namespace {

constexpr int16_t kScreenW = DisplayManager::kScreenWidth;

// Layout fuer 800x480 (XL). Werte proportional zur 2,8"-Vorlage vergroessert.
constexpr int16_t kFieldX = 20;
constexpr int16_t kFieldY = 56;
constexpr int16_t kFieldW = kScreenW - 40;
constexpr int16_t kFieldH = 56;

constexpr int16_t kKeyTop = 128;
constexpr int16_t kKeyRowH = 64;
constexpr int16_t kKeyColW = kScreenW / 3;

// Telefon-Tastenfeld-Anordnung: 1-9, dann Punkt/0/Loeschen.
const char *kKeys[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "<-"};

// 4 Reihen Tasten enden bei kKeyTop + 4*kKeyRowH = 128+256 = 384; Buttons
// darunter muessen bis Bildschirmhoehe 480 passen.
constexpr int16_t kCancelX = 20;
constexpr int16_t kCancelY = kKeyTop + 4 * kKeyRowH + 8;
constexpr int16_t kCancelW = 200;
constexpr int16_t kCancelH = 72;
constexpr int16_t kOkX = kScreenW - 220;
constexpr int16_t kOkY = kCancelY;
constexpr int16_t kOkW = 200;
constexpr int16_t kOkH = 72;

void draw(DisplayManager &display, const String &title, const String &value) {
	TFT_eSPI &tft = display.raw();
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	tft.setTextDatum(TL_DATUM);
	tft.setTextFont(4);
	tft.drawString(title, 20, 12);

	tft.drawRect(kFieldX, kFieldY, kFieldW, kFieldH, TFT_BLACK);
	tft.setTextDatum(MC_DATUM);
	tft.setTextFont(6);
	tft.drawString(value, kFieldX + kFieldW / 2, kFieldY + kFieldH / 2);

	tft.setTextFont(6);
	for (int i = 0; i < 12; i++) {
		int16_t col = i % 3;
		int16_t row = i / 3;
		int16_t x = col * kKeyColW;
		int16_t y = kKeyTop + row * kKeyRowH;
		tft.drawRect(x, y, kKeyColW, kKeyRowH - 4, TFT_BLACK);
		tft.drawString(kKeys[i], x + kKeyColW / 2, y + (kKeyRowH - 4) / 2);
	}

	tft.drawRect(kCancelX, kCancelY, kCancelW, kCancelH, TFT_BLACK);
	tft.setTextFont(4);
	tft.drawString("Abbrechen", kCancelX + kCancelW / 2, kCancelY + kCancelH / 2);

	tft.drawRect(kOkX, kOkY, kOkW, kOkH, TFT_BLACK);
	tft.drawString("OK", kOkX + kOkW / 2, kOkY + kOkH / 2);

	tft.setTextDatum(TL_DATUM);
}

} // namespace

String NumericKeypad::run(DisplayManager &display, TouchManager &touch, const String &title,
                           const String &initialValue, bool &cancelled) {
	String value = initialValue;
	cancelled = false;
	draw(display, title, value);

	while (true) {
		int16_t x, y;
		waitForTapEvent(touch, x, y);

		if (hitRect(x, y, kCancelX, kCancelY, kCancelW, kCancelH)) {
			cancelled = true;
			return initialValue;
		}
		if (hitRect(x, y, kOkX, kOkY, kOkW, kOkH)) {
			return value;
		}

		bool hitKey = false;
		for (int i = 0; i < 12 && !hitKey; i++) {
			int16_t col = i % 3;
			int16_t row = i / 3;
			int16_t kx = col * kKeyColW;
			int16_t ky = kKeyTop + row * kKeyRowH;
			if (hitRect(x, y, kx, ky, kKeyColW, kKeyRowH - 4)) {
				hitKey = true;
				String k = kKeys[i];
				if (k == "<-") {
					if (!value.isEmpty()) value.remove(value.length() - 1);
				} else if (value.length() < 15) { // "255.255.255.255" max
					value += k;
				}
			}
		}
		if (hitKey) {
			draw(display, title, value);
		}
	}
}
