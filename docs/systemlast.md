# Systemlast (Flash, RAM, Zeitbudget)

Zwei Datenquellen, klar getrennt nach Belastbarkeit:

1. **Gemessen** – reale `pio run`-Build-Ausgaben (Flash/RAM), festgehalten
   bei jeder Phase, sowie die tatsächlich generierte Partitionstabelle
   (`gen_esp32part.py` gegen `partitions.bin`, nicht nur angenommen).
2. **Abgeschätzt** – Blockierzeit im Hauptloop pro Vorgang, hergeleitet aus
   bekannten Protokoll-/Bibliotheks-Timings (DHT11-Protokoll, SNMP-/
   Ping-Timeouts). Keine Messung auf echter Hardware.

## 1. Flash/RAM-Wachstum je Phase (gemessen)

| Phase | Flash | RAM | Zuwachs Flash | Anlass |
|---|---|---|---|---|
| P0 | 25,2 % (329.781 B) | 6,7 % (21.964 B) | – | Display-/Touch-Grundgerüst |
| P1 | 61,0 % (799.477 B) | 13,4 % (43.948 B) | +469.696 B | WLAN-Ersteinrichtung (WiFi-Stack erstmals gelinkt) |
| P2/P3 | 66,1 % (866.093 B) | 14,0 % (45.792 B) | +66.616 B | Statusleiste, DHT11-Datenquelle, NTP (TimeSync) |
| P4/P5 | 66,9 % (877.001 B) | 14,0 % (45.952 B) | +10.908 B | Uhrzeit-Ansicht, Einstellungen-UI (mehrere Screens) |
| P7/P8 | 68,2 % (893.741 B) | 14,3 % (46.704 B) | +16.740 B | SNMP-Client, Ping-Überwachung, LED |
| P6 | 68,4 % (896.165 B) | 14,3 % (46.840 B) | +2.424 B | Snake (keine neue Bibliothek nötig) |
| Webserver+OTA | 73,3 % (960.453 B) | 14,4 % (47.240 B) | +64.288 B | ESPAsyncWebServer/AsyncTCP, OTA, eigene Partitionstabelle |
| Warnschwellwerte | 75,1 % (984.085 B) | 14,7 % (48.048 B) | +23.632 B | Blink-Alarm (rot/blau), Graph-Referenzlinien, Webserver-Schwellwerttabelle (pro Sensor bei PRO-Zielen) |
| Öffentliches Dashboard | 75,8 % (993.069 B) | 14,7 % (48.056 B) | +8.984 B | Zweite Webseite (Status-Dashboard, kein Login), Design aus Admin-Guide übernommen |
| Dashboard-Feinschliff | 76,2 % (998.385 B) | 14,7 % (48.064 B) | +5.316 B | SVG-Verlaufsgraph, Ziel-IP änderbar, NTP-Sync-Anzeige, Rahmenbreite vereinheitlicht |

**Reserve (Stand Dashboard-Feinschliff):** 1.310.720 − 998.385 = **312.335
Byte Flash frei je App-Slot (23,8 %)**, 279.616 von 327.680 Byte statisches
RAM frei (85,3 %) → weiterhin viel Luft für Heap/Stack zur Laufzeit
(WiFi/TCP-Puffer, TFT_eSPI-Textpuffer). Alle Werte stammen direkt aus
`pio run`, keine Schätzung. Flash-Reserve bezieht sich auf je einen der
beiden OTA-App-Slots (`ota_0`/`ota_1`), siehe `partitions_ota.csv` und
`docs/entscheidungen.md`.

Auffällig: P1 (WLAN) kostet mit Abstand am meisten Flash (WiFi/LWIP-Stack
wird hier erstmals gelinkt) - alle folgenden netzwerkbezogenen Phasen
(SNMP-Client, Ping, Webserver) bauen auf demselben bereits vorhandenen
Stack auf und kosten dadurch vergleichsweise wenig zusätzlich.

