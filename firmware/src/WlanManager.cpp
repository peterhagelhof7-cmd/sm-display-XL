#include "WlanManager.h"

#include <algorithm>

void WlanManager::begin() {
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	// Nach einem WLAN-Aussetzer selbsttaetig wieder verbinden - der Display-
	// Client hat keine eigene Reconnect-Schleife und haengt sonst bis zum
	// Neustart ohne Netz (und damit ohne Sensordaten) fest.
	WiFi.setAutoReconnect(true);
	// WLAN-Powersave abschalten - stabilisiert die Verbindung (ESP32-Default-
	// Modem-Sleep verursacht mit manchen APs haeufige kurze Abbrueche).
	WiFi.setSleep(false);
}

std::vector<WlanManager::NetworkInfo> WlanManager::scan(uint8_t maxResults) {
	std::vector<NetworkInfo> results;

	int16_t count = WiFi.scanNetworks();
	if (count <= 0) {
		return results;
	}

	for (int16_t i = 0; i < count; i++) {
		String ssid = WiFi.SSID(i);
		if (ssid.isEmpty()) {
			continue;
		}
		int32_t rssi = WiFi.RSSI(i);

		bool merged = false;
		for (auto &existing : results) {
			if (existing.ssid == ssid) {
				if (rssi > existing.rssi) {
					existing.rssi = rssi;
				}
				merged = true;
				break;
			}
		}
		if (!merged) {
			NetworkInfo info;
			info.ssid = ssid;
			info.rssi = rssi;
			info.secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
			results.push_back(info);
		}
	}

	std::sort(results.begin(), results.end(), [](const NetworkInfo &a, const NetworkInfo &b) {
		return a.rssi > b.rssi;
	});

	if (results.size() > maxResults) {
		results.resize(maxResults);
	}

	WiFi.scanDelete();
	return results;
}

bool WlanManager::hasCredentials() {
	prefs.begin("wifi", true);
	bool has = prefs.isKey("ssid");
	prefs.end();
	return has;
}

bool WlanManager::loadCredentials(String &ssid, String &psk) {
	prefs.begin("wifi", true);
	if (!prefs.isKey("ssid")) {
		prefs.end();
		return false;
	}
	ssid = prefs.getString("ssid", "");
	psk = prefs.getString("psk", "");
	prefs.end();
	return !ssid.isEmpty();
}

void WlanManager::saveCredentials(const String &ssid, const String &psk) {
	prefs.begin("wifi", false);
	prefs.putString("ssid", ssid);
	prefs.putString("psk", psk);
	prefs.end();
}

bool WlanManager::hasStaticIp() {
	prefs.begin("wifi", true);
	bool has = prefs.getBool("staticIp", false);
	prefs.end();
	return has;
}

bool WlanManager::loadStaticIp(IPAddress &ip, IPAddress &gateway, IPAddress &subnet, IPAddress &dns) {
	prefs.begin("wifi", true);
	bool has = prefs.getBool("staticIp", false);
	if (has) {
		ip = IPAddress(prefs.getUInt("ip", 0));
		gateway = IPAddress(prefs.getUInt("gw", 0));
		subnet = IPAddress(prefs.getUInt("sn", 0));
		dns = IPAddress(prefs.getUInt("dns", 0)); // 0.0.0.0 = nicht gesetzt -> Gateway
	}
	prefs.end();
	return has;
}

void WlanManager::saveStaticIp(const IPAddress &ip, const IPAddress &gateway, const IPAddress &subnet,
                               const IPAddress &dns) {
	prefs.begin("wifi", false);
	prefs.putBool("staticIp", true);
	prefs.putUInt("ip", static_cast<uint32_t>(ip));
	prefs.putUInt("gw", static_cast<uint32_t>(gateway));
	prefs.putUInt("sn", static_cast<uint32_t>(subnet));
	prefs.putUInt("dns", static_cast<uint32_t>(dns));
	prefs.end();
}

