# scripts/

## flash.ps1 / flash.cmd

Richtet auf einem beliebigen Windows-PC alles ein, was für einen
Flash-Vorgang nötig ist, und flasht anschließend eines von vier
Sensormeter-Schwesterprojekten:

1. **Projekt wählen** – interaktiv (1-4-Menü) oder per `-Project
   sensormeter|wlan|display|poe`:
   - **Sensormeter** (WT32-ETH01, Ethernet + bis zu 2 Sensoren)
   - **Sensormeter WLAN** (generisches ESP32-WROOM-32-DevKit, WLAN-only)
   - **Sensormeter Display** (ESP32-Touchdisplay, SNMP-Client)
   - **Sensormeter PoE** (Waveshare ESP32-S3-ETH, PoE optional, Relais)
2. Python installieren (falls nicht vorhanden, über winget)
3. Git installieren (falls nicht vorhanden, über winget)
4. PlatformIO installieren (falls nicht vorhanden, über pip)
5. Das gewählte Repo klonen (falls noch nicht vorhanden) bzw. aktualisieren
   (`git pull`, nur wenn keine lokalen Änderungen vorliegen)
6. `firmware/include/config.h` aus der Vorlage anlegen, falls das gewählte
   Projekt eine braucht und sie noch fehlt (eine vorhandene wird nie
   überschrieben; **Sensormeter Display** braucht keine – alle
   Einstellungen wie WLAN, Betriebsmodus, Sensormeter-Ziel, Ping-Ziele
   werden am Gerät selbst per Touch eingerichtet und in NVS gespeichert)
7. Firmware bauen (`pio run`)
8. Firmware flashen (`pio run --target upload`)

Dieses Skript liegt identisch in allen vier Repos (`scripts/flash.ps1`) –
unabhängig davon, welches Projekt gerade lokal ausgecheckt ist, lässt sich
darüber jedes der vier flashen (die jeweils anderen werden bei Bedarf
automatisch in einen Unterordner neben dem Skript geklont).

**Anschluss-Voraussetzungen:**
- **Sensormeter** (WT32-ETH01): Board am USB-Seriell-Adapter
  (Debug-Burning-Schnittstelle, nicht am 20-Pin-Hauptheader!)
  angeschlossen, siehe
  [`../docs/flash-vorbereitung.pdf`](../docs/flash-vorbereitung.pdf)
  (im Sensormeter-Repo).
- **Sensormeter WLAN** / **Sensormeter PoE**: Board per USB-C angeschlossen,
  keine bekannten Besonderheiten.
- **Sensormeter Display** (HW-458B): Board per USB-C oder USB-Micro
  angeschlossen, ggf. CH340-Treiber installieren.

### Nutzung

Nur diese eine Datei `flash.ps1` (oder zusätzlich die `.cmd` zum
Doppelklicken) auf den Ziel-PC kopieren und ausführen – das jeweilige Repo
muss dafür noch **nicht** vorher geklont sein, das Skript erledigt das
selbst.

**Per Doppelklick:** `flash.cmd` – öffnet ein Konsolenfenster, fragt nach
dem gewünschten Projekt und bleibt nach Abschluss offen (zum Lesen von
Meldungen/Fehlern).

**Per PowerShell:**

```powershell
.\flash.ps1                                  # fragt interaktiv nach dem Projekt
.\flash.ps1 -Project sensormeter             # Projekt direkt angeben
.\flash.ps1 -Project wlan -Port COM5         # fester COM-Port
.\flash.ps1 -Project display -SkipUpload     # nur bauen, nicht flashen
.\flash.ps1 -Project poe -RepoPath C:\Projekte\sensormeter-poe
```

Falls PowerShell die Ausführung wegen der Execution Policy verweigert:

```powershell
powershell -ExecutionPolicy Bypass -File .\flash.ps1
```

## flash.sh (macOS/Linux)

Bash-Pendant zu `flash.ps1` für macOS (**nur Apple Silicon/arm64, kein
Intel-Mac**) und Linux – liegt identisch in allen vier Repos, analog zu
`flash.ps1`. Deckt bewusst **nur** den Flash-Vorgang ab: `convert-logo.ps1`
und `snmp-load.ps1` bleiben Windows-only, es gibt (noch) keine
Bash-Fassung davon.

Gleicher Ablauf wie `flash.ps1` (Projekt wählen → Python/Git/PlatformIO
prüfen/installieren → Repo klonen/aktualisieren → `config.h` anlegen →
`pio run` → `pio run --target upload`), nur mit plattformgerechten
Anpassungen:

- **Werkzeug-Installation**: über Homebrew (`brew`) auf macOS, über den
  erkannten Paketmanager (`apt`/`dnf`/`pacman`/`zypper`) auf Linux –
  Homebrew selbst wird nicht automatisch installiert, das Skript bricht
  mit einem Hinweis auf <https://brew.sh> ab, falls es fehlt.
- **Serielle Geräte** statt COM-Ports: `/dev/cu.*` auf macOS,
  `/dev/ttyUSB*`/`/dev/ttyACM*` auf Linux.
- **PEP-668-Fallback**: schlägt die PlatformIO-Installation wegen
  „externally-managed-environment" fehl (neuere Debian-/Ubuntu-Versionen),
  wird automatisch mit `--break-system-packages` erneut versucht (betrifft
  nur das `platformio`-Paket selbst).

