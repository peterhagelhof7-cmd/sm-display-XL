#pragma once

#include <Arduino.h>
#include <freertos/semphr.h>

#include "SettingsManager.h"
#include "SnmpClient.h"

// Fragt bis zu SettingsManager::kMaxSensormeterTargets Sensormeter-Ziele
// (WT32-ETH01, SNMP v1) im Round-Robin ab - ein Ziel pro update()-Aufruf,
// analog zu PingManager::pingNextTarget(), damit ein einzelnes nicht
// erreichbares Ziel nicht den ganzen Hauptloop blockiert. OIDs siehe
// lastenheft.txt Abschnitt 7.3 / docs/entscheidungen.md.
//
// Pro Ziel wird zuerst einmalig die Identitaet aufgeloest (Systemname, Typ -
// zur PRO-Erkennung ueber den String "Sensormeter PRO" - und Sensor-1/2-
// Name), danach laufend nur noch die Messwerte aktualisiert. Ein Ziel vom
// Typ "Sensormeter PRO" liefert zwei Sensor-Slides (Sensor 1 + 2), alle
// anderen genau einen - siehe SensormeterView, die diese Slides anzeigt.
//
// Mutex-geschuetzt wie SettingsManager/PingManager: update() laeuft im
// Hauptloop, die Getter werden zusaetzlich vom asynchronen Webserver-Task
// gelesen. Wie bei PingManager liegen die blockierenden SNMP-Rundreisen
// (resolveIdentity()/refreshReadings(), mehrere Sekunden bei einem nicht
// erreichbaren Ziel) AUSSERHALB der Sperre - nur das Uebernehmen des
// Ergebnisses in targets_ ist kurz gesperrt (siehe docs/entscheidungen.md).
class SensormeterManager {
public:
	static constexpr size_t kMaxTargets = SettingsManager::kMaxSensormeterTargets;
	static constexpr uint32_t kCheckIntervalMs = 4000;

	void begin();

	// Liefert true, wenn in diesem Aufruf tatsaechlich ein SNMP-Vorgang
	// stattfand (fuer die Aufrufer-seitige Redraw-Entscheidung).
	bool update(const SettingsManager &settings);

	size_t targetCount() const;
	bool isResolved(size_t i) const;
	bool isPro(size_t i) const;
	String systemName(size_t i) const;
	// sensorIndex: 0 = Sensor 1 (immer vorhanden), 1 = Sensor 2 (nur wenn isPro()).
	String sensorName(size_t i, uint8_t sensorIndex) const;
	bool sensorValid(size_t i, uint8_t sensorIndex) const;
	float sensorTempC(size_t i, uint8_t sensorIndex) const;
	float sensorHumidityPct(size_t i, uint8_t sensorIndex) const;
	// Fuer die Sensormeter-Uebersichtsseite (Name + Uptime aller Ziele) -
	// wird wie die Messwerte laufend in refreshReadings() nachgezogen, nicht
	// nur einmalig bei resolveIdentity().
	bool uptimeValid(size_t i) const;
	uint32_t uptimeSeconds(size_t i) const;

private:
	struct Target {
		String ip;
		bool identityResolved = false;
		String systemName = "Sensormeter";
		bool isPro = false;
		String sensorName[2] = {"Intern", ""};
		bool valid[2] = {false, false};
		float tempC[2] = {0, 0};
		float humidityPct[2] = {0, 0};
		bool uptimeValid = false;
		uint32_t uptimeSeconds = 0;
	};

	// Gleicht targets_ mit den in den Einstellungen konfigurierten IPs ab -
	// bei Aenderung (neues/entferntes/geaendertes Ziel) wird der betroffene
	// Slot zurueckgesetzt, damit seine Identitaet neu aufgeloest wird. Keine
	// Netzwerk-I/O, daher komplett gesperrt unproblematisch.
	void syncTargets(const SettingsManager &settings);
	// idx statt Referenz: der eigentliche SNMP-Verkehr (in .cpp) laeuft
	// AUSSERHALB der Sperre, targets_[idx] wird erst beim Uebernehmen des
	// Ergebnisses kurz gesperrt angefasst.
	void resolveIdentity(size_t idx, const String &ip, const String &community);
	void refreshReadings(size_t idx, const String &ip, bool isPro, const String &community);

	SemaphoreHandle_t mutex_ = nullptr;
	Target targets_[kMaxTargets];
	size_t count_ = 0;
	size_t nextIndex_ = 0;
	uint32_t lastCheckMs_ = 0;  // nur vom Hauptloop-Task gelesen/geschrieben, keine Sperre noetig
	SnmpClient snmp_;
};
