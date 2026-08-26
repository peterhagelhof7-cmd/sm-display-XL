# Stückliste (BOM)

## Pro Gerät

| Bauteil | Menge | Hinweis |
|---|---|---|
| HW-458B (ESP-WROOM-32 + 2,8" TFT ST7789P3, resistiver Touch) | 1 | Hauptmodul, siehe `CBAA0055-008_DE.pdf` |
| DHT11, 3-Draht-Modul (mit eingebautem Pull-up) | 1 | Data → GPIO22 über Steckverbinder "Erweiterungsanschluss IO2", siehe `entscheidungen.md` |
| Verbindungskabel 4p 1,25 mm (liegt dem Board bei) | 1 | Für den Erweiterungsanschluss IO2 (GND/IO22/IO27/3,3V) |
| USB-Kabel (USB-C oder USB-C-zu-USB-C, liegt dem Board bei) | 1 | Stromversorgung + Flashen über USB-C oder USB-Micro |
| Netzteil 5V, ≥ 1 A (USB) | 1 | Herleitung siehe `stromversorgung.md` (Board-Spitzenstrom 300 mA @ 3,3V + Backlight bis 80 mA, umgerechnet auf die 5V-Schiene, plus Kabel-/Reserve-Puffer) |
| Gehäuse | 0–1 | Laut Datenblatt optional erhältlich ("mit/ohne Gehäuse"), nicht Teil dieses Repos |

## Im Lieferumfang des Boards enthalten (kein separater Kauf nötig)

| Teil | Hinweis |
|---|---|
| Eingabestift | Für den resistiven Touchscreen |
| Anschlusskabel | Für Erweiterungsanschlüsse IO1/IO2 bzw. Serial Port (4p 1,25 mm) |

## Werkzeug (einmalig, nicht pro Gerät)

| Werkzeug | Hinweis |
|---|---|
| CH340-Treiber (Windows) | Für die USB-Serial-Erkennung beim Flashen, siehe `CBAA0055-008_DE.pdf` Abschnitt "Gebrauchsanweisung" |

## Nicht Teil dieses Scopes

Laut Datenblatt vom Board unterstützt, aber im Lastenheft nicht gefordert und
daher nicht angeschafft/verdrahtet: TF-/SD-Karte, Lautsprecher (1,5 W/4 Ω),
Audio-Ausgang (GPIO26).

Einstellungs-Webserver und OTA-Update (lastenheft.txt Abschnitt 11) sind
reine Software-Funktionen ohne zusätzlichen Hardwarebedarf.
