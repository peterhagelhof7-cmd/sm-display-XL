#pragma once

#include <Arduino.h>
#include "LGFX_XL.h"  // TFT_eSPI-Alias (LovyanGFX)

#include "TouchManager.h"

// Kleine, mehrfach genutzte Touch-UI-Bausteine (Treffertest, blockierendes
// Warten auf einen Tipp-Zyklus) - urspruenglich in WifiOnboarding.cpp,
// seit SettingsUI ein zweiter Verbraucher hinzukam hierher ausgelagert.
namespace UiHelpers {

bool hitRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh);

// Blockiert bis zu einem vollstaendigen Tipp-Zyklus (Druck + Loslassen) und
// liefert die Koordinate des Drucks.
void waitForTapEvent(TouchManager &touch, int16_t &x, int16_t &y);

// "X"-Schliessen-Button, wie er in SettingsUI/InfoUI wiederkehrt - seit
// InfoUI ein zweiter Verbraucher hinzukam hierher ausgelagert (analog zu
// hitRect/waitForTapEvent oben).
void drawCloseButton(TFT_eSPI &tft, int16_t x, int16_t y, int16_t w, int16_t h);

} // namespace UiHelpers
