#pragma once

#include <Arduino.h>
#include <DHT.h>
#include <freertos/semphr.h>
#include <time.h>

#include "pins.h"
#include "SettingsManager.h"

// DHT11 an GPIO22 (Expansion-IO2-Steckverbinder, siehe pins.h). Abfrage
// alle 5s (lastenheft.txt Abschnitt 8), mit Plausibilitaetspruefung -
// bei Fehlmessung bleibt der letzte gueltige Wert erhalten statt "--".
//
// Mutex-geschuetzt wie SettingsManager (siehe dortigen Klassenkommentar):
// update() laeuft im Arduino-Hauptloop, die Getter werden zusaetzlich vom
// asynchronen Webserver-Task gelesen (Dashboard) - ohne Sperre waere das
// ein Datenrennen (docs/entscheidungen.md).
class SensorManager {
public:
	static constexpr uint32_t kPollIntervalMs = 5000;

	void begin();
	// In loop() aufrufen; liest den Sensor hoechstens alle 5s. Wendet die
	// Kalibrierkorrektur (settings.dhtTempOffsetC()/dhtHumOffsetPct(), siehe
	// SettingsManager) direkt auf den validierten Rohmesswert an, damit
	// ALLE Verbraucher (Touch-UI, Webserver, Warnschwellwert-Auswertung)
	// automatisch den bereits korrigierten Wert sehen, ohne die Korrektur
	// an jeder Anzeigestelle einzeln nachzubilden. Liefert true, wenn in
	// diesem Aufruf tatsaechlich gelesen wurde (auch bei implausiblem
	// Ergebnis) - Aufrufer kann daran haengen, wann sich eine Neuzeichnung/
	// Aufzeichnung lohnt, statt jeden loop()-Durchlauf.
	bool update(const SettingsManager &settings);

	bool hasValidReading() const;
	float temperatureC() const;
	float humidityPercent() const;
	// Wall-Zeitpunkt (time(nullptr)) der zuletzt tatsaechlich erfassten
	// (plausiblen) Messung - fuer die Anzeige "Stand HH:MM" im Webserver-
	// Dashboard. 0, falls noch nie ein plausibler Wert gelesen wurde oder
	// vor dem ersten NTP-Sync (siehe TimeSync).
	time_t lastReadTime() const;

private:
	bool isPlausible(float tempC, float humidityPct) const;

	DHT dht{DHT11_PIN, DHT11_TYPE};
	// Nicht mutable noetig (wie bei SettingsManager) - Take/Give aendern nur
	// den internen FreeRTOS-Zustand, nicht den Handle-Wert selbst.
	SemaphoreHandle_t mutex_ = nullptr;
	uint32_t lastPollMs = 0;
	bool valid = false;
	float lastTempC = 0.0f;
	float lastHumidityPct = 0.0f;
	time_t lastReadTs = 0;
};
