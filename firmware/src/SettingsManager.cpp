#include "SettingsManager.h"

void SettingsManager::begin() {
	mutex_ = xSemaphoreCreateMutex();
	load(); // unlocked: laeuft in begin(), bevor ein zweiter Task existieren kann
}

void SettingsManager::load() {
	prefs.begin("settings", true);
	mode_ = static_cast<OperatingMode>(prefs.getUChar("mode", static_cast<uint8_t>(OperatingMode::Slide)));
	slideIntervalSec_ = prefs.getUShort("slideSec", kSlideIntervalDefaultSec);
	staticSource_ = static_cast<DataSource>(prefs.getUChar("staticSrc", static_cast<uint8_t>(DataSource::Dht11)));
	brightnessPercent_ = prefs.getUChar("brightness", kBrightnessDefaultPercent);
	sensormeterCommunity_ = prefs.getString("smCommunity", "public");
	// Migration vom frueheren Einzelziel-Schema ("smIp"): falls vorhanden und
	// noch keine Ziel-Liste existiert, als erstes Listen-Element uebernehmen.
	String legacyIp = prefs.getString("smIp", "");
	sensormeterTargetCount_ = prefs.getUChar("smCount", legacyIp.isEmpty() ? 0 : 1);
	if (sensormeterTargetCount_ > kMaxSensormeterTargets) sensormeterTargetCount_ = kMaxSensormeterTargets;
	if (!prefs.isKey("smCount") && !legacyIp.isEmpty()) {
		sensormeterTargets_[0] = legacyIp;
	} else {
		for (size_t i = 0; i < sensormeterTargetCount_; i++) {
			char key[8];
			snprintf(key, sizeof(key), "sm%u", static_cast<unsigned>(i));
			sensormeterTargets_[i] = prefs.getString(key, "");
		}
	}
	pingTargetCount_ = prefs.getUChar("pingCount", 0);
	if (pingTargetCount_ > kMaxPingTargets) pingTargetCount_ = kMaxPingTargets;
	for (size_t i = 0; i < pingTargetCount_; i++) {
		char key[8];
		snprintf(key, sizeof(key), "ping%u", static_cast<unsigned>(i));
		pingTargets_[i] = prefs.getString(key, "");
	}
	deviceName_ = prefs.getString("name", "Sensormeter Display");
	webPassword_ = prefs.getString("webPw", "admin");
	brandingVendorName_ = prefs.getString("brandName", "");

	dhtTempOffsetC_ = prefs.getShort("dTOff", 0);
	dhtHumOffsetPct_ = prefs.getShort("dHOff", 0);
	dhtOffsetSetTs_ = prefs.getUInt("dTOffTs", 0);

	dhtTempMinC_ = prefs.getShort("dTMin", kThresholdDisabled);
	dhtTempMaxC_ = prefs.getShort("dTMax", kThresholdDisabled);
	dhtHumMinPct_ = prefs.getShort("dHMin", kThresholdDisabled);
	dhtHumMaxPct_ = prefs.getShort("dHMax", kThresholdDisabled);

	// Alle kMax-Slots laden, nicht nur die aktuell belegten - so behalten
	// auch (noch) unbenutzte Slots ihren zuletzt gespeicherten Zustand
	// (Default: kThresholdDisabled/0), falls spaeter per addXTarget() ein
	// neues Ziel genau in diesem Slot-Index landet.
	for (size_t i = 0; i < kMaxSensormeterTargets; i++) {
		for (uint8_t s = 0; s < kMaxSensorsPerTarget; s++) {
			char key[8];
			snprintf(key, sizeof(key), "smTn%u%u", static_cast<unsigned>(i), static_cast<unsigned>(s));
			smTempMin_[i][s] = prefs.getShort(key, kThresholdDisabled);
			snprintf(key, sizeof(key), "smTx%u%u", static_cast<unsigned>(i), static_cast<unsigned>(s));
			smTempMax_[i][s] = prefs.getShort(key, kThresholdDisabled);
			snprintf(key, sizeof(key), "smHn%u%u", static_cast<unsigned>(i), static_cast<unsigned>(s));
			smHumMin_[i][s] = prefs.getShort(key, kThresholdDisabled);
			snprintf(key, sizeof(key), "smHx%u%u", static_cast<unsigned>(i), static_cast<unsigned>(s));
			smHumMax_[i][s] = prefs.getShort(key, kThresholdDisabled);
		}
	}
	for (size_t i = 0; i < kMaxPingTargets; i++) {
		char key[8];
		snprintf(key, sizeof(key), "pMax%u", static_cast<unsigned>(i));
		pingMaxMs_[i] = prefs.getUShort(key, 0);
	}
	googlePingMaxMs_ = prefs.getUShort("pMaxG", 0);
	prefs.end();

	if (slideIntervalSec_ < kSlideIntervalMinSec || slideIntervalSec_ > kSlideIntervalMaxSec) {
		slideIntervalSec_ = kSlideIntervalDefaultSec;
	}
	if (brightnessPercent_ > 100) {
		brightnessPercent_ = kBrightnessDefaultPercent;
	}
}

