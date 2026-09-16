# Systemlast (Flash, RAM, Zeitbudget) — Sensormeter Display XL

## 1. Flash/RAM (gemessen, v1.0.0)

Reale `pio run`-Ausgabe des aktuellen Builds (Env `esp32-8048S070C`,
ESP32-S3, 16 MB Flash / 8 MB Octal-PSRAM):

| Ressource | Belegt | Gesamt | Frei |
|---|---|---|---|
| **Flash (App-Slot)** | ~26,7 % (~1.119.000 B) | 4.194.304 B (`ota_0`/`ota_1`, je 4 MB) | ~73,3 % |
| **Interner RAM (statisch)** | ~28,1 % (~92.200 B) | 327.680 B | ~71,9 % |
| **PSRAM** | Framebuffer + große Puffer | 8 MB (Octal) | reichlich |

Wesentlich für die niedrige interne RAM-Belegung: Der 800×480-RGB-Framebuffer
(800 × 480 × 2 B ≈ **768 KB**) liegt im **PSRAM**, nicht im internen RAM
(`LGFX_USE_V1` + `BOARD_HAS_PSRAM`). Dadurch bleibt trotz des großen Panels viel
interner RAM für WiFi/TCP-Puffer, Heap und Stacks frei. Partitionslayout siehe
`firmware/partitions_ota_16m.csv` (zwei 4-MB-App-Slots für OTA).

Der WiFi/LWIP-Stack ist der mit Abstand größte einzelne Flash-Posten; alle
netzwerkbezogenen Funktionen (SNMP-Client, Ping, Webserver, OTA) bauen darauf
auf und kosten vergleichsweise wenig zusätzlich.

## 2. Blockierzeit im Hauptloop pro Vorgang (abgeschätzt)

Der Hauptloop arbeitet pro Durchlauf Touch, Sensor-Polling, Ping, Redraws und
(blockierend, solange geöffnet) Settings-UI/Snake/Onboarding ab.

| Vorgang | Kosten | Takt | Ø-Blockierzeit |
|---|---|---|---|
| GT911-Touch-Read (I2C-Polling) | < 1 ms | jeder Durchlauf | gering |
| DHT11-Auslesung (Protokoll-Timing) | ~20–25 ms | alle 5 s | ~0,4–0,5 % |
| Statusleisten-/View-Redraw (RGB-Panel via PSRAM-Framebuffer) | wenige ms | ereignis-/taktgesteuert | gering |
| Sensormeter-SNMP-Abfrage (Erfolg, LAN) | ~10–50 ms | alle 30 s | < 0,2 % |
| Sensormeter-SNMP-Abfrage (Timeout) | bis 2.000 ms | alle 30 s | bis ~6,7 % |
| Ping google.com + 1 Zusatzziel (Erfolg) | wenige ms bis ~100 ms | alle 2 s | < 5 % |

### Identifiziertes Risiko: Ping-Timeouts können den Hauptloop bei Ausfall dominieren

`PingManager::update()` ruft pro Zyklus (alle 2 s) bis zu zwei blockierende
`Ping.ping(...)`-Aufrufe auf (google.com + ein Zusatzziel, round-robin). Die
Bibliothek (`marian-craciunescu/ESP32Ping`) exponiert keinen Timeout-Parameter.
Bei anhaltendem Netzwerkausfall kann ein einzelner `update()`-Aufruf nahe an die
vollen 2 s dauern — ein **Touch-Reaktionsrisiko** (kurze Verzögerungen bei der
Bedienung), keine echte CPU-Dauerlast. Mögliche spätere Abhilfe: eigener
nicht-blockierender ICMP-Client mit kurzem Timeout, oder Ping in einen eigenen
FreeRTOS-Task auslagern.

## Fazit

Flash- und RAM-Reserven sind komfortabel; der große Framebuffer liegt im PSRAM.
Die Haupt-Unsicherheit ist nicht Speicher, sondern das Touch-Reaktionsrisiko bei
Netzwerkausfällen — ein guter Kandidat für einen realen Hardware-Test, sobald ein
Panel zur Verfügung steht.
