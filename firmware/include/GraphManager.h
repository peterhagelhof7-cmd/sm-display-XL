#pragma once

#include <Arduino.h>
#include <freertos/semphr.h>
#include <time.h>

#include "DisplayManager.h"

// DHT11-Datenquelle: aktueller Wert (oberes Drittel) + Temperatur-/
// Feuchte-Verlauf (untere zwei Drittel), siehe lastenheft.txt Abschnitt 7.1.
// Verlauf als Ringpuffer (24 Eintraege a 30 Minuten = 12h) in
// history.csv auf LittleFS, wird beim Boot geladen.
//
// Mutex-geschuetzt wie SettingsManager, aber NUR fuer die vier oeffentlichen
// entry*()-Getter (siehe dortigen Kommentar) - die werden vom asynchronen
// Webserver-Task gelesen (Dashboard-Graph). appendEntry()/drawFullScreen()/
// drawGraph() sowie der Redraw-Cache (everDrawn etc.) laufen ausschliesslich
// im Hauptloop-/UI-Task und brauchen daher keine Sperre untereinander -
// siehe docs/entscheidungen.md.
class GraphManager {
public:
	static constexpr size_t kMaxEntries = 24;
	static constexpr uint32_t kRecordIntervalSec = 30UL * 60UL;

	void begin();
	// In loop() aufrufen; legt hoechstens alle 30 Minuten einen neuen
	// Eintrag an - nur wenn eine gueltige Uhrzeit vorliegt (TimeSync).
	void maybeRecord(float tempC, float humidityPct);

	// tempMinC/tempMaxC/humMinPct/humMaxPct: Warnschwellwerte (siehe
	// SettingsManager::kThresholdDisabled fuer "kein Schwellwert") - werden
	// als gestrichelte Referenzlinien im Graph eingezeichnet.
	void drawFullScreen(DisplayManager &display, float currentTempC, float currentHumidityPct, bool sensorValid,
	                     int16_t tempMinC, int16_t tempMaxC, int16_t humMinPct, int16_t humMaxPct,
	                     uint16_t bgColor = TFT_WHITE);

	// Verwirft den Redraw-Cache: noetig, wenn zwischenzeitlich ein anderer
	// Bildschirm (InfoUI/SettingsUI) den ganzen Screen ueberschrieben hat -
	// sonst wuerde drawFullScreen() bei unveraenderten Werten faelschlich
	// gar nichts mehr zeichnen, siehe main.cpp.
	void forceRedraw() { everDrawn = false; }

	// Lesender Zugriff auf den Ringpuffer fuer das Webserver-Dashboard
	// (eigener SVG-Graph dort, siehe WebServerManager) - liefert 0/leere
	// Werte bei Index ausserhalb von [0, entryCount()).
	size_t entryCount() const;
	time_t entryTs(size_t i) const;
	int16_t entryTempC(size_t i) const;
	int16_t entryHumidityPct(size_t i) const;

	// Werksreset "Nur Messwerte" (siehe WebServerManager/main.cpp
	// Serial-Kommandozeile): leert den Ringpuffer und loescht history.csv.
	void reset();

private:
	struct Entry {
		time_t ts;
		int16_t tempC;
		int16_t humidityPct;
	};

	void load();
	void save() const;
	void appendEntry(time_t ts, int16_t tempC, int16_t humidityPct);
	void drawGraph(DisplayManager &display, int16_t x, int16_t y, int16_t w, int16_t h, int16_t tempMinC,
	                int16_t tempMaxC, int16_t humMinPct, int16_t humMaxPct) const;

	SemaphoreHandle_t mutex_ = nullptr;
	Entry entries[kMaxEntries];
	size_t count = 0;
	time_t lastRecordTs = 0;

	// Redraw-Cache wie in StatusBar (siehe dort/docs/entscheidungen.md):
	// drawFullScreen() wird bei jedem DHT11-Poll (alle 5s) aufgerufen, auch
	// wenn sich der gerundete Anzeigewert nicht geaendert hat - ein
	// bedingungsloser Full-Redraw fuehrte zu sichtbarem Flackern.
	bool everDrawn = false;
	bool lastSensorValid = false;
	int lastTempRounded = 0;
	int lastHumidityRounded = 0;
	uint16_t lastBgColor = 0;
	size_t lastCount = 0;
	int16_t lastTempMinC = 0;
	int16_t lastTempMaxC = 0;
	int16_t lastHumMinPct = 0;
	int16_t lastHumMaxPct = 0;
};
