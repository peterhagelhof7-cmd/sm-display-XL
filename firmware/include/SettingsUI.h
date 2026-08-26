#pragma once

#include "DisplayManager.h"
#include "TouchManager.h"
#include "WlanManager.h"
#include "WifiOnboarding.h"
#include "SettingsManager.h"
#include "BacklightManager.h"

// Einstellungen-Dialog (Zahnrad), siehe lastenheft.txt Abschnitt 6:
// Auswahl des Betriebsmodus Slide/Static/Snake/Systemeinstellungen.
// Blockierend, bis der Nutzer schliesst oder einen Modus uebernimmt.
class SettingsUI {
public:
	void run(DisplayManager &display, TouchManager &touch, WlanManager &wlan,
	         SettingsManager &settings, BacklightManager &backlight, WifiOnboarding &onboarding);
};
