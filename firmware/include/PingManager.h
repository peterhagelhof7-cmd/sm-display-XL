#pragma once

#include <Arduino.h>
#include <freertos/semphr.h>

#include "SettingsManager.h"

// Kontinuierlicher Ping an google.com (lastenheft.txt Abschnitt 7.4 + 8/9)
// plus bis zu 5 zusaetzliche, ueber die Einstellungen konfigurierte Ziele.
// Um die Touch-Reaktion nicht durch mehrere Timeouts pro Zyklus zu
// blockieren, wird pro update()-Aufruf hoechstens EIN Google-Ping UND EIN
// Ziel-Ping (round-robin) ausgefuehrt, nicht alle auf einmal - siehe
// docs/entscheidungen.md.
//
// Mutex-geschuetzt wie SettingsManager: update() laeuft im Hauptloop, die
// Getter werden zusaetzlich vom asynchronen Webserver-Task gelesen. Anders
// als bei SettingsManager duerfen die blockierenden Ping()-Aufrufe (bis zu
// ~1s pro Ziel) NICHT innerhalb der Sperre liegen, sonst wuerde der
// Webserver-Task bei jedem Dashboard-Aufruf bis zu 1-2s warten - pingGoogle()/
// pingNextTarget() rechnen daher ausserhalb der Sperre und uebernehmen das
// Ergebnis nur kurz gesperrt (siehe docs/entscheidungen.md).
class PingManager {
public:
	static constexpr uint32_t kCheckIntervalMs = 2000;
	static constexpr uint32_t kFailureAlertMs = 60000;
	static constexpr size_t kMaxTargets = 5;

	void begin();
	// Liefert true, wenn in diesem Aufruf tatsaechlich gepingt wurde.
	bool update(const SettingsManager &settings);

	bool hasGoogleReading() const;
	float googleAverageLatencyMs() const;
	bool isFailingOver1Min() const;

	size_t targetCount() const;
	String targetIp(size_t i) const;
	bool targetOk(size_t i) const;
	bool targetChecked(size_t i) const;
	// Letzte erfolgreich gemessene Latenz (ms). Bleibt bei einem
	// fehlgeschlagenen Ping auf dem zuletzt erfolgreichen Wert stehen -
	// targetHasLatency() unterscheidet "noch nie erfolgreich" von "0ms".
	float targetLatencyMs(size_t i) const;
	bool targetHasLatency(size_t i) const;

private:
	void pingGoogle();
	void pingNextTarget(const SettingsManager &settings);

	struct TargetState {
		String ip;
		bool ok = false;
		bool checked = false;
		float lastLatencyMs = 0.0f;
		bool hasLatency = false;
	};

	SemaphoreHandle_t mutex_ = nullptr;

	uint32_t lastCheckMs = 0;  // nur vom Hauptloop-Task gelesen/geschrieben, keine Sperre noetig
	float recentLatencies[5] = {0, 0, 0, 0, 0};
	size_t recentCount = 0;
	size_t recentWriteIndex = 0;

	uint32_t failureStreakStartMs = 0;

	TargetState targets[kMaxTargets];
	size_t numTargets = 0;
	size_t nextTargetIndex = 0;
};
