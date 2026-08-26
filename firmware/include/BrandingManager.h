#pragma once

#include <Arduino.h>
#include <FS.h>
#include <freertos/semphr.h>

// Anbieter-Branding (Weisslabel) - Logo-Bild, verwaltet analog zu den drei
// OLED-Geschwisterprojekten (Sensormeter, Sensormeter WLAN, Sensormeter
// PoE, siehe deren docs/entscheidungen.md), aber ANDERS als dort: dieses
// Board hat ein Farb-TFT (ST7789P3), kein monochromes OLED. Der
// Anbietername selbst liegt NICHT hier, sondern in SettingsManager
// (NVS) - nur das Logo-Bild ist zu gross fuer einen einzelnen NVS-Eintrag
// und liegt daher wie bei den Geschwistern auf LittleFS.
//
// Format: 128x64 Pixel (kLogoWidth x kLogoHeight - bewusst dieselbe
// Aufloesung wie die drei OLED-Geschwisterprojekte, nur mit Farbtiefe
// RGB565 statt 1bpp, damit `scripts/convert-logo.ps1` fuer alle vier
// Projekte dieselbe Zielgroesse nutzt und nur die Farbtiefe je Display
// umschaltet), 2 Byte pro Pixel Little-Endian, kein Padding - exakt
// kLogoBytes = 16384 Byte. KEIN PNG/JPEG-Decoder eingebunden (gleiche
// Begruendung wie bei den OLED-Projekten: 30-80 KB zusaetzliches Flash
// fuer eine Decoder-Bibliothek).
//
// WICHTIG - nicht auf echter Hardware verifiziert: platformio.ini setzt
// TFT_RGB_ORDER=0 (BGR) fuer dieses Panel (siehe docs/entscheidungen.md,
// "mit RGB (1) wurde Weiss zu Schwarz"), und selbst unter der gewaehlten
// BGR-Einstellung mussten dort einzelne benannte Farben (TFT_YELLOW statt
// TFT_RED) empirisch kompensiert werden - der MADCTL-Farbkanaltausch
// verhaelt sich auf diesem Panel/Klon also nicht rein spezifikationsgemaess.
// Dieses Modul verpackt Logos bewusst in STANDARD-RGB565 (R oben, G Mitte,
// B unten, wie TFT_eSPI::color565() es auch tut) statt vorab zu
// kompensieren, um nicht doppelt oder falsch herum zu kompensieren. Faerbt
// ein hochgeladenes Logo auf echter Hardware sichtbar falsch (Rot/Blau
// vertauscht), hilft `scripts/convert-logo.ps1 -Display display
// -SwapRedBlue` als Escape-Hatch.
//
// Mutex-geschuetzt wie SettingsManager: hasLogo()/loadLogo() werden sowohl
// vom Hauptloop (BrandingView::draw()) als auch vom asynchronen
// Webserver-Task (Dashboard-Banner, Einstellungsseite, Logo-Upload)
// gelesen/geschrieben.
//
// hasLogo() haelt den Vorhanden-Zustand in einem RAM-Cache statt bei jeder
// Abfrage LittleFS.exists() aufzurufen - bei den OLED-Geschwisterprojekten
// fuehrte das wiederholte exists()-Polling zu einer dauerhaft
// wiederkehrenden, aber harmlosen ESP-IDF-VFS-Fehlerzeile im Boot-Log
// (LittleFS.exists() ist intern als open(path,"r") implementiert), siehe
// sensormeter-wlan/repo/docs/entscheidungen.md - hier von Anfang an
// vermieden.
class BrandingManager {
 public:
  void begin();

  bool hasLogo() const;

  static constexpr int kLogoWidth = 128;
  static constexpr int kLogoHeight = 64;
  static constexpr size_t kLogoBytes = static_cast<size_t>(kLogoWidth) * kLogoHeight * 2;

  // Liest das gespeicherte Logo vollstaendig in buffer (muss mindestens
  // kLogoBytes gross sein, als uint16_t-Array interpretierbar - Little-
  // Endian, passend zur ESP32-eigenen Speicherordnung). false falls kein
  // Logo vorhanden, die Datei nicht exakt kLogoBytes gross ist, oder ein
  // Lesefehler auftritt.
  bool loadLogo(uint8_t *buffer, size_t bufferSize) const;

  // Streaming-Callbacks aus WebServerManager fuer den Logo-Upload (gleiches
  // Tmp-Datei-plus-Umbenennen-Muster wie GraphManager::save() bzw. die
  // OLED-Geschwisterprojekte - ein Stromausfall oder eine falsch grosse
  // Datei mitten im Upload darf ein zuvor funktionierendes Logo nicht
  // zerstoeren).
  bool beginLogoUpload();
  bool writeLogoUploadChunk(const uint8_t *data, size_t len);
  bool endLogoUpload();

  bool deleteLogo();

 private:
  bool checkLogoOnDisk() const;
  // Schreibt das in DefaultLogo.h eingebettete Familien-Standardlogo einmalig
  // auf LittleFS - nur aus begin() aufgerufen, und dort auch nur, wenn
  // checkLogoOnDisk() kein eigenes (hochgeladenes) Logo gefunden hat, siehe
  // docs/entscheidungen.md.
  void provisionDefaultLogo();

  SemaphoreHandle_t mutex_ = nullptr;
  bool logoPresent_ = false;

  fs::File uploadFile_;
  size_t uploadBytesWritten_ = 0;
  bool uploadOpen_ = false;
};
