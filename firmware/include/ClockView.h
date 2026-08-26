#pragma once

#include "DisplayManager.h"

// Uhrzeit als eigene Datenquelle (lastenheft.txt Abschnitt 7.2): obere
// zwei Drittel HH:MM (24h), unteres Drittel Wochentag + Datum. Der
// Inhaltsbereich wird als Parameter uebergeben statt fest aus Layout.h
// gelesen, weil im Static-Modus mit Quelle "Uhrzeit" die untere
// Statusleiste entfaellt und die Ansicht dadurch groesser wird (siehe
// SettingsUI/main.cpp).
class ClockView {
public:
	void draw(DisplayManager &display, int16_t contentTop, int16_t contentBottom,
	          uint16_t bgColor = TFT_WHITE);
};