// save()/savePingTargets(): nur NVS-Schreiben, KEINE eigene Sperre - werden
// von den Settern aufgerufen, waehrend der Mutex bereits gehalten wird
// (xSemaphoreCreateMutex() ist nicht rekursiv, ein zweites Take vom selben
// Task wuerde blockieren).
void SettingsManager::save() {
	prefs.begin("settings", false);
	prefs.putUChar("mode", static_cast<uint8_t>(mode_));
	prefs.putUShort("slideSec", slideIntervalSec_);
	prefs.putUChar("staticSrc", static_cast<uint8_t>(staticSource_));
	prefs.putUChar("brightness", brightnessPercent_);
	prefs.putString("smCommunity", sensormeterCommunity_);
	prefs.putString("name", deviceName_);
	prefs.putString("webPw", webPassword_);
	prefs.putString("brandName", brandingVendorName_);
	prefs.putShort("dTOff", dhtTempOffsetC_);
	prefs.putShort("dHOff", dhtHumOffsetPct_);
	prefs.putUInt("dTOffTs", dhtOffsetSetTs_);
	prefs.end();
}

void SettingsManager::saveSensormeterTargets() {
	prefs.begin("settings", false);
	prefs.putUChar("smCount", static_cast<uint8_t>(sensormeterTargetCount_));
	for (size_t i = 0; i < sensormeterTargetCount_; i++) {
		char key[8];
		snprintf(key, sizeof(key), "sm%u", static_cast<unsigned>(i));
		prefs.putString(key, sensormeterTargets_[i]);
	}
	prefs.end();
}

void SettingsManager::savePingTargets() {
	prefs.begin("settings", false);
	prefs.putUChar("pingCount", static_cast<uint8_t>(pingTargetCount_));
	for (size_t i = 0; i < pingTargetCount_; i++) {
		char key[8];
		snprintf(key, sizeof(key), "ping%u", static_cast<unsigned>(i));
		prefs.putString(key, pingTargets_[i]);
	}
	prefs.end();
}

void SettingsManager::saveDhtThresholds() {
	prefs.begin("settings", false);
	prefs.putShort("dTMin", dhtTempMinC_);
	prefs.putShort("dTMax", dhtTempMaxC_);
	prefs.putShort("dHMin", dhtHumMinPct_);
	prefs.putShort("dHMax", dhtHumMaxPct_);
	prefs.end();
}

