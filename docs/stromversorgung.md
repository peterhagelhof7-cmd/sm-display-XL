# Strombedarf & Stromversorgung — Sensormeter Display XL

Strombudget für ein Sensormeter-Display-XL-Gerät (ESP32-8048S070C), damit ein
passendes Netzteil gewählt werden kann.

> **Hinweis:** Für dieses Board liegt (anders als beim 2,8"-HW-458B) kein
> herstellerseitiges Strom-Datenblatt vor. Die folgenden Werte sind **Schätzungen
> für diese Board-Klasse** (7"-RGB-Panel + ESP32-S3 + Octal-PSRAM), nicht
> gemessen. Vor Serieneinsatz am realen Board nachmessen.

## Strombudget pro Komponente (5V-USB-Eingang, geschätzt)

| Komponente | Ø-Strom (geschätzt) | Spitze (geschätzt) | Anmerkung |
|---|---|---|---|
| 7"-Backlight (LED-Hinterleuchtung) | ~250–450 mA | ~500 mA | Größter Verbraucher; per LEDC-PWM (`BacklightManager`) gedimmt — Default < 100 %, daher meist unterhalb der Spitze |
| ESP32-S3 + RGB-Panel-Treiber + PSRAM | ~150–250 mA | ~500 mA | WLAN-Sendebursts (Scan/Verbindung), kontinuierliches RGB-Refresh des 800×480-Framebuffers aus dem PSRAM, Flash-Schreibzugriffe |
| DHT11 (P4-Anschluss) | ~0,3 mA | ~2,5 mA | 5s-Abfragetakt, sonst Standby |

Keine RGB-Status-LED auf diesem Board (entfällt gegenüber dem 2,8"-Gerät).

## Gesamtbedarf pro Gerät (5V-Schiene, geschätzt)

| Szenario | Ø-Strom | Spitze |
|---|---|---|
| Normalbetrieb (Backlight gedimmt) | ~450–700 mA | ~900 mA–1 A |
| Volle Helligkeit + WLAN-Burst | — | bis ~1 A |

Die Spitze wird vom Backlight (bei hoher Helligkeit) und den WLAN-Sendebursts
des ESP32-S3 bestimmt; das kontinuierliche RGB-Panel-Refresh sorgt für eine
höhere Grundlast als beim SPI-Panel des 2,8"-Boards.

## Empfohlene Stromversorgung

**5V-USB-Netzteil, mindestens 2 A (2000 mA).**

Begründung:
- Das 7"-Backlight allein zieht ein Mehrfaches der 2,8"-Variante; zusammen mit
  ESP32-S3/PSRAM und WLAN-Spitzen ist 1 A zu knapp.
- 2 A bietet Reserve gegen Spannungsabfall über dünne/billige USB-Kabel.
- Ein ausreichend dickes USB-Kabel ist wichtiger als die reine Nennleistung —
  dünne Kabel erzeugen bei Backlight-/WLAN-Stromspitzen spürbaren Spannungsabfall.

Nicht verwenden: den Ausgang eines USB-Seriell-Adapters oder einen reinen
Daten-USB-Port (oft auf 500 mA begrenzt — für dieses Board deutlich zu wenig).

## Nicht Teil dieser Schätzung

Deep-Sleep/Energiesparbetrieb ist für dieses Gerät nicht relevant — es läuft
dauerhaft im aktiven Anzeigebetrieb (Display-Energiesparen ist bewusst nicht
Teil des Scopes).
