#include "AlertEvaluator.h"

#include <math.h>

namespace {

struct CategoryAlert {
	bool active;
	bool blue;
	// Explizite Konstruktoren statt Default-Member-Initializer - siehe
	// Kommentar bei AlertInfo in AlertEvaluator.h (sonst kein Aggregat
	// mehr, die {true,false}-Rueckgaben unten scheitern am Compiler).
	CategoryAlert() : active(false), blue(false) {}
	CategoryAlert(bool activeIn, bool blueIn) : active(activeIn), blue(blueIn) {}
};

CategoryAlert checkIntern(const SensorManager &sensor, const SettingsManager &settings) {
	if (!sensor.hasValidReading()) return CategoryAlert();
	int t = static_cast<int>(lroundf(sensor.temperatureC()));
	int h = static_cast<int>(lroundf(sensor.humidityPercent()));
	int16_t tMin = settings.dhtTempMinC(), tMax = settings.dhtTempMaxC();
	int16_t hMin = settings.dhtHumMinPct(), hMax = settings.dhtHumMaxPct();
	if (tMax != SettingsManager::kThresholdDisabled && t > tMax) return {true, false};
	if (hMax != SettingsManager::kThresholdDisabled && h > hMax) return {true, false};
	if (tMin != SettingsManager::kThresholdDisabled && t < tMin) return {true, true};
	if (hMin != SettingsManager::kThresholdDisabled && h < hMin) return {true, true};
	return CategoryAlert();
}

CategoryAlert checkSensormeter(const SensormeterManager &sensormeterManager, const SettingsManager &settings) {
	for (size_t i = 0; i < sensormeterManager.targetCount(); i++) {
		if (!sensormeterManager.isResolved(i)) continue;
		uint8_t sensorCount = sensormeterManager.isPro(i) ? 2 : 1;
		for (uint8_t s = 0; s < sensorCount; s++) {
			if (!sensormeterManager.sensorValid(i, s)) continue;
			int16_t tMin = settings.sensormeterTempMinC(i, s), tMax = settings.sensormeterTempMaxC(i, s);
			int16_t hMin = settings.sensormeterHumMinPct(i, s), hMax = settings.sensormeterHumMaxPct(i, s);
			int t = static_cast<int>(lroundf(sensormeterManager.sensorTempC(i, s)));
			int h = static_cast<int>(lroundf(sensormeterManager.sensorHumidityPct(i, s)));
			if (tMax != SettingsManager::kThresholdDisabled && t > tMax) return {true, false};
			if (hMax != SettingsManager::kThresholdDisabled && h > hMax) return {true, false};
			if (tMin != SettingsManager::kThresholdDisabled && t < tMin) return {true, true};
			if (hMin != SettingsManager::kThresholdDisabled && h < hMin) return {true, true};
		}
	}
	return CategoryAlert();
}

CategoryAlert checkPing(const PingManager &pingManager, const SettingsManager &settings) {
	if (pingManager.isFailingOver1Min()) return {true, false};
	uint16_t googleMaxMs = settings.googlePingMaxLatencyMs();
	if (googleMaxMs != 0 && pingManager.hasGoogleReading() &&
	    lroundf(pingManager.googleAverageLatencyMs()) > googleMaxMs) {
		return {true, false};
	}
	for (size_t i = 0; i < pingManager.targetCount(); i++) {
		uint16_t maxMs = settings.pingMaxLatencyMs(i);
		if (maxMs != 0 && pingManager.targetHasLatency(i) && lroundf(pingManager.targetLatencyMs(i)) > maxMs) {
			return {true, false};
		}
	}
	return CategoryAlert();
}

} // namespace

AlertInfo computeAlertInfo(const SensorManager &sensor, const SensormeterManager &sensormeterManager,
                            const PingManager &pingManager, const SettingsManager &settings) {
	CategoryAlert intern = checkIntern(sensor, settings);
	CategoryAlert sm = checkSensormeter(sensormeterManager, settings);
	CategoryAlert ping = checkPing(pingManager, settings);

	uint8_t activeCount = static_cast<uint8_t>(intern.active) + static_cast<uint8_t>(sm.active) +
	                       static_cast<uint8_t>(ping.active);
	if (intern.active) return AlertInfo(true, intern.blue, "Intern", activeCount - 1);
	if (sm.active) return AlertInfo(true, sm.blue, "Sensormeter", activeCount - 1);
	if (ping.active) return AlertInfo(true, ping.blue, "Ping", activeCount - 1);
	return AlertInfo();
}
