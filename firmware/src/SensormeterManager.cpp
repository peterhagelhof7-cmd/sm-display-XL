#include "SensormeterManager.h"

namespace {
constexpr const char *kOidSystemName = "1.3.6.1.4.1.99999.1.1.0";
constexpr const char *kOidSystemType = "1.3.6.1.4.1.99999.1.3.0";
constexpr const char *kOidSensor1Name = "1.3.6.1.4.1.99999.3.1.0";
constexpr const char *kOidSensor1Temp = "1.3.6.1.4.1.99999.3.2.0";
constexpr const char *kOidSensor1Hum = "1.3.6.1.4.1.99999.3.3.0";
constexpr const char *kOidSensor2Name = "1.3.6.1.4.1.99999.4.1.0";
constexpr const char *kOidSensor2Temp = "1.3.6.1.4.1.99999.4.2.0";
constexpr const char *kOidSensor2Hum = "1.3.6.1.4.1.99999.4.3.0";
// TimeTicks (1/100s), siehe SnmpClient::getTimeTicks() - Gegenstelle
// (sensormeter/-poe, SNMPManager.cpp) liefert Systemstatus, .5.1 = Uptime.
constexpr const char *kOidUptime = "1.3.6.1.4.1.99999.5.1.0";
} // namespace

void SensormeterManager::begin() {
	mutex_ = xSemaphoreCreateMutex();
}

void SensormeterManager::syncTargets(const SettingsManager &settings) {
	size_t newCount = settings.sensormeterTargetCount();
	if (newCount > kMaxTargets) newCount = kMaxTargets;

	// IPs vorab lesen (settings haelt dabei nur ihre eigene, separate
	// Sperre kurz) - vermeidet verschachtelte Sperren.
	String ips[kMaxTargets];
	for (size_t i = 0; i < newCount; i++) {
		ips[i] = settings.sensormeterTargetIp(i);
	}

	xSemaphoreTake(mutex_, portMAX_DELAY);
	for (size_t i = 0; i < newCount; i++) {
		if (targets_[i].ip != ips[i]) {
			targets_[i] = Target();
			targets_[i].ip = ips[i];
		}
	}
	for (size_t i = newCount; i < kMaxTargets; i++) {
		if (!targets_[i].ip.isEmpty()) {
			targets_[i] = Target();
		}
	}
	count_ = newCount;
	if (nextIndex_ >= count_) nextIndex_ = 0;
	xSemaphoreGive(mutex_);
}

void SensormeterManager::resolveIdentity(size_t idx, const String &ip, const String &community) {
	// Einmalig pro Ziel (nicht auf einzelne Ticks aufgeteilt): passiert nur
	// beim erstmaligen Hinzufuegen/Aendern eines Ziels, nicht im laufenden
	// Betrieb - ein etwas laengerer Blockierzeitraum bei einem gerade neu
	// eingetragenen, noch nicht erreichbaren Ziel ist dafuer akzeptabel
	// (Nutzer sitzt ohnehin gerade in den Einstellungen). Alle SNMP-
	// Rundreisen (blockierend, mehrere Sekunden bei Nichterreichbarkeit)
	// laufen bewusst AUSSERHALB der Sperre - nur lokale Variablen werden
	// hier beschrieben, siehe Klassenkommentar.
	String name, type;
	bool okName = snmp_.getString(ip, community, kOidSystemName, name);
	bool okType = snmp_.getString(ip, community, kOidSystemType, type);
	if (!okName || !okType) {
		return; // naechster Round-Robin-Durchlauf versucht es erneut
	}
	bool isPro = (type == "Sensormeter PRO");

	String s1;
	if (!snmp_.getString(ip, community, kOidSensor1Name, s1)) {
		return;
	}

	String s2;
	if (isPro && !snmp_.getString(ip, community, kOidSensor2Name, s2)) {
		return;
	}

	xSemaphoreTake(mutex_, portMAX_DELAY);
	// idx koennte waehrend der (mehrere Sekunden dauernden) SNMP-Rundreise
	// durch eine zwischenzeitliche Aenderung der Zielliste (syncTargets())
	// ein anderes Ziel bekommen haben - vor dem Uebernehmen die IP erneut
	// pruefen, sonst wuerde das Ergebnis dem falschen (neuen) Ziel
	// zugeschrieben.
	if (idx < kMaxTargets && targets_[idx].ip == ip) {
		targets_[idx].systemName = name;
		targets_[idx].isPro = isPro;
		targets_[idx].sensorName[0] = s1;
		if (isPro) targets_[idx].sensorName[1] = s2;
		targets_[idx].identityResolved = true;
	}
	xSemaphoreGive(mutex_);
}

