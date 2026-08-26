#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "SettingsManager.h"
#include "BacklightManager.h"
#include "OtaManager.h"
#include "WlanManager.h"
#include "SensormeterManager.h"
#include "SensorManager.h"
#include "PingManager.h"
#include "GraphManager.h"
#include "BrandingManager.h"
#include "DisplayMirror.h"

// Webserver mit zwei Seiten (siehe docs/entscheidungen.md):
// - "/" : oeffentliches, NICHT passwortgeschuetztes Status-Dashboard
//   (Nutzerwunsch: alle aktuellen Werte auf einen Blick, Auto-Refresh 30s,
//   kein Login noetig) - rein lesend, keine Formulare.
// - "/settings" : der bisherige Einstellungs-Bereich (Name, Betriebsmodus,
//   Helligkeit, Sensormeter-/Ping-Ziele, Warnschwellwerte, Netzwerk,
//   OTA-Update), weiterhin per HTTP-Basic-Auth geschuetzt.
// Async (ESPAsyncWebServer/AsyncTCP), damit HTTP-Anfragen den Hauptloop
// (Touch, Sensor-Polling, Ping, Snake) nicht blockieren.
class WebServerManager {
public:
	// sensormeterManager/sensor/pingManager/graph: nur lesend -
	// sensormeterManager fuer die PRO-Sensor-Erkennung in der Schwellwert-
	// Tabelle UND fuer die Sensormeter-Anzeige im Dashboard; sensor/
	// pingManager/graph ausschliesslich fuer die aktuellen Werte bzw. den
	// SVG-Verlaufsgraph im Dashboard.
	WebServerManager(SettingsManager &settings, BacklightManager &backlight, OtaManager &ota, WlanManager &wlan,
	                  const SensormeterManager &sensormeterManager, const SensorManager &sensor,
	                  const PingManager &pingManager, GraphManager &graph, BrandingManager &brandingManager,
	                  const DisplayMirrorState &mirror);

	void begin();

private:
	bool checkAuth(AsyncWebServerRequest *request) const;
	// saved: zeigt einen kurzen "Gespeichert"-Hinweis oben auf der Seite -
	// gesetzt, wenn der aufrufende Redirect "?saved=1" mitgibt.
	String buildSettingsPage(bool saved) const;
	String buildDashboardPage() const;
	// Web-Spiegel des Displays (Nutzeranforderung "B"): /display liefert eine
	// als 320x240-Kachel gestylte Seite, die per JavaScript ~1s /api/display
	// (JSON) pollt und genau das aktuell aktive Slide nachbildet (inkl.
	// Drill-down/Halten und Alarm-Hintergrund). Echtes Pixel-Auslesen des
	// TFT ist auf diesem PSRAM-losen Board nicht praktikabel (siehe
	// docs/entscheidungen.md) - daher Daten-, kein Pixel-Spiegel.
	String buildMirrorPage() const;
	String buildDisplayJson() const;
	String sharedCss() const;
	// linkHref leer = kein Link-Badge (nicht gebraucht, wenn eine Seite
	// eh nur die andere Richtung anbieten wuerde).
	String bannerHtml(const String &eyebrow, const String &subLine, const String &linkHref,
	                   const String &linkLabel) const;
	String dataSourceOptions(DataSource selected) const;
	String dhtGraphSvg() const;
	String wifiBarsHtml(int8_t bars) const;
	// Klartext-Dump aller SettingsManager-Werte fuer /settings/export (siehe
	// docs/entscheidungen.md) - analog zum config.xml-Export bei Sensormeter/
	// Sensormeter WLAN, hier als text/plain, da SettingsManager NVS-basiert
	// ist und kein XML-Dokument fuehrt.
	String buildConfigExport() const;

	void handleSave(AsyncWebServerRequest *request);
	void handlePingAdd(AsyncWebServerRequest *request);
	void handlePingRemove(AsyncWebServerRequest *request);
	void handleSensormeterAdd(AsyncWebServerRequest *request);
	void handleSensormeterRemove(AsyncWebServerRequest *request);
	void handleSensormeterThresholds(AsyncWebServerRequest *request);
	void handlePingThreshold(AsyncWebServerRequest *request);
	void handleNetworkSave(AsyncWebServerRequest *request);
	void handleFactoryReset(AsyncWebServerRequest *request);
	void handleOtaUploadChunk(AsyncWebServerRequest *request, const String &filename, size_t index,
	                           uint8_t *data, size_t len, bool final);
	void handleOtaUploadComplete(AsyncWebServerRequest *request);

	// Anbieter-Branding: Logo-Upload (Streaming, analog handleOtaUploadChunk/
	// -Complete), Logo-Auslieferung als on-the-fly synthetisiertes 24-Bit-BMP
	// (kein PNG/JPEG-Decoder noetig, siehe BrandingManager.h) und Loeschen.
	void handleBrandingLogoUploadChunk(AsyncWebServerRequest *request, const String &filename, size_t index,
	                                    uint8_t *data, size_t len, bool final);
	void handleBrandingLogoUploadComplete(AsyncWebServerRequest *request);
	void handleBrandingLogoBmp(AsyncWebServerRequest *request) const;
	void handleBrandingLogoDelete(AsyncWebServerRequest *request);
	// Kleines "Branding-Banner" (Logo und/oder Anbietername) unterhalb des
	// Haupt-Banners - nur ausgegeben, wenn tatsaechlich etwas konfiguriert
	// ist, siehe bannerHtml().
	String brandingBannerHtml() const;

	SettingsManager &settings_;
	BacklightManager &backlight_;
	OtaManager &ota_;
	WlanManager &wlan_;
	const SensormeterManager &sensormeterManager_;
	const SensorManager &sensor_;
	const PingManager &pingManager_;
	GraphManager &graph_;
	BrandingManager &brandingManager_;
	const DisplayMirrorState &mirror_;
	AsyncWebServer server_{80};

	bool otaInProgress_ = false;
	bool otaSuccess_ = false;
	bool brandingUploadOk_ = false;
};
