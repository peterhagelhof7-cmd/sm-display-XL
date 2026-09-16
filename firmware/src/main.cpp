#include <Arduino.h>
#include <ESPmDNS.h>
#include <esp_system.h>
#include <math.h>

#include "DisplayManager.h"
#include "TouchManager.h"
#include "WlanManager.h"
#include "WifiOnboarding.h"
#include "SensorManager.h"
#include "GraphManager.h"
#include "ClockView.h"
#include "SensormeterManager.h"
#include "SensormeterView.h"
#include "PingManager.h"
#include "PingView.h"
#include "StatusBar.h"
#include "TimeSync.h"
#include "Layout.h"
#include "DataSource.h"
#include "SettingsManager.h"
#include "BacklightManager.h"
#include "SettingsUI.h"
#include "UiHelpers.h"
#include "OtaManager.h"
#include "WebServerManager.h"
#include "InfoUI.h"
#include "AlertEvaluator.h"
#include "BrandingManager.h"
#include "BrandingView.h"
#include "DisplayMirror.h"
#include "EventLog.h"

// Der Arduino-Default-loopTask-Stack (8 KB) reicht fuer die tiefen
// Zeichen-Aufrufketten NICHT: die drei OLED-Schwesterprojekte (sm/sm-poe/
// sm-wlan) brauchten alle SET_LOOP_TASK_STACK_SIZE(16384), sonst Stack-
// Overflow-Reboot. Auf dem XL ist der Bedarf hoeher (LovyanGFX statt
// TFT_eSPI = tiefere Aufrufketten, 800x480, modaler Tastatur-/Einstellungs-
// Dialog aus loop() heraus) -> grosszuegig 24 KB (RAM ist reichlich da).
SET_LOOP_TASK_STACK_SIZE(24576);

// Anders als bei den Schwesterprojekten ist config.h hier NICHT
// verpflichtend (kein #error bei Fehlen) - dieses Projekt braucht zur
// Kompilierzeit keine WLAN-Zugangsdaten, siehe config.h.example und
// scripts/flash.sh/.ps1 (project_has_config_h liefert hier false).
#if __has_include("config.h")
#include "config.h"
#endif
#ifndef DEVICE_FIRMWARE_VERSION
#define DEVICE_FIRMWARE_VERSION "0.0.0"
#endif
#ifndef FIRMWARE_PROJECT_ID
#define FIRMWARE_PROJECT_ID "UNKNOWN"
#endif

// Eingebetteter Marker fuer die OTA-Herkunfts-/Versionspruefung (siehe
// OtaManager.h/.cpp) - wird beim Firmware-Upload auf einem Schwestergeraet
// im Byte-Stream dieser .bin gesucht, um Projekt-Identitaet und Version zu
// pruefen. Ueber den Serial.println() in setup() referenziert, damit der
// Linker ihn nicht wegoptimiert.
const char kFirmwareIdentityMarker[] = "SM-FW-ID:" FIRMWARE_PROJECT_ID ":" DEVICE_FIRMWARE_VERSION ":SM-FW-END";

namespace {
// Wandelt den frei waehlbaren Systemnamen in einen gueltigen mDNS-Hostnamen
// um (nur a-z/0-9/-, keine Umlaute/Leerzeichen) - Systemname kann Leer-
// zeichen/Umlaute/Grossbuchstaben enthalten, mDNS-Hostnamen duerfen das
// nicht. Fallback auf einen festen Namen, falls nach dem Filtern nichts
// uebrig bleibt (z.B. Systemname nur aus Umlauten).
String sanitizeHostname(const String &name) {
	String out;
	for (size_t i = 0; i < name.length(); i++) {
		char c = name[i];
		if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
			out += c;
		} else if ((c == ' ' || c == '_') && out.length() > 0 && out[out.length() - 1] != '-') {
			out += '-';
		}
	}
	while (out.length() > 0 && out[out.length() - 1] == '-') out.remove(out.length() - 1);
	while (out.length() > 0 && out[0] == '-') out.remove(0, 1);
	if (out.isEmpty()) out = "sensormeter-display";
	return out;
}
} // namespace

DisplayManager display;
TouchManager touch;
WlanManager wlan;
WifiOnboarding onboarding;
SensorManager sensor;
GraphManager graph;
ClockView clockView;
SensormeterManager sensormeterManager;
SensormeterView sensormeterView;
PingManager pingManager;
PingView pingView;
StatusBar statusBar;
SettingsManager settings;
BacklightManager backlight;
SettingsUI settingsUI;
OtaManager ota;
BrandingManager brandingManager;
BrandingView brandingView;
EventLog eventLog;
// Vom Hauptloop befuellter Momentanzustand des Displays fuer den Web-Spiegel.
// Muss VOR webServer stehen (Konstruktor nimmt eine const-Referenz darauf).
DisplayMirrorState mirrorState;
WebServerManager webServer(settings, backlight, ota, wlan, sensormeterManager, sensor, pingManager, graph,
                            brandingManager, mirrorState);

