#include "OtaManager.h"

#include <Update.h>
#include <cstring>

#if __has_include("config.h")
#include "config.h"
#endif
#ifndef DEVICE_FIRMWARE_VERSION
#define DEVICE_FIRMWARE_VERSION "0.0.0"
#endif
#ifndef FIRMWARE_PROJECT_ID
#define FIRMWARE_PROJECT_ID "UNKNOWN"
#endif

namespace {
const char *const kMarkerPrefix = "SM-FW-ID:";
const size_t kMarkerPrefixLen = 9;
const char *const kMarkerSuffix = ":SM-FW-END";
// Obergrenze fuer "<FIRMWARE_PROJECT_ID>:<DEVICE_FIRMWARE_VERSION>" - reicht
// grosszuegig fuer alle Projektnamen/Versionsstrings der Familie.
const size_t kMaxCaptureLen = 96;

// Zerlegt "MAJOR.MINOR.PATCH[-SUFFIX]" in seine Teile - kein vollstaendiger
// Semver-Parser (z.B. keine Build-Metadaten "+..."), deckt aber das in
// diesem Projekt genutzte a.b.c[-rcN]-Schema ab.
void parseVersion(const String &v, int &major, int &minor, int &patch, String &suffix) {
	major = minor = patch = 0;
	suffix = "";
	int dashPos = v.indexOf('-');
	String core = (dashPos >= 0) ? v.substring(0, dashPos) : v;
	if (dashPos >= 0) suffix = v.substring(dashPos + 1);
	int firstDot = core.indexOf('.');
	int secondDot = (firstDot >= 0) ? core.indexOf('.', firstDot + 1) : -1;
	if (firstDot < 0 || secondDot < 0) return;
	major = core.substring(0, firstDot).toInt();
	minor = core.substring(firstDot + 1, secondDot).toInt();
	patch = core.substring(secondDot + 1).toInt();
}

// Vergleicht zwei Versionen nach obigem Schema, liefert <0/0/>0 wie strcmp.
// Bei gleichem a.b.c hat "kein Suffix" Vorrang vor "mit Suffix" (ein
// Release gilt als neuer als jede Vorabversion derselben Kernversion), bei
// zwei Suffixen entscheidet der lexikografische Vergleich (deckt
// "rc3" < "rc4" ab).
int compareVersions(const String &a, const String &b) {
	int aMajor, aMinor, aPatch, bMajor, bMinor, bPatch;
	String aSuffix, bSuffix;
	parseVersion(a, aMajor, aMinor, aPatch, aSuffix);
	parseVersion(b, bMajor, bMinor, bPatch, bSuffix);
	if (aMajor != bMajor) return aMajor - bMajor;
	if (aMinor != bMinor) return aMinor - bMinor;
	if (aPatch != bPatch) return aPatch - bPatch;
	if (aSuffix.length() == 0 && bSuffix.length() > 0) return 1;
	if (aSuffix.length() > 0 && bSuffix.length() == 0) return -1;
	return aSuffix.compareTo(bSuffix);
}
}  // namespace

namespace {
// Byte-sichere Teilstring-Suche (memmem-Ersatz - nicht auf allen Plattformen
// verfuegbar) - im Unterschied zu String::indexOf()/strstr() bricht das
// NICHT am ersten eingebetteten Null-Byte ab.
int findBytes(const uint8_t *haystack, size_t haystackLen, const char *needle, size_t needleLen) {
	if (needleLen == 0 || haystackLen < needleLen) return -1;
	for (size_t i = 0; i + needleLen <= haystackLen; i++) {
		if (memcmp(haystack + i, needle, needleLen) == 0) return (int)i;
	}
	return -1;
}
}  // namespace

bool OtaManager::beginLocalUpdate(size_t contentLength) {
	markerFound_ = false;
	identityMatches_ = false;
	versionAllowed_ = false;
	capturing_ = false;
	tailLen_ = 0;
	captureLen_ = 0;
	return Update.begin(contentLength);
}

bool OtaManager::writeLocalUpdateChunk(uint8_t *data, size_t len) {
	if (!markerFound_) scanChunkForMarker(data, len);
	return Update.write(data, len) == len;
}

