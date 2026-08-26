#include "BrandingManager.h"

#include <LittleFS.h>

#include "DefaultLogo.h"

namespace {
const char *kLogoPath = "/branding-logo.bin";
const char *kLogoTmpPath = "/branding-logo.bin.tmp";
}  // namespace

void BrandingManager::begin() {
	mutex_ = xSemaphoreCreateMutex();
	if (!LittleFS.begin(true)) {
		Serial.println("[BRANDING] LittleFS-Mount fehlgeschlagen (auch nach Formatierungsversuch)");
		return;
	}
	// Unlocked: laeuft in begin(), bevor ein zweiter Task existieren kann
	// (analog SettingsManager::begin()).
	logoPresent_ = checkLogoOnDisk();
	if (!logoPresent_) {
		provisionDefaultLogo();
	}
}

bool BrandingManager::checkLogoOnDisk() const {
	if (!LittleFS.exists(kLogoPath)) return false;
	fs::File f = LittleFS.open(kLogoPath, "r");
	if (!f) return false;
	size_t size = f.size();
	f.close();
	return size == kLogoBytes;
}

bool BrandingManager::hasLogo() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	bool v = logoPresent_;
	xSemaphoreGive(mutex_);
	return v;
}

bool BrandingManager::loadLogo(uint8_t *buffer, size_t bufferSize) const {
	if (bufferSize < kLogoBytes) return false;

	xSemaphoreTake(mutex_, portMAX_DELAY);
	bool present = logoPresent_;
	xSemaphoreGive(mutex_);
	if (!present) return false;

	fs::File f = LittleFS.open(kLogoPath, "r");
	if (!f) return false;
	if (f.size() != kLogoBytes) {
		f.close();
		return false;
	}
	size_t read = f.read(buffer, kLogoBytes);
	f.close();
	return read == kLogoBytes;
}

bool BrandingManager::beginLogoUpload() {
	uploadFile_ = LittleFS.open(kLogoTmpPath, "w");
	uploadBytesWritten_ = 0;
	uploadOpen_ = static_cast<bool>(uploadFile_);
	if (!uploadOpen_) {
		Serial.println("[BRANDING] Konnte Logo-Tmp-Datei nicht oeffnen");
	}
	return uploadOpen_;
}

bool BrandingManager::writeLogoUploadChunk(const uint8_t *data, size_t len) {
	if (!uploadOpen_) return false;
	// Sicherheitsabbruch bei deutlich zu grosser Datei, damit ein
	// missverstandener Upload nicht unbegrenzt LittleFS-Platz verbraucht -
	// die eigentliche Groessenpruefung (exakt kLogoBytes) passiert erst am
	// Ende in endLogoUpload(), da die Gesamtlaenge dem Handler vorab nicht
	// zuverlaessig bekannt ist.
	if (uploadBytesWritten_ + len > kLogoBytes * 4) {
		Serial.println("[BRANDING] Logo-Upload abgebrochen (deutlich zu gross)");
		uploadFile_.close();
		LittleFS.remove(kLogoTmpPath);
		uploadOpen_ = false;
		return false;
	}
	size_t written = uploadFile_.write(data, len);
	uploadBytesWritten_ += written;
	return written == len;
}

bool BrandingManager::endLogoUpload() {
	if (!uploadOpen_) return false;
	uploadFile_.close();
	uploadOpen_ = false;

	if (uploadBytesWritten_ != kLogoBytes) {
		Serial.printf("[BRANDING] Logo-Upload verworfen: %u Byte empfangen, erwartet %u (128x64, RGB565)\n",
		              (unsigned)uploadBytesWritten_, (unsigned)kLogoBytes);
		LittleFS.remove(kLogoTmpPath);
		return false;
	}

	LittleFS.remove(kLogoPath);
	if (!LittleFS.rename(kLogoTmpPath, kLogoPath)) {
		Serial.println("[BRANDING] Konnte Logo-Tmp-Datei nicht umbenennen");
		return false;
	}
	Serial.println("[BRANDING] Logo gespeichert");

	xSemaphoreTake(mutex_, portMAX_DELAY);
	logoPresent_ = true;
	xSemaphoreGive(mutex_);
	return true;
}

void BrandingManager::provisionDefaultLogo() {
	if (kDefaultLogoBytes != kLogoBytes) {
		Serial.println("[BRANDING] Standard-Logo-Groesse passt nicht zum Display, uebersprungen");
		return;
	}
	fs::File f = LittleFS.open(kLogoTmpPath, "w");
	if (!f) {
		Serial.println("[BRANDING] Konnte Standard-Logo nicht schreiben (Tmp-Datei)");
		return;
	}
	size_t written = f.write(kDefaultLogo, kDefaultLogoBytes);
	f.close();
	if (written != kLogoBytes) {
		Serial.println("[BRANDING] Standard-Logo unvollstaendig geschrieben, verworfen");
		LittleFS.remove(kLogoTmpPath);
		return;
	}
	LittleFS.remove(kLogoPath);
	if (!LittleFS.rename(kLogoTmpPath, kLogoPath)) {
		Serial.println("[BRANDING] Konnte Standard-Logo nicht aktivieren");
		return;
	}
	Serial.println("[BRANDING] Standard-Logo (Familienmarke) automatisch eingerichtet");
	logoPresent_ = true;
}

bool BrandingManager::deleteLogo() {
	bool ok = !LittleFS.exists(kLogoPath) || LittleFS.remove(kLogoPath);
	if (ok) {
		xSemaphoreTake(mutex_, portMAX_DELAY);
		logoPresent_ = false;
		xSemaphoreGive(mutex_);
	}
	return ok;
}
