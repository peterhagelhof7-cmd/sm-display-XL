#include "WifiOnboarding.h"

#include <cctype>
#include <cstring>

#include "UiHelpers.h"

using UiHelpers::hitRect;
using UiHelpers::waitForTapEvent;

namespace {

constexpr int16_t kScreenW = DisplayManager::kScreenWidth;
constexpr int16_t kScreenH = DisplayManager::kScreenHeight;

constexpr int16_t kRefreshX = kScreenW - 90;
constexpr int16_t kRefreshY = 4;
constexpr int16_t kRefreshW = 86;
constexpr int16_t kRefreshH = 28;

constexpr int16_t kRowStartY = 40;
constexpr int16_t kRowHeight = 32;

void drawNetworkList(DisplayManager &display, const std::vector<WlanManager::NetworkInfo> &networks) {
	TFT_eSPI &tft = display.raw();
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	tft.setTextDatum(TL_DATUM);

	tft.setTextFont(4);
	tft.drawString("WLAN waehlen", 10, 6);

	tft.drawRect(kRefreshX, kRefreshY, kRefreshW, kRefreshH, TFT_BLACK);
	tft.setTextFont(2);
	tft.setTextDatum(MC_DATUM);
	tft.drawString("Aktualisieren", kRefreshX + kRefreshW / 2, kRefreshY + kRefreshH / 2);
	tft.setTextDatum(TL_DATUM);

	if (networks.empty()) {
		tft.drawString("Keine WLANs gefunden.", 10, kRowStartY + 4);
		return;
	}

	for (size_t i = 0; i < networks.size(); i++) {
		int16_t y = kRowStartY + static_cast<int16_t>(i) * kRowHeight;
		tft.drawRect(10, y, kScreenW - 20, kRowHeight - 4, TFT_BLACK);

		String label = networks[i].ssid;
		if (!networks[i].secured) {
			label += "  (offen)";
		}
		tft.drawString(label, 18, y + 8);

		// Empfangsbalken rechts in der Zeile (1-3 Balken nach RSSI).
		int8_t bars = 1;
		if (networks[i].rssi >= -60) bars = 3;
		else if (networks[i].rssi >= -75) bars = 2;
		int16_t bx = kScreenW - 50;
		for (int8_t b = 0; b < 3; b++) {
			int16_t bh = 6 + b * 5;
			int16_t by = y + (kRowHeight - 4) - bh - 4;
			uint16_t color = (b < bars) ? TFT_BLACK : TFT_LIGHTGREY;
			tft.fillRect(bx + b * 8, by, 6, bh, color);
		}
	}
}

int hitTestNetworkRow(int16_t x, int16_t y, size_t count) {
	if (x < 10 || x > kScreenW - 10) {
		return -1;
	}
	for (size_t i = 0; i < count; i++) {
		int16_t rowY = kRowStartY + static_cast<int16_t>(i) * kRowHeight;
		if (y >= rowY && y < rowY + (kRowHeight - 4)) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

// --- Bildschirmtastatur -----------------------------------------------

struct Key {
	const char *label; // Anzeige- und Grundwert; Sonderfunktionen per Name erkannt
	uint8_t weight;    // relative Breite in der Zeile
};

constexpr uint8_t kNumRowWeights = 12; // 10 Ziffern (1) + Backspace (2)
const Key kRowDigits[] = {
	{"1", 1}, {"2", 1}, {"3", 1}, {"4", 1}, {"5", 1},
	{"6", 1}, {"7", 1}, {"8", 1}, {"9", 1}, {"0", 1},
	{"<-", 2},
};

const Key kRowLettersTop[] = {
	{"q", 1}, {"w", 1}, {"e", 1}, {"r", 1}, {"t", 1},
	{"z", 1}, {"u", 1}, {"i", 1}, {"o", 1}, {"p", 1},
};

const Key kRowLettersMid[] = {
	{"CAPS", 2},
	{"a", 1}, {"s", 1}, {"d", 1}, {"f", 1}, {"g", 1},
	{"h", 1}, {"j", 1}, {"k", 1}, {"l", 1},
	{"OK", 2},
};

const Key kRowLettersBottom[] = {
	{"SYM", 2},
	{"y", 1}, {"x", 1}, {"c", 1}, {"v", 1}, {"b", 1}, {"n", 1}, {"m", 1},
	{"LEER", 4},
};

const Key kRowSymbolsTop[] = {
	{"!", 1}, {"@", 1}, {"#", 1}, {"$", 1}, {"%", 1},
	{"^", 1}, {"&", 1}, {"*", 1}, {"(", 1}, {")", 1},
};

const Key kRowSymbolsMid[] = {
	{"CAPS", 2},
	{"-", 1}, {"_", 1}, {"=", 1}, {"+", 1}, {".", 1},
	{",", 1}, {"?", 1}, {"/", 1}, {":", 1},
	{"OK", 2},
};

const Key kRowSymbolsBottom[] = {
	{"ABC", 2},
	{";", 1}, {"'", 1}, {"\"", 1}, {"<", 1}, {">", 1}, {"~", 1}, {"|", 1},
	{"LEER", 4},
};

constexpr int16_t kKbTop = 100;
constexpr int16_t kKbRowH = 35;

constexpr int16_t kCancelX = kScreenW - 70;
constexpr int16_t kCancelY = 24;
constexpr int16_t kCancelW = 60;
constexpr int16_t kCancelH = 26;

// Zeichnet eine Tastenreihe und ruft optional cb(key, x, y, w, h) fuer jede
// Taste auf - genutzt sowohl zum Rendern als auch (mit Treffertest statt
// Zeichnen) zum Ermitteln der angetippten Taste.
template <typename Fn>
void layoutRow(const Key *keys, size_t count, int16_t y, int16_t rowWeight, Fn &&cb) {
	int16_t x = 0;
	for (size_t i = 0; i < count; i++) {
		int16_t w = static_cast<int16_t>(kScreenW * keys[i].weight / rowWeight);
		cb(keys[i], x, y, w, kKbRowH);
		x += w;
	}
}

const char *rowUpper(const char *label, char *buf, bool shift) {
	if (!shift || strlen(label) != 1 || !isalpha(static_cast<unsigned char>(label[0]))) {
		return label;
	}
	buf[0] = static_cast<char>(toupper(static_cast<unsigned char>(label[0])));
	buf[1] = '\0';
	return buf;
}

void drawKeyboardScreen(DisplayManager &display, const String &ssid, const String &typed,
                        bool symbolsPage, bool shiftActive) {
	TFT_eSPI &tft = display.raw();
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	tft.setTextDatum(TL_DATUM);

	tft.setTextFont(2);
	tft.drawString("Netzwerk: " + ssid, 10, 4);

	// Eingabefeld links, "Abbrechen"-Schaltflaeche rechts daneben (nicht
	// ueberlappend - reserviert eigenen Platz statt ueber dem Feld zu liegen).
	tft.drawRect(10, 24, kCancelX - 16, 26, TFT_BLACK);
	tft.setTextFont(2);
	tft.drawString(typed, 16, 31);

	tft.drawRect(kCancelX, kCancelY, kCancelW, kCancelH, TFT_BLACK);
	tft.setTextDatum(MC_DATUM);
	tft.drawString("Abbrechen", kCancelX + kCancelW / 2, kCancelY + kCancelH / 2);
	tft.setTextDatum(TL_DATUM);

	auto drawKey = [&](const Key &k, int16_t x, int16_t y, int16_t w, int16_t h) {
		tft.drawRect(x, y, w, h, TFT_BLACK);
		char buf[2];
		const char *shown = rowUpper(k.label, buf, shiftActive && !symbolsPage);
		tft.setTextDatum(MC_DATUM);
		tft.setTextFont(strlen(shown) == 1 ? 4 : 2);
		tft.drawString(shown, x + w / 2, y + h / 2);
		tft.setTextDatum(TL_DATUM);
	};

	layoutRow(kRowDigits, sizeof(kRowDigits) / sizeof(Key), kKbTop, kNumRowWeights, drawKey);

	if (!symbolsPage) {
		layoutRow(kRowLettersTop, sizeof(kRowLettersTop) / sizeof(Key), kKbTop + kKbRowH, 10, drawKey);
		layoutRow(kRowLettersMid, sizeof(kRowLettersMid) / sizeof(Key), kKbTop + 2 * kKbRowH, 13, drawKey);
		layoutRow(kRowLettersBottom, sizeof(kRowLettersBottom) / sizeof(Key), kKbTop + 3 * kKbRowH, 13, drawKey);
	} else {
		layoutRow(kRowSymbolsTop, sizeof(kRowSymbolsTop) / sizeof(Key), kKbTop + kKbRowH, 10, drawKey);
		layoutRow(kRowSymbolsMid, sizeof(kRowSymbolsMid) / sizeof(Key), kKbTop + 2 * kKbRowH, 13, drawKey);
		layoutRow(kRowSymbolsBottom, sizeof(kRowSymbolsBottom) / sizeof(Key), kKbTop + 3 * kKbRowH, 13, drawKey);
	}
}

// Ermittelt die angetippte Taste (Label als String) oder "" bei Fehltreffer.
String hitTestKeyboard(int16_t x, int16_t y, bool symbolsPage) {
	String hit;
	auto test = [&](const Key &k, int16_t kx, int16_t ky, int16_t kw, int16_t kh) {
		if (hitRect(x, y, kx, ky, kw, kh)) {
			hit = k.label;
		}
	};

	if (y >= kKbTop && y < kKbTop + kKbRowH) {
		layoutRow(kRowDigits, sizeof(kRowDigits) / sizeof(Key), kKbTop, kNumRowWeights, test);
	} else if (y >= kKbTop + kKbRowH && y < kKbTop + 2 * kKbRowH) {
		if (!symbolsPage)
			layoutRow(kRowLettersTop, sizeof(kRowLettersTop) / sizeof(Key), kKbTop + kKbRowH, 10, test);
		else
			layoutRow(kRowSymbolsTop, sizeof(kRowSymbolsTop) / sizeof(Key), kKbTop + kKbRowH, 10, test);
	} else if (y >= kKbTop + 2 * kKbRowH && y < kKbTop + 3 * kKbRowH) {
		if (!symbolsPage)
			layoutRow(kRowLettersMid, sizeof(kRowLettersMid) / sizeof(Key), kKbTop + 2 * kKbRowH, 13, test);
		else
			layoutRow(kRowSymbolsMid, sizeof(kRowSymbolsMid) / sizeof(Key), kKbTop + 2 * kKbRowH, 13, test);
	} else if (y >= kKbTop + 3 * kKbRowH && y < kKbTop + 4 * kKbRowH) {
		if (!symbolsPage)
			layoutRow(kRowLettersBottom, sizeof(kRowLettersBottom) / sizeof(Key), kKbTop + 3 * kKbRowH, 13, test);
		else
			layoutRow(kRowSymbolsBottom, sizeof(kRowSymbolsBottom) / sizeof(Key), kKbTop + 3 * kKbRowH, 13, test);
	}
	return hit;
}

// Laeuft bis OK (PSK zurueckgegeben) oder Abbruch (leerer optional,
// cancelled=true). Ruecksprung zur Netzliste erfolgt beim Aufrufer.
String runPasswordEntry(DisplayManager &display, TouchManager &touch, const String &ssid, bool &cancelled) {
	String typed;
	bool symbolsPage = false;
	bool shiftActive = false;
	cancelled = false;

	drawKeyboardScreen(display, ssid, typed, symbolsPage, shiftActive);

	while (true) {
		int16_t x, y;
		waitForTapEvent(touch, x, y);

		if (hitRect(x, y, kCancelX, kCancelY, kCancelW, kCancelH)) {
			cancelled = true;
			return "";
		}

		String key = hitTestKeyboard(x, y, symbolsPage);
		if (key.isEmpty()) {
			continue;
		}

		if (key == "<-") {
			if (!typed.isEmpty()) typed.remove(typed.length() - 1);
		} else if (key == "LEER") {
			typed += ' ';
		} else if (key == "CAPS") {
			shiftActive = !shiftActive;
		} else if (key == "SYM") {
			symbolsPage = true;
		} else if (key == "ABC") {
			symbolsPage = false;
		} else if (key == "OK") {
			return typed;
		} else {
			char c = key[0];
			if (shiftActive && !symbolsPage && isalpha(static_cast<unsigned char>(c))) {
				c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
			}
			typed += c;
		}

		drawKeyboardScreen(display, ssid, typed, symbolsPage, shiftActive);
	}
}

void drawStatusScreen(DisplayManager &display, const char *line1, const char *line2) {
	display.drawBootScreen(line1, line2);
}

} // namespace

void WifiOnboarding::run(DisplayManager &display, TouchManager &touch, WlanManager &wlan) {
	drawStatusScreen(display, "WLAN-Suche", "Scanne Netzwerke ...");
	std::vector<WlanManager::NetworkInfo> networks = wlan.scan();
	drawNetworkList(display, networks);

	while (true) {
		int16_t x, y;
		waitForTapEvent(touch, x, y);

		if (hitRect(x, y, kRefreshX, kRefreshY, kRefreshW, kRefreshH)) {
			drawStatusScreen(display, "WLAN-Suche", "Scanne Netzwerke ...");
			networks = wlan.scan();
			drawNetworkList(display, networks);
			continue;
		}

		int idx = hitTestNetworkRow(x, y, networks.size());
		if (idx < 0) {
			continue;
		}

		String ssid = networks[static_cast<size_t>(idx)].ssid;
		bool cancelled = false;
		String psk = runPasswordEntry(display, touch, ssid, cancelled);
		if (cancelled) {
			drawNetworkList(display, networks);
			continue;
		}

		drawStatusScreen(display, "Verbinde...", ssid.c_str());
		if (wlan.connect(ssid, psk)) {
			wlan.saveCredentials(ssid, psk);
			drawStatusScreen(display, "Verbunden!", ssid.c_str());
			delay(1000);
			return;
		}

		drawStatusScreen(display, "Fehlgeschlagen", "Bitte erneut versuchen");
		delay(1500);
		drawNetworkList(display, networks);
	}
}
