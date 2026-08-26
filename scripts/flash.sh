#!/bin/bash
#
# flash.sh - Installiert alle Abhaengigkeiten und flasht eines der vier
# Sensormeter-Firmware-Projekte (Sensormeter / Sensormeter WLAN /
# Sensormeter Display / Sensormeter PoE) auf macOS (nur Apple Silicon/arm64)
# oder Linux.
#
# Bash-Pendant zu scripts/flash.ps1 (Windows) - liegt identisch in allen
# vier Repos, siehe dortiger Kommentarkopf fuer den vollen Hintergrund
# (PlatformIO-Paket-Pool-Isolation zwischen "espressif32" und "pioarduino"
# bei Sensormeter PoE etc.). Bewusst NUR der Flash-Vorgang - anders als auf
# Windows gibt es hier (noch) kein convert-logo.sh/snmp-load.sh-Pendant,
# siehe docs/entscheidungen.md.
#
# Nutzung:
#   ./flash.sh                                   # fragt interaktiv nach dem Projekt
#   ./flash.sh --project wlan                    # Projekt direkt angeben
#   ./flash.sh --project wlan --port /dev/cu.usbserial-1410
#   ./flash.sh --project display --skip-upload   # nur bauen, nicht flashen
#   ./flash.sh --project poe --repo-path ~/Projekte/sensormeter-poe
#
# Abhaengigkeits-Erkennung ist bewusst "funktional" (ruft z.B.
# "python3 --version" auf und prueft die Ausgabe), nicht nur eine
# PATH-Pruefung - analog zum "python"-Store-Alias-Problem auf Windows, das
# flash.ps1 umgeht (siehe dortiger Kommentar).
#
# Changelog:
#   1.0.0 (2026-07-12) - Erste Fassung (macOS arm64 + Linux), Pendant zu
#                         flash.ps1 v1.1.0. Nur Flashen, kein Logo-Konverter,
#                         kein SNMP-Lastgenerator.

set -euo pipefail

FLASH_SCRIPT_VERSION="1.0.0"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd -P)"

PROJECT=""
REPO_PATH=""
PORT=""
SKIP_UPLOAD=0

print_help() {
  cat <<'EOF'
Nutzung: ./flash.sh [--project sensormeter|wlan|display|poe]
                     [--repo-path PFAD] [--port GERAET] [--skip-upload]

  --project      Projekt direkt angeben, ohne interaktive Abfrage.
  --repo-path    Zielordner fuer den Checkout (Default: Ordner neben
                 diesem Skript, z.B. ./sensormeter-wlan).
  --port         Serielles Geraet (z.B. /dev/cu.usbserial-1410 auf macOS,
                 /dev/ttyUSB0 auf Linux), falls die automatische Erkennung
                 fehlschlaegt oder mehrere Boards angeschlossen sind.
  --skip-upload  Nur bauen, nicht flashen.
  -h, --help     Diese Hilfe anzeigen.
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    --project)
      PROJECT="$2"
      shift 2
      ;;
    --repo-path)
      REPO_PATH="$2"
      shift 2
      ;;
    --port)
      PORT="$2"
      shift 2
      ;;
    --skip-upload)
      SKIP_UPLOAD=1
      shift
      ;;
    -h|--help)
      print_help
      exit 0
      ;;
    *)
      echo "Unbekannte Option: $1" >&2
      print_help
      exit 1
      ;;
  esac
done

echo "Sensormeter Flash-Skript v${FLASH_SCRIPT_VERSION}"

step() {
  echo ""
  echo "==> $1"
}

# ----------------------------------------------------------------------
# Plattform-Erkennung. Windows wird bewusst nicht unterstuetzt (dafuer gibt
# es flash.ps1) - ebenso wenig Intel-Macs, siehe Kommentarkopf.
# ----------------------------------------------------------------------
UNAME_S="$(uname -s)"
case "$UNAME_S" in
  Darwin)
    PLATFORM="mac"
    ARCH="$(uname -m)"
    if [ "$ARCH" != "arm64" ]; then
      echo "FEHLER: Auf macOS wird nur Apple Silicon (arm64) unterstuetzt - erkannt: $ARCH (Intel-Mac)." >&2
      echo "Siehe docs/entscheidungen.md fuer den Hintergrund dieser Einschraenkung." >&2
      exit 1
    fi
    ;;
  Linux)
    PLATFORM="linux"
    ;;
  *)
    echo "FEHLER: Nicht unterstuetztes Betriebssystem '$UNAME_S' - dieses Skript deckt nur macOS (arm64) und Linux ab. Fuer Windows siehe flash.ps1." >&2
    exit 1
    ;;
esac

