#pragma once

#include "BrandingManager.h"
#include "DisplayManager.h"
#include "SettingsManager.h"

// Anbieter-Branding als eigene Datenquelle (siehe DataSource::Branding):
// zeigt bevorzugt das Logo zentriert (kein Platz-Sharing mit Text, analog
// zu den OLED-Geschwisterprojekten), ohne Logo den Anbietername als
// grosse Textzeile, ohne beides einen Platzhalter ("Kein Branding
// konfiguriert") - gleiche Konvention wie PingView::drawTargetList() bei
// leerer Zielliste.
class BrandingView {
 public:
	void draw(DisplayManager &display, const BrandingManager &branding, const SettingsManager &settings,
	          int16_t contentTop, int16_t contentBottom, uint16_t bgColor);

	// Verwirft den Redraw-Cache - noetig, wenn zwischenzeitlich ein anderer
	// Bildschirm (InfoUI/SettingsUI) den ganzen Screen ueberschrieben hat,
	// siehe main.cpp und GraphManager::forceRedraw().
	void forceRedraw() { everDrawn_ = false; }

 private:
	bool everDrawn_ = false;
	bool lastHasLogo_ = false;
	String lastVendorName_;
	uint16_t lastBgColor_ = 0;
};
