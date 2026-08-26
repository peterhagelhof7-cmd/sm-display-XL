#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/semphr.h>
#include <stdint.h>
#include <time.h>

#include "DataSource.h"

// Persistiert die Einstellungen aus lastenheft.txt Abschnitt 6 in NVS
// (Namespace "settings"): Betriebsmodus, Slide-Intervall, Static-Quelle,
// Helligkeit, Sensormeter-Ziel(e), Ping-Ziele, Geraetename + Web-Passwort
// (siehe lastenheft.txt Abschnitt 8/11, Webserver-Zusatzfunktion), sowie
// die spaeter ergaenzte DHT11-Kalibrierkorrektur und die Warnschwellwerte
// (DHT11 intern, Sensormeter pro Ziel/Sensor, Ping - siehe
// docs/entscheidungen.md). WLAN-Zugangsdaten liegen separat in
// WlanManager (Namespace "wifi").
//
// Mutex-geschuetzt (wie DataManager im Sensormeter-Projekt): wird sowohl
// aus dem Hauptloop/Touch-UI als auch aus den asynchronen Webserver-
// Callbacks (ESPAsyncWebServer laeuft in einem eigenen FreeRTOS-Task,
// nicht im Arduino-Hauptloop) gleichzeitig gelesen/geschrieben - ohne
// Sperre waere das ein Datenrennen auf den String-Feldern.
class SettingsManager {
public:
	static constexpr uint16_t kSlideIntervalMinSec = 5;
	static constexpr uint16_t kSlideIntervalMaxSec = 60;
	static constexpr uint16_t kSlideIntervalStepSec = 5;
	static constexpr uint16_t kSlideIntervalDefaultSec = 10;
	static constexpr uint8_t kBrightnessDefaultPercent = 60;
	static constexpr size_t kMaxPingTargets = 5;
	static constexpr size_t kMaxSensormeterTargets = 5;
	// Sensor 1 (immer vorhanden) + Sensor 2 (nur bei "Sensormeter PRO"-
	// Zielen, siehe SensormeterManager::isPro()) - fuer die pro Sensor
	// getrennten Warnschwellwerte unten.
	static constexpr uint8_t kMaxSensorsPerTarget = 2;
	// Sentinel fuer Temperatur-/Feuchte-Schwellwerte: "kein Schwellwert
	// gesetzt" (auch 0 ist ein plausibler echter Messwert, daher kein
	// Sentinel dafuer moeglich). Ping-Latenzschwellwerte nutzen dagegen 0
	// als "aus", da 0ms nie eine reale Latenz ist (Ping.averageTime() liefert
	// mindestens 1ms) - siehe docs/entscheidungen.md.
	static constexpr int16_t kThresholdDisabled = INT16_MIN;

	void begin();

	OperatingMode mode() const;
	uint16_t slideIntervalSec() const;
	DataSource staticSource() const;
	uint8_t brightnessPercent() const;

	void setSlideMode(uint16_t intervalSec);
	void setStaticMode(DataSource source);
	void setBrightnessPercent(uint8_t percent);

	// Kalibrierkorrektur fuer den internen DHT11-Sensor (ganze °C/%,
	// positiv oder negativ) - falls er systematisch von einem Referenz-
	// sensor am gleichen Standort abweicht. Wird direkt in
	// SensorManager::update() auf den Rohmesswert angewendet, damit Touch-
	// UI, Webserver UND die Warnschwellwert-Auswertung immer denselben,
	// bereits korrigierten Wert sehen (siehe docs/entscheidungen.md).
	int16_t dhtTempOffsetC() const;
	int16_t dhtHumOffsetPct() const;
	void setDhtOffsets(int16_t tempOffsetC, int16_t humOffsetPct);
	// Wall-Clock-Zeitpunkt (time(nullptr)), zu dem die Offsets zuletzt
	// TATSAECHLICH geaendert wurden (nicht nur gespeichert - setDhtOffsets()
	// aktualisiert diesen Zeitstempel nur, wenn sich mindestens einer der
	// beiden Werte gegenueber dem bisherigen unterscheidet). 0 = noch nie
	// kalibriert. Persistiert in NVS, ueberlebt also einen Neustart.
	time_t dhtOffsetSetTime() const;

	// Bis zu kMaxSensormeterTargets Ziele, eine gemeinsame SNMP-Community fuer
	// alle (siehe docs/entscheidungen.md). Wie bei den Ping-Zielen ist 0 der
	// technische Startzustand eines frisch geflashten Geraets; die
	// Bedienoberflaechen (Touch/Web) verhindern aber das Entfernen des
	// letzten verbliebenen Ziels, sodass im laufenden Betrieb immer
	// mindestens eins konfiguriert bleibt, sobald eins hinzugefuegt wurde.
	String sensormeterCommunity() const;
	void setSensormeterCommunity(const String &community);

	size_t sensormeterTargetCount() const;
	String sensormeterTargetIp(size_t i) const;
	// Liefert false, wenn bereits kMaxSensormeterTargets erreicht sind.
	bool addSensormeterTarget(const String &ip);
	void removeSensormeterTarget(size_t i);
	// Aendert die IP eines bestehenden Ziels in-place (Schwellwerte bleiben
	// erhalten) - macht auch das letzte verbliebene Ziel "zuruecksetzbar",
	// obwohl es sich (siehe removeSensormeterTarget()) nicht entfernen
	// laesst, siehe docs/entscheidungen.md.
	void setSensormeterTargetIp(size_t i, const String &ip);

	size_t pingTargetCount() const;
	String pingTargetIp(size_t i) const;
	// Liefert false, wenn bereits kMaxPingTargets erreicht sind.
	bool addPingTarget(const String &ip);
	void removePingTarget(size_t i);