void WlanManager::clearStaticIp() {
	prefs.begin("wifi", false);
	prefs.putBool("staticIp", false);
	prefs.end();
}

void WlanManager::clearCredentials() {
	prefs.begin("wifi", false);
	prefs.clear();
	prefs.end();
}

bool WlanManager::connect(const String &ssid, const String &psk, uint32_t timeoutMs) {
	WiFi.disconnect();
	delay(100);

	IPAddress ip, gateway, subnet, dns;
	if (loadStaticIp(ip, gateway, subnet, dns)) {
		// WiFi.config() ohne DNS-Argument setzt den DNS-Server auf 0.0.0.0 -
		// dann schlaegt JEDE Namensaufloesung fehl (NTP "de.pool.ntp.org",
		// Internet-Ping-Check, SNMP-by-Name). Daher DNS immer setzen: den in
		// den Einstellungen konfigurierten DNS, falls angegeben - sonst das
		// Gateway als sinnvollen Default (der Router loest im LAN praktisch
		// immer auf). Cloudflare 1.1.1.1 als sekundaerer Fallback, falls der
		// primaere Resolver ausfaellt. Bei DHCP liefert der Server den DNS.
		IPAddress primaryDns = (static_cast<uint32_t>(dns) != 0) ? dns : gateway;
		WiFi.config(ip, gateway, subnet, primaryDns, IPAddress(1, 1, 1, 1));
	}

	WiFi.begin(ssid.c_str(), psk.c_str());

	uint32_t start = millis();
	while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
		delay(200);
	}
	return WiFi.status() == WL_CONNECTED;
}

bool WlanManager::autoConnect(uint32_t timeoutMs) {
	String ssid, psk;
	if (!loadCredentials(ssid, psk)) {
		return false;
	}
	return connect(ssid, psk, timeoutMs);
}

int8_t WlanManager::signalBars() const {
	if (!isConnected()) {
		smoothedRssi = NAN;
		lastBars = -1;
		return -1; // kein WLAN - Symbol soll blinken
	}

	float rssi = static_cast<float>(WiFi.RSSI());
	smoothedRssi = isnan(smoothedRssi) ? rssi : (smoothedRssi * 0.8f + rssi * 0.2f);

	// Hysterese gegen Balken-Flackern (Hardware-Befund: RSSI pendelte knapp
	// um die -60dBm-Schwelle, dadurch schneller Sprung zwischen 2 und 3
	// Balken jede StatusBar-Aktualisierung). Ein Schwellenwechsel muss den
	// alten Grenzwert um kHysteresisDb ueber-/unterschreiten, je nach
	// Richtung - direkt am Grenzwert bleibt die zuletzt gezeigte Stufe
	// stabil.
	constexpr float kHysteresisDb = 4.0f;
	constexpr float kThreshold32 = -60.0f; // Grenze zwischen 2 und 3 Balken
	constexpr float kThreshold21 = -75.0f; // Grenze zwischen 1 und 2 Balken

	int8_t bars;
	if (lastBars < 1) {
		// Erste Messung nach Verbindungsaufbau: kein Vorzustand fuer die
		// Hysterese vorhanden, direkt ohne Toleranz einordnen.
		bars = (smoothedRssi >= kThreshold32) ? 3 : (smoothedRssi >= kThreshold21 ? 2 : 1);
	} else {
		bars = lastBars;
		if (bars == 3 && smoothedRssi < kThreshold32 - kHysteresisDb) {
			bars = 2;
		} else if (bars == 1 && smoothedRssi >= kThreshold21 + kHysteresisDb) {
			bars = 2;
		} else if (bars == 2) {
			if (smoothedRssi >= kThreshold32 + kHysteresisDb) {
				bars = 3;
			} else if (smoothedRssi < kThreshold21 - kHysteresisDb) {
				bars = 1;
			}
		}
	}
	lastBars = bars;
	return bars;
}