# ----------------------------------------------------------------------
# Projekt-Metadaten (bewusst als Funktionen statt assoziativem Array, damit
# das Skript auch unter macOS' Standard-/bin/bash 3.2 laeuft, das keine
# "declare -A" kennt).
# ----------------------------------------------------------------------
project_display_name() {
  case "$1" in
    sensormeter) echo "Sensormeter (WT32-ETH01, Ethernet + bis zu 2 Sensoren)" ;;
    wlan) echo "Sensormeter WLAN (generisches ESP32-WROOM-32-DevKit, WLAN-only)" ;;
    display) echo "Sensormeter Display (ESP32-Touchdisplay, HW-458B)" ;;
    poe) echo "Sensormeter PoE (ESP32-S3-ETH, W5500 + WLAN, RJ45-Modul + Relais)" ;;
  esac
}

project_repo_url() {
  case "$1" in
    sensormeter) echo "https://github.com/peterhagelhof7-cmd/sensormeter.git" ;;
    wlan) echo "https://github.com/peterhagelhof7-cmd/sensormeter-wlan.git" ;;
    display) echo "https://github.com/peterhagelhof7-cmd/sensormeter-display.git" ;;
    poe) echo "https://github.com/peterhagelhof7-cmd/sensormeter-poe.git" ;;
  esac
}

project_folder_name() {
  case "$1" in
    sensormeter) echo "sensormeter" ;;
    wlan) echo "sensormeter-wlan" ;;
    display) echo "sensormeter-display" ;;
    poe) echo "sensormeter-poe" ;;
  esac
}

# 1 = braucht firmware/include/config.h aus der Vorlage, 0 = nicht (Display:
# Einstellungen liegen komplett in NVS, siehe dortiges README).
project_has_config_h() {
  case "$1" in
    display) echo 0 ;;
    *) echo 1 ;;
  esac
}

project_flash_note() {
  case "$1" in
    sensormeter) echo "Board muss am USB-Seriell-Adapter (Debug-Burning-Schnittstelle, NICHT am 20-Pin-Hauptheader!) angeschlossen sein und sich im Boot-/Download-Modus befinden - siehe docs/flash-vorbereitung.pdf." ;;
    wlan) echo "Board per USB-C-Kabel anschliessen - ggf. CP2102-/CH340-USB-Treiber wird vom Betriebssystem automatisch erkannt." ;;
    display) echo "Board per USB-Kabel anschliessen - ggf. CH340-USB-Treiber wird vom Betriebssystem automatisch erkannt." ;;
    poe) echo "Board per USB-C-Kabel anschliessen. Erster Build laedt eine eigene, isolierte PlatformIO-Toolchain herunter (siehe Skript-Kommentarkopf zu 'pioarduino') - dauert deutlich laenger als bei den anderen drei Projekten und braucht Internetzugang. Bislang nur per 'pio run' gebaut/verifiziert, nicht auf echter Hardware getestet (siehe docs/entscheidungen.md) - beim ersten Flashen entsprechend aufmerksam pruefen." ;;
  esac
}

project_success_note() {
  case "$1" in
    sensormeter) echo "Seriellen Monitor ansehen: pio device monitor (115200 Baud)." ;;
    wlan) echo "Beim ersten Start: WLAN ueber die Weboberflaeche einrichten - ohne gespeicherte Zugangsdaten versucht das Geraet nach 5 Minuten, dem Netz 'installer'/'installer' beizutreten (siehe docs/admin-guide.pdf Abschnitt 2.2)." ;;
    display) echo "Beim ersten Start: Touch-Kalibrierung durchfuehren, dann WLAN einrichten (siehe README.md)." ;;
    poe) echo "Beim ersten Start: RJ45-Modul-Erkennung laeuft automatisch waehrend des Boot-Countdowns; WLAN/Ethernet und ggf. MQTT ueber die Weboberflaeche einrichten (siehe docs/lastenheft.txt)." ;;
  esac
}

# ----------------------------------------------------------------------
if [ -z "$PROJECT" ]; then
  echo "Welches Projekt soll geflasht werden?"
  echo "  1) $(project_display_name sensormeter)"
  echo "  2) $(project_display_name wlan)"
  echo "  3) $(project_display_name display)"
  echo "  4) $(project_display_name poe)"
  while [ -z "$PROJECT" ]; do
    printf "Auswahl [1-4]: "
    read -r choice
    case "$choice" in
      1|sensormeter) PROJECT="sensormeter" ;;
      2|wlan) PROJECT="wlan" ;;
      3|display) PROJECT="display" ;;
      4|poe) PROJECT="poe" ;;
      *) echo "Ungueltige Eingabe - bitte 1, 2, 3, 4 oder den Projektnamen eingeben." ;;
    esac
  done
fi

