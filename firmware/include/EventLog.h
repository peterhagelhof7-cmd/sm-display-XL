#pragma once

#include <Arduino.h>

// Persistentes Ereignis-Protokoll fuer Warnmeldungen (das Rot/Blau-Blinken
// der Anzeige, siehe AlertEvaluator/main.cpp). Jede Zustandsaenderung einer
// Warnung (Beginn, Wechsel der Quelle/Farbe, Entwarnung) wird mit Zeitstempel
// als Textzeile an /events.txt (LittleFS) angehaengt. Die Datei ist in den
// Web-Einstellungen herunterladbar (WebServerManager: /settings/events).
//
// Groessenbegrenzung: bei Ueberschreitung von kMaxBytes werden die aeltesten
// Zeilen verworfen (Tail behalten), damit das Dateisystem nicht volllaeuft.
class EventLog {
public:
	static constexpr const char *kPath = "/events.txt";

	// Mountet LittleFS (idempotent - BrandingManager mountet es ebenfalls).
	void begin();

	// Haengt eine Ereigniszeile an (Zeitstempel wird vorangestellt). No-op,
	// falls das Dateisystem nicht bereit ist.
	void append(const String &line);

private:
	bool ready_ = false;
};
