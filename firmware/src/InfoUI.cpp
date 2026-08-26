#include "InfoUI.h"

#include "UiHelpers.h"

#if __has_include("config.h")
#include "config.h"
#endif
#ifndef DEVICE_FIRMWARE_VERSION
#define DEVICE_FIRMWARE_VERSION "0.0.0"
#endif

namespace {
constexpr int16_t kScreenW = DisplayManager::kScreenWidth;
constexpr int16_t kCloseX = kScreenW - 34;
constexpr int16_t kCloseY = 4;
constexpr int16_t kCloseW = 28;
constexpr int16_t kCloseH = 28;
} // namespace

void InfoUI::run(DisplayManager &display, TouchManager &touch, SettingsManager &settings, WlanManager &wlan) {
	TFT_eSPI &tft = display.raw();
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	tft.setTextDatum(TL_DATUM);
	tft.setTextFont(4);
	tft.drawString("Geraete-Info", 10, 6);
	UiHelpers::drawCloseButton(tft, kCloseX, kCloseY, kCloseW, kCloseH);

	tft.setTextFont(2);
	tft.drawString("Systemname (im Webinterface aenderbar):", 10, 48);
	tft.setTextFont(4);
	tft.drawString(settings.deviceName(), 10, 66);

	tft.setTextFont(2);
	tft.drawString("IP-Adresse:", 10, 94);
	tft.setTextFont(4);
	tft.drawString(WiFi.localIP().toString(), 10, 112);

	tft.setTextFont(2);
	tft.drawString("Verbindungsart:", 10, 140);
	tft.setTextFont(4);
	tft.drawString(wlan.hasStaticIp() ? "Statisch" : "DHCP (automatisch)", 10, 158);

	tft.setTextFont(2);
	tft.drawString("Firmware:", 10, 186);
	tft.setTextFont(4);
	tft.drawString(DEVICE_FIRMWARE_VERSION, 10, 204);

	while (true) {
		int16_t x, y;
		UiHelpers::waitForTapEvent(touch, x, y);
		if (UiHelpers::hitRect(x, y, kCloseX, kCloseY, kCloseW, kCloseH)) {
			return;
		}
	}
}
