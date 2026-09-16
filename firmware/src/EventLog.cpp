#include "EventLog.h"

#include <LittleFS.h>

#include "TimeSync.h"

namespace {
constexpr size_t kMaxBytes = 64 * 1024;   // ab hier wird gekuerzt
constexpr size_t kKeepBytes = 32 * 1024;  // so viel Tail bleibt beim Kuerzen

// Kompakter Zeitstempel "DD.MM.JJJJ HH:MM"; ohne NTP-Sync die Laufzeit.
String timestamp() {
	if (TimeSync::isValid()) {
		String d = TimeSync::formatDate();  // "Wochentag, DD.MM.JJJJ"
		int c = d.indexOf(", ");
		if (c >= 0) d = d.substring(c + 2);
		return d + " " + TimeSync::formatTime();
	}
	return "Uptime " + String(static_cast<unsigned long>(millis() / 1000)) + "s";
}
} // namespace

void EventLog::begin() {
	ready_ = LittleFS.begin(true);
}

void EventLog::append(const String &line) {
	if (!ready_) return;

	// Groessenbegrenzung: ist die Datei zu gross, nur den letzten Teil (an
	// einer Zeilengrenze) behalten und neu schreiben, bevor angehaengt wird.
	fs::File rf = LittleFS.open(kPath, "r");
	if (rf) {
		if (rf.size() > kMaxBytes) {
			rf.seek(rf.size() - kKeepBytes);
			String tail = rf.readString();
			rf.close();
			int nl = tail.indexOf('\n');  // angeschnittene erste Zeile verwerfen
			if (nl >= 0) tail = tail.substring(nl + 1);
			fs::File wf = LittleFS.open(kPath, "w");
			if (wf) {
				wf.print(tail);
				wf.close();
			}
		} else {
			rf.close();
		}
	}

	fs::File f = LittleFS.open(kPath, "a");
	if (!f) return;
	f.print(timestamp());
	f.print(" | ");
	f.print(line);
	f.print("\n");
	f.close();
}
