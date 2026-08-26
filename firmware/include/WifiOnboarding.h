#pragma once

#include <Arduino.h>

#include "DisplayManager.h"
#include "TouchManager.h"
#include "WlanManager.h"

// WLAN-Ersteinrichtung (Scan -> Liste -> Auswahl -> Bildschirmtastatur ->
// Verbindungsversuch -> Speichern), siehe lastenheft.txt Abschnitt 5.2.
// Blockierend: laeuft, bis eine Verbindung erfolgreich hergestellt und in
// Flash gespeichert wurde (kein Timeout - der Nutzer kann jederzeit neu
// scannen oder ein anderes Netz waehlen).
class WifiOnboarding {
public:
	void run(DisplayManager &display, TouchManager &touch, WlanManager &wlan);
};