void SensormeterManager::refreshReadings(size_t idx, const String &ip, bool isPro, const String &community) {
	int32_t temp10 = 0, hum10 = 0;
	bool okTemp = snmp_.getInteger(ip, community, kOidSensor1Temp, temp10);
	bool okHum = okTemp && snmp_.getInteger(ip, community, kOidSensor1Hum, hum10);

	int32_t temp2_10 = 0, hum2_10 = 0;
	bool okTemp2 = false, okHum2 = false;
	if (isPro) {
		okTemp2 = snmp_.getInteger(ip, community, kOidSensor2Temp, temp2_10);
		okHum2 = okTemp2 && snmp_.getInteger(ip, community, kOidSensor2Hum, hum2_10);
	}

	// Fuer die Uebersichtsseite (Name + Uptime) - hier statt in
	// resolveIdentity() abgefragt, weil sie sich laufend aendert, nicht nur
	// einmalig beim erstmaligen Aufloesen des Ziels.
	uint32_t uptimeTicks = 0;
	bool okUptime = snmp_.getTimeTicks(ip, community, kOidUptime, uptimeTicks);

	xSemaphoreTake(mutex_, portMAX_DELAY);
	if (idx < kMaxTargets && targets_[idx].ip == ip) {
		if (okTemp && okHum) {
			targets_[idx].tempC[0] = temp10 / 10.0f;
			targets_[idx].humidityPct[0] = hum10 / 10.0f;
			targets_[idx].valid[0] = true;
		}
		if (isPro && okTemp2 && okHum2) {
			targets_[idx].tempC[1] = temp2_10 / 10.0f;
			targets_[idx].humidityPct[1] = hum2_10 / 10.0f;
			targets_[idx].valid[1] = true;
		}
		if (okUptime) {
			targets_[idx].uptimeSeconds = uptimeTicks / 100;
			targets_[idx].uptimeValid = true;
		}
	}
	xSemaphoreGive(mutex_);
}

bool SensormeterManager::update(const SettingsManager &settings) {
	syncTargets(settings);

	xSemaphoreTake(mutex_, portMAX_DELAY);
	size_t count = count_;
	xSemaphoreGive(mutex_);
	if (count == 0) {
		return false;
	}

	uint32_t now = millis();
	if (lastCheckMs_ != 0 && (now - lastCheckMs_) < kCheckIntervalMs) {
		return false;
	}
	lastCheckMs_ = now;

	String community = settings.sensormeterCommunity();

	xSemaphoreTake(mutex_, portMAX_DELAY);
	size_t idx = nextIndex_;
	String ip = targets_[idx].ip;
	bool alreadyResolved = targets_[idx].identityResolved;
	bool isPro = targets_[idx].isPro;
	nextIndex_ = (nextIndex_ + 1) % count_;
	xSemaphoreGive(mutex_);

	if (!alreadyResolved) {
		resolveIdentity(idx, ip, community);
	} else {
		refreshReadings(idx, ip, isPro, community);
	}
	return true;
}

size_t SensormeterManager::targetCount() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	size_t v = count_;
	xSemaphoreGive(mutex_);
	return v;
}

bool SensormeterManager::isResolved(size_t i) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	bool v = (i < count_) && targets_[i].identityResolved;
	xSemaphoreGive(mutex_);
	return v;
}

bool SensormeterManager::isPro(size_t i) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	bool v = (i < count_) && targets_[i].isPro;
	xSemaphoreGive(mutex_);
	return v;
}

String SensormeterManager::systemName(size_t i) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	String v = (i < count_) ? targets_[i].systemName : String();
	xSemaphoreGive(mutex_);
	return v;
}

String SensormeterManager::sensorName(size_t i, uint8_t sensorIndex) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	String v = (i < count_ && sensorIndex < 2) ? targets_[i].sensorName[sensorIndex] : String();
	xSemaphoreGive(mutex_);
	return v;
}

bool SensormeterManager::sensorValid(size_t i, uint8_t sensorIndex) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	bool v = (i < count_ && sensorIndex < 2) && targets_[i].valid[sensorIndex];
	xSemaphoreGive(mutex_);
	return v;
}

float SensormeterManager::sensorTempC(size_t i, uint8_t sensorIndex) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	float v = (i < count_ && sensorIndex < 2) ? targets_[i].tempC[sensorIndex] : 0.0f;
	xSemaphoreGive(mutex_);
	return v;
}

float SensormeterManager::sensorHumidityPct(size_t i, uint8_t sensorIndex) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	float v = (i < count_ && sensorIndex < 2) ? targets_[i].humidityPct[sensorIndex] : 0.0f;
	xSemaphoreGive(mutex_);
	return v;
}

bool SensormeterManager::uptimeValid(size_t i) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	bool v = (i < count_) && targets_[i].uptimeValid;
	xSemaphoreGive(mutex_);
	return v;
}

uint32_t SensormeterManager::uptimeSeconds(size_t i) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	uint32_t v = (i < count_) ? targets_[i].uptimeSeconds : 0;
	xSemaphoreGive(mutex_);
	return v;
}
