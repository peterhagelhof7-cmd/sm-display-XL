#include "SettingsUI.h"

#include "UiHelpers.h"
#include "DataSource.h"
#include "NumericKeypad.h"
#include "SnakeGame.h"

using UiHelpers::hitRect;
using UiHelpers::waitForTapEvent;

namespace {

constexpr int16_t kScreenW = DisplayManager::kScreenWidth;
constexpr int16_t kScreenH = DisplayManager::kScreenHeight;

constexpr int16_t kCloseX = kScreenW - 74;
constexpr int16_t kCloseY = 8;
constexpr int16_t kCloseW = 64;
constexpr int16_t kCloseH = 64;

constexpr int16_t kMenuRowY = 92;
constexpr int16_t kMenuRowH = 84;

// Gemeinsame Layout-Werte fuer die Ziel-Listen (Sensormeter/Ping), 800x480.
constexpr int16_t kListRowStep = 58;
constexpr int16_t kListRowH = 48;
constexpr int16_t kListRemoveW = 60;
constexpr int16_t kSmListY0 = 120;   // Sensormeter: unter der Community-Zeile
constexpr int16_t kPingListY0 = 92;  // Ping: direkt unter der Ueberschrift

const char *kMenuItems[] = {"Slide", "Static", "Snake", "Systemeinstellungen"};
constexpr size_t kMenuItemCount = 4;

void drawCloseButton(TFT_eSPI &tft) {
	UiHelpers::drawCloseButton(tft, kCloseX, kCloseY, kCloseW, kCloseH);
}

void drawMenu(DisplayManager &display) {
	TFT_eSPI &tft = display.raw();
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	tft.setTextDatum(TL_DATUM);
	tft.setTextFont(4);
	tft.drawString("Einstellungen", 16, 12);

	drawCloseButton(tft);

	for (size_t i = 0; i < kMenuItemCount; i++) {
		int16_t y = kMenuRowY + static_cast<int16_t>(i) * kMenuRowH;
		tft.drawRect(10, y, kScreenW - 20, kMenuRowH - 8, TFT_BLACK);
		tft.setTextDatum(MC_DATUM);
		tft.setTextFont(4);
		tft.drawString(kMenuItems[i], kScreenW / 2, y + (kMenuRowH - 8) / 2);
		tft.setTextDatum(TL_DATUM);
	}
}

int hitTestMenu(int16_t x, int16_t y) {
	if (x < 10 || x > kScreenW - 10) return -1;
	for (size_t i = 0; i < kMenuItemCount; i++) {
		int16_t rowY = kMenuRowY + static_cast<int16_t>(i) * kMenuRowH;
		if (y >= rowY && y < rowY + (kMenuRowH - 8)) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

constexpr int16_t kMinusX = 90;
constexpr int16_t kPlusX = kScreenW - 90 - 96;
constexpr int16_t kAdjustY = 170;
constexpr int16_t kAdjustW = 96;
constexpr int16_t kAdjustH = 96;
constexpr int16_t kApplyX = kScreenW / 2 - 140;
constexpr int16_t kApplyY = 320;
constexpr int16_t kApplyW = 280;
constexpr int16_t kApplyH = 80;

void drawAdjustScreen(DisplayManager &display, const char *title, const String &valueText) {
	TFT_eSPI &tft = display.raw();
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	tft.setTextDatum(TL_DATUM);
	tft.setTextFont(4);
	tft.drawString(title, 16, 12);
	drawCloseButton(tft);

	tft.drawRect(kMinusX, kAdjustY, kAdjustW, kAdjustH, TFT_BLACK);
	tft.drawRect(kPlusX, kAdjustY, kAdjustW, kAdjustH, TFT_BLACK);
	tft.setTextDatum(MC_DATUM);
	tft.setTextFont(4);
	tft.drawString("-", kMinusX + kAdjustW / 2, kAdjustY + kAdjustH / 2);
	tft.drawString("+", kPlusX + kAdjustW / 2, kAdjustY + kAdjustH / 2);

	tft.fillRect(kMinusX + kAdjustW + 4, kAdjustY, kPlusX - (kMinusX + kAdjustW + 4), kAdjustH, TFT_WHITE);
	tft.setTextFont(7);
	tft.drawString(valueText, kScreenW / 2, kAdjustY + kAdjustH / 2);

	tft.drawRect(kApplyX, kApplyY, kApplyW, kApplyH, TFT_BLACK);
	tft.setTextFont(4);
	tft.drawString("Uebernehmen", kApplyX + kApplyW / 2, kApplyY + kApplyH / 2);
	tft.setTextDatum(TL_DATUM);
}

// Slide-Intervall waehlen (5..60s in 5s-Schritten). Rueckgabe true, wenn
// uebernommen (Aufrufer soll SettingsUI dann komplett verlassen).
bool runSlideConfig(DisplayManager &display, TouchManager &touch, SettingsManager &settings) {
	uint16_t interval = settings.slideIntervalSec();

	auto redraw = [&]() { drawAdjustScreen(display, "Slide-Intervall", String(interval) + "s"); };
	redraw();

	while (true) {
		int16_t x, y;
		waitForTapEvent(touch, x, y);

		if (hitRect(x, y, kCloseX, kCloseY, kCloseW, kCloseH)) {
			return false;
		}
		if (hitRect(x, y, kMinusX, kAdjustY, kAdjustW, kAdjustH)) {
			if (interval > SettingsManager::kSlideIntervalMinSec) interval -= SettingsManager::kSlideIntervalStepSec;
			redraw();
		} else if (hitRect(x, y, kPlusX, kAdjustY, kAdjustW, kAdjustH)) {
			if (interval < SettingsManager::kSlideIntervalMaxSec) interval += SettingsManager::kSlideIntervalStepSec;
			redraw();
		} else if (hitRect(x, y, kApplyX, kApplyY, kApplyW, kApplyH)) {
			settings.setSlideMode(interval);
			return true;
		}
	}
}

// Eigene, kleinere Zeilenhoehe als das 4-Punkt-Hauptmenu (kMenuRowH):
// kAvailableDataSourceCount ist mittlerweile auf 7 gewachsen (Sensormeter-
// Uebersicht dazugekommen) - mit kMenuRowH (44px) oder dem froeheren 40px
// waere die letzte Zeile ueber den Bildschirmrand (240px) hinausgelaufen.
// 28px * 7 = 196px, ab y=36 bis 232, passt mit Reserve.
// 800x480: ab y=84, 54px Zeilenhoehe. 7 Quellen * 54 = 378 -> 84..462, passt.
constexpr int16_t kSourceRowY = 84;
constexpr int16_t kSourceRowH = 54;

void drawStaticSourceList(DisplayManager &display) {
	TFT_eSPI &tft = display.raw();
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	tft.setTextDatum(TL_DATUM);
	tft.setTextFont(4);
	tft.drawString("Static-Datenquelle", 16, 12);
	drawCloseButton(tft);

	for (size_t i = 0; i < kAvailableDataSourceCount; i++) {
		int16_t y = kSourceRowY + static_cast<int16_t>(i) * kSourceRowH;
		tft.drawRect(10, y, kScreenW - 20, kSourceRowH - 6, TFT_BLACK);
		tft.setTextDatum(MC_DATUM);
		tft.setTextFont(4);
		tft.drawString(dataSourceLabel(kAvailableDataSources[i]), kScreenW / 2, y + (kSourceRowH - 6) / 2);
		tft.setTextDatum(TL_DATUM);
	}
}

int hitTestSourceList(int16_t x, int16_t y) {
	if (x < 10 || x > kScreenW - 10) return -1;
	for (size_t i = 0; i < kAvailableDataSourceCount; i++) {
		int16_t rowY = kSourceRowY + static_cast<int16_t>(i) * kSourceRowH;
		if (y >= rowY && y < rowY + (kSourceRowH - 6)) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

bool runStaticConfig(DisplayManager &display, TouchManager &touch, SettingsManager &settings) {
	drawStaticSourceList(display);
	while (true) {
		int16_t x, y;
		waitForTapEvent(touch, x, y);

		if (hitRect(x, y, kCloseX, kCloseY, kCloseW, kCloseH)) {
			return false;
		}
		int idx = hitTestSourceList(x, y);
		if (idx >= 0) {
			settings.setStaticMode(kAvailableDataSources[static_cast<size_t>(idx)]);
			return true;
		}
	}
}

void runSnake(DisplayManager &display, TouchManager &touch) {
	SnakeGame snake;
	snake.run(display, touch);
}

constexpr int16_t kSysButtonX = kScreenW / 2 - 240;
constexpr int16_t kSysButtonW = 480;
constexpr int16_t kSysButtonH = 52;
constexpr int16_t kWlanButtonY = 300;
constexpr int16_t kSensormeterButtonY = 360;
constexpr int16_t kPingButtonY = 420;

void drawSystemSettings(DisplayManager &display, uint8_t brightnessPercent, size_t sensormeterTargetCount,
                        size_t pingTargetCount) {
	TFT_eSPI &tft = display.raw();
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	tft.setTextDatum(TL_DATUM);
	tft.setTextFont(4);
	tft.drawString("Systemeinstellungen", 16, 12);
	drawCloseButton(tft);

	tft.setTextFont(4);
	tft.drawString("Helligkeit", 90, kAdjustY - 40);

	tft.drawRect(kMinusX, kAdjustY, kAdjustW, kAdjustH, TFT_BLACK);
	tft.drawRect(kPlusX, kAdjustY, kAdjustW, kAdjustH, TFT_BLACK);
	tft.setTextDatum(MC_DATUM);
	tft.setTextFont(4);
	tft.drawString("-", kMinusX + kAdjustW / 2, kAdjustY + kAdjustH / 2);
	tft.drawString("+", kPlusX + kAdjustW / 2, kAdjustY + kAdjustH / 2);

	tft.fillRect(kMinusX + kAdjustW + 4, kAdjustY, kPlusX - (kMinusX + kAdjustW + 4), kAdjustH, TFT_WHITE);
	tft.setTextFont(7);
	tft.drawString(String(brightnessPercent) + "%", kScreenW / 2, kAdjustY + kAdjustH / 2);

	tft.setTextFont(4);
	tft.drawRect(kSysButtonX, kWlanButtonY, kSysButtonW, kSysButtonH, TFT_BLACK);
	tft.drawString("WLAN neu waehlen", kSysButtonX + kSysButtonW / 2, kWlanButtonY + kSysButtonH / 2);

	tft.drawRect(kSysButtonX, kSensormeterButtonY, kSysButtonW, kSysButtonH, TFT_BLACK);
	tft.drawString("Sensormeter-Ziele (" + String(sensormeterTargetCount) + "/" +
	                   String(static_cast<int>(SettingsManager::kMaxSensormeterTargets)) + ")",
	               kSysButtonX + kSysButtonW / 2, kSensormeterButtonY + kSysButtonH / 2);

	tft.drawRect(kSysButtonX, kPingButtonY, kSysButtonW, kSysButtonH, TFT_BLACK);
	tft.drawString("Ping-Ziele (" + String(pingTargetCount) + "/" +
	                   String(static_cast<int>(SettingsManager::kMaxPingTargets)) + ")",
	               kSysButtonX + kSysButtonW / 2, kPingButtonY + kSysButtonH / 2);
	tft.setTextDatum(TL_DATUM);
}

// Liste der Sensormeter-Ziele (IP-Adressen, bis zu kMaxSensormeterTargets) -
// analog zur Ping-Zielliste. Die SNMP-Community ist absichtlich nicht per
// Touch editierbar (nur ueber das Webinterface): das Zifferntastenfeld
// deckt keine Buchstaben ab, eine volle Bildschirmtastatur waere fuer dieses
// eine, selten geaenderte Feld unverhaeltnismaessig - siehe
// docs/entscheidungen.md. Das letzte verbleibende Ziel kann nicht entfernt
// werden (mindestens eins bleibt konfiguriert, siehe SettingsManager).
void drawSensormeterList(DisplayManager &display, const SettingsManager &settings) {
	TFT_eSPI &tft = display.raw();
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	tft.setTextDatum(TL_DATUM);
	tft.setTextFont(4);
	tft.drawString("Sensormeter-Ziele", 16, 12);
	drawCloseButton(tft);

	tft.setTextFont(2);
	tft.drawString("Community: " + settings.sensormeterCommunity() + " (nur im Webinterface aenderbar)", 16, 78);

	size_t count = settings.sensormeterTargetCount();
	for (size_t i = 0; i < count; i++) {
		int16_t y = kSmListY0 + static_cast<int16_t>(i) * kListRowStep;
		tft.drawRect(10, y, kScreenW - 20 - kListRemoveW - 8, kListRowH, TFT_BLACK);
		tft.setTextDatum(ML_DATUM);
		tft.setTextFont(4);
		tft.drawString(settings.sensormeterTargetIp(i), 24, y + kListRowH / 2);
		if (count > 1) {
			tft.drawRect(kScreenW - kListRemoveW - 10, y, kListRemoveW, kListRowH, TFT_BLACK);
			tft.setTextDatum(MC_DATUM);
			tft.drawString("X", kScreenW - kListRemoveW - 10 + kListRemoveW / 2, y + kListRowH / 2);
		}
	}

	if (count < SettingsManager::kMaxSensormeterTargets) {
		int16_t y = kSmListY0 + static_cast<int16_t>(count) * kListRowStep;
		tft.drawRect(10, y, kScreenW - 20, kListRowH, TFT_BLACK);
		tft.setTextDatum(MC_DATUM);
		tft.setTextFont(4);
		tft.drawString("+ Ziel hinzufuegen", kScreenW / 2, y + kListRowH / 2);
	}
	tft.setTextDatum(TL_DATUM);
}

void runSensormeterConfig(DisplayManager &display, TouchManager &touch, SettingsManager &settings) {
	drawSensormeterList(display, settings);

	while (true) {
		int16_t x, y;
		waitForTapEvent(touch, x, y);

		if (hitRect(x, y, kCloseX, kCloseY, kCloseW, kCloseH)) {
			return;
		}

		size_t count = settings.sensormeterTargetCount();
		bool handled = false;
		if (count > 1) {
			for (size_t i = 0; i < count && !handled; i++) {
				int16_t rowY = kSmListY0 + static_cast<int16_t>(i) * kListRowStep;
				if (hitRect(x, y, kScreenW - kListRemoveW - 10, rowY, kListRemoveW, kListRowH)) {
					settings.removeSensormeterTarget(i);
					handled = true;
				}
			}
		}
		if (!handled && count < SettingsManager::kMaxSensormeterTargets) {
			int16_t addY = kSmListY0 + static_cast<int16_t>(count) * kListRowStep;
			if (hitRect(x, y, 10, addY, kScreenW - 20, kListRowH)) {
				bool cancelled = false;
				String newIp = NumericKeypad::run(display, touch, "Sensormeter-IP", "", cancelled);
				if (!cancelled && !newIp.isEmpty()) {
					settings.addSensormeterTarget(newIp);
				}
				handled = true;
			}
		}
		if (handled) {
			drawSensormeterList(display, settings);
		}
	}
}

void drawPingTargetList(DisplayManager &display, const SettingsManager &settings) {
	TFT_eSPI &tft = display.raw();
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	tft.setTextDatum(TL_DATUM);
	tft.setTextFont(4);
	tft.drawString("Ping-Ziele", 16, 12);
	drawCloseButton(tft);

	size_t count = settings.pingTargetCount();
	for (size_t i = 0; i < count; i++) {
		int16_t y = kPingListY0 + static_cast<int16_t>(i) * kListRowStep;
		tft.drawRect(10, y, kScreenW - 20 - kListRemoveW - 8, kListRowH, TFT_BLACK);
		tft.setTextDatum(ML_DATUM);
		tft.setTextFont(4);
		tft.drawString(settings.pingTargetIp(i), 24, y + kListRowH / 2);
		tft.drawRect(kScreenW - kListRemoveW - 10, y, kListRemoveW, kListRowH, TFT_BLACK);
		tft.setTextDatum(MC_DATUM);
		tft.drawString("X", kScreenW - kListRemoveW - 10 + kListRemoveW / 2, y + kListRowH / 2);
	}

	if (count < SettingsManager::kMaxPingTargets) {
		int16_t y = kPingListY0 + static_cast<int16_t>(count) * kListRowStep;
		tft.drawRect(10, y, kScreenW - 20, kListRowH, TFT_BLACK);
		tft.setTextDatum(MC_DATUM);
		tft.setTextFont(4);
		tft.drawString("+ Ziel hinzufuegen", kScreenW / 2, y + kListRowH / 2);
	}
	tft.setTextDatum(TL_DATUM);
}

void runPingTargetsConfig(DisplayManager &display, TouchManager &touch, SettingsManager &settings) {
	drawPingTargetList(display, settings);

	while (true) {
		int16_t x, y;
		waitForTapEvent(touch, x, y);

		if (hitRect(x, y, kCloseX, kCloseY, kCloseW, kCloseH)) {
			return;
		}

		size_t count = settings.pingTargetCount();
		bool handled = false;
		for (size_t i = 0; i < count && !handled; i++) {
			int16_t rowY = kPingListY0 + static_cast<int16_t>(i) * kListRowStep;
			if (hitRect(x, y, kScreenW - kListRemoveW - 10, rowY, kListRemoveW, kListRowH)) {
				settings.removePingTarget(i);
				handled = true;
			}
		}
		if (!handled && count < SettingsManager::kMaxPingTargets) {
			int16_t addY = kPingListY0 + static_cast<int16_t>(count) * kListRowStep;
			if (hitRect(x, y, 10, addY, kScreenW - 20, kListRowH)) {
				bool cancelled = false;
				String newIp = NumericKeypad::run(display, touch, "Ping-Ziel IP", "", cancelled);
				if (!cancelled && !newIp.isEmpty()) {
					settings.addPingTarget(newIp);
				}
				handled = true;
			}
		}
		if (handled) {
			drawPingTargetList(display, settings);
		}
	}
}

// Helligkeit +/-, WLAN-Neuauswahl, Sensormeter-Ziel, Ping-Ziele. Kehrt zum
// Aufrufer (Hauptmenu) zurueck, aendert also nie den Betriebsmodus - daher
// kein bool-Rueckgabewert noetig.
void runSystemSettings(DisplayManager &display, TouchManager &touch, WlanManager &wlan,
                       SettingsManager &settings, BacklightManager &backlight,
                       WifiOnboarding &onboarding) {
	uint8_t brightness = settings.brightnessPercent();
	auto redraw = [&]() {
		drawSystemSettings(display, brightness, settings.sensormeterTargetCount(), settings.pingTargetCount());
	};
	redraw();

	while (true) {
		int16_t x, y;
		waitForTapEvent(touch, x, y);

		if (hitRect(x, y, kCloseX, kCloseY, kCloseW, kCloseH)) {
			return;
		}
		if (hitRect(x, y, kMinusX, kAdjustY, kAdjustW, kAdjustH)) {
			if (brightness >= 5) brightness -= 5;
			backlight.setPercent(brightness);
			settings.setBrightnessPercent(brightness);
			redraw();
		} else if (hitRect(x, y, kPlusX, kAdjustY, kAdjustW, kAdjustH)) {
			if (brightness <= 95) brightness += 5;
			backlight.setPercent(brightness);
			settings.setBrightnessPercent(brightness);
			redraw();
		} else if (hitRect(x, y, kSysButtonX, kWlanButtonY, kSysButtonW, kSysButtonH)) {
			onboarding.run(display, touch, wlan);
			redraw();
		} else if (hitRect(x, y, kSysButtonX, kSensormeterButtonY, kSysButtonW, kSysButtonH)) {
			runSensormeterConfig(display, touch, settings);
			redraw();
		} else if (hitRect(x, y, kSysButtonX, kPingButtonY, kSysButtonW, kSysButtonH)) {
			runPingTargetsConfig(display, touch, settings);
			redraw();
		}
	}
}

} // namespace

void SettingsUI::run(DisplayManager &display, TouchManager &touch, WlanManager &wlan,
                      SettingsManager &settings, BacklightManager &backlight,
                      WifiOnboarding &onboarding) {
	drawMenu(display);

	while (true) {
		int16_t x, y;
		waitForTapEvent(touch, x, y);

		if (hitRect(x, y, kCloseX, kCloseY, kCloseW, kCloseH)) {
			return;
		}

		int idx = hitTestMenu(x, y);
		if (idx < 0) {
			continue;
		}

		bool exitSettings = false;
		switch (idx) {
			case 0: // Slide
				exitSettings = runSlideConfig(display, touch, settings);
				break;
			case 1: // Static
				exitSettings = runStaticConfig(display, touch, settings);
				break;
			case 2: // Snake
				runSnake(display, touch);
				break;
			case 3: // Systemeinstellungen
				runSystemSettings(display, touch, wlan, settings, backlight, onboarding);
				break;
		}

		if (exitSettings) {
			return;
		}
		drawMenu(display);
	}
}
