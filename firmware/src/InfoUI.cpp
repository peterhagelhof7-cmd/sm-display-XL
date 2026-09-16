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
constexpr int16_t kCloseX = kScreenW - 74;
constexpr int16_t kCloseY = 8;
constexpr int16_t kCloseW = 64;
constexpr int16_t kCloseH = 64;
} // namespace

void InfoUI::run(DisplayManager &display, TouchManager &touch, SettingsManager &settings, WlanManager &wlan) {
	TFT_eSPI &tft = display.raw();
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	tft.setTextDatum(TL_DATUM);
	tft.setTextFont(4);
	tft.drawString("Geraete-Info", 16, 12);
	UiHelpers::drawCloseButton(tft, kCloseX, kCloseY, kCloseW, kCloseH);

	// 800x480: Label/Wert-Bloecke ueber die Seite verteilt (Font 4 statt der
	// alten 2,8"-Font-2-Labels), Blockschritt 84px.
	tft.setTextFont(4);
	tft.drawString("Systemname (im Webinterface aenderbar):", 16, 96);
	tft.setTextFont(4);
	tft.drawString(settings.deviceName(), 16, 128);

	tft.setTextFont(4);
	tft.drawString("IP-Adresse:", 16, 180);
	tft.setTextFont(4);
	tft.drawString(WiFi.localIP().toString(), 16, 212);

	tft.setTextFont(4);
	tft.drawString("Verbindungsart:", 16, 264);
	tft.setTextFont(4);
	tft.drawString(wlan.hasStaticIp() ? "Statisch" : "DHCP (automatisch)", 16, 296);

	tft.setTextFont(4);
	tft.drawString("Firmware:", 16, 348);
	tft.setTextFont(4);
	tft.drawString(DEVICE_FIRMWARE_VERSION, 16, 380);

	while (true) {
		int16_t x, y;
		UiHelpers::waitForTapEvent(touch, x, y);
		if (UiHelpers::hitRect(x, y, kCloseX, kCloseY, kCloseW, kCloseH)) {
			return;
		}
	}
}