// Sucht ueber Chunk-Grenzen hinweg nach kMarkerPrefix, faengt danach alles
// bis kMarkerSuffix ab (bzw. bricht ab, wenn kMaxCaptureLen ueberschritten
// wird, ohne dass ein Suffix gefunden wurde) und wertet den eingefangenen
// Text dann in handleMarkerPayload() aus. Das eigentliche Update.write()
// laeuft unabhaengig davon weiter - erst endLocalUpdate() entscheidet
// anhand des Scan-Ergebnisses, ob committet oder verworfen wird.
//
// Arbeitet bewusst auf rohen Bytes (nicht Arduino String/strstr) - eine
// .bin ist Binaerdaten mit reichlich eingebetteten Null-Bytes (bereits ab
// Byte 9 im ESP32-Image-Header gesehen), String-basierte Suche wuerde dort
// abbrechen und den Marker nie finden, egal wie weit hinten er im File
// liegt. Siehe docs/entscheidungen.md fuer den Befund, der das aufgedeckt
// hat (uebernommen aus sensormeter).
//
// 2026-07-18 korrigiert (uebernommen aus sensormeter, siehe dortiges
// docs/entscheidungen.md): die vorherige Fassung kopierte jeden Chunk in
// einen auf kTailCap_+512 Byte GEDECKELTEN Zwischenpuffer und durchsuchte
// nur diesen - ein echter HTTP-Upload liefert aber regelmaessig groessere
// Chunks als 512 Byte, wodurch alles jenseits der Deckelung
// STILLSCHWEIGEND uebersprungen wurde (weder gescannt noch als Tail
// vorgemerkt). Bei sensormeter hat der erste echte End-to-End-OTA-Test
// auf realer Hardware genau das aufgedeckt: der Marker war nachweislich
// in der .bin vorhanden, wurde aber trotzdem nicht gefunden. Fix: kein
// kopierter Zwischenpuffer mehr fuer den Chunk selbst - findBytes()
// durchsucht "data"/"len" direkt (beliebig gross, keine Kopie noetig, da
// bereits zusammenhaengend im Speicher). Der kleine Join-Puffer wird nur
// noch fuer den echten Grenzfall gebraucht, dass der Prefix im vorigen
// Tail beginnt und in den ersten Bytes dieses Chunks endet - dafuer
// reichen kTailCap_+kMarkerPrefixLen Byte, unabhaengig von der
// tatsaechlichen Chunkgroesse.
void OtaManager::scanChunkForMarker(uint8_t *data, size_t len) {
	if (!capturing_) {
		// 1. Grenzfall: Prefix beginnt im Tail des vorigen Chunks und setzt
		// sich in den ersten Bytes dieses Chunks fort. Nur relevant, wenn
		// ueberhaupt ein Tail vorliegt.
		bool spansTail = false;
		size_t afterPrefixInData = 0;
		if (tailLen_ > 0) {
			uint8_t joinBuf[kTailCap_ + kMarkerPrefixLen];
			size_t headLen = len < kMarkerPrefixLen ? len : kMarkerPrefixLen;
			memcpy(joinBuf, tailBuf_, tailLen_);
			memcpy(joinBuf + tailLen_, data, headLen);
			size_t joinLen = tailLen_ + headLen;
			int p = findBytes(joinBuf, joinLen, kMarkerPrefix, kMarkerPrefixLen);
			// Nur als "spannend" werten, wenn der Fund tatsaechlich noch im
			// Tail-Anteil beginnt - sonst liegt er komplett in "data" und
			// wird gleich ohnehin von der Direktsuche unten gefunden.
			if (p >= 0 && (size_t)p < tailLen_) {
				spansTail = true;
				afterPrefixInData = (size_t)p + kMarkerPrefixLen - tailLen_;
			}
		}

		// 2. Regulaerer Fall: Prefix komplett innerhalb des aktuellen Chunks
		// - direkt auf "data"/"len" gesucht, keine Groessenbeschraenkung.
		int prefixPos = spansTail ? -1 : findBytes(data, len, kMarkerPrefix, kMarkerPrefixLen);

		if (!spansTail && prefixPos < 0) {
			// Kein Treffer - letzte kMarkerPrefixLen-1 Byte DIESES Chunks
			// (nicht eines gedeckelten Zwischenpuffers) als Tail fuer den
			// naechsten Aufruf vormerken.
			size_t keep = kMarkerPrefixLen > 0 ? kMarkerPrefixLen - 1 : 0;
			if (keep > kTailCap_) keep = kTailCap_;
			size_t start = len > keep ? len - keep : 0;
			tailLen_ = len - start;
			memcpy(tailBuf_, data + start, tailLen_);
			return;
		}
		capturing_ = true;
		size_t afterPrefix = spansTail ? afterPrefixInData : (size_t)prefixPos + kMarkerPrefixLen;
		size_t remaining = len - afterPrefix;
		if (remaining > kCaptureCap_) remaining = kCaptureCap_;
		memcpy(captureBuf_, data + afterPrefix, remaining);
		captureLen_ = remaining;
		tailLen_ = 0;
	} else {
		size_t copyLen = len;
		if (captureLen_ + copyLen > kCaptureCap_) copyLen = kCaptureCap_ - captureLen_;
		memcpy(captureBuf_ + captureLen_, data, copyLen);
		captureLen_ += copyLen;
	}

	int suffixPos = findBytes(captureBuf_, captureLen_, kMarkerSuffix, strlen(kMarkerSuffix));
	if (suffixPos >= 0) {
		String payload;
		payload.reserve(suffixPos);
		for (int i = 0; i < suffixPos; i++) payload += (char)captureBuf_[i];
		handleMarkerPayload(payload);
		capturing_ = false;
		captureLen_ = 0;
		tailLen_ = 0;
		return;
	}
	if (captureLen_ >= kCaptureCap_) {
		// Kein gueltiger Marker in plausibler Laenge - wie "nicht gefunden"
		// behandeln, nicht endlos weiter aufsammeln.
		capturing_ = false;
		captureLen_ = 0;
		tailLen_ = 0;
	}
}

void OtaManager::handleMarkerPayload(const String &payload) {
	int sep = payload.indexOf(':');
	if (sep < 0) return;
	String projectId = payload.substring(0, sep);
	String version = payload.substring(sep + 1);

	markerFound_ = true;
	identityMatches_ = (projectId == FIRMWARE_PROJECT_ID);
	versionAllowed_ = allowDowngrade_ || compareVersions(version, DEVICE_FIRMWARE_VERSION) >= 0;
}

bool OtaManager::endLocalUpdate() {
	if (!markerFound_ || !identityMatches_ || !versionAllowed_) {
		Update.abort();
		return false;
	}
	return Update.end(true);
}