void SettingsManager::saveSensormeterThresholds() {
	prefs.begin("settings", false);
	for (size_t i = 0; i < kMaxSensormeterTargets; i++) {
		for (uint8_t s = 0; s < kMaxSensorsPerTarget; s++) {
			char key[8];
			snprintf(key, sizeof(key), "smTn%u%u", static_cast<unsigned>(i), static_cast<unsigned>(s));
			prefs.putShort(key, smTempMin_[i][s]);
			snprintf(key, sizeof(key), "smTx%u%u", static_cast<unsigned>(i), static_cast<unsigned>(s));
			prefs.putShort(key, smTempMax_[i][s]);
			snprintf(key, sizeof(key), "smHn%u%u", static_cast<unsigned>(i), static_cast<unsigned>(s));
			prefs.putShort(key, smHumMin_[i][s]);
			snprintf(key, sizeof(key), "smHx%u%u", static_cast<unsigned>(i), static_cast<unsigned>(s));
			prefs.putShort(key, smHumMax_[i][s]);
		}
	}
	prefs.end();
}

void SettingsManager::savePingThresholds() {
	prefs.begin("settings", false);
	for (size_t i = 0; i < kMaxPingTargets; i++) {
		char key[8];
		snprintf(key, sizeof(key), "pMax%u", static_cast<unsigned>(i));
		prefs.putUShort(key, pingMaxMs_[i]);
	}
	prefs.putUShort("pMaxG", googlePingMaxMs_);
	prefs.end();
}

OperatingMode SettingsManager::mode() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	OperatingMode v = mode_;
	xSemaphoreGive(mutex_);
	return v;
}

uint16_t SettingsManager::slideIntervalSec() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	uint16_t v = slideIntervalSec_;
	xSemaphoreGive(mutex_);
	return v;
}

DataSource SettingsManager::staticSource() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	DataSource v = staticSource_;
	xSemaphoreGive(mutex_);
	return v;
}

uint8_t SettingsManager::brightnessPercent() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	uint8_t v = brightnessPercent_;
	xSemaphoreGive(mutex_);
	return v;
}

void SettingsManager::setSlideMode(uint16_t intervalSec) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	mode_ = OperatingMode::Slide;
	if (intervalSec < kSlideIntervalMinSec) intervalSec = kSlideIntervalMinSec;
	if (intervalSec > kSlideIntervalMaxSec) intervalSec = kSlideIntervalMaxSec;
	slideIntervalSec_ = intervalSec;
	save();
	xSemaphoreGive(mutex_);
}

void SettingsManager::setStaticMode(DataSource source) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	mode_ = OperatingMode::Static;
	staticSource_ = source;
	save();
	xSemaphoreGive(mutex_);
}

void SettingsManager::setBrightnessPercent(uint8_t percent) {
	if (percent > 100) percent = 100;
	xSemaphoreTake(mutex_, portMAX_DELAY);
	brightnessPercent_ = percent;
	save();
	xSemaphoreGive(mutex_);
}

int16_t SettingsManager::dhtTempOffsetC() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	int16_t v = dhtTempOffsetC_;
	xSemaphoreGive(mutex_);
	return v;
}

int16_t SettingsManager::dhtHumOffsetPct() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	int16_t v = dhtHumOffsetPct_;
	xSemaphoreGive(mutex_);
	return v;
}

time_t SettingsManager::dhtOffsetSetTime() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	uint32_t v = dhtOffsetSetTs_;
	xSemaphoreGive(mutex_);
	return static_cast<time_t>(v);
}

void SettingsManager::setDhtOffsets(int16_t tempOffsetC, int16_t humOffsetPct) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	// Zeitstempel nur bei TATSAECHLICHER Aenderung aktualisieren - setDhtOffsets()
	// wird bei jedem Speichern der Einstellungsseite aufgerufen (Kalibrierung
	// und Warnschwellwerte teilen sich ein Formular), nicht nur wenn die
	// Kalibrierung selbst geaendert wurde. Ohne diese Pruefung wuerde
	// "Zuletzt kalibriert" bei jedem beliebigen Speichern auf "jetzt"
	// springen, auch wenn sich die Offsets gar nicht geaendert haben.
	if (tempOffsetC != dhtTempOffsetC_ || humOffsetPct != dhtHumOffsetPct_) {
		dhtOffsetSetTs_ = static_cast<uint32_t>(time(nullptr));
	}
	dhtTempOffsetC_ = tempOffsetC;
	dhtHumOffsetPct_ = humOffsetPct;
	save();
	xSemaphoreGive(mutex_);
}