case "$PROJECT" in
  sensormeter|wlan|display|poe) ;;
  *)
    echo "FEHLER: Unbekanntes Projekt '$PROJECT' (erlaubt: sensormeter, wlan, display, poe)." >&2
    exit 1
    ;;
esac

DISPLAY_NAME="$(project_display_name "$PROJECT")"
REPO_URL="$(project_repo_url "$PROJECT")"
FOLDER_NAME="$(project_folder_name "$PROJECT")"
HAS_CONFIG_H="$(project_has_config_h "$PROJECT")"
FLASH_NOTE="$(project_flash_note "$PROJECT")"
SUCCESS_NOTE="$(project_success_note "$PROJECT")"

step "Projekt: $DISPLAY_NAME"

if [ -z "$REPO_PATH" ]; then
  REPO_PATH="$SCRIPT_DIR/$FOLDER_NAME"
fi

# ----------------------------------------------------------------------
# Funktionale Werkzeug-Pruefung/-Installation (siehe Kommentarkopf).
# ----------------------------------------------------------------------
tool_works() {
  local cmd="$1" pattern="$2" output
  output="$("$cmd" --version 2>&1)" || return 1
  echo "$output" | grep -Eq "$pattern"
}

detect_pkg_manager() {
  if command -v apt-get >/dev/null 2>&1; then echo "apt"
  elif command -v dnf >/dev/null 2>&1; then echo "dnf"
  elif command -v pacman >/dev/null 2>&1; then echo "pacman"
  elif command -v zypper >/dev/null 2>&1; then echo "zypper"
  else echo "unknown"
  fi
}

install_python() {
  case "$PLATFORM" in
    mac)
      if ! command -v brew >/dev/null 2>&1; then
        echo "FEHLER: Homebrew nicht gefunden - bitte zuerst von https://brew.sh installieren, dann dieses Skript erneut ausfuehren." >&2
        exit 1
      fi
      brew install python
      ;;
    linux)
      case "$(detect_pkg_manager)" in
        apt) sudo apt-get update && sudo apt-get install -y python3 python3-pip python3-venv ;;
        dnf) sudo dnf install -y python3 python3-pip ;;
        pacman) sudo pacman -Sy --noconfirm python python-pip ;;
        zypper) sudo zypper install -y python3 python3-pip ;;
        *)
          echo "FEHLER: Kein bekannter Paketmanager (apt/dnf/pacman/zypper) gefunden - bitte Python 3 manuell installieren." >&2
          exit 1
          ;;
      esac
      ;;
  esac
}

install_git() {
  case "$PLATFORM" in
    mac)
      if ! command -v brew >/dev/null 2>&1; then
        echo "FEHLER: Homebrew nicht gefunden - bitte zuerst von https://brew.sh installieren, dann dieses Skript erneut ausfuehren." >&2
        exit 1
      fi
      brew install git
      ;;
    linux)
      case "$(detect_pkg_manager)" in
        apt) sudo apt-get update && sudo apt-get install -y git ;;
        dnf) sudo dnf install -y git ;;
        pacman) sudo pacman -Sy --noconfirm git ;;
        zypper) sudo zypper install -y git ;;
        *)
          echo "FEHLER: Kein bekannter Paketmanager (apt/dnf/pacman/zypper) gefunden - bitte Git manuell installieren." >&2
          exit 1
          ;;
      esac
      ;;
  esac
}

# Ermittelt den funktionierenden Python-3-Befehl (python3 bevorzugt, sonst
# python, falls das auf diesem System bereits Python 3 ist) - wird nach
# einer install_python()-Installation erneut aufgerufen.
resolve_python_cmd() {
  if command -v python3 >/dev/null 2>&1 && tool_works python3 "^Python 3"; then
    echo "python3"
  elif command -v python >/dev/null 2>&1 && tool_works python "^Python 3"; then
    echo "python"
  else
    echo ""
  fi
}

install_platformio() {
  local py="$1"
  # PEP 668 ("externally-managed-environment") blockiert auf neueren Debian-/
  # Ubuntu-Versionen ein einfaches "pip install" ins System-Python - erst
  # ohne Zusatzflag versuchen, bei Fehlschlag mit --break-system-packages
  # erneut (nur fuer platformio selbst, kein systemweiter Eingriff in andere
  # Pakete).
  "$py" -m pip install --upgrade pip >/dev/null 2>&1 || true
  if ! "$py" -m pip install --upgrade platformio 2>/tmp/pio-install.log; then
    if grep -qi "externally-managed-environment" /tmp/pio-install.log 2>/dev/null; then
      echo "System-Python ist 'externally managed' - installiere mit --break-system-packages (betrifft nur platformio)."
      "$py" -m pip install --break-system-packages --upgrade platformio
    else
      cat /tmp/pio-install.log >&2
      return 1
    fi
  fi
  rm -f /tmp/pio-install.log
}