// Serial-Kommandozeile fuer den Fall, dass das Geraet nur per USB, aber
// nicht per Netzwerk erreichbar ist (z.B. falsche/unbekannte
// WLAN-Zugangsdaten, statische IP in einem fremden Netzsegment). Bewusst
// dasselbe Vertrauensmodell wie bei den drei OLED-Geschwisterprojekten
// (physischer USB-Zugriff = vertrauenswuerdig, kein Web-Passwort noetig) -
// NICHT pauschal destruktiv: nur "reset"/"reset all" loeschen etwas, alle
// anderen Kommandos aendern gezielt nur die WLAN-/IP-Felder. Anders als bei
// den Geschwisterprojekten gibt es hier kein "dump"/"upload" (kein
// XML-Konfigurationsdokument - Einstellungen liegen einzeln in NVS/
// Preferences, siehe SettingsManager/WlanManager), siehe docs/entscheidungen.md.
//
// Kommandos (jeweils + Enter):
//   dhcp                          WLAN auf DHCP umstellen, statische
//                                  IP/Maske/Gateway loeschen, neu starten
//   ip <ip> <maske> <gateway> [dns]  statische IP setzen, neu starten. Anders
//                                  als die Einstellungsseite OHNE
//                                  Ping-Kollisionspruefung - bewusst einfach
//                                  gehalten. DNS optional (4. Argument); fehlt
//                                  er, nutzt WlanManager das Gateway als DNS.
//   wifi <ssid> <passwort>        neue WLAN-Zugangsdaten setzen, neu starten
//   status                        aktuellen Zustand ausgeben (WLAN, IP,
//                                  Signal, Sensor, Heap, Laufzeit) - liest
//                                  nur, aendert nichts, kein Neustart
//   reset                         Werksreset nur der Konfiguration (wie
//                                  Umfang "Nur Konfiguration" auf der
//                                  Einstellungsseite, inkl. WLAN, ohne
//                                  Branding-Erhalt), neu starten - der
//                                  12h-Verlauf bleibt erhalten
//   reset all                     wie oben, zusaetzlich wird der 12h-Verlauf
//                                  geloescht UND das Branding entfernt
void handleSerialCommands() {
	static String line;

	while (Serial.available()) {
		char c = static_cast<char>(Serial.read());
		if (c == '\r') continue;
		if (c != '\n') {
			line += c;
			continue;
		}
		line.trim();

		String cmd = line;
		String args;
		int sp = line.indexOf(' ');
		if (sp >= 0) {
			cmd = line.substring(0, sp);
			args = line.substring(sp + 1);
			args.trim();
		}

		if (cmd.equalsIgnoreCase("dhcp")) {
			wlan.clearStaticIp();
			Serial.println("[SERIAL] WLAN auf DHCP umgestellt, starte neu...");
			delay(300);
			ESP.restart();

		} else if (cmd.equalsIgnoreCase("ip")) {
			String parts[4];
			int count = 0;
			String rest = args;
			while (rest.length() > 0 && count < 4) {
				int sp2 = rest.indexOf(' ');
				if (sp2 < 0) {
					parts[count++] = rest;
					rest = "";
				} else {
					parts[count++] = rest.substring(0, sp2);
					rest = rest.substring(sp2 + 1);
					rest.trim();
				}
			}
			IPAddress ip, mask, gateway, dns;
			if (count < 3 || !ip.fromString(parts[0]) || !mask.fromString(parts[1]) ||
			    !gateway.fromString(parts[2])) {
				Serial.println("[SERIAL] Nutzung: ip <adresse> <maske> <gateway> [dns]");
			} else {
				// DNS optional (4. Argument): fehlt/ungueltig -> 0.0.0.0, dann
				// verwendet WlanManager das Gateway als DNS.
				if (count < 4 || !dns.fromString(parts[3])) {
					dns = IPAddress((uint32_t)0);
				}
				wlan.saveStaticIp(ip, gateway, mask, dns);
				Serial.println("[SERIAL] Statische IP gesetzt, starte neu...");
				delay(300);
				ESP.restart();
			}

		} else if (cmd.equalsIgnoreCase("wifi")) {
			int sp3 = args.indexOf(' ');
			if (sp3 < 0 || args.substring(0, sp3).length() == 0) {
				Serial.println("[SERIAL] Nutzung: wifi <ssid> <passwort>");
			} else {
				String ssid = args.substring(0, sp3);
				String psk = args.substring(sp3 + 1);
				psk.trim();
				wlan.saveCredentials(ssid, psk);
				Serial.println("[SERIAL] WLAN-Zugangsdaten gesetzt, starte neu...");
				delay(300);
				ESP.restart();
			}

		} else if (cmd.equalsIgnoreCase("status")) {
			Serial.println("[SERIAL] --- Status ---");
			Serial.print("WLAN: ");
			if (wlan.isConnected()) {
				Serial.println("verbunden");
			} else {
				Serial.println("nicht verbunden");
			}
			Serial.print("IP: ");
			Serial.println(WiFi.localIP());
			Serial.print("Signal: ");
			Serial.print(wlan.isConnected() ? WiFi.RSSI() : 0);
			Serial.println(" dBm");
			Serial.print("Sensor: ");
			if (sensor.hasValidReading()) {
				Serial.print(sensor.temperatureC(), 1);
				Serial.print(" C / ");
				Serial.print(sensor.humidityPercent(), 1);
				Serial.println(" %");
			} else {
				Serial.println("kein gueltiger Messwert");
			}
			Serial.print("Freier Heap: ");
			Serial.print(ESP.getFreeHeap() / 1024);
			Serial.println(" kB");
			Serial.print("Laufzeit: ");
			Serial.print((unsigned long)(esp_timer_get_time() / 1000000ULL));
			Serial.println(" s");
			Serial.println("[SERIAL] --- Ende Status ---");

		} else if (cmd.equalsIgnoreCase("reset")) {
			bool full = args.equalsIgnoreCase("all");
			settings.resetConfig(/*keepBrandingName=*/!full);
			wlan.clearCredentials();
			if (full) {
				graph.reset();
				brandingManager.deleteLogo();
				Serial.println("[SERIAL] Werksreset: Konfiguration, Verlauf und Branding geloescht, starte neu...");
			} else {
				Serial.println("[SERIAL] Werksreset: Konfiguration auf Standard zurueckgesetzt, starte neu...");
			}
			delay(300);
			ESP.restart();
		}

		line = "";
	}
}

