#pragma once

#include <Arduino.h>

#include "DisplayManager.h"
#include "TouchManager.h"

// Einfaches numerisches Tastenfeld (0-9, Punkt, Loeschen) fuer IP-Adressen -
// Sensormeter-Ziel (P7) und Ping-Ziele (P8) brauchen beide reine
// Zifferneingabe, eine volle Bildschirmtastatur (siehe WifiOnboarding) waere
// dafuer unnoetig komplex. Blockierend bis OK oder Abbrechen.
namespace NumericKeypad {

String run(DisplayManager &display, TouchManager &touch, const String &title,
           const String &initialValue, bool &cancelled);

} // namespace NumericKeypad
