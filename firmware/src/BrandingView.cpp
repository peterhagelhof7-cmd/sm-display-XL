#include "BrandingView.h"

#include <new>

void BrandingView::draw(DisplayManager &display, const BrandingManager &branding, const SettingsManager &settings,
                         int16_t contentTop, int16_t contentBottom, uint16_t bgColor) {
	bool hasLogo = branding.hasLogo();
	String vendorName = settings.brandingVendorName();
	bool changed = !everDrawn_ || hasLogo != lastHasLogo_ || vendorName != lastVendorName_ || bgColor != lastBgColor_;
	if (!changed) {
		return;
	}
	everDrawn_ = true;
	lastHasLogo_ = hasLogo;
	lastVendorName_ = vendorName;
	lastBgColor_ = bgColor;

	TFT_eSPI &tft = display.raw();
	int16_t h = contentBottom - contentTop;
	tft.fillRect(0, contentTop, DisplayManager::kScreenWidth, h, bgColor);

	if (hasLogo) {
		// Heap- statt stackallokiert (40000 Byte wuerden den 8-KB-Loop-Task-
		// Stack sprengen) und nur transient fuer diesen Aufruf, statt
		// dauerhaft als static Array reserviert - draw() laeuft nur bei
		// tatsaechlicher Aenderung (siehe Redraw-Cache oben), ein Malloc pro
		// Aufruf ist hier unkritisch.
		uint8_t *logoBuf = new (std::nothrow) uint8_t[BrandingManager::kLogoBytes];
		if (logoBuf && branding.loadLogo(logoBuf, BrandingManager::kLogoBytes)) {
			int16_t x = (DisplayManager::kScreenWidth - BrandingManager::kLogoWidth) / 2;
			int16_t y = contentTop + (h - BrandingManager::kLogoHeight) / 2;
			tft.pushImage(x, y, BrandingManager::kLogoWidth, BrandingManager::kLogoHeight,
			              reinterpret_cast<uint16_t *>(logoBuf));
			delete[] logoBuf;
			return;
		}
		delete[] logoBuf;
		// Fall-through bei Allokations-/Lesefehler: Platzhalter unten zeigen
		// statt eines halb gezeichneten/leeren Bildschirms.
	}

	tft.setTextColor(TFT_BLACK, bgColor);
	tft.setTextDatum(MC_DATUM);
	tft.setTextFont(4);
	int16_t midY = contentTop + h / 2;
	if (vendorName.length() > 0) {
		tft.drawString(vendorName, DisplayManager::kScreenWidth / 2, midY);
	} else {
		tft.drawString("Kein Branding konfiguriert", DisplayManager::kScreenWidth / 2, midY);
	}
	tft.setTextDatum(TL_DATUM);
}