## 2. Blockierzeit im Hauptloop pro Vorgang (abgeschätzt)

Der Hauptloop läuft mit `delay(20)` am Ende jedes Durchlaufs, dazwischen
werden Touch, Sensor-Polling, Ping, Redraws und (blockierend, solange
geöffnet) Settings-UI/Snake/Onboarding abgearbeitet.

| Vorgang | Kosten | Takt | Ø-Blockierzeit |
|---|---|---|---|
| Touch-Bit-Bang-Read (unberührt) | < 0,1 ms | jeder Durchlauf (~20 ms) | < 0,5 % |
| DHT11-Auslesung (Protokoll-Timing) | ~20–25 ms | alle 5 s | ~0,4–0,5 % |
| Statusleisten-Redraw (kleine SPI-Schreibzugriffe) | < 5 ms | alle 300 ms | ~1,5 % |
| Sensormeter-SNMP-Abfrage (Erfolg, LAN-Antwortzeit) | ~10–50 ms | alle 30 s (falls konfiguriert) | < 0,2 % |
| Sensormeter-SNMP-Abfrage (Timeout, kein Gerät erreichbar) | bis 2.000 ms | alle 30 s | bis ~6,7 % |
| Ping google.com + 1 Zusatzziel (beide erfolgreich) | wenige ms bis ~100 ms | alle 2 s | < 5 % |

**Ø-Last im Normalbetrieb (alles erreichbar): geschätzt niedrig
einstellig-Prozent** – der Hauptloop verbringt die meiste Zeit im
`delay(20)`.

### Identifiziertes Risiko: Ping-Timeouts können den Hauptloop bei Ausfall dominieren

`PingManager::update()` ruft pro Zyklus (alle 2 s) bis zu zwei blockierende
`Ping.ping(...)`-Aufrufe auf (google.com + ein Zusatzziel, round-robin -
siehe `docs/entscheidungen.md`). Die verwendete Bibliothek
(`marian-craciunescu/ESP32Ping`) legt den Timeout pro Ping-Versuch intern
fest, ohne ihn als Parameter zu exponieren. Bei anhaltendem Netzwerkausfall
(WLAN weg oder Zielgeräte nicht erreichbar) kann ein einzelner
`update()`-Aufruf dadurch nahe an die vollen 2 s dauern - bei einem
2-Sekunden-Takt im ungünstigsten Fall **nahe 100 % der Zeit zwischen zwei
Ping-Zyklen**, während der Hauptloop nicht auf Touch reagieren kann.

Das ist keine CPU-Auslastung im eigentlichen Sinn (der Prozessor blockiert
in einer Warteschleife mit `delay()`, die an FreeRTOS abgibt), sondern ein
**Touch-Reaktionsrisiko**: Bei WLAN-Ausfall oder unerreichbaren Ping-Zielen
könnte die Bedienung des Displays für kurze Zeitfenster spürbar
verzögert wirken. lastenheft.txt schließt explizite Zeitvorgaben
(Touch-Reaktion < 100 ms) zwar ausdrücklich aus dem Scope aus (Abschnitt
12), das Risiko ist aber real genug, um es hier festzuhalten. Mögliche
spätere Abhilfe (nicht umgesetzt, da außerhalb des aktuellen Auftrags):
eigener, nicht-blockierender ICMP-Client mit kurzem, konfigurierbarem
Timeout statt der aktuellen Bibliothek, oder Ping-Prüfung in einen
separaten FreeRTOS-Task auslagern.

## Fazit

Flash- und RAM-Reserve sind komfortabel (26,7 % bzw. 85,6 % frei). Die
Haupt-Unsicherheit ist nicht Speicherplatz, sondern das oben beschriebene
Touch-Reaktionsrisiko bei Netzwerkausfällen durch blockierende
Ping-Timeouts - ein guter Kandidat für einen realen Hardware-Test
(WLAN währenddessen deaktivieren, Touch-Reaktion beobachten), sobald ein
Board zur Verfügung steht.
