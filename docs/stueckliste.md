# Stückliste (BOM) — Sensormeter Display XL

## Pro Gerät

| Bauteil | Menge | Hinweis |
|---|---|---|
| ESP32-8048S070C (ESP32-S3 + 7" 800×480 RGB-Panel, kapazitiver GT911-Touch) | 1 | Hauptmodul; 16 MB Flash + 8 MB Octal-PSRAM. Pinbelegung Display/Touch siehe `firmware/include/LGFX_XL.h` |
| DHT11, 3-Draht-Modul (mit eingebautem Pull-up) | 1 | Data → `GPIO18` am P4-Erweiterungsanschluss, siehe `firmware/include/pins.h` |
| Verbindungskabel (JST, passend zum P4-Anschluss) | 1 | Für den DHT-Erweiterungsanschluss |
| USB-Kabel | 1 | Stromversorgung + Flashen (CH340/UART0 auf dem Board) |
| Netzteil 5V, **≥ 2 A** (USB) | 1 | Herleitung siehe `stromversorgung.md` — das 7"-Backlight + ESP32-S3/PSRAM ziehen deutlich mehr als das 2,8"-Board |
| Gehäuse | 0–1 | optional, nicht Teil dieses Repos |

## Werkzeug (einmalig, nicht pro Gerät)

| Werkzeug | Hinweis |
|---|---|
| CH340-Treiber (Windows) | Für die USB-Serial-Erkennung beim Flashen (Konsole/Upload laufen über UART0/CH340, **nicht** über die native USB-Buchse) |

## Hinweise zur Hardware

- **Kapazitiver Touch (GT911):** kein Eingabestift nötig (anders als der
  resistive Touch des 2,8"-Boards) und keine 2-Punkt-Kalibrierung.
- **Keine RGB-Status-LED** auf diesem Board — Warnungen werden über die
  rote/blaue Bildschirmfärbung angezeigt.
- **Native USB nicht nutzbar:** Der GT911 belegt `GPIO19`/`20` (die nativen
  USB-D-/D+-Pins des S3). Flashen/Konsole daher ausschließlich über UART0/CH340.

## Nicht Teil dieses Scopes

Auf dem Board vorhandene, aber nicht genutzte Peripherie (z. B. microSD-Slot,
Audio-Ausgang) ist im Lastenheft nicht gefordert und daher nicht verdrahtet.
Einstellungs-Webserver und OTA-Update sind reine Software-Funktionen ohne
zusätzlichen Hardwarebedarf.
