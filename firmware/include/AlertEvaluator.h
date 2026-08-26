#pragma once

#include "PingManager.h"
#include "SensorManager.h"
#include "SensormeterManager.h"
#include "SettingsManager.h"

// Warnschwellwert-Auswertung (DHT11 intern, Sensormeter-Ziele/Sensoren,
// Ping) - ausgelagert aus main.cpp, damit dieselbe Logik auch vom
// unauthentifizierten Webserver-Dashboard genutzt werden kann, ohne sie
// dort ein zweites Mal nachzubilden (siehe docs/entscheidungen.md).
struct AlertInfo {
	bool active;
	bool blue; // true = Unterschreitung, false = Ueberschreitung/Ausfall
	const char *source; // "Intern" / "Sensormeter" / "Ping"
	// Anzahl WEITERER Kategorien (von max. 2, da 3 Kategorien insgesamt
	// existieren), die ZUSAETZLICH zu `source` gerade ebenfalls einen
	// Verstoss melden - z.B. Intern UND Ping gleichzeitig ausserhalb der
	// Spec: source="Intern", extraCount=1. Nur auf Kategorie-Ebene
	// gezaehlt (nicht z.B. pro einzelnem Sensormeter-Sensor), da ohnehin
	// nur eine Quelle in der Statusleiste anzeigbar ist.
	uint8_t extraCount;

	// Explizite Konstruktoren statt Default-Member-Initializer: mit
	// Default-Initializern ist die Struct kein Aggregat mehr (vor C++14),
	// die {true,false,"X"}-Rueckgaben in computeAlertInfo() scheitern sonst
	// am Compiler.
	AlertInfo() : active(false), blue(false), source(""), extraCount(0) {}
	AlertInfo(bool activeIn, bool blueIn, const char *sourceIn, uint8_t extraCountIn = 0)
	    : active(activeIn), blue(blueIn), source(sourceIn), extraCount(extraCountIn) {}
};

// Prioritaet bei mehreren gleichzeitigen Verstoessen: erster Treffer in
// fester Reihenfolge Intern -> Sensormeter -> Ping gewinnt (nur eine
// Quelle gleichzeitig anzeigbar, siehe StatusBar) - keine inhaltliche
// Rangfolge, nur zur deterministischen Anzeige. `extraCount` macht
// zusaetzliche, gleichzeitig aktive Kategorien sichtbar, statt sie
// stillschweigend zu verdecken.
AlertInfo computeAlertInfo(const SensorManager &sensor, const SensormeterManager &sensormeterManager,
                            const PingManager &pingManager, const SettingsManager &settings);