```bash
./flash.sh                                          # fragt interaktiv nach dem Projekt
./flash.sh --project wlan                           # Projekt direkt angeben
./flash.sh --project wlan --port /dev/cu.usbserial-1410
./flash.sh --project display --skip-upload          # nur bauen, nicht flashen
./flash.sh --project poe --repo-path ~/Projekte/sensormeter-poe
```

Ausführbar machen, falls das Ausführungsbit beim Kopieren verloren ging:
`chmod +x flash.sh`.

**Linux-Hinweis:** Der eigene Benutzer braucht Zugriff auf das serielle
Gerät (meist Gruppe `dialout` bei Debian/Ubuntu, `uucp` bei Arch) –
`sudo usermod -aG dialout $USER` und danach ab-/anmelden, falls
`pio run --target upload` mit „Permission denied" auf `/dev/ttyUSB0` o. Ä.
fehlschlägt.

## convert-logo.ps1

Konvertiert ein Anbieter-Logo (beliebiges Bildformat) in das fürs
Anbieter-Branding-Feature kompatible Rohformat – liegt identisch in allen
vier Repos, analog zu `flash.ps1`. Fragt zuerst (interaktiv oder per
`-Display sensormeter|wlan|poe|display|custom`), für welches Display
konvertiert werden soll, und reduziert Auflösung **und Farbtiefe**
konsequent auf das, was das jeweilige Display tatsächlich darstellen kann
– ein 24-Bit-Foto wird also nicht einfach verkleinert, sondern für die
monochromen OLEDs auf echte 1-Bit-Schwarzweiß-Werte reduziert (kein
Graustufen-„so tun als ob").

**Alle vier Projekte haben ein implementiertes Branding-Feature**
(`BrandingManager`, siehe jeweiliges `docs/entscheidungen.md`):

| Preset | Projekt(e) | Display | Zielgröße | Format |
|---|---|---|---|---|
| `sensormeter` / `wlan` / `poe` | Sensormeter, Sensormeter WLAN, Sensormeter PoE | OLED SSD1306 | 128×64 | 1-Bit monochrom |
| `display` | Sensormeter Display | TFT ST7789P3 (Farbe) | 128×64 | RGB565, 2 Byte/Pixel |

Das größere SH1107-OLED (128×128), früher intern bei Sensormeter PoE
verbaut, gibt es seit der familienweiten Display-Standardisierung nur
noch als optionales externes RJ45-Steckmodul (siehe
`sensormeter-family/repo/module-design/sh1107-display-modul.md`) –
eigenes Logo-Format dafür noch nicht über dieses Skript abgedeckt, bei
Bedarf `-Display custom -Width 128 -Height 128 -ColorMode mono` nutzen.

Die Farbziel-Größe (128×64) ist bewusst **dieselbe** wie bei den
monochromen Projekten (nicht die native Panel-Auflösung 240×320) – nur die
Farbtiefe unterscheidet sich, damit dieses eine Skript für alle vier
Projekte eine einheitliche Logo-Größe nutzt.

Monochromes Ausgabeformat: exakt Breite/8 × Höhe Byte, MSB-zuerst je
Zeile, kein Padding – identisch zu dem, was `Adafruit_GFX::drawBitmap()`
erwartet und was `BrandingManager.h` als `LOGO_WIDTH`/`LOGO_HEIGHT`/
`LOGO_BYTES` vorschreibt.

Farb-Ausgabeformat (Sensormeter Display): RGB565, Little-Endian,
zeilenweise ohne Padding. **Achtung – nicht auf echter Hardware
verifiziert:** dieses Panel läuft mit `TFT_RGB_ORDER=0` (BGR) und zeigte
selbst dabei einzelne falsch interpretierte Farben (siehe
`sensormeter-display/docs/entscheidungen.md`) – färbt ein hochgeladenes
Logo auf echter Hardware sichtbar falsch (Rot/Blau vertauscht), hilft der
Schalter `-SwapRedBlue`.

Das Quellbild wird seitenverhältnistreu eingepasst (nicht verzerrt) und
zentriert mit der Padding-Farbe (Default Schwarz) aufgefüllt. Transparente
Bereiche einer PNG-Quelle werden dabei automatisch mit der Padding-Farbe
unterlegt.

```powershell
.\convert-logo.ps1 -InputPath .\firmenlogo.png                # fragt interaktiv nach dem Display
.\convert-logo.ps1 -InputPath .\firmenlogo.png -Display wlan   # 128x64, 1bpp, direkt hochladbar
.\convert-logo.ps1 -InputPath .\firmenlogo.png -Display poe    # 128x64, 1bpp
.\convert-logo.ps1 -InputPath .\firmenlogo.png -Display display -SwapRedBlue
.\convert-logo.ps1 -InputPath .\firmenlogo.png -Display custom -Width 96 -Height 48 -ColorMode mono
```

Das Ergebnis (`<Name>-<Display>-<Breite>x<Höhe>.bin`) lässt sich direkt
über die Einstellungsseite des jeweiligen Geräts hochladen – bei
Sensormeter/Sensormeter WLAN/Sensormeter PoE unter „Anbieter-Branding →
Logo hochladen", bei Sensormeter Display unter „Branding-Logo → Logo
hochladen".

## ../firmware/tools/simulate_json_load.cpp

**Nur im Sensormeter-Repo vorhanden** (nicht Teil der drei
Geschwisterprojekte, dieser Abschnitt betrifft also nur diese eine Kopie
der Datei). Siehe
[`../firmware/tools/README.md`](../firmware/tools/README.md) – natives
Testprogramm zur Heap-Last-Simulation der REST-API, kein
Setup-/Flash-Werkzeug.
