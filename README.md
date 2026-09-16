# Sensormeter Display XL (ESP32-8048S070C, 7")

**▶ [Web-Flasher der Sensormeter-Familie](https://peterhagelhof7-cmd.github.io/sensormeter-family/)** — Firmware aller Geräte direkt im Browser flashen.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/projektfamilie-dark.png">
  <source media="(prefers-color-scheme: light)" srcset="docs/projektfamilie-light.png">
  <img alt="Sensormeter Projektfamilie: Sensormeter (LAN), Sensormeter WLAN (WLAN), Sensormeter PoE (LAN+PoE) und Sensormeter Display (Touchscreen), verbunden über gemeinsame Architektur und SNMP" src="docs/projektfamilie-light.png">
</picture>

Großes 7"-Touchdisplay-System auf Basis des **ESP32-8048S070C** (ESP32-S3, 7"
**800×480**-RGB-Parallel-Panel, kapazitiver **GT911**-Touch). Zeigt wahlweise
Innenraumklima (DHT11), Uhrzeit/Datum, Messwerte des
[Sensormeter](https://github.com/peterhagelhof7-cmd/sensormeter)-Projekts
(SNMP) oder Ping-Laufzeiten an. WLAN-Konfiguration und Bedienung direkt am
Gerät per Touch, keine Cloud-Anbindung. Zusätzlich ein öffentliches,
nicht passwortgeschütztes Status-Dashboard sowie ein passwortgeschützter
Einstellungs-Webserver (Systemname, Betriebsmodus, Ping-/Sensormeter-Ziele,
Warnschwellwerte, DHT11-Kalibrierkorrektur) inkl. lokalem OTA-Update per
`.bin`-Upload.

Dies ist die **XL-Variante** des
[Sensormeter Display](https://github.com/peterhagelhof7-cmd/sensormeter-display)
(2,8", HW-458B): identischer Funktionsumfang, größeres Display. Portiert wurde
die Display-Schicht von TFT_eSPI (nur SPI-Panels) auf **LovyanGFX**, das sowohl
das RGB-Parallel-Panel als auch den GT911 ansteuert.

## Dokumentation

| Datei | Inhalt |
|---|---|
| [docs/stueckliste.md](docs/stueckliste.md) | Bauteile pro Gerät + Werkzeug (XL) |
| [docs/stromversorgung.md](docs/stromversorgung.md) | Strombudget (7"-Panel + PSRAM) + Netzteilempfehlung |
| [docs/systemlast.md](docs/systemlast.md) | Flash/RAM (gemessen, ESP32-S3), Blockierzeit-Abschätzung |
| [docs/entscheidungen.md](docs/entscheidungen.md) | Entscheidungsprotokoll (inkl. XL-Portierungs-Notiz oben: LovyanGFX, GT911, PSRAM, native-USB-Konflikt) |
| [docs/ZABBIX.md](docs/ZABBIX.md) | Zabbix-Integration (agentenlose ICMP-Erreichbarkeitsüberwachung) |
| [docs/systemuebersicht.pdf](docs/systemuebersicht.pdf) | Familienweite Systemübersicht (identisch in allen Repos) |
| [docs/projektfamilie.html](docs/projektfamilie.html) | Architekturskizze: wie die Sensormeter-Projekte zusammenhängen |
| [docs/lastenheft.txt](docs/lastenheft.txt) · [docs/pflichtenheft.txt](docs/pflichtenheft.txt) | Anforderungen/Umsetzung (Basis 2,8"-Projekt; funktional identisch, Display-Schicht siehe XL-Notiz) |

> Hinweis: `admin-guide`, `verdrahtungsplan.html`/`verdrahtungsschema.*` und die
> One-Pager stammen noch aus dem 2,8"-Projekt und beschreiben dessen Verdrahtung
> (HW-458B). Für das XL-Board gilt die Pinbelegung unter **Hardware** bzw. in
> `firmware/include/LGFX_XL.h` / `firmware/include/pins.h`.

## Hardware

- **Board:** ESP32-8048S070C — ESP32-S3, **16 MB Flash + 8 MB Octal-PSRAM**
- **Display:** 7" 800×480 RGB-Parallel-Panel (16-bit), Ansteuerung über
  LovyanGFX (`Bus_RGB` + `Panel_RGB`), Timings/Pins in
  `firmware/include/LGFX_XL.h`
- **Touch:** GT911 kapazitiv über I2C — SDA `GPIO19`, SCL `GPIO20`,
  RST `GPIO38`, **kein INT-Pin** (Polling, damit `GPIO18` für den DHT frei
  bleibt), I2C-Adresse `0x14` (alternativ `0x5D`)
- **Hintergrundbeleuchtung:** `GPIO2` (aktiv HIGH), per LEDC-PWM vom
  `BacklightManager`
- **DHT11:** `GPIO18` am P4-Erweiterungsanschluss
- **Keine RGB-Status-LED** (auf dem XL-Board entfallen; der Alarm bleibt über
  die rote/blaue Bildschirmfärbung sichtbar)
- **Serielle Konsole über UART0** (`GPIO43`/`44`, CH340). **Native USB ist
  bewusst deaktiviert**, weil der GT911 auf `GPIO19`/`20` liegt — das sind die
  nativen USB-D-/D+-Pins des S3; native USB einzuschalten kollidiert mit dem
  Touch (sofortiger Reboot-Loop).

## Firmware

`firmware/` ist ein PlatformIO-Projekt — Board-Env **`esp32-8048S070C`**
(`esp32-s3-devkitc-1`, `qio_opi` = QIO-Flash + Octal-PSRAM), Framework Arduino.

**Version:** `1.0.0` — Teil des einheitlichen Familien-Releases.

Fertiges Binary für das lokale OTA-Update (kein PlatformIO nötig):
[Releases → v1.0.0](https://github.com/peterhagelhof7-cmd/sm-display-XL/releases/tag/v1.0.0).
Ein **leeres** Board bespielst du am einfachsten über den
[Web-Flasher](https://peterhagelhof7-cmd.github.io/sensormeter-family/)
(Kachel „Sensormeter Display XL — 7"").

```
cd firmware
pio run                     # bauen
pio run --target upload     # flashen (über den CH340/UART0)
pio device monitor          # seriellen Log ansehen (115200 Baud)
```

**Partitionstabelle:** `firmware/partitions_ota_16m.csv` (16 MB, zwei App-Slots
à 4 MB für OTA + SPIFFS/LittleFS + coredump). Ein Board, das zuvor mit einer
anderen Tabelle geflasht wurde, braucht vorher `pio run --target erase`.

Funktionsumfang (identisch zum 2,8"-Projekt, nur Display-/Touch-Schicht anders):
- **LovyanGFX-Ansteuerung** des 800×480-RGB-Panels (natives Querformat,
  Rotation 0) — ersetzt die TFT_eSPI/ST7789-Schicht des 2,8"-Boards
- **GT911-Kapazitiv-Touch** über LovyanGFX (Polling, keine 2-Punkt-
  Kalibrierung nötig wie beim resistiven 2,8"-Panel)
- WLAN-Ersteinrichtung (`WlanManager`, `WifiOnboarding`): Scan mit
  „Aktualisieren"-Button, Netzliste mit Empfangsbalken, Bildschirmtastatur zur
  PSK-Eingabe, Speicherung in NVS, automatischer Verbindungsaufbau
- Statusleiste (`StatusBar`): Zahnrad (öffnet Einstellungen), WLAN-Balken,
  DHT11-Werte oben; Uhrzeit/Datum unten; bildschirmweite rote/blaue Färbung bei
  Warnung (mit Hysterese/Entprellung, siehe `docs/entscheidungen.md`)
- NTP-Zeit (`TimeSync`): de.pool.ntp.org, deutsche Zeitzone inkl. Sommerzeit
- DHT11-Datenquelle (`SensorManager`, `GraphManager`): Abfrage alle 5s,
  Verlaufsgraph (12h) mit Ringpuffer-Persistenz auf LittleFS
- Uhrzeit-Datenquelle (`ClockView`): große 7-Segment-Uhr + Datum
- Sensormeter-Datenquelle (`SensormeterView`): schlanker SNMP-v1-GET-Client,
  Temperatur/Luftfeuchte des Sensormeter-Projekts
- Ping-Datenquellen (`PingManager`, `PingView`): Durchschnitt zu google.com +
  bis zu 5 weitere Ziele; Warnung bei anhaltendem Ausfall/Latenzüberschreitung
- Warn-Ereignisprotokoll (`EventLog`, `/events.txt` auf LittleFS, per Web
  herunterladbar) mit konkretem Auslöser (Ping-Ziel/Latenz, Sensor, Schwellwert)
- Snake (`SnakeGame`): Touch-Steuerung, Highscore in NVS
- Einstellungen (`SettingsUI`, `SettingsManager`, `BacklightManager`):
  Slide/Static/Snake/Systemeinstellungen (Helligkeit, WLAN, Sensormeter-/Ping-
  Ziele über `NumericKeypad`) — in NVS persistiert
- Webserver (`WebServerManager`, async): öffentliches Status-Dashboard (`/`) +
  passwortgeschützter Einstellungsbereich (`/settings`) inkl. OTA-`.bin`-Upload
- Anbieter-Branding (`BrandingManager`, `BrandingView`): Anbietername + Logo
- Werksreset mit wählbarem Umfang; Serial-Kommandozeile (`dhcp`, `ip`, `wifi`,
  `status`, `reset[ all]`) über UART0

**Stand:** Portierung auf das XL-Panel läuft — Build grün (Flash ~27 %, RAM
~28 %), Firmware v1.0.0. UI-Skalierung auf 800×480, Events-Download und
Alarm-Hysterese umgesetzt. HW-Tests am realen Panel stehen noch aus.

## Zusammenhang mit dem Sensormeter-Projekt

Die Datenquelle „Sensormeter" fragt das separate
[Sensormeter](https://github.com/peterhagelhof7-cmd/sensormeter)-Projekt
(WT32-ETH01) per SNMP v1 ab (`.1.3.6.1.4.1.99999.x`). Dieselbe OID-Basis gilt
für [Sensormeter PoE](https://github.com/peterhagelhof7-cmd/sensormeter-poe) und
[Sensormeter WLAN Lite](https://github.com/peterhagelhof7-cmd/sensormeter-wlan-lite)
— alle lassen sich ohne Codeänderung hier abfragen.

## Über dieses Projekt

Repo-Struktur und Dokumentation entstehen in Zusammenarbeit mit
[Claude](https://claude.com/claude-code) (Anthropic) als KI-Coding-Assistent.