uint32_t lastStatusBarMs = 0;
// 300ms statt z.B. 1000ms, damit das 500ms-Blinken des WLAN-Symbols nicht
// mit der Redraw-Rate aliast (bei exaktem 1000ms-Takt wuerde immer dieselbe
// Blink-Phase abgetastet und das Symbol nie sichtbar blinken).
constexpr uint32_t kStatusBarIntervalMs = 300;

uint32_t slideLastSwitchMs = 0;
size_t slideIndex = 0;
// Drill-down aus der Sensormeter-Uebersicht: Nach Tippen auf ein sm wird
// dessen Detail-Slide "gehalten" (Auto-Rotation angehalten), bis irgendwo
// zuruueckgetippt wird. Nur im Slide-Modus (Nutzeranforderung). heldTarget/
// heldSensor identifizieren das gehaltene Ziel stabil, auch wenn sich die
// Slide-Liste zwischen zwei loop()-Durchlaeufen umsortiert.
bool slideHeld = false;
int8_t heldTarget = -1;
uint8_t heldSensor = 0;
bool contentDirty = true;
uint32_t lastPeriodicRedrawMs = 0;
constexpr uint32_t kPeriodicRedrawIntervalMs = 5000;

// Ein Eintrag der dynamischen Slide-Liste im Slide-Modus. smTargetIndex/
// smSensorIndex sind nur fuer type==Sensormeter relevant und werden direkt
// an SensormeterView::draw() durchgereicht (siehe dort); -1 bedeutet
// "keine feste Zuordnung, View entscheidet selbst" (Static-Modus bzw. der
// Platzhalter-Slide, wenn kein Ziel aufgeloest ist).
struct SlideEntry {
	DataSource type = DataSource::Dht11;
	int8_t smTargetIndex = -1;
	uint8_t smSensorIndex = 0;

	SlideEntry() = default;
	// Default-Member-Initializer oben verhindern Aggregate-Initialisierung
	// mit {...} beim hier verwendeten C++-Standard - deshalb expliziter
	// Konstruktor statt reinem Aggregate.
	SlideEntry(DataSource t, int8_t targetIdx = -1, uint8_t sensorIdx = 0)
	    : type(t), smTargetIndex(targetIdx), smSensorIndex(sensorIdx) {}
};

