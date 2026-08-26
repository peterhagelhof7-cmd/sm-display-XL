#include "TimeSync.h"

#include <time.h>

#include "esp_sntp.h"

namespace {
const char *kWeekdaysDe[7] = {"Sonntag", "Montag", "Dienstag", "Mittwoch",
                              "Donnerstag", "Freitag", "Samstag"};

// Vom SNTP-Callback gesetzt (siehe begin()) - 0 = noch nie erfolgreich
// synchronisiert. Getrennt von der laufenden Systemzeit, da diese
// zwischen zwei Sync-Ereignissen weiterlaeuft (per RTC/millis()) und daher
// selbst kein Indikator dafuer ist, WANN zuletzt tatsaechlich per NTP
// synchronisiert wurde.
time_t lastSyncTs = 0;

void onSntpSync(struct timeval *tv) {
	lastSyncTs = tv->tv_sec;
}
}

void TimeSync::begin() {
	sntp_set_time_sync_notification_cb(onSntpSync);
	// M3.5.0 = letzter Sonntag im Maerz (Sommerzeit-Beginn),
	// M10.5.0/3 = letzter Sonntag im Oktober, 3 Uhr (Sommerzeit-Ende).
	configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "de.pool.ntp.org");
}

bool TimeSync::isValid() {
	struct tm t;
	if (!getLocalTime(&t, 10)) {
		return false;
	}
	return (t.tm_year + 1900) > 2020;
}

String TimeSync::formatTime() {
	struct tm t;
	if (!getLocalTime(&t, 10) || (t.tm_year + 1900) <= 2020) {
		return "";
	}
	char buf[6];
	snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
	return String(buf);
}

String TimeSync::formatDate() {
	struct tm t;
	if (!getLocalTime(&t, 10) || (t.tm_year + 1900) <= 2020) {
		return "";
	}
	char buf[32];
	snprintf(buf, sizeof(buf), "%s, %02d.%02d.%04d", kWeekdaysDe[t.tm_wday], t.tm_mday,
	         t.tm_mon + 1, t.tm_year + 1900);
	return String(buf);
}

String TimeSync::formatLastSync() {
	if (lastSyncTs == 0) {
		return "";
	}
	struct tm t;
	localtime_r(&lastSyncTs, &t);
	char buf[18];
	snprintf(buf, sizeof(buf), "%02d.%02d.%02d %02d:%02d", t.tm_mday, t.tm_mon + 1, (t.tm_year + 1900) % 100,
	         t.tm_hour, t.tm_min);
	return String(buf);
}
