#pragma once

#include "DisplayManager.h"
#include "TouchManager.h"
#include "SettingsManager.h"
#include "WlanManager.h"

// Vollbild-Info-Dialog (Nutzerwunsch): zeigt Systemname (SettingsManager::
// deviceName(), ueber den Webserver aenderbar), aktuelle IP und ob diese per
// DHCP oder statisch bezogen ist. Rein lesend, kein editierbares Feld -
// gleiches Ein-/Ausstiegsmuster wie SettingsUI (X-Button oben rechts,
// blockierend bis Antippen).
namespace InfoUI {

void run(DisplayManager &display, TouchManager &touch, SettingsManager &settings, WlanManager &wlan);

} // namespace InfoUI