	String deviceName() const;
	String webPassword() const;
	void setDeviceName(const String &name);
	void setWebPassword(const String &password);

	// Anbieter-Branding (Weisslabel), siehe BrandingManager fuer das
	// zugehoerige Logo (liegt dort separat auf LittleFS, nicht in NVS - ein
	// 40000-Byte-Logo waere fuer einen einzelnen NVS-Eintrag ungeeignet
	// gross). Leer = kein Anbietername gesetzt, anders als deviceName()
	// OHNE Default-Fallback.
	String brandingVendorName() const;
	void setBrandingVendorName(const String &name);

	// Werksreset "Konfiguration"/"Alles" (siehe WebServerManager/main.cpp
	// Serial-Kommandozeile): leert den kompletten NVS-Namespace "settings"
	// (Betriebsmodus, Helligkeit, Sensormeter-/Ping-Ziele samt
	// Schwellwerten, DHT-Kalibrierung, Geraetename, Web-Passwort) und laedt
	// anschliessend die eingebauten Defaults neu. keepBrandingName: true
	// beim Umfang "Konfiguration" (Branding hat dort einen eigenen
	// Reset-Umfang), false beim Umfang "Alles". Ruehrt NICHT an
	// WlanManager (siehe dortiges clearCredentials()), GraphManager (siehe
	// reset()) oder der Touch-Kalibrierung (physische Hardware-Kalibrierung,
	// kein Konfigurationswert) - der Aufrufer kombiniert das je nach Umfang.
	void resetConfig(bool keepBrandingName);

	// Warnschwellwerte (nur ueber den Webserver einstellbar, siehe
	// docs/entscheidungen.md): bei Ueber-/Unterschreitung faerbt main.cpp den
	// gesamten Bildschirm rot - derselbe Mechanismus wie der bestehende
	// Ping-Ausfall-Alarm (lastenheft.txt Abschnitt 9). kThresholdDisabled
	// (bzw. 0 bei Ping-Latenz) = kein Schwellwert gesetzt.

	// DHT11 (intern)
	int16_t dhtTempMinC() const;
	int16_t dhtTempMaxC() const;
	int16_t dhtHumMinPct() const;
	int16_t dhtHumMaxPct() const;
	void setDhtThresholds(int16_t tempMinC, int16_t tempMaxC, int16_t humMinPct, int16_t humMaxPct);

	// Sensormeter, pro Ziel UND pro Sensor getrennt (sensorIdx: 0 = Sensor 1,
	// immer vorhanden; 1 = Sensor 2, nur bei PRO-Zielen relevant - siehe
	// SensormeterManager::isPro()/sensorValid()).
	int16_t sensormeterTempMinC(size_t targetIdx, uint8_t sensorIdx) const;
	int16_t sensormeterTempMaxC(size_t targetIdx, uint8_t sensorIdx) const;
	int16_t sensormeterHumMinPct(size_t targetIdx, uint8_t sensorIdx) const;
	int16_t sensormeterHumMaxPct(size_t targetIdx, uint8_t sensorIdx) const;
	void setSensormeterThresholds(size_t targetIdx, uint8_t sensorIdx, int16_t tempMinC, int16_t tempMaxC,
	                               int16_t humMinPct, int16_t humMaxPct);

	// Ping-Latenz (ms), pro Ziel einzeln plus google.com separat.
	uint16_t pingMaxLatencyMs(size_t i) const;
	void setPingMaxLatencyMs(size_t i, uint16_t ms);
	uint16_t googlePingMaxLatencyMs() const;
	void setGooglePingMaxLatencyMs(uint16_t ms);

private:
	void load();
	void save();
	void savePingTargets();
	void saveSensormeterTargets();
	void saveDhtThresholds();
	void saveSensormeterThresholds();
	void savePingThresholds();

	Preferences prefs;
	// Nicht mutable: Take/Give aendern nur den internen FreeRTOS-Zustand des
	// Semaphors, nicht den Wert des Handles selbst - const-Methoden duerfen
	// das Handle unveraendert weiterreichen.
	SemaphoreHandle_t mutex_ = nullptr;

	OperatingMode mode_ = OperatingMode::Slide;
	uint16_t slideIntervalSec_ = kSlideIntervalDefaultSec;
	DataSource staticSource_ = DataSource::Dht11;
	uint8_t brightnessPercent_ = kBrightnessDefaultPercent;

	String sensormeterTargets_[kMaxSensormeterTargets];
	size_t sensormeterTargetCount_ = 0;
	String sensormeterCommunity_ = "public";

	String pingTargets_[kMaxPingTargets];
	size_t pingTargetCount_ = 0;

	String deviceName_ = "Sensormeter Display";
	String webPassword_ = "installer";
	String brandingVendorName_;

	int16_t dhtTempOffsetC_ = 0;
	int16_t dhtHumOffsetPct_ = 0;
	uint32_t dhtOffsetSetTs_ = 0;

	int16_t dhtTempMinC_ = kThresholdDisabled;
	int16_t dhtTempMaxC_ = kThresholdDisabled;
	int16_t dhtHumMinPct_ = kThresholdDisabled;
	int16_t dhtHumMaxPct_ = kThresholdDisabled;

	int16_t smTempMin_[kMaxSensormeterTargets][kMaxSensorsPerTarget];
	int16_t smTempMax_[kMaxSensormeterTargets][kMaxSensorsPerTarget];
	int16_t smHumMin_[kMaxSensormeterTargets][kMaxSensorsPerTarget];
	int16_t smHumMax_[kMaxSensormeterTargets][kMaxSensorsPerTarget];

	uint16_t pingMaxMs_[kMaxPingTargets] = {0, 0, 0, 0, 0};
	uint16_t googlePingMaxMs_ = 0;
};
