#include "SensorManager.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void SensorManager::begin(const SettingsManager &settings) {
	settings_ = &settings;
	mutex_ = xSemaphoreCreateMutex();
	dht.begin();
	// Auf Core 0 (PRO_CPU) pinnen: der DHT-Read sperrt kurz die Interrupts,
	// und das darf NICHT den Core treffen, der die RGB-Panel-ISR bedient
	// (Core 1 = Arduino-loop/setup). Prioritaet niedrig (unter WLAN/loop),
	// Stack 3 KB (Adafruit-DHT + kurze Locals). Siehe Klassenkommentar.
	xTaskCreatePinnedToCore(&SensorManager::taskThunk, "dht", 3072, this, 2, nullptr, 0);
}

void SensorManager::taskThunk(void *arg) {
	static_cast<SensorManager *>(arg)->pollLoop();
}

void SensorManager::pollLoop() {
	// Anlaufzeit des Sensors nach Power-on abwarten (wie esp-infoscreen).
	vTaskDelay(pdMS_TO_TICKS(2000));
	for (;;) {
		readOnce();
		vTaskDelay(pdMS_TO_TICKS(kPollIntervalMs));
	}
}

void SensorManager::readOnce() {
	// dht.readTemperature()/readHumidity() sind kurze, aber interrupts-
	// sperrende 1-Wire-Zugriffe - laeuft hier bewusst auf Core 0 (siehe begin()).
	float t = dht.readTemperature();
	float h = dht.readHumidity();

	xSemaphoreTake(mutex_, portMAX_DELAY);
	// Plausibilitaetspruefung auf dem ROHEN Messwert (nicht dem korrigierten) -
	// die Korrektur ist eine kleine Kalibrierkonstante, keine Fehlerkompensation,
	// und soll den Garbage-Filter nicht verfaelschen.
	if (isPlausible(t, h)) {
		lastTempC = t + static_cast<float>(settings_->dhtTempOffsetC());
		lastHumidityPct = h + static_cast<float>(settings_->dhtHumOffsetPct());
		if (lastHumidityPct < 0.0f) lastHumidityPct = 0.0f;
		if (lastHumidityPct > 100.0f) lastHumidityPct = 100.0f;
		lastReadTs = time(nullptr);
		valid = true;
	}
	// bei Implausibilitaet: letzter gueltiger Wert bleibt bestehen.
	newReading_ = true;  // signalisiert dem Hauptloop: es gab eine (neue) Messung
	xSemaphoreGive(mutex_);
}

bool SensorManager::isPlausible(float tempC, float humidityPct) const {
	if (isnan(tempC) || isnan(humidityPct)) {
		return false;
	}
	// DHT11-Messbereich lt. Hersteller: 0-50 degC, 20-90% rH.
	if (tempC < -10.0f || tempC > 60.0f) {
		return false;
	}
	if (humidityPct < 0.0f || humidityPct > 100.0f) {
		return false;
	}
	return true;
}

bool SensorManager::update(const SettingsManager &settings) {
	// Liest NICHT mehr selbst - der Core-0-Task (siehe pollLoop/readOnce)
	// erledigt die Messung. Hier nur das "neue Messung"-Flag konsumieren,
	// damit der Hauptloop weiss, wann sich Neuzeichnen/Aufzeichnen lohnt.
	(void)settings;  // Kalibrierung wendet der Task ueber settings_ an
	xSemaphoreTake(mutex_, portMAX_DELAY);
	bool wasNew = newReading_;
	newReading_ = false;
	xSemaphoreGive(mutex_);
	return wasNew;
}

bool SensorManager::hasValidReading() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	bool v = valid;
	xSemaphoreGive(mutex_);
	return v;
}

float SensorManager::temperatureC() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	float v = lastTempC;
	xSemaphoreGive(mutex_);
	return v;
}

float SensorManager::humidityPercent() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	float v = lastHumidityPct;
	xSemaphoreGive(mutex_);
	return v;
}

time_t SensorManager::lastReadTime() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	time_t v = lastReadTs;
	xSemaphoreGive(mutex_);
	return v;
}
