#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <math.h>
#include <vector>

// Verwaltet WLAN-Scan, -Verbindung und die Zugangsdaten im Flash (NVS).
// Genau ein gespeichertes Netz (SSID+PSK) - Lastenheft sieht kein
// Multi-WLAN-Fallback vor, nur "WLAN erneut auswaehlen" in den
// Systemeinstellungen (ersetzt die gespeicherten Zugangsdaten).
class WlanManager {
public:
	struct NetworkInfo {
		String ssid;
		int32_t rssi;
		bool secured;
	};

	void begin();

	// Scannt und liefert bis zu maxResults Netze, nach Signalstaerke
	// sortiert, Duplikate (mehrere APs derselben SSID) zusammengefasst.
	std::vector<NetworkInfo> scan(uint8_t maxResults = 6);

	bool hasCredentials();
	bool loadCredentials(String &ssid, String &psk);
	void saveCredentials(const String &ssid, const String &psk);

	// Statische IP (optional, sonst DHCP) - wird in connect()/autoConnect()
	// automatisch angewendet, falls vorhanden. Gespeichert im selben
	// NVS-Namespace wie die WLAN-Zugangsdaten, da beides zur
	// Verbindungskonfiguration gehoert. Der DNS-Server ist optional: ist er
	// 0.0.0.0 (nicht gesetzt), verwendet connect() das Gateway als DNS.
	bool hasStaticIp();
	bool loadStaticIp(IPAddress &ip, IPAddress &gateway, IPAddress &subnet, IPAddress &dns);
	void saveStaticIp(const IPAddress &ip, const IPAddress &gateway, const IPAddress &subnet,
	                  const IPAddress &dns);
	void clearStaticIp();

	// Werksreset "Konfiguration"/"Alles" (siehe WebServerManager/main.cpp
	// Serial-Kommandozeile): loescht SSID/PSK UND die statische IP - der
	// komplette NVS-Namespace "wifi" wird geleert, ein Neustart landet danach
	// wieder im Onboarding (siehe main.cpp).
	void clearCredentials();

	// Verbindet mit den uebergebenen Zugangsdaten, blockierend bis
	// verbunden oder Timeout. Speichert bei Erfolg NICHT automatisch
	// (das entscheidet der Aufrufer, siehe WifiOnboarding).
	bool connect(const String &ssid, const String &psk, uint32_t timeoutMs = 15000);

	// Laedt gespeicherte Zugangsdaten und verbindet damit.
	bool autoConnect(uint32_t timeoutMs = 15000);

	bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
	int8_t signalBars() const; // 0-3, fuer das Statusleisten-Symbol

private:
	Preferences prefs;

	// Redraw-nahes Flackern der Balkenanzeige (RSSI pendelte knapp um eine
	// Schwelle wie -60dBm, dadurch schneller Wechsel zwischen 2 und 3
	// Balken) - geglaetteter RSSI-Wert plus Hysterese beim Schwellenwechsel
	// beheben das, siehe WlanManager.cpp. mutable, da signalBars() const ist
	// (wird bei jedem StatusBar::draw()-Aufruf gelesen, aendert aber
	// intern geglaetteten Zustand).
	mutable float smoothedRssi = NAN;
	mutable int8_t lastBars = -1;
};
