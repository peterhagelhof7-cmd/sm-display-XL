#pragma once

#include <Arduino.h>

// Minimale NTP-Anbindung, vorgezogen aus P4 (Uhrzeit-Datenquelle): die
// Statusleiste (P2) braucht bereits eine Uhrzeit, und der volle
// TimeManager (Formatierung fuer die grosse Uhrzeit-Datenquellen-Ansicht)
// baut in P4 direkt auf diesen Funktionen auf statt sie zu ersetzen.
namespace TimeSync {

// de.pool.ntp.org, deutsche Zeitzone inkl. automatischer Sommerzeit
// (POSIX-TZ-String: CET-1CEST, Umstellung letzter Sonntag Maerz/Oktober).
void begin();

// true, sobald ein plausibles Datum vorliegt (Jahr > 2020) - vor dem
// ersten erfolgreichen NTP-Sync liefert das System sonst 1.1.1970.
bool isValid();

// "HH:MM", leerer String falls !isValid().
String formatTime();

// "Wochentag, DD.MM.JJJJ" (deutsch), leerer String falls !isValid().
String formatDate();

// "DD.MM.YY HH:MM" des letzten erfolgreichen NTP-Sync-Ereignisses (nicht
// der aktuellen Systemzeit!) - ueber den SNTP-Callback erfasst, siehe
// begin(). Leerer String, falls noch nie erfolgreich synchronisiert.
String formatLastSync();

} // namespace TimeSync