String SettingsManager::sensormeterCommunity() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	String v = sensormeterCommunity_;
	xSemaphoreGive(mutex_);
	return v;
}

void SettingsManager::setSensormeterCommunity(const String &community) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	sensormeterCommunity_ = community.isEmpty() ? "public" : community;
	save();
	xSemaphoreGive(mutex_);
}

size_t SettingsManager::sensormeterTargetCount() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	size_t v = sensormeterTargetCount_;
	xSemaphoreGive(mutex_);
	return v;
}

String SettingsManager::sensormeterTargetIp(size_t i) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	String v = (i < sensormeterTargetCount_) ? sensormeterTargets_[i] : String();
	xSemaphoreGive(mutex_);
	return v;
}

bool SettingsManager::addSensormeterTarget(const String &ip) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	bool ok = false;
	if (sensormeterTargetCount_ < kMaxSensormeterTargets) {
		sensormeterTargets_[sensormeterTargetCount_++] = ip;
		saveSensormeterTargets();
		ok = true;
	}
	xSemaphoreGive(mutex_);
	return ok;
}

void SettingsManager::removeSensormeterTarget(size_t i) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	// Mindestens ein Ziel bleibt erhalten, sobald eins konfiguriert wurde
	// (Nutzervorgabe) - zentral hier statt nur in der UI durchgesetzt, damit
	// Touch- und Web-Oberflaeche nicht getrennt darauf achten muessen.
	if (i < sensormeterTargetCount_ && sensormeterTargetCount_ > 1) {
		for (size_t j = i; j + 1 < sensormeterTargetCount_; j++) {
			sensormeterTargets_[j] = sensormeterTargets_[j + 1];
			for (uint8_t s = 0; s < kMaxSensorsPerTarget; s++) {
				smTempMin_[j][s] = smTempMin_[j + 1][s];
				smTempMax_[j][s] = smTempMax_[j + 1][s];
				smHumMin_[j][s] = smHumMin_[j + 1][s];
				smHumMax_[j][s] = smHumMax_[j + 1][s];
			}
		}
		sensormeterTargetCount_--;
		// Der jetzt freigewordene letzte Slot muss auf "kein Schwellwert"
		// zurueckgesetzt werden - sonst wuerde ein spaeter dort neu
		// hinzugefuegtes Ziel faelschlich die Schwellwerte des entfernten
		// Ziels erben (Slot-Index wird bei addSensormeterTarget() wiederverwendet).
		for (uint8_t s = 0; s < kMaxSensorsPerTarget; s++) {
			smTempMin_[sensormeterTargetCount_][s] = kThresholdDisabled;
			smTempMax_[sensormeterTargetCount_][s] = kThresholdDisabled;
			smHumMin_[sensormeterTargetCount_][s] = kThresholdDisabled;
			smHumMax_[sensormeterTargetCount_][s] = kThresholdDisabled;
		}
		saveSensormeterTargets();
		saveSensormeterThresholds();
	}
	xSemaphoreGive(mutex_);
}

void SettingsManager::setSensormeterTargetIp(size_t i, const String &ip) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	if (i < sensormeterTargetCount_ && !ip.isEmpty()) {
		sensormeterTargets_[i] = ip;
		saveSensormeterTargets();
	}
	xSemaphoreGive(mutex_);
}

size_t SettingsManager::pingTargetCount() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	size_t v = pingTargetCount_;
	xSemaphoreGive(mutex_);
	return v;
}

String SettingsManager::pingTargetIp(size_t i) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	String v = (i < pingTargetCount_) ? pingTargets_[i] : String();
	xSemaphoreGive(mutex_);
	return v;
}

bool SettingsManager::addPingTarget(const String &ip) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	bool ok = false;
	if (pingTargetCount_ < kMaxPingTargets) {
		pingTargets_[pingTargetCount_++] = ip;
		savePingTargets();
		ok = true;
	}
	xSemaphoreGive(mutex_);
	return ok;
}