constexpr size_t kMaxSlideEntries = 2 + SensormeterManager::kMaxTargets * 2 + 1 + 3;
SlideEntry slideEntries[kMaxSlideEntries];
size_t slideEntryCount = 0;

// Baut die Slide-Liste fuer den Slide-Modus bei jedem loop()-Durchlauf neu
// auf - guenstig genug (<= kMaxSlideEntries Eintraege, keine I/O), analog
// zum bestehenden Muster in SensormeterView::draw(), das sein eigenes
// kleines slides[]-Array ebenfalls bei jedem Aufruf neu aufbaut.
//
// Nutzeranforderung (siehe docs/entscheidungen.md): bisher war
// "Sensormeter" EIN Slide in dieser Liste, der intern (eigener Timer in
// SensormeterView) durch alle Ziele rotierte - fuer den Betrachter zeigte
// "der Slide" dadurch faktisch nur eines von mehreren abgefragten sm.
// Jetzt bekommt jedes aufgeloeste Ziel (und bei PRO-Geraeten Sensor 2
// zusaetzlich) einen eigenen Slide in der AEUSSEREN Rotation, plus eine
// zusaetzliche Uebersichtsseite (Name + Uptime aller Ziele).
void buildSlideEntries() {
	slideEntryCount = 0;
	slideEntries[slideEntryCount++] = {DataSource::Dht11};
	slideEntries[slideEntryCount++] = {DataSource::Uhrzeit};

	size_t smTargetCount = sensormeterManager.targetCount();
	size_t resolvedCount = 0;
	for (size_t i = 0; i < smTargetCount && slideEntryCount < kMaxSlideEntries - 4; i++) {
		if (!sensormeterManager.isResolved(i)) continue;
		slideEntries[slideEntryCount++] = {DataSource::Sensormeter, static_cast<int8_t>(i), 0};
		resolvedCount++;
		if (sensormeterManager.isPro(i)) {
			slideEntries[slideEntryCount++] = {DataSource::Sensormeter, static_cast<int8_t>(i), 1};
			resolvedCount++;
		}
	}
	if (resolvedCount == 0) {
		// Kein Ziel konfiguriert oder noch keins aufgeloest - ein einzelner
		// Platzhalter-Slide wie bisher (SensormeterView zeigt dafuer per
		// overrideTargetIndex=-1 ihren bestehenden Hinweistext-Pfad).
		slideEntries[slideEntryCount++] = {DataSource::Sensormeter, -1, 0};
	}
	slideEntries[slideEntryCount++] = {DataSource::SensormeterOverview};

	slideEntries[slideEntryCount++] = {DataSource::Ping};
	slideEntries[slideEntryCount++] = {DataSource::PingTargets};
	slideEntries[slideEntryCount++] = {DataSource::Branding};
}

// Sucht in der aktuellen slideEntries-Liste den Index eines Eintrags. Fuer
// type==Sensormeter muessen zusaetzlich smTargetIndex/smSensorIndex passen,
// fuer andere Typen zaehlt nur der Typ. -1, wenn nicht vorhanden.
int findSlideIndex(DataSource type, int8_t targetIdx, uint8_t sensorIdx) {
	for (size_t i = 0; i < slideEntryCount; i++) {
		if (slideEntries[i].type != type) continue;
		if (type == DataSource::Sensormeter &&
		    (slideEntries[i].smTargetIndex != targetIdx || slideEntries[i].smSensorIndex != sensorIdx)) {
			continue;
		}
		return static_cast<int>(i);
	}
	return -1;
}

