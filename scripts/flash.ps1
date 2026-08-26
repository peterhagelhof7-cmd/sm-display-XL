<#
.SYNOPSIS
  Installiert alle Abhaengigkeiten und flasht eines der vier Sensormeter-
  Firmware-Projekte (Sensormeter / Sensormeter WLAN / Sensormeter Display /
  Sensormeter PoE) auf einem beliebigen Windows-PC.

.DESCRIPTION
  Fragt zuerst interaktiv (oder per -Project), welches der vier
  Schwesterprojekte geflasht werden soll:
    1) Sensormeter        (WT32-ETH01, Ethernet + bis zu 2 Sensoren)
    2) Sensormeter WLAN    (generisches ESP32-WROOM-32-DevKit, WLAN-only)
    3) Sensormeter Display (ESP32-Touchdisplay, SNMP-Client)
    4) Sensormeter PoE     (ESP32-S3-ETH, W5500 + WLAN, RJ45-Modul + Relais)

  Anschliessend identischer Ablauf fuer alle vier:
  - Installiert Python + PlatformIO, falls nicht vorhanden (ueber winget/pip)
  - Installiert Git, falls nicht vorhanden (ueber winget)
  - Klont das gewaehlte Repository, falls unter -RepoPath noch kein
    Checkout liegt (bei vorhandenem, sauberem Checkout wird stattdessen
    "git pull" versucht)
  - Legt firmware/include/config.h aus der Vorlage an, falls das gewaehlte
    Projekt eine braucht und sie noch fehlt (wird nie ueberschrieben)
  - Baut die Firmware (pio run) zur Kontrolle
  - Flasht sie auf das per USB angeschlossene Board (pio run --target upload)

  Dieses Skript liegt identisch in allen vier Repos (scripts/flash.ps1) -
  unabhaengig davon, welches der vier Projekte gerade lokal ausgecheckt ist,
  laesst sich darueber jedes der vier flashen (die jeweils anderen werden
  bei Bedarf automatisch in einen Unterordner neben dem Skript geklont).

  Abhaengigkeits-Erkennung ist bewusst "funktional" (ruft z.B. "python
  --version" auf und prueft die Ausgabe), nicht nur eine PATH-Pruefung:
  Windows legt standardmaessig einen "python"-Store-Alias auf PATH, der
  vorhanden aussieht, aber kein echtes Python ist - eine reine
  Get-Command-Pruefung wuerde das faelschlich als "installiert" werten.
  Aus demselben Grund wird nach einer Installation der tatsaechliche
  Funktionstest wiederholt statt sich auf den winget-Exitcode zu verlassen
  (winget liefert z.B. bei "bereits installiert, kein Update verfuegbar"
  einen Nicht-Null-Exitcode, obwohl das kein Fehler ist).

  WICHTIG - Mehrere Projekte auf demselben Rechner: Sensormeter PoE nutzt
  eine andere PlatformIO-Platform (den Community-Fork "pioarduino", noetig
  fuer W5500-Ethernet-Support unter Arduino-ESP32 3.x, siehe dortiges
  docs/entscheidungen.md) als die anderen drei Projekte (offizielles
  "espressif32", Arduino-ESP32 2.0.17). Beide registrieren Platform- und
  Framework-Pakete unter DENSELBEN Namen in PlatformIOs global geteiltem
  Paket-Pool (~/.platformio) - ein Sensormeter-PoE-Build ohne Isolation
  wuerde die von den anderen drei Projekten benoetigten Pakete
  ueberschreiben und deren naechsten Build zum Absturz bringen (real
  passiert, siehe sensormeter-poe/docs/entscheidungen.md). Das ist bereits
  geloest: Sensormeter PoEs eigene firmware/platformio.ini setzt
  "core_dir = .pio-core" und isoliert damit alle eigenen Pakete vollstaendig
  vom globalen Pool - dieses Skript muss dafuer nichts Zusaetzliches tun,
  einfach "pio run"/"pio run --target upload" wie bei den anderen drei
  Projekten auch. Wichtig ist nur: diese core_dir-Einstellung in Sensormeter
  PoEs platformio.ini darf nicht entfernt werden, sonst wiederholt sich das
  Problem beim naechsten gemeinsamen Einsatz auf demselben Rechner.

.PARAMETER Project
  "sensormeter", "wlan", "display" oder "poe". Wird weggelassen, fragt das
  Skript interaktiv nach (Eingabe von 1/2/3/4 oder dem Projektnamen).

.PARAMETER RepoPath
  Zielordner fuer den Checkout, falls das gewaehlte Repo dort noch nicht
  vorhanden ist. Default: ein projektspezifischer Ordnername neben diesem
  Skript (z.B. .\sensormeter-wlan).

.PARAMETER Port
  Serieller Port des Boards (z.B. COM5), falls die automatische Erkennung
  fehlschlaegt oder mehrere USB-Seriell-Adapter angeschlossen sind.

.PARAMETER SkipUpload
  Nur bauen, nicht flashen (z.B. um vorab zu pruefen, ob alles compiliert,
  ohne dass ein Board angeschlossen ist).

.EXAMPLE
  .\flash.ps1

.EXAMPLE
  .\flash.ps1 -Project wlan -Port COM5

.EXAMPLE
  .\flash.ps1 -Project display -RepoPath C:\Projekte\sensormeter-display -SkipUpload

.EXAMPLE
  .\flash.ps1 -Project poe -SkipUpload
#>

[CmdletBinding()]
param(
  [ValidateSet("sensormeter", "wlan", "display", "poe")]
  [string]$Project,
  [string]$RepoPath,
  [string]$Port,
  [switch]$SkipUpload
)

$ErrorActionPreference = "Stop"

# ------------------------------------------------------------------
# Versionierung dieses Skripts (unabhaengig von den Firmware-Versionen der
# einzelnen Projekte) - liegt identisch in vier Repos, daher hier verfolgt,
# damit erkennbar ist, ob eine lokal vorliegende Kopie veraltet ist.
#
# Changelog:
#   1.1.0 (2026-07-11) - Sensormeter PoE als viertes Projekt ergaenzt,
#                         Hinweis zur PlatformIO-Paket-Pool-Isolation
#                         zwischen "espressif32" und "pioarduino" ergaenzt.
#   1.0.0 (2026-07-10) - Erste versionierte Fassung (Sensormeter,
#                         Sensormeter WLAN, Sensormeter Display).
$FlashScriptVersion = "1.1.0"

$Projects = @{
  "sensormeter" = @{
    DisplayName = "Sensormeter (WT32-ETH01, Ethernet + bis zu 2 Sensoren)"
    RepoUrl     = "https://github.com/peterhagelhof7-cmd/sensormeter.git"
    FolderName  = "sensormeter"
    HasConfigH  = $true
    FlashNote   = "Board muss am USB-Seriell-Adapter (Debug-Burning-Schnittstelle, NICHT am 20-Pin-Hauptheader!) angeschlossen sein und sich im Boot-/Download-Modus befinden - siehe docs/flash-vorbereitung.pdf."
    SuccessNote = "Seriellen Monitor ansehen: pio device monitor (115200 Baud)."
  }
  "wlan" = @{
    DisplayName = "Sensormeter WLAN (generisches ESP32-WROOM-32-DevKit, WLAN-only)"
    RepoUrl     = "https://github.com/peterhagelhof7-cmd/sensormeter-wlan.git"
    FolderName  = "sensormeter-wlan"
    HasConfigH  = $true
    FlashNote   = "Board per USB-C-Kabel anschliessen - ggf. CP2102-/CH340-Treiber installieren."
    SuccessNote = "Beim ersten Start: WLAN ueber die Weboberflaeche einrichten - ohne gespeicherte Zugangsdaten versucht das Geraet nach 5 Minuten, dem Netz 'installer'/'installer' beizutreten (siehe docs/admin-guide.pdf Abschnitt 2.2)."
  }
  "display" = @{
    DisplayName = "Sensormeter Display (ESP32-Touchdisplay, HW-458B)"
    RepoUrl     = "https://github.com/peterhagelhof7-cmd/sensormeter-display.git"
    FolderName  = "sensormeter-display"
    HasConfigH  = $false
    FlashNote   = "Board per USB-Kabel anschliessen - ggf. CH340-Treiber installieren."
    SuccessNote = "Beim ersten Start: Touch-Kalibrierung durchfuehren, dann WLAN einrichten (siehe README.md)."
  }
  "poe" = @{
    DisplayName = "Sensormeter PoE (ESP32-S3-ETH, W5500 + WLAN, RJ45-Modul + Relais)"
    RepoUrl     = "https://github.com/peterhagelhof7-cmd/sensormeter-poe.git"
    FolderName  = "sensormeter-poe"
    HasConfigH  = $true
    FlashNote   = "Board per USB-C-Kabel anschliessen. Erster Build laedt eine eigene, isolierte PlatformIO-Toolchain herunter (siehe Skript-Hinweis oben zu 'pioarduino') - dauert deutlich laenger als bei den anderen drei Projekten und braucht Internetzugang. Bislang nur per 'pio run' gebaut/verifiziert, nicht auf echter Hardware getestet (siehe docs/entscheidungen.md) - beim ersten Flashen entsprechend aufmerksam pruefen."
    SuccessNote = "Beim ersten Start: RJ45-Modul-Erkennung laeuft automatisch waehrend des Boot-Countdowns; WLAN/Ethernet und ggf. MQTT ueber die Weboberflaeche einrichten (siehe docs/lastenheft.txt)."
  }
}

Write-Host "Sensormeter Flash-Skript v$FlashScriptVersion" -ForegroundColor DarkGray

function Write-Step {
  param([string]$Text)
  Write-Host ""
  Write-Host "==> $Text" -ForegroundColor Cyan
}

# ------------------------------------------------------------------
if (-not $Project) {
  Write-Host "Welches Projekt soll geflasht werden?"
  Write-Host "  1) $($Projects['sensormeter'].DisplayName)"
  Write-Host "  2) $($Projects['wlan'].DisplayName)"
  Write-Host "  3) $($Projects['display'].DisplayName)"
  Write-Host "  4) $($Projects['poe'].DisplayName)"
  do {
    $choice = Read-Host "Auswahl [1-4]"
    $Project = switch ($choice) {
      "1" { "sensormeter" }
      "2" { "wlan" }
      "3" { "display" }
      "4" { "poe" }
      "sensormeter" { "sensormeter" }
      "wlan" { "wlan" }
      "display" { "display" }
      "poe" { "poe" }
      default { $null }
    }
    if (-not $Project) { Write-Host "Ungueltige Eingabe - bitte 1, 2, 3, 4 oder den Projektnamen eingeben." -ForegroundColor Yellow }
  } while (-not $Project)
}

$Selected = $Projects[$Project]
Write-Step "Projekt: $($Selected.DisplayName)"

if (-not $RepoPath) {
  $RepoPath = Join-Path $PSScriptRoot $Selected.FolderName
}

function Update-SessionPath {
  $machinePath = [System.Environment]::GetEnvironmentVariable("Path", "Machine")
  $userPath = [System.Environment]::GetEnvironmentVariable("Path", "User")
  $env:Path = "$machinePath;$userPath"
}

# Funktionale Pruefung statt reiner PATH-Pruefung (siehe .DESCRIPTION oben:
# der Windows-eigene "python"-Store-Alias sieht fuer Get-Command vorhanden
# aus, ist aber kein echtes Python).
function Test-ToolWorks {
  param(
    [string]$Command,
    [string]$Pattern
  )
  try {
    $output = & $Command --version 2>&1 | Out-String
  } catch {
    return $false
  }
  if ($LASTEXITCODE -ne 0) { return $false }
  return ($output -match $Pattern)
}

function Install-ToolIfMissing {
  param(
    [string]$Name,
    [string]$Command,
    [string]$Pattern,
    [scriptblock]$InstallAction
  )
  Write-Step "Pruefe $Name..."
  if (Test-ToolWorks -Command $Command -Pattern $Pattern) {
    Write-Host "OK: $Name ist bereits vorhanden und funktionsfaehig."
    return
  }

  Write-Host "$Name nicht gefunden oder nicht funktionsfaehig - installiere..."
  & $InstallAction
  Update-SessionPath

  if (-not (Test-ToolWorks -Command $Command -Pattern $Pattern)) {
    throw "$Name ist nach der Installation weiterhin nicht nutzbar. Bitte Terminal/PowerShell neu starten und Skript erneut ausfuehren (PATH-Aenderungen greifen sonst erst in einer neuen Sitzung)."
  }
  Write-Host "OK: $Name einsatzbereit."
}

# ------------------------------------------------------------------
Install-ToolIfMissing -Name "Python" -Command "python" -Pattern "^Python \d" -InstallAction {
  winget install --id Python.Python.3.12 -e --source winget --accept-package-agreements --accept-source-agreements
}

# ------------------------------------------------------------------
Install-ToolIfMissing -Name "Git" -Command "git" -Pattern "^git version" -InstallAction {
  winget install --id Git.Git -e --source winget --accept-package-agreements --accept-source-agreements
}

# ------------------------------------------------------------------
Install-ToolIfMissing -Name "PlatformIO" -Command "pio" -Pattern "PlatformIO Core" -InstallAction {
  python -m pip install --upgrade pip
  python -m pip install --upgrade platformio
}

# ------------------------------------------------------------------
Write-Step "Pruefe Projekt-Checkout..."
$firmwarePath = Join-Path $RepoPath "firmware"

if (Test-Path (Join-Path $firmwarePath "platformio.ini")) {
  Write-Host "Repo bereits vorhanden unter $RepoPath"
  $status = git -C $RepoPath status --porcelain
  if ([string]::IsNullOrWhiteSpace($status)) {
    Write-Host "Keine lokalen Aenderungen - hole neueste Version (git pull)..."
    git -C $RepoPath pull
  } else {
    Write-Host "Lokale Aenderungen im Checkout gefunden - ueberspringe 'git pull', um nichts zu ueberschreiben." -ForegroundColor Yellow
  }
} else {
  Write-Step "Klone Repository nach $RepoPath ..."
  git clone $Selected.RepoUrl $RepoPath
  if ($LASTEXITCODE -ne 0) {
    throw "git clone fehlgeschlagen (Exitcode $LASTEXITCODE)"
  }
}

if (-not (Test-Path (Join-Path $firmwarePath "platformio.ini"))) {
  throw "firmware/platformio.ini wurde auch nach dem Checkout nicht gefunden - stimmt -RepoPath ($RepoPath)?"
}

# ------------------------------------------------------------------
if ($Selected.HasConfigH) {
  Write-Step "Pruefe config.h..."
  $configExample = Join-Path $firmwarePath "include\config.h.example"
  $configReal = Join-Path $firmwarePath "include\config.h"

  if (-not (Test-Path $configReal)) {
    Copy-Item $configExample $configReal
    Write-Host "include/config.h aus der Vorlage angelegt."
  } else {
    Write-Host "config.h existiert bereits - wird nicht ueberschrieben."
  }
}

# ------------------------------------------------------------------
Set-Location $firmwarePath

Write-Step "Baue Firmware (pio run)..."
pio run
if ($LASTEXITCODE -ne 0) {
  throw "Build fehlgeschlagen (pio-Exitcode $LASTEXITCODE)"
}

if ($SkipUpload) {
  Write-Host ""
  Write-Host "SkipUpload gesetzt - Build erfolgreich, kein Flash-Vorgang durchgefuehrt." -ForegroundColor Green
  exit 0
}

# ------------------------------------------------------------------
Write-Step "Verfuegbare serielle Ports:"
$ports = [System.IO.Ports.SerialPort]::GetPortNames()
if ($ports.Count -eq 0) {
  Write-Host "  (keine gefunden - ist das Board per USB angeschlossen?)" -ForegroundColor Yellow
} else {
  $ports | ForEach-Object { Write-Host "  - $_" }
}

Write-Host ""
Write-Host "Hinweis: $($Selected.FlashNote)" -ForegroundColor Yellow

Write-Step "Flashe Firmware (pio run --target upload)..."
if ($Port) {
  pio run --target upload --upload-port $Port
} else {
  pio run --target upload
}

if ($LASTEXITCODE -ne 0) {
  Write-Host ""
  Write-Host "Upload fehlgeschlagen. Haeufige Ursachen:" -ForegroundColor Red
  Write-Host "  - Falscher COM-Port (siehe Liste oben, ggf. mit -Port COM<n> erneut versuchen)"
  Write-Host "  - CH340-/CP2102-USB-Treiber fehlt (Geraete-Manager pruefen)"
  Write-Host "  - Board nicht angeschlossen oder nicht im Bootloader-/Download-Modus"
  throw "pio run --target upload fehlgeschlagen (Exitcode $LASTEXITCODE)"
}

Write-Host ""
Write-Host "Fertig! Firmware erfolgreich geflasht." -ForegroundColor Green
Write-Host $Selected.SuccessNote
