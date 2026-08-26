#pragma once

#include "DisplayManager.h"
#include "PingManager.h"

// Zwei Ansichten fuer die Ping-Datenquellen (lastenheft.txt Abschnitt 7.4):
// drawAverage() = durchschnittliche Latenz zu google.com (grosse Zahl),
// drawTargetList() = Liste der zusaetzlich konfigurierten Ziele, je Zeile
// IP + Status (gruen=OK, rot=Fehler).
class PingView {
public:
	void drawAverage(DisplayManager &display, const PingManager &ping, int16_t contentTop,
	                  int16_t contentBottom, uint16_t bgColor);
	void drawTargetList(DisplayManager &display, const PingManager &ping, int16_t contentTop,
	                     int16_t contentBottom, uint16_t bgColor);

	// Verwirft beide Redraw-Caches: noetig, wenn zwischenzeitlich ein
	// anderer Bildschirm (InfoUI/SettingsUI) den ganzen Screen
	// ueberschrieben hat, siehe main.cpp und GraphManager::forceRedraw().
	void forceRedraw() {
		everDrawnAvg = false;
		everDrawnList = false;
	}

private:
	// Redraw-Cache wie in StatusBar/GraphManager (siehe dort/docs/entscheidungen.md):
	// PingManager pollt alle 2s, ein bedingungsloser Full-Redraw bei jedem
	// Poll (auch ohne geaenderten Anzeigewert) fuehrte zu sichtbarem Flackern.
	bool everDrawnAvg = false;
	bool lastHasGoogleReading = false;
	int lastLatencyRounded = 0;
	uint16_t lastAvgBgColor = 0;

	bool everDrawnList = false;
	String lastListSignature;
	uint16_t lastListBgColor = 0;
};