void setup() {
	Serial.begin(115200);
	delay(200);
	Serial.println("Sensormeter Display - Boot");
	// DIAGNOSE: Grund des letzten Resets (1=POR 3=SW 4=PANIC 5=INT_WDT
	// 6=TASK_WDT 7=WDT 9=BROWNOUT 15=USB 16=PWR_GLITCH). Zeigt, ob der
	// Reboot-Loop ein Code-Absturz (PANIC/WDT) oder ein Strom-/Spannungs-
	// problem (BROWNOUT/PWR_GLITCH) ist.
	Serial.printf("[RST] reason=%d\n", (int)esp_reset_reason());
	Serial.println(kFirmwareIdentityMarker);
	Serial.println("[SERIAL] Kommandos: dhcp, ip, wifi, status, reset[ all] (+ Enter)");

	display.begin();
	display.drawBootScreen("Sensormeter Display", "P7/P8: Sensormeter + Ping");

	touch.begin(display);
	if (!touch.isCalibrated()) {
		Serial.println("Keine Touch-Kalibrierung gefunden - starte Kalibrierroutine");
		touch.runCalibration(display);
	}

	settings.begin();
	backlight.begin(settings.brightnessPercent());

	wlan.begin();
	bool connected = false;
	if (wlan.hasCredentials()) {
		display.drawBootScreen("WLAN", "Verbinde mit gespeichertem Netz ...");
		connected = wlan.autoConnect();
	}
	if (!connected) {
		Serial.println("Kein WLAN verbunden - starte Onboarding");
		onboarding.run(display, touch, wlan);
	}
	Serial.print("WLAN verbunden, IP: ");
	Serial.println(WiFi.localIP());

	// mDNS: Geraet unter http://<hostname>.local/ erreichbar, ohne die IP
	// erst am Geraet ablesen zu muessen. Hostname wird bei jedem Boot neu
	// aus dem aktuellen Systemnamen abgeleitet - eine spaetere Aenderung des
	// Systemnamens wirkt sich wie bei den Netzwerkeinstellungen erst nach
	// einem Neustart aus.
	String mdnsHost = sanitizeHostname(settings.deviceName());
	if (MDNS.begin(mdnsHost.c_str())) {
		MDNS.addService("http", "tcp", 80);
		Serial.print("mDNS gestartet: http://");
		Serial.print(mdnsHost);
		Serial.println(".local/");
	} else {
		Serial.println("mDNS konnte nicht gestartet werden");
	}

	TimeSync::begin();
	eventLog.begin();
	sensor.begin(settings);
	graph.begin();
	pingManager.begin();
	sensormeterManager.begin();
	brandingManager.begin();
	webServer.begin();
}

// slideEntries/slideEntryCount muessen von buildSlideEntries() bereits
// befuellt sein, bevor diese Funktion im Slide-Modus aufgerufen wird (siehe
// loop() - buildSlideEntries() laeuft dort bewusst VOR dieser Funktion,
// auch vor der Tap-Weiterschalt-Behandlung, die ebenfalls slideEntryCount
// braucht).
SlideEntry currentActiveSource() {
	if (settings.mode() == OperatingMode::Static) {
		return SlideEntry{settings.staticSource(), -1, 0};
	}
	if (slideEntryCount == 0) {
		return SlideEntry{DataSource::Dht11, -1, 0};
	}
	if (slideIndex >= slideEntryCount) slideIndex = 0;

	// Gehaltener Drill-down: keine Auto-Rotation, aktuelles Slide bleibt stehen.
	if (slideHeld) {
		return slideEntries[slideIndex];
	}

	uint32_t now = millis();
	uint32_t intervalMs = static_cast<uint32_t>(settings.slideIntervalSec()) * 1000UL;
	if (now - slideLastSwitchMs >= intervalMs) {
		slideLastSwitchMs = now;
		slideIndex = (slideIndex + 1) % slideEntryCount;
		contentDirty = true;
	}
	return slideEntries[slideIndex];
}