step "Pruefe Python..."
PYTHON_CMD="$(resolve_python_cmd)"
if [ -z "$PYTHON_CMD" ]; then
  echo "Python 3 nicht gefunden oder nicht funktionsfaehig - installiere..."
  install_python
  PYTHON_CMD="$(resolve_python_cmd)"
  if [ -z "$PYTHON_CMD" ]; then
    echo "FEHLER: Python 3 ist nach der Installation weiterhin nicht nutzbar. Bitte Terminal neu starten und Skript erneut ausfuehren (PATH-Aenderungen greifen sonst erst in einer neuen Sitzung)." >&2
    exit 1
  fi
fi
echo "OK: Python 3 ist vorhanden ($PYTHON_CMD)."

step "Pruefe Git..."
if ! tool_works git "^git version"; then
  echo "Git nicht gefunden oder nicht funktionsfaehig - installiere..."
  install_git
  if ! tool_works git "^git version"; then
    echo "FEHLER: Git ist nach der Installation weiterhin nicht nutzbar. Bitte Terminal neu starten und Skript erneut ausfuehren." >&2
    exit 1
  fi
fi
echo "OK: Git ist vorhanden."

step "Pruefe PlatformIO..."
if ! tool_works pio "PlatformIO Core"; then
  echo "PlatformIO nicht gefunden oder nicht funktionsfaehig - installiere..."
  install_platformio "$PYTHON_CMD"
  hash -r
  if ! tool_works pio "PlatformIO Core"; then
    echo "FEHLER: PlatformIO ist nach der Installation weiterhin nicht nutzbar. Haeufige Ursache: der pip-Installationsordner (z.B. ~/.local/bin) liegt nicht im PATH - Terminal neu starten oder PATH pruefen und Skript erneut ausfuehren." >&2
    exit 1
  fi
fi
echo "OK: PlatformIO ist vorhanden."

# ----------------------------------------------------------------------
step "Pruefe Projekt-Checkout..."
FIRMWARE_PATH="$REPO_PATH/firmware"

if [ -f "$FIRMWARE_PATH/platformio.ini" ]; then
  echo "Repo bereits vorhanden unter $REPO_PATH"
  if [ -z "$(git -C "$REPO_PATH" status --porcelain)" ]; then
    echo "Keine lokalen Aenderungen - hole neueste Version (git pull)..."
    git -C "$REPO_PATH" pull
  else
    echo "Lokale Aenderungen im Checkout gefunden - ueberspringe 'git pull', um nichts zu ueberschreiben."
  fi
else
  step "Klone Repository nach $REPO_PATH ..."
  git clone "$REPO_URL" "$REPO_PATH"
fi

if [ ! -f "$FIRMWARE_PATH/platformio.ini" ]; then
  echo "FEHLER: firmware/platformio.ini wurde auch nach dem Checkout nicht gefunden - stimmt --repo-path ($REPO_PATH)?" >&2
  exit 1
fi

# ----------------------------------------------------------------------
if [ "$HAS_CONFIG_H" = "1" ]; then
  step "Pruefe config.h..."
  CONFIG_EXAMPLE="$FIRMWARE_PATH/include/config.h.example"
  CONFIG_REAL="$FIRMWARE_PATH/include/config.h"
  if [ ! -f "$CONFIG_REAL" ]; then
    cp "$CONFIG_EXAMPLE" "$CONFIG_REAL"
    echo "include/config.h aus der Vorlage angelegt."
  else
    echo "config.h existiert bereits - wird nicht ueberschrieben."
  fi
fi

# ----------------------------------------------------------------------
cd "$FIRMWARE_PATH"

step "Baue Firmware (pio run)..."
pio run

if [ "$SKIP_UPLOAD" = "1" ]; then
  echo ""
  echo "--skip-upload gesetzt - Build erfolgreich, kein Flash-Vorgang durchgefuehrt."
  exit 0
fi

# ----------------------------------------------------------------------
step "Verfuegbare serielle Geraete:"
case "$PLATFORM" in
  mac) DEVICES="$(ls /dev/cu.* 2>/dev/null || true)" ;;
  linux) DEVICES="$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || true)" ;;
esac
if [ -z "$DEVICES" ]; then
  echo "  (keine gefunden - ist das Board per USB angeschlossen?)"
else
  echo "$DEVICES" | sed 's/^/  - /'
fi

echo ""
echo "Hinweis: $FLASH_NOTE"

step "Flashe Firmware (pio run --target upload)..."
if [ -n "$PORT" ]; then
  pio run --target upload --upload-port "$PORT"
else
  pio run --target upload
fi

echo ""
echo "Fertig! Firmware erfolgreich geflasht."
echo "$SUCCESS_NOTE"
