# Strombedarf & Stromversorgung

Strombudget für ein Sensormeter-Display-Gerät (HW-458B), damit ein
passendes Netzteil gewählt werden kann. Unterschieden wird zwischen
**Durchschnitt** (Dauerlast/Wärmeentwicklung) und **Spitze** (wie kräftig
das Netzteil kurzzeitig sein muss, damit die Spannung nicht einbricht).

## Strombudget pro Komponente (bei 3,3V, Board-interne Schiene)

| Komponente | Ø-Strom | Spitzenstrom | Quelle |
|---|---|---|---|
| HW-458B Hauptmodul (ESP-WROOM-32 + TFT-Controller) | ~80–120 mA | 300 mA | Herstellerdatenblatt `CBAA0055-008_DE.pdf`: "Stromaufnahme: 300mA bei 3,3V" (WLAN-Sendebursts/Flash-Schreibzugriffe); Deep-Sleep-Wert (6,5µA) laut Datenblatt hier nicht relevant, da das Gerät dauerhaft aktiv betrieben wird |
| TFT-Hintergrundbeleuchtung (4 weiße LEDs) | 40–80 mA | 80 mA | Herstellerdatenblatt: "V_LED=3,2V; I_LED=80mA (max.)"; per PWM (`BacklightManager`) auf 60% Default gedimmt → ~48 mA typisch, 80 mA bei 100% Helligkeit |
| DHT11 (Erweiterungsanschluss IO2) | ~0,3 mA | ~2,5 mA | Standard-Datenblattwerte; 5s-Abfragetakt (lastenheft.txt Abschnitt 8), DHT liegt die meiste Zeit im Standby, Spitze nur während der ~20ms-Messung |
| RGB-Status-LED (nur bei Ping-Alarm aktiv) | 0 mA im Normalbetrieb | ~20 mA | Nur ein Farbkanal (rot) blinkend bei anhaltendem Ping-Fehler, sonst aus |

## Gesamtbedarf pro Gerät (3,3V-Schiene)

| Szenario | Ø-Strom | Spitzenstrom |
|---|---|---|
| Normalbetrieb (Helligkeit 60%, kein Ping-Alarm) | ~130–170 mA | ~385 mA |
| Ping-Alarm aktiv (LED blinkt zusätzlich) | ~130–170 mA | ~405 mA |

Der Spitzenwert wird praktisch komplett vom Hauptmodul selbst bestimmt
(WLAN-Sendebursts, insbesondere durch WLAN-Scan/Verbindungsaufbau,
NVS-Schreibzugriffe, SNMP-/Ping-Anfragen, OTA-Flash-Schreibvorgänge) plus
der Hintergrundbeleuchtung bei hoher Helligkeitseinstellung.

## Umrechnung auf die 5V-USB-Versorgung

Die 3,3V-Schiene wird board-intern aus der 5V-USB-Versorgung erzeugt
(USB-C oder USB-Micro, siehe Datenblatt). Der genaue Wandlerwirkungsgrad
ist im Datenblatt nicht spezifiziert; bei einer angenommenen Effizienz von
70–85% (typischer Bereich für LDO bis einfachen Schaltregler) ergibt sich
am 5V-Eingang ein Spitzenstrom von etwa **300–360 mA**.

## Empfohlene Stromversorgung

**5V-USB-Netzteil, mindestens 1A (1000 mA).**

Begründung:
- Deckt den berechneten 5V-Spitzenstrom (~300–360 mA) mit deutlicher
  Reserve gegen Spannungsabfall durch dünne/billige USB-Kabel ab.
- Gleiche Empfehlung wie beim Sensormeter-Projekt (WT32-ETH01) - ein
  handelsübliches 5V/1A-USB-Netzteil (Ladegerät) genügt, kein
  spezialisiertes Netzteil nötig.
- Ein gutes, ausreichend dickes USB-Kabel ist wichtiger als die reine
  Netzteil-Nennleistung - dünne Kabel erzeugen bei den kurzen
  WLAN-/Backlight-Stromspitzen spürbaren Spannungsabfall.

Nicht verwenden: der Ausgang eines USB-Seriell-Adapters (zu schwach) oder
ein reiner Daten-USB-Port ohne Ladefunktion (kann auf 500 mA begrenzt sein
und wäre bei Spitzenlast knapp).

## Nicht Teil dieser Schätzung

Deep-Sleep/Energiesparbetrieb (Datenblatt nennt 6,5µA) ist für dieses
Gerät nicht relevant, da laut lastenheft.txt Abschnitt 12 Display-
Energiesparen/automatische Abschaltung bewusst nicht Teil des Scopes ist -
das Gerät läuft dauerhaft im aktiven Anzeigebetrieb.
