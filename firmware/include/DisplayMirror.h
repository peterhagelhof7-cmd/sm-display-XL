#pragma once

#include "DataSource.h"

// Momentaufnahme dessen, was das Display gerade anzeigt - vom Hauptloop
// (main.cpp) bei jedem Durchlauf befuellt und vom WebServerManager nur
// lesend fuer den Web-Spiegel (/display, /api/display) ausgewertet.
//
// Bewusst eine einfache POD-Struktur ohne Sperre: Beide Zugriffe laufen im
// Rahmen desselben Musters wie die uebrigen Manager (Hauptloop schreibt,
// Async-Webserver-Callback liest kohaerente Einzelfelder) - ein kurzzeitig
// gemischter Stand einzelner Felder ist fuer eine reine Anzeige unkritisch
// und wird beim naechsten ~1s-Polling ohnehin korrigiert.
struct DisplayMirrorState {
	DataSource activeSource = DataSource::Dht11;
	// Nur fuer activeSource == Sensormeter relevant (welches Ziel/welcher
	// Sensor gerade gezeigt wird); -1 = keine feste Zuordnung.
	int8_t smTargetIndex = -1;
	uint8_t smSensorIndex = 0;
	// true, wenn der Nutzer aus der Uebersicht in ein sm hineingetippt hat
	// und die Auto-Rotation dafuer angehalten ist ("gehalten").
	bool held = false;
	bool alertActive = false;
	bool alertBlue = false;
	OperatingMode mode = OperatingMode::Slide;
};
