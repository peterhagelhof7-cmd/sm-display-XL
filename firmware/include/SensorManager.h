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
// die Getter werden vom Hauptloop UND vom asynchronen Webserver-Task gelesen
// (Dashboard) - ohne Sperre waere das ein Datenrennen (docs/entscheidungen.md).
//
// XL-BESONDERHEIT (siehe project_sm-display-XL / esp-infoscreen dht22.c): der
// eigentliche 1-Wire-Lesevorgang des DHT sperrt kurz die Interrupts. Auf dem
// ESP32-8048S070 (RGB-Panel) DARF das nicht auf demselben Core passieren, der
// die RGB-Panel-ISR bedient (das ist Core 1 = Arduino-loop/setup-Core), sonst
// verhungert dessen DMA -> Artefakte bis Absturz. Deshalb liest ein eigener,
// auf Core 0 gepinnter Task (statt direkt im Hauptloop wie bei den 2,8"-/
// OLED-Schwesterprojekten), und legt die Werte mutex-geschuetzt ab.
class SensorManager {
public:
	static constexpr uint32_t kPollIntervalMs = 5000;

	// settings wird gespeichert (Zeiger), damit der Hintergrund-Task die
	// aktuelle Kalibrierkorrektur bei jeder Messung anwenden kann.
	void begin(const SettingsManager &settings);
	// In loop() aufrufen. Liest NICHT mehr selbst den Sensor (das macht der
	// Core-0-Task), sondern meldet nur, ob seit dem letzten Aufruf eine neue
	// Messung eingetroffen ist - Aufrufer haengt daran, wann sich eine
	// Neuzeichnung/Aufzeichnung lohnt. settings bleibt aus Kompatibilitaet in
	// der Signatur (der Task nutzt den in begin() gespeicherten Zeiger).
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
	void readOnce();               // fuehrt eine DHT-Messung durch, legt sie ab
	void pollLoop();               // Endlosschleife des Hintergrund-Tasks
	static void taskThunk(void *arg);

	DHT dht{DHT11_PIN, DHT11_TYPE};
	const SettingsManager *settings_ = nullptr;
	// Nicht mutable noetig (wie bei SettingsManager) - Take/Give aendern nur
	// den internen FreeRTOS-Zustand, nicht den Handle-Wert selbst.
	SemaphoreHandle_t mutex_ = nullptr;
	bool newReading_ = false;      // vom Task gesetzt, von update() konsumiert
	bool valid = false;
	float lastTempC = 0.0f;
	float lastHumidityPct = 0.0f;
	time_t lastReadTs = 0;
};
