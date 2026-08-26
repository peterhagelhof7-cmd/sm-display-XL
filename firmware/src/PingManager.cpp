#include "PingManager.h"

#include <ESP32Ping.h>

void PingManager::begin() {
	mutex_ = xSemaphoreCreateMutex();
}

void PingManager::pingGoogle() {
	// Blockierender Netzwerkzugriff (bis zu ~1s) AUSSERHALB der Sperre -
	// siehe Klassenkommentar.
	bool ok = Ping.ping("google.com", 1);
	float latency = ok ? static_cast<float>(Ping.averageTime()) : 0.0f;

	xSemaphoreTake(mutex_, portMAX_DELAY);
	if (ok) {
		recentLatencies[recentWriteIndex] = latency;
		recentWriteIndex = (recentWriteIndex + 1) % 5;
		if (recentCount < 5) recentCount++;
		failureStreakStartMs = 0;
	} else if (failureStreakStartMs == 0) {
		failureStreakStartMs = millis();
	}
	xSemaphoreGive(mutex_);
}

void PingManager::pingNextTarget(const SettingsManager &settings) {
	// settings.pingTargetCount() haelt selbst nur kurz die (separate)
	// SettingsManager-Sperre - unproblematisch, vor unserer eigenen Sperre
	// aufgerufen zu werden (keine verschachtelten Sperren).
	size_t count = settings.pingTargetCount();

	xSemaphoreTake(mutex_, portMAX_DELAY);
	numTargets = count;
	if (numTargets == 0) {
		xSemaphoreGive(mutex_);
		return;
	}
	if (nextTargetIndex >= numTargets) {
		nextTargetIndex = 0;
	}
	size_t i = nextTargetIndex;
	nextTargetIndex = (nextTargetIndex + 1) % numTargets;
	xSemaphoreGive(mutex_);

	// Blockierender Netzwerkzugriff AUSSERHALB der Sperre.
	String ip = settings.pingTargetIp(i);
	bool ok = Ping.ping(ip.c_str(), 1);
	float latency = ok ? static_cast<float>(Ping.averageTime()) : 0.0f;

	xSemaphoreTake(mutex_, portMAX_DELAY);
	// Absicherung: waehrend des blockierenden Pings koennte sich die
	// Zielliste ueber die Einstellungen geaendert haben (numTargets kleiner
	// geworden) - vor dem Schreiben erneut pruefen, um nicht versehentlich
	// einen inzwischen ungueltigen Slot-Index zu beschreiben.
	if (i < numTargets) {
		targets[i].ip = ip;
		targets[i].ok = ok;
		targets[i].checked = true;
		if (ok) {
			targets[i].lastLatencyMs = latency;
			targets[i].hasLatency = true;
		}
	}
	xSemaphoreGive(mutex_);
}

bool PingManager::update(const SettingsManager &settings) {
	uint32_t now = millis();
	if (now - lastCheckMs < kCheckIntervalMs && lastCheckMs != 0) {
		return false;
	}
	lastCheckMs = now;

	pingGoogle();
	pingNextTarget(settings);
	return true;
}

bool PingManager::hasGoogleReading() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	bool v = recentCount > 0;
	xSemaphoreGive(mutex_);
	return v;
}

float PingManager::googleAverageLatencyMs() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	float sum = 0.0f;
	for (size_t i = 0; i < recentCount; i++) {
		sum += recentLatencies[i];
	}
	size_t count = recentCount;
	xSemaphoreGive(mutex_);
	return count == 0 ? 0.0f : sum / static_cast<float>(count);
}

bool PingManager::isFailingOver1Min() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	bool v = failureStreakStartMs != 0 && (millis() - failureStreakStartMs) > kFailureAlertMs;
	xSemaphoreGive(mutex_);
	return v;
}

size_t PingManager::targetCount() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	size_t v = numTargets;
	xSemaphoreGive(mutex_);
	return v;
}

String PingManager::targetIp(size_t i) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	String v = (i < numTargets) ? targets[i].ip : String();
	xSemaphoreGive(mutex_);
	return v;
}

bool PingManager::targetOk(size_t i) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	bool v = (i < numTargets) ? targets[i].ok : false;
	xSemaphoreGive(mutex_);
	return v;
}

bool PingManager::targetChecked(size_t i) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	bool v = (i < numTargets) ? targets[i].checked : false;
	xSemaphoreGive(mutex_);
	return v;
}

float PingManager::targetLatencyMs(size_t i) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	float v = (i < numTargets) ? targets[i].lastLatencyMs : 0.0f;
	xSemaphoreGive(mutex_);
	return v;
}

bool PingManager::targetHasLatency(size_t i) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	bool v = (i < numTargets) ? targets[i].hasLatency : false;
	xSemaphoreGive(mutex_);
	return v;
}
