#pragma once

#include <Arduino.h>
#include <WiFiUdp.h>

// Sehr schlanker SNMP-v1-GET-Client fuer einzelne skalare Werte (INTEGER
// oder OCTET STRING) pro Aufruf. Kein allgemeiner SNMP-Stack: die
// Sensormeter-Gegenstelle hat eine feste, bekannte OID-Struktur (siehe
// lastenheft.txt Abschnitt 7.3), eine volle SNMP-Bibliothek waere
// unverhaeltnismaessiger Aufwand fuer wenige Werte alle paar Sekunden.
// BER-Kodierung/-Dekodierung nur im Umfang, den diese feste Paketform
// braucht (kurze Laengen, keine Geschwister-Elemente auf oberster Ebene) -
// siehe docs/entscheidungen.md.
class SnmpClient {
public:
	// Liefert true und schreibt outValue, wenn innerhalb von timeoutMs eine
	// Antwort mit einem INTEGER-Wert eintraf.
	bool getInteger(const String &host, const String &community, const String &oidDotted,
	                 int32_t &outValue, uint16_t port = 161, uint32_t timeoutMs = 2000);

	// Wie getInteger(), aber fuer OCTET-STRING-Werte (z.B. Systemname/-typ,
	// Sensor-Bezeichnung).
	bool getString(const String &host, const String &community, const String &oidDotted,
	               String &outValue, uint16_t port = 161, uint32_t timeoutMs = 2000);

	// Wie getInteger(), aber fuer TimeTicks-Werte (ASN.1-Tag 0x43, eigener
	// SNMP-SMI-Typ, unsigned statt INTEGERs Zweierkomplement) - Gegenstelle
	// liefert die Uptime damit (siehe SensormeterManager.cpp, OID .5.1.0).
	bool getTimeTicks(const String &host, const String &community, const String &oidDotted,
	                   uint32_t &outValue, uint16_t port = 161, uint32_t timeoutMs = 2000);

private:
	// Gemeinsamer Kern von getInteger()/getString(): baut die GetRequest,
	// sendet sie, wartet auf Antwort und liefert den Rohinhalt des letzten
	// TLV-Elements mit passendem Tag (siehe .cpp fuer die Begruendung, warum
	// "letztes Element" hier ausreicht). outLen ist beim Aufruf die
	// Puffergroesse von outContent, danach die tatsaechliche Laenge.
	bool getRaw(const String &host, const String &community, const String &oidDotted, uint8_t wantTag,
	            uint8_t *outContent, size_t &outLen, uint16_t port, uint32_t timeoutMs);

	WiFiUDP udp;
};
