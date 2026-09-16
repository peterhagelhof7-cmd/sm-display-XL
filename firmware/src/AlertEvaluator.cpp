#include "AlertEvaluator.h"

#include <math.h>

namespace {

struct CategoryAlert {
	bool active;
	bool blue;
	String detail;  // konkretes ausloesendes Element, siehe AlertInfo::detail
	// Explizite Konstruktoren statt Default-Member-Initializer - siehe
	// Kommentar bei AlertInfo in AlertEvaluator.h (sonst kein Aggregat
	// mehr, die {true,false}-Rueckgaben unten scheitern am Compiler).
	CategoryAlert() : active(false), blue(false) {}
	CategoryAlert(bool activeIn, bool blueIn, const String &detailIn = String())
	    : active(activeIn), blue(blueIn), detail(detailIn) {}
};

CategoryAlert checkIntern(const SensorManager &sensor, const SettingsManager &settings) {
	if (!sensor.hasValidReading()) return CategoryAlert();
	int t = static_cast<int>(lroundf(sensor.temperatureC()));
	int h = static_cast<int>(lroundf(sensor.humidityPercent()));
	int16_t tMin = settings.dhtTempMinC(), tMax = settings.dhtTempMaxC();
	int16_t hMin = settings.dhtHumMinPct(), hMax = settings.dhtHumMaxPct();
	if (tMax != SettingsManager::kThresholdDisabled && t > tMax)
		return {true, false, "Intern Temp " + String(t) + " C > " + String(tMax) + " C"};
	if (hMax != SettingsManager::kThresholdDisabled && h > hMax)
		return {true, false, "Intern Feuchte " + String(h) + "% > " + String(hMax) + "%"};
	if (tMin != SettingsManager::kThresholdDisabled && t < tMin)
		return {true, true, "Intern Temp " + String(t) + " C < " + String(tMin) + " C"};
	if (hMin != SettingsManager::kThresholdDisabled && h < hMin)
		return {true, true, "Intern Feuchte " + String(h) + "% < " + String(hMin) + "%"};
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
			// Label des ausloesenden Ziels/Sensors fuer das Warn-Protokoll.
			String who = sensormeterManager.systemName(i);
			String sname = sensormeterManager.sensorName(i, s);
			if (!sname.isEmpty()) who += " (" + sname + ")";
			if (tMax != SettingsManager::kThresholdDisabled && t > tMax)
				return {true, false, who + " Temp " + String(t) + " C > " + String(tMax) + " C"};
			if (hMax != SettingsManager::kThresholdDisabled && h > hMax)
				return {true, false, who + " Feuchte " + String(h) + "% > " + String(hMax) + "%"};
			if (tMin != SettingsManager::kThresholdDisabled && t < tMin)
				return {true, true, who + " Temp " + String(t) + " C < " + String(tMin) + " C"};
			if (hMin != SettingsManager::kThresholdDisabled && h < hMin)
				return {true, true, who + " Feuchte " + String(h) + "% < " + String(hMin) + "%"};
		}
	}
	return CategoryAlert();
}

CategoryAlert checkPing(const PingManager &pingManager, const SettingsManager &settings) {
	if (pingManager.isFailingOver1Min()) return {true, false, "google.com (Ausfall > 1 Min.)"};
	uint16_t googleMaxMs = settings.googlePingMaxLatencyMs();
	if (googleMaxMs != 0 && pingManager.hasGoogleReading() &&
	    lroundf(pingManager.googleAverageLatencyMs()) > googleMaxMs) {
		return {true, false,
		        "google.com " + String(lroundf(pingManager.googleAverageLatencyMs())) + " ms > " +
		            String(googleMaxMs) + " ms"};
	}
	for (size_t i = 0; i < pingManager.targetCount(); i++) {
		uint16_t maxMs = settings.pingMaxLatencyMs(i);
		if (maxMs != 0 && pingManager.targetHasLatency(i) && lroundf(pingManager.targetLatencyMs(i)) > maxMs) {
			return {true, false,
			        pingManager.targetIp(i) + " " + String(lroundf(pingManager.targetLatencyMs(i))) +
			            " ms > " + String(maxMs) + " ms"};
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
	if (intern.active) return AlertInfo(true, intern.blue, "Intern", activeCount - 1, intern.detail);
	if (sm.active) return AlertInfo(true, sm.blue, "Sensormeter", activeCount - 1, sm.detail);
	if (ping.active) return AlertInfo(true, ping.blue, "Ping", activeCount - 1, ping.detail);
	return AlertInfo();
}
