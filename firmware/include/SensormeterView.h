#pragma once

#include "DisplayManager.h"
#include "SensormeterManager.h"

// Ansicht fuer die Sensormeter-Datenquelle (SNMP-Abfrage anderer
// Netzwerkgeraete, siehe lastenheft.txt Abschnitt 7.3). Kein Verlauf/Graph
// gefordert, nur der aktuelle Wert.
//
// Zwei Aufrufarten fuer draw():
// - overrideTargetIndex < 0 (Default): interne Auto-Rotation mit eigenem
//   Timer durch alle aufgeloesten Sensor-Slides, unabhaengig davon, wie oft
//   draw() vom Hauptloop aufgerufen wird (analog zum WLAN-Blink-Symbol in
//   StatusBar) - genutzt vom Static-Modus, wo "Sensormeter" ein einzelner,
//   dauerhaft gewaehlter Datenquellen-Slot ist.
// - overrideTargetIndex >= 0: zeigt genau dieses eine Ziel/Sensor an, keine
//   interne Rotation - genutzt vom Slide-Modus, wo main.cpp::
//   buildSlideEntries() pro aufgeloestem Ziel (und ggf. Sensor 2 bei PRO)
//   einen eigenen Slide in der aeusseren Rotation erzeugt (Nutzeranforderung:
//   "es sollen fuer alle abgefragten sm slides generiert werden", statt sie
//   in einem einzigen Slide zu verstecken).
class SensormeterView {
public:
	void draw(DisplayManager &display, const SensormeterManager &manager, int16_t contentTop,
	          int16_t contentBottom, uint16_t bgColor, int overrideTargetIndex = -1,
	          uint8_t overrideSensorIndex = 0);

	// Uebersichtsseite: eine Zeile pro konfiguriertem Ziel mit Systemname +
	// Uptime (Nutzeranforderung, siehe draw()-Kommentar). Keine interne
	// Rotation noetig - alle Ziele passen gleichzeitig auf den Bildschirm
	// (kMaxTargets ist klein, siehe SettingsManager::kMaxSensormeterTargets).
	void drawOverview(DisplayManager &display, const SensormeterManager &manager, int16_t contentTop,
	                   int16_t contentBottom, uint16_t bgColor);

	// Trefferpruefung fuer die Uebersicht: liefert den Ziel-Index (0-basiert,
	// wie manager.targetCount()) der angetippten Zeile oder -1. Nutzt exakt
	// dieselbe Zeilen-Geometrie wie drawOverview() (gemeinsamer Helfer in der
	// .cpp), damit Tap-Bereich und gezeichnete Zeile deckungsgleich sind.
	int overviewHitTest(const SensormeterManager &manager, int16_t contentTop, int16_t contentBottom,
	                     int16_t x, int16_t y) const;

private:
	static constexpr uint32_t kSubSlideIntervalMs = 4000;

	size_t currentSlide_ = 0;
	uint32_t lastRotateMs_ = 0;
};