void loop() {
	handleSerialCommands();

	// Muss vor der Tap-Weiterschalt-Behandlung unten UND vor
	// currentActiveSource() laufen, die beide slideEntryCount lesen. Im
	// Static-Modus ungenutzt (currentActiveSource() umgeht die Liste dort
	// komplett), daher nicht extra gegate't - der Aufbau ist guenstig genug.
	buildSlideEntries();

	// Gehaltenen Drill-down stabil nachfuehren: das gehaltene Ziel kann in der
	// neu aufgebauten Liste an anderer Position stehen (weitere Ziele aufgeloest/
	// weggefallen). Verschwindet es ganz (nicht mehr erreichbar), Haltung loesen
	// und zurueck zur Uebersicht.
	if (slideHeld) {
		int idx = findSlideIndex(DataSource::Sensormeter, heldTarget, heldSensor);
		if (idx >= 0) {
			slideIndex = static_cast<size_t>(idx);
		} else {
			slideHeld = false;
			int ov = findSlideIndex(DataSource::SensormeterOverview, -1, 0);
			if (ov >= 0) slideIndex = static_cast<size_t>(ov);
			contentDirty = true;
		}
	}

	// Zahnrad in der Statusleiste antippbar - oeffnet die Einstellungen
	// (blockierend). Erst auf Loslassen warten, damit der modale Dialog
	// nicht denselben, noch gehaltenen Tipp als seinen ersten Tastendruck
	// missversteht.
	int16_t tx, ty;
	bool touchedNow = touch.read(tx, ty);
	if (touchedNow &&
	    UiHelpers::hitRect(tx, ty, StatusBar::kGearHitX, StatusBar::kGearHitY, StatusBar::kGearHitW,
	                        StatusBar::kGearHitH)) {
		while (touch.read(tx, ty)) {
			delay(15);
		}
		settingsUI.run(display, touch, wlan, settings, backlight, onboarding);
		contentDirty = true;
		lastStatusBarMs = 0;
		graph.forceRedraw();
		pingView.forceRedraw();
		statusBar.forceRedraw();
		brandingView.forceRedraw();
	}

	// Info-Symbol antippbar - oeffnet InfoUI (Systemname/IP/DHCP-Static),
	// gleiches Muster wie das Zahnrad oben.
	if (touchedNow && UiHelpers::hitRect(tx, ty, StatusBar::kInfoHitX, StatusBar::kInfoHitY,
	                                     StatusBar::kInfoHitW, StatusBar::kInfoHitH)) {
		while (touch.read(tx, ty)) {
			delay(15);
		}
		InfoUI::run(display, touch, settings, wlan);
		contentDirty = true;
		lastStatusBarMs = 0;
		graph.forceRedraw();
		pingView.forceRedraw();
		statusBar.forceRedraw();
		brandingView.forceRedraw();
	}

	// Inhaltsbereich-Tap im Slide-Modus. Drei Faelle:
	//  1. Gehaltenes sm sichtbar        -> irgendein Tap loest die Haltung und
	//                                       kehrt zur Uebersicht zurueck.
	//  2. Uebersicht sichtbar, Tap auf
	//     eine (aufgeloeste) sm-Zeile   -> in dessen Detail-Slide springen und
	//                                       halten (Auto-Rotation stoppt).
	//  3. sonst                         -> wie bisher linke/rechte Haelfte =
	//                                       vorheriges/naechstes Slide.
	// Im Static-Modus gibt es keine "naechste" Ansicht, daher dort ohne Wirkung
	// (Nutzeranforderung: Drill-down nur im Slide-Modus).
	if (touchedNow && settings.mode() == OperatingMode::Slide && ty >= Layout::kContentTop &&
	    ty < Layout::kContentBottom) {
		bool overviewShown = (slideEntryCount > 0 && slideIndex < slideEntryCount &&
		                      slideEntries[slideIndex].type == DataSource::SensormeterOverview);
		int16_t pressX = tx, pressY = ty;
		bool tappedLeft = pressX < DisplayManager::kScreenWidth / 2;
		while (touch.read(tx, ty)) {
			delay(15);
		}

		if (slideHeld) {
			// Fall 1: zurueck zur Uebersicht, Haltung loesen, Rotation weiter.
			slideHeld = false;
			int ov = findSlideIndex(DataSource::SensormeterOverview, -1, 0);
			if (ov >= 0) slideIndex = static_cast<size_t>(ov);
			slideLastSwitchMs = millis();
			contentDirty = true;
		} else if (overviewShown) {
			int target = sensormeterView.overviewHitTest(sensormeterManager, Layout::kContentTop,
			                                              Layout::kContentBottom, pressX, pressY);
			int detailIdx = -1;
			if (target >= 0 && sensormeterManager.isResolved(static_cast<size_t>(target))) {
				detailIdx = findSlideIndex(DataSource::Sensormeter, static_cast<int8_t>(target), 0);
			}
			if (detailIdx >= 0) {
				// Fall 2: hineinspringen + halten.
				slideIndex = static_cast<size_t>(detailIdx);
				heldTarget = static_cast<int8_t>(target);
				heldSensor = 0;
				slideHeld = true;
				contentDirty = true;
			} else {
				// Tap nicht auf einer aufgeloesten Zeile -> wie Fall 3.
				if (slideEntryCount > 0) {
					slideIndex = (slideIndex + (tappedLeft ? slideEntryCount - 1 : 1)) % slideEntryCount;
				}
				slideLastSwitchMs = millis();
				contentDirty = true;
			}
		} else {
			// Fall 3: vorheriges/naechstes Slide.
			if (slideEntryCount > 0) {
				slideIndex = (slideIndex + (tappedLeft ? slideEntryCount - 1 : 1)) % slideEntryCount;
			}
			slideLastSwitchMs = millis();
			contentDirty = true;
		}
	}

	bool dhtPolled = sensor.update(settings);
	if (dhtPolled) {
		graph.maybeRecord(sensor.temperatureC(), sensor.humidityPercent());
	}
	bool sensormeterPolled = sensormeterManager.update(settings);
	bool pingPolled = pingManager.update(settings);

	// Anhaltender Ping-Fehler (>1 Min.) oder ueberschrittener Warnschwellwert:
	// LED und Bildschirmhintergrund blinken (1s Taktwechsel) rot
	// (Ueberschreitung/Ausfall) bzw. blau (Unterschreitung), unabhaengig von
	// der gerade aktiven Datenquelle (lastenheft.txt Abschnitt 9 + Nutzer-
	// Erweiterung um Warnschwellwerte, siehe docs/entscheidungen.md).
	AlertInfo alert = computeAlertInfo(sensor, sensormeterManager, pingManager, settings);

	// Alarm-Entprellung (Hysterese): der Rohzustand `alert` flattert bei
	// Werten dicht am Schwellwert im Mess-/Ping-Takt (2s) im Sekundentakt.
	// Daher ASYMMETRISCH: sofort warnen (schnell sichtbar), aber erst
	// ENTWARNEN, wenn der Rohzustand kAlertClearHoldMs am Stueck inaktiv war.
	// Kurze Erholungen dazwischen (= das Flattern) werden so zu EINER
	// durchgehenden Warnung zusammengefasst - im Protokoll UND auf dem Schirm.
	// Der entprellte `effectiveAlert` treibt Blinken/Statusleiste/Web-Spiegel;
	// das Protokoll (events.txt) wird nur bei echter, entprellter
	// Zustandsaenderung geschrieben.
	static constexpr uint32_t kAlertClearHoldMs = 15000;
	static bool stableActive = false;
	static bool stableBlue = false;
	static String stableSource = "";
	static String stableDetail = "";
	static uint32_t clearCandidateSinceMs = 0;

	auto logWarnung = [&](const String &src, const String &detail) {
		String l = String("WARNUNG ") +
		           (stableBlue ? "BLAU (Unterschreitung)" : "ROT (Ueberschreitung/Ausfall)") + " | Quelle: " + src;
		if (detail.length()) l += " | " + detail;
		if (alert.extraCount > 0) l += " | +" + String(alert.extraCount) + " weitere Kategorie(n)";
		eventLog.append(l);
	};

	if (alert.active) {
		clearCandidateSinceMs = 0;   // jede erneute Verletzung bricht die Entwarn-Haltezeit ab
		stableDetail = alert.detail; // aktuellstes Detail mitfuehren (Anzeige + spaetere Entwarnung)
		if (!stableActive) {
			stableActive = true;
			stableBlue = alert.blue;
			stableSource = alert.source;
			logWarnung(stableSource, stableDetail);
		} else if (stableBlue != alert.blue || stableSource != alert.source) {
			// Weiter aktiv, aber Kategorie/Richtung hat gewechselt -> als neue
			// Zustandsaenderung protokollieren. Reine Messwert-Schwankungen bei
			// gleicher Quelle werden NICHT geloggt (sonst wieder Takt-Spam).
			stableBlue = alert.blue;
			stableSource = alert.source;
			logWarnung(stableSource, stableDetail);
		}
	} else if (stableActive) {
		if (clearCandidateSinceMs == 0) clearCandidateSinceMs = millis();
		if (millis() - clearCandidateSinceMs >= kAlertClearHoldMs) {
			String l = String("Entwarnung | Quelle: ") + stableSource;
			if (stableDetail.length()) l += " | " + stableDetail;
			eventLog.append(l);
			stableActive = false;
			clearCandidateSinceMs = 0;
		}
	}

	// Entprellter Zustand fuer Anzeige/Statusleiste/Web-Spiegel (stableSource
	// ist static -> der c_str()-Zeiger bleibt bis zum naechsten Loop gueltig).
	AlertInfo effectiveAlert(stableActive, stableBlue, stableActive ? stableSource.c_str() : "",
	                         alert.active ? alert.extraCount : 0, stableDetail);

	// (LED entfaellt auf dem XL-Board; der Alarm bleibt ueber die rote/blaue
	// Bildschirmfaerbung sichtbar - siehe bgColor unten.)
	bool blinkOn = (millis() / 1000) % 2 == 0;
	uint16_t bgColor =
	    (effectiveAlert.active && blinkOn) ? (effectiveAlert.blue ? TFT_BLUE : TFT_RED) : TFT_WHITE;

	SlideEntry activeEntry = currentActiveSource();
	DataSource activeSource = activeEntry.type;
	bool showBottomBar = !(settings.mode() == OperatingMode::Static && activeSource == DataSource::Uhrzeit);
	int16_t contentBottom = showBottomBar ? Layout::kContentBottom : DisplayManager::kScreenHeight;

	// Momentanzustand fuer den Web-Spiegel (/display, /api/display) - genau das,
	// was gleich auch gezeichnet wird.
	mirrorState.activeSource = activeSource;
	mirrorState.smTargetIndex = activeEntry.smTargetIndex;
	mirrorState.smSensorIndex = activeEntry.smSensorIndex;
	mirrorState.held = slideHeld;
	mirrorState.alertActive = effectiveAlert.active;
	mirrorState.alertBlue = effectiveAlert.blue;
	mirrorState.mode = settings.mode();

	uint32_t now = millis();
	// Uhrzeit-, Sensormeter- und Sensormeter-Uebersichts-Ansicht brauchen
	// einen Redraw ohne neue Messung (Uhrzeit aendert sich rein
	// zeitgesteuert; Sensormeter im Static-Modus rotiert intern durch die
	// Slides, siehe SensormeterView; die Uebersicht zeigt eine laufende
	// Uptime). Fuer die anderen Quellen erzeugte ein unbedingter 5s-Takt
	// einen sichtbaren, unnoetigen Full-Redraw ohne Datenaenderung
	// (Hardware-Befund: Bildschirm "zitterte" gelegentlich, siehe
	// docs/entscheidungen.md) - dafuer reicht bereits sourceJustPolled/contentDirty.
	bool periodicDue = (activeSource == DataSource::Uhrzeit || activeSource == DataSource::Sensormeter ||
	                     activeSource == DataSource::SensormeterOverview) &&
	                    (now - lastPeriodicRedrawMs >= kPeriodicRedrawIntervalMs);

	bool sourceJustPolled =
	    (activeSource == DataSource::Dht11 && dhtPolled) ||
	    ((activeSource == DataSource::Sensormeter || activeSource == DataSource::SensormeterOverview) &&
	     sensormeterPolled) ||
	    ((activeSource == DataSource::Ping || activeSource == DataSource::PingTargets) && pingPolled);
	// bgColor aendert sich nicht nur bei An-/Abklingen eines Alarms, sondern
	// auch bei jedem 1s-Blink-Taktwechsel waehrend ein Alarm aktiv ist -
	// erzwingt in beiden Faellen ein Neuzeichnen, auch ohne neue Messung.
	static uint16_t lastLoopBgColor = TFT_WHITE;
	bool bgColorChanged = (bgColor != lastLoopBgColor);
	lastLoopBgColor = bgColor;

	if (contentDirty || sourceJustPolled || periodicDue || bgColorChanged) {
		switch (activeSource) {
			case DataSource::Dht11:
				graph.drawFullScreen(display, sensor.temperatureC(), sensor.humidityPercent(),
				                     sensor.hasValidReading(), settings.dhtTempMinC(), settings.dhtTempMaxC(),
				                     settings.dhtHumMinPct(), settings.dhtHumMaxPct(), bgColor);
				break;
			case DataSource::Uhrzeit:
				clockView.draw(display, Layout::kContentTop, contentBottom, bgColor);
				break;
			case DataSource::Sensormeter:
				sensormeterView.draw(display, sensormeterManager, Layout::kContentTop, contentBottom, bgColor,
				                     activeEntry.smTargetIndex, activeEntry.smSensorIndex);
				break;
			case DataSource::SensormeterOverview:
				sensormeterView.drawOverview(display, sensormeterManager, Layout::kContentTop, contentBottom,
				                             bgColor);
				break;
			case DataSource::Ping:
				pingView.drawAverage(display, pingManager, Layout::kContentTop, contentBottom, bgColor);
				break;
			case DataSource::PingTargets:
				pingView.drawTargetList(display, pingManager, Layout::kContentTop, contentBottom, bgColor);
				break;
			case DataSource::Branding:
				brandingView.draw(display, brandingManager, settings, Layout::kContentTop, contentBottom, bgColor);
				break;
		}
		contentDirty = false;
		lastPeriodicRedrawMs = now;
	}

	if (now - lastStatusBarMs >= kStatusBarIntervalMs) {
		lastStatusBarMs = now;
		// alert.extraCount > 0: weitere Kategorien sind ZUSAETZLICH zu
		// alert.source gerade auch ausserhalb der Spec, aber in der
		// schmalen Statusleiste passt nur eine Quelle - "+N" macht
		// wenigstens sichtbar, dass da noch mehr ist (Nutzerwunsch).
		String alertLabel;
		if (effectiveAlert.active) {
			alertLabel = String(effectiveAlert.source);
			if (effectiveAlert.extraCount > 0) {
				alertLabel += " +" + String(effectiveAlert.extraCount);
			}
		}
		statusBar.draw(display, wlan, sensor.hasValidReading(), sensor.temperatureC(),
		               sensor.humidityPercent(), TimeSync::formatTime(), TimeSync::formatDate(),
		               showBottomBar, bgColor, alertLabel, effectiveAlert.blue);
	}

	delay(20);
}