void SettingsManager::removePingTarget(size_t i) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	if (i < pingTargetCount_) {
		for (size_t j = i; j + 1 < pingTargetCount_; j++) {
			pingTargets_[j] = pingTargets_[j + 1];
			pingMaxMs_[j] = pingMaxMs_[j + 1];
		}
		pingTargetCount_--;
		// Siehe Kommentar in removeSensormeterTarget() - freigewordenen Slot
		// zuruecksetzen, damit ein spaeter wiederverwendeter Slot nicht den
		// Schwellwert des entfernten Ziels erbt.
		pingMaxMs_[pingTargetCount_] = 0;
		savePingTargets();
		savePingThresholds();
	}
	xSemaphoreGive(mutex_);
}

String SettingsManager::deviceName() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	String v = deviceName_;
	xSemaphoreGive(mutex_);
	return v;
}

String SettingsManager::webPassword() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	String v = webPassword_;
	xSemaphoreGive(mutex_);
	return v;
}

void SettingsManager::setDeviceName(const String &name) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	deviceName_ = name.isEmpty() ? "Sensormeter Display" : name;
	save();
	xSemaphoreGive(mutex_);
}

void SettingsManager::setWebPassword(const String &password) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	webPassword_ = password.isEmpty() ? "admin" : password;
	save();
	xSemaphoreGive(mutex_);
}

String SettingsManager::brandingVendorName() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	String v = brandingVendorName_;
	xSemaphoreGive(mutex_);
	return v;
}

void SettingsManager::setBrandingVendorName(const String &name) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	brandingVendorName_ = name;
	save();
	xSemaphoreGive(mutex_);
}

void SettingsManager::resetConfig(bool keepBrandingName) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	String keptBrandName = keepBrandingName ? brandingVendorName_ : String();

	prefs.begin("settings", false);
	prefs.clear();
	prefs.end();

	// In-Memory-Zustand ebenfalls auf die eingebauten Defaults zuruecksetzen
	// (nicht nur NVS) - fuer den Fall, dass der Aufrufer nicht sofort neu
	// startet.
	mode_ = OperatingMode::Slide;
	slideIntervalSec_ = kSlideIntervalDefaultSec;
	staticSource_ = DataSource::Dht11;
	brightnessPercent_ = kBrightnessDefaultPercent;
	sensormeterCommunity_ = "public";
	for (size_t i = 0; i < kMaxSensormeterTargets; i++) sensormeterTargets_[i] = String();
	sensormeterTargetCount_ = 0;
	for (size_t i = 0; i < kMaxPingTargets; i++) pingTargets_[i] = String();
	pingTargetCount_ = 0;
	deviceName_ = "Sensormeter Display";
	webPassword_ = "admin";
	brandingVendorName_ = keptBrandName;
	dhtTempOffsetC_ = 0;
	dhtHumOffsetPct_ = 0;
	dhtOffsetSetTs_ = 0;
	dhtTempMinC_ = kThresholdDisabled;
	dhtTempMaxC_ = kThresholdDisabled;
	dhtHumMinPct_ = kThresholdDisabled;
	dhtHumMaxPct_ = kThresholdDisabled;
	for (size_t i = 0; i < kMaxSensormeterTargets; i++) {
		for (uint8_t s = 0; s < kMaxSensorsPerTarget; s++) {
			smTempMin_[i][s] = kThresholdDisabled;
			smTempMax_[i][s] = kThresholdDisabled;
			smHumMin_[i][s] = kThresholdDisabled;
			smHumMax_[i][s] = kThresholdDisabled;
		}
	}
	for (size_t i = 0; i < kMaxPingTargets; i++) pingMaxMs_[i] = 0;
	googlePingMaxMs_ = 0;

	save();
	xSemaphoreGive(mutex_);
}

int16_t SettingsManager::dhtTempMinC() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	int16_t v = dhtTempMinC_;
	xSemaphoreGive(mutex_);
	return v;
}

int16_t SettingsManager::dhtTempMaxC() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	int16_t v = dhtTempMaxC_;
	xSemaphoreGive(mutex_);
	return v;
}

int16_t SettingsManager::dhtHumMinPct() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	int16_t v = dhtHumMinPct_;
	xSemaphoreGive(mutex_);
	return v;
}

int16_t SettingsManager::dhtHumMaxPct() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	int16_t v = dhtHumMaxPct_;
	xSemaphoreGive(mutex_);
	return v;
}

void SettingsManager::setDhtThresholds(int16_t tempMinC, int16_t tempMaxC, int16_t humMinPct, int16_t humMaxPct) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	dhtTempMinC_ = tempMinC;
	dhtTempMaxC_ = tempMaxC;
	dhtHumMinPct_ = humMinPct;
	dhtHumMaxPct_ = humMaxPct;
	saveDhtThresholds();
	xSemaphoreGive(mutex_);
}

int16_t SettingsManager::sensormeterTempMinC(size_t targetIdx, uint8_t sensorIdx) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	int16_t v = (targetIdx < kMaxSensormeterTargets && sensorIdx < kMaxSensorsPerTarget)
	                ? smTempMin_[targetIdx][sensorIdx]
	                : kThresholdDisabled;
	xSemaphoreGive(mutex_);
	return v;
}

int16_t SettingsManager::sensormeterTempMaxC(size_t targetIdx, uint8_t sensorIdx) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	int16_t v = (targetIdx < kMaxSensormeterTargets && sensorIdx < kMaxSensorsPerTarget)
	                ? smTempMax_[targetIdx][sensorIdx]
	                : kThresholdDisabled;
	xSemaphoreGive(mutex_);
	return v;
}

int16_t SettingsManager::sensormeterHumMinPct(size_t targetIdx, uint8_t sensorIdx) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	int16_t v = (targetIdx < kMaxSensormeterTargets && sensorIdx < kMaxSensorsPerTarget)
	                ? smHumMin_[targetIdx][sensorIdx]
	                : kThresholdDisabled;
	xSemaphoreGive(mutex_);
	return v;
}

int16_t SettingsManager::sensormeterHumMaxPct(size_t targetIdx, uint8_t sensorIdx) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	int16_t v = (targetIdx < kMaxSensormeterTargets && sensorIdx < kMaxSensorsPerTarget)
	                ? smHumMax_[targetIdx][sensorIdx]
	                : kThresholdDisabled;
	xSemaphoreGive(mutex_);
	return v;
}

void SettingsManager::setSensormeterThresholds(size_t targetIdx, uint8_t sensorIdx, int16_t tempMinC,
                                                int16_t tempMaxC, int16_t humMinPct, int16_t humMaxPct) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	if (targetIdx < kMaxSensormeterTargets && sensorIdx < kMaxSensorsPerTarget) {
		smTempMin_[targetIdx][sensorIdx] = tempMinC;
		smTempMax_[targetIdx][sensorIdx] = tempMaxC;
		smHumMin_[targetIdx][sensorIdx] = humMinPct;
		smHumMax_[targetIdx][sensorIdx] = humMaxPct;
		saveSensormeterThresholds();
	}
	xSemaphoreGive(mutex_);
}

uint16_t SettingsManager::pingMaxLatencyMs(size_t i) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	uint16_t v = (i < kMaxPingTargets) ? pingMaxMs_[i] : 0;
	xSemaphoreGive(mutex_);
	return v;
}

void SettingsManager::setPingMaxLatencyMs(size_t i, uint16_t ms) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	if (i < kMaxPingTargets) {
		pingMaxMs_[i] = ms;
		savePingThresholds();
	}
	xSemaphoreGive(mutex_);
}

uint16_t SettingsManager::googlePingMaxLatencyMs() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	uint16_t v = googlePingMaxMs_;
	xSemaphoreGive(mutex_);
	return v;
}

void SettingsManager::setGooglePingMaxLatencyMs(uint16_t ms) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	googlePingMaxMs_ = ms;
	savePingThresholds();
	xSemaphoreGive(mutex_);
}
