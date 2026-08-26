<#
.SYNOPSIS
  Konvertiert ein Anbieter-Logo (beliebiges Bildformat) in das fuer das
  Anbieter-Branding-Feature der Sensormeter-Familie kompatible Rohformat.

.DESCRIPTION
  Gemeinsames Werkzeug fuer alle vier Sensormeter-Projekte (liegt identisch
  in allen vier Repos, analog zu scripts/flash.ps1). Fragt zuerst, fuer
  welches Display konvertiert werden soll, und reduziert Aufloesung UND
  Farbtiefe konsequent auf das, was das jeweilige Display tatsaechlich
  darstellen kann - ein 24-Bit-Foto wird also nicht einfach verkleinert,
  sondern fuer die monochromen OLEDs auf echte 1-Bit-Schwarzweiss-Werte
  reduziert (kein Graustufen-"so tun als ob").

  Bekannte Displays der Familie - alle vier Projekte haben inzwischen ein
  implementiertes Branding-Feature (BrandingManager, siehe jeweiliges
  docs/entscheidungen.md):
    - Sensormeter (WT32-ETH01) / Sensormeter WLAN / Sensormeter PoE: OLED
      SSD1306, 128x64, 1 Bit monochrom (seit der familienweiten
      Display-Standardisierung, siehe sensormeter-poe/repo/docs/
      entscheidungen.md "Internes Display: SH1107 -> SSD1306").
    - Optionales externes SH1107-Display-Steckmodul (128x128, an
      Sensormeter/Sensormeter PoE, siehe sensormeter-family/repo/
      module-design/sh1107-display-modul.md): eigenes Logo-Format noch
      nicht ueber dieses Skript abgedeckt, bei Bedarf 'custom' waehlen.
    - Sensormeter Display: TFT ST7789P3 (Farbe) - bewusst DIESELBE
      Zielgroesse 128x64 wie die OLED-Projekte (nur RGB565 statt 1bpp),
      damit dieses Skript fuer alle vier Projekte eine einheitliche
      Logo-Groesse nutzt.

  Monochromes Ausgabeformat (Sensormeter/Sensormeter WLAN/Sensormeter PoE):
  exakt Breite/8 * Hoehe Byte, MSB-zuerst je Zeile, kein Padding - identisch
  zu dem, was Adafruit_GFX::drawBitmap() erwartet (die rotierenden
  OLED-Seiten nutzen dasselbe Format) und exakt was
  sensormeter-wlan/repo/firmware/src/BrandingManager.h als
  LOGO_WIDTH/LOGO_HEIGHT/LOGO_BYTES vorschreibt.

  Farb-Ausgabeformat (Sensormeter Display): RGB565, 2 Byte pro Pixel,
  Little-Endian, zeilenweise ohne Padding. ACHTUNG - nicht auf echter
  Hardware verifiziert: dieses Panel laeuft mit TFT_RGB_ORDER=0 (BGR) und
  zeigte selbst dabei einzelne falsch interpretierte Farben (siehe
  sensormeter-display/repo/docs/entscheidungen.md) - faerbt ein
  hochgeladenes Logo auf echter Hardware sichtbar falsch (Rot/Blau
  vertauscht), hilft der Schalter -SwapRedBlue.

  Das Quellbild wird seitenverhaeltnistreu in die Zielgroesse eingepasst
  (nicht verzerrt) und mit der Padding-Farbe (Default: Schwarz, der
  OLED-Hintergrundfarbe) zentriert aufgefuellt. Transparente Bereiche einer
  PNG-Quelle werden dabei automatisch mit der Padding-Farbe unterlegt.

.PARAMETER InputPath
  Pfad zum Quellbild (PNG/JPG/BMP/GIF - alles was .NET System.Drawing laden
  kann).

.PARAMETER Display
  sensormeter | poe | display | custom. Ohne Angabe fragt das Skript
  interaktiv (nummeriertes Menue, analog flash.ps1).

.PARAMETER Width
.PARAMETER Height
  Nur bei -Display custom: Zielaufloesung in Pixeln.

.PARAMETER ColorMode
  Nur bei -Display custom: mono oder color.

.PARAMETER OutputPath
  Zieldatei fuer die Rohdaten. Default: <Eingabename>-<Display>-<Breite>x<Hoehe>.bin
  neben dem Quellbild.

.PARAMETER Threshold
  Nur fuer monochrome Ziele: Schwellwert 0-255 fuer Schwarz/Weiss nach der
  Graustufen-Umwandlung. Default 128.

.PARAMETER Invert
  Nur fuer monochrome Ziele: kehrt die Schwarz/Weiss-Zuordnung um. Ohne
  diesen Schalter werden helle Quellpixel zu leuchtenden (1) und dunkle zu
  unbeleuchteten (0) Pixeln - bei einem Logo, das dunkel auf hellem
  Hintergrund gezeichnet ist, wirkt das Ergebnis dadurch invertiert
  (Hintergrund leuchtet, Logo bleibt dunkel); in dem Fall diesen Schalter
  setzen.

.PARAMETER PadColor
  Auffuellfarbe fuer den Bereich, der nach seitenverhaeltnistreuer
  Einpassung frei bleibt. Default Black (entspricht dem OLED-Hintergrund).
  Jeder von .NET Color.FromName erkannte Farbname oder ein Hex-Code
  (#RRGGBB) ist gueltig.

.PARAMETER SwapRedBlue
  Nur fuer Farbziele (-Display display / -Display custom -ColorMode
  color): vertauscht Rot- und Blau-Kanal beim RGB565-Packen. Escape-Hatch
  fuer den Fall, dass ein Logo auf Sensormeter Display sichtbar
  falschfarbig erscheint (siehe Hinweis oben zu TFT_RGB_ORDER) - einfach
  ausprobieren, ob mit oder ohne Schalter die Farben stimmen.

.EXAMPLE
  .\convert-logo.ps1 -InputPath .\firmenlogo.png -Display wlan
  Fragt nichts weiter, erzeugt firmenlogo-wlan-128x64.bin (1024 Byte).

.EXAMPLE
  .\convert-logo.ps1 -InputPath .\firmenlogo.png
  Interaktives Menue zur Display-Auswahl.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [ValidateSet('sensormeter', 'wlan', 'poe', 'display', 'custom')]
    [string]$Display,

    [int]$Width,
    [int]$Height,

    [ValidateSet('mono', 'color')]
    [string]$ColorMode,

    [string]$OutputPath,

    [ValidateRange(0, 255)]
    [int]$Threshold = 128,

    [switch]$Invert,

    [string]$PadColor = 'Black',

    [switch]$SwapRedBlue
)

$ScriptVersion = '1.1.0'

Add-Type -AssemblyName System.Drawing

if (-not (Test-Path -LiteralPath $InputPath -PathType Leaf)) {
    Write-Error "Eingabedatei nicht gefunden: $InputPath"
    exit 1
}
$InputPath = (Resolve-Path -LiteralPath $InputPath).Path

Write-Host "Sensormeter Logo-Konverter v$ScriptVersion" -ForegroundColor Cyan

# ----------------------------------------------------------------------------
# Display-Presets der Familie
# ----------------------------------------------------------------------------
$Presets = [ordered]@{
    'sensormeter' = @{ Name = 'Sensormeter / Sensormeter WLAN'; Note = 'OLED SSD1306'; W = 128; H = 64; Mode = 'mono' }
    'wlan'        = @{ Name = 'Sensormeter / Sensormeter WLAN'; Note = 'OLED SSD1306'; W = 128; H = 64; Mode = 'mono' }
    'poe'         = @{ Name = 'Sensormeter PoE'; Note = 'OLED SSD1306'; W = 128; H = 64; Mode = 'mono' }
    'display'     = @{ Name = 'Sensormeter Display'; Note = 'TFT ST7789P3, Farbe'; W = 128; H = 64; Mode = 'color' }
}

if (-not $Display) {
    Write-Host ''
    Write-Host 'Fuer welches Display soll konvertiert werden?'
    Write-Host '  1) Sensormeter / Sensormeter WLAN - OLED SSD1306, 128x64, 1-Bit monochrom'
    Write-Host '  2) Sensormeter PoE                - OLED SSD1306, 128x64, 1-Bit monochrom'
    Write-Host '  3) Sensormeter Display             - TFT ST7789P3, 128x64, 16-Bit Farbe (RGB565)'
    Write-Host '  4) Eigene Groesse / Farbtiefe (manuell)'
    $choice = Read-Host 'Auswahl (1-4)'
    switch ($choice) {
        '1' { $Display = 'sensormeter' }
        '2' { $Display = 'poe' }
        '3' { $Display = 'display' }
        '4' { $Display = 'custom' }
        default { Write-Error 'Ungueltige Auswahl.'; exit 1 }
    }
}

if ($Display -eq 'custom') {
    if (-not $Width) { $Width = [int](Read-Host 'Zielbreite in Pixeln') }
    if (-not $Height) { $Height = [int](Read-Host 'Zielhoehe in Pixeln') }
    if (-not $ColorMode) {
        $cm = Read-Host 'Farbtiefe - (m)onochrom 1-Bit oder (c)olor RGB565 [m/c]'
        $ColorMode = if ($cm -match '^c') { 'color' } else { 'mono' }
    }
    $TargetW = $Width
    $TargetH = $Height
    $TargetMode = $ColorMode
    $DisplayLabel = "Eigene Groesse ${Width}x${Height}, $ColorMode"
} else {
    $preset = $Presets[$Display]
    $TargetW = $preset.W
    $TargetH = $preset.H
    $TargetMode = $preset.Mode
    $DisplayLabel = "$($preset.Name) ($($preset.Note))"
}

Write-Host ''
Write-Host "Ziel: $DisplayLabel -> ${TargetW}x${TargetH}, $(if ($TargetMode -eq 'mono') { '1-Bit monochrom' } else { '16-Bit Farbe (RGB565)' })" -ForegroundColor Yellow

# ----------------------------------------------------------------------------
# Bild laden, seitenverhaeltnistreu einpassen, mit PadColor auffuellen
# ----------------------------------------------------------------------------
$srcImage = [System.Drawing.Image]::FromFile($InputPath)
$srcBpp = [System.Drawing.Image]::GetPixelFormatSize($srcImage.PixelFormat)
Write-Host "Quellbild: $($srcImage.Width)x$($srcImage.Height), $srcBpp Bit/Pixel ($($srcImage.PixelFormat))"

try {
    $pad = [System.Drawing.Color]::FromName($PadColor)
    if (-not $pad.IsKnownColor -and $PadColor -match '^#?[0-9A-Fa-f]{6}$') {
        $hex = $PadColor.TrimStart('#')
        $pad = [System.Drawing.Color]::FromArgb(
            [Convert]::ToInt32($hex.Substring(0, 2), 16),
            [Convert]::ToInt32($hex.Substring(2, 2), 16),
            [Convert]::ToInt32($hex.Substring(4, 2), 16))
    }
} catch {
    $pad = [System.Drawing.Color]::Black
}

$canvas = New-Object System.Drawing.Bitmap $TargetW, $TargetH, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$gfx = [System.Drawing.Graphics]::FromImage($canvas)
$gfx.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$gfx.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$gfx.Clear($pad)

$scale = [Math]::Min($TargetW / $srcImage.Width, $TargetH / $srcImage.Height)
$drawW = [int][Math]::Round($srcImage.Width * $scale)
$drawH = [int][Math]::Round($srcImage.Height * $scale)
$offsetX = [int](($TargetW - $drawW) / 2)
$offsetY = [int](($TargetH - $drawH) / 2)
$gfx.DrawImage($srcImage, $offsetX, $offsetY, $drawW, $drawH)
$gfx.Dispose()
$srcImage.Dispose()

Write-Host "Seitenverhaeltnistreu eingepasst auf ${drawW}x${drawH}, zentriert, Rand mit $PadColor aufgefuellt."

# ----------------------------------------------------------------------------
# Farbtiefe auf das reduzieren, was das Display tatsaechlich anzeigen kann
# ----------------------------------------------------------------------------
if (-not $OutputPath) {
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($InputPath)
    $srcDir = [System.IO.Path]::GetDirectoryName($InputPath)
    $OutputPath = Join-Path $srcDir "$baseName-$Display-${TargetW}x${TargetH}.bin"
}

if ($TargetMode -eq 'mono') {
    $rowBytes = [Math]::Ceiling($TargetW / 8)
    $bytes = New-Object byte[] ($rowBytes * $TargetH)

    for ($y = 0; $y -lt $TargetH; $y++) {
        for ($x = 0; $x -lt $TargetW; $x++) {
            $px = $canvas.GetPixel($x, $y)
            # Alpha bereits gegen PadColor geflacht (Clear() + DrawImage auf
            # opakem Canvas), Graustufenwert per Standard-Luminanzformel.
            # [int]-Cast noetig: $px.R/.G/.B sind System.Byte, 0.299*Byte
            # bleibt in PowerShell ein Byte-Ausdruck und ueberliefe sonst
            # stillschweigend (siehe [int]-Casts weiter unten im
            # RGB565-Zweig fuer denselben Effekt bei -shl).
            $gray = [int](0.299 * [int]$px.R + 0.587 * [int]$px.G + 0.114 * [int]$px.B)
            $lit = $gray -gt $Threshold
            if ($Invert) { $lit = -not $lit }
            if ($lit) {
                $byteIdx = $y * $rowBytes + [Math]::Floor($x / 8)
                $bit = 7 - ($x % 8)
                $bytes[$byteIdx] = $bytes[$byteIdx] -bor (1 -shl $bit)
            }
        }
    }

    [System.IO.File]::WriteAllBytes($OutputPath, $bytes)
    Write-Host ''
    Write-Host "Farbtiefe reduziert: $srcBpp Bit/Pixel -> 1 Bit/Pixel (2 Farben, Schwellwert $Threshold$(if ($Invert) { ', invertiert' }))." -ForegroundColor Green
    Write-Host "Geschrieben: $OutputPath ($($bytes.Length) Byte, erwartet $($rowBytes * $TargetH) Byte fuer ${TargetW}x${TargetH})" -ForegroundColor Green
    Write-Host "Direkt ueber die Einstellungsseite (Anbieter-Branding -> Logo hochladen) hochladbar." -ForegroundColor Green
} else {
    $bytes = New-Object byte[] ($TargetW * $TargetH * 2)
    $i = 0
    for ($y = 0; $y -lt $TargetH; $y++) {
        for ($x = 0; $x -lt $TargetW; $x++) {
            $px = $canvas.GetPixel($x, $y)
            # [int]-Cast zwingend: PowerShells -shl/-shr behalten den Typ
            # des LINKEN Operanden bei (System.Drawing.Color.R/.G/.B sind
            # System.Byte), ein Byte -shl 11 ueberlaeuft dadurch
            # stillschweigend statt auf Int32 zu erweitern (getestet -
            # ohne Cast wurde aus reinem Weiss z.B. 0x00FF statt 0xFFFF).
            $r5 = [int]$px.R -shr 3
            $g6 = [int]$px.G -shr 2
            $b5 = [int]$px.B -shr 3
            # -SwapRedBlue: Escape-Hatch fuer Panels mit vertauschter
            # Farbkanal-Interpretation (siehe Sensormeter Display,
            # TFT_RGB_ORDER=0/BGR - nicht auf echter Hardware verifiziert,
            # ob Standard-RGB565 oder getauschtes Packen richtig ist).
            if ($SwapRedBlue) { $tmp = $r5; $r5 = $b5; $b5 = $tmp }
            $rgb565 = ([int]$r5 -shl 11) -bor ([int]$g6 -shl 5) -bor [int]$b5
            # Little-Endian (Low-Byte zuerst) - passend zu einem direkten
            # memcpy eines uint16_t[]-Arrays auf der (Little-Endian-)
            # ESP32-Zielplattform.
            $bytes[$i] = $rgb565 -band 0xFF
            $bytes[$i + 1] = ($rgb565 -shr 8) -band 0xFF
            $i += 2
        }
    }

    [System.IO.File]::WriteAllBytes($OutputPath, $bytes)
    Write-Host ''
    Write-Host "Farbtiefe reduziert: $srcBpp Bit/Pixel -> 16 Bit/Pixel RGB565 (65.536 Farben)." -ForegroundColor Green
    Write-Host "Geschrieben: $OutputPath ($($bytes.Length) Byte, erwartet $($TargetW * $TargetH * 2) Byte fuer ${TargetW}x${TargetH})" -ForegroundColor Green
    Write-Host "Direkt ueber die Einstellungsseite (Branding-Logo -> Logo hochladen) hochladbar." -ForegroundColor Green
    if (-not $SwapRedBlue) {
        Write-Host "Hinweis: Faerbt das Logo auf dem Geraet sichtbar falsch (Rot/Blau vertauscht), erneut mit -SwapRedBlue versuchen - nicht auf echter Hardware verifiziert, welche Variante hier korrekt ist." -ForegroundColor Yellow
    }
}

$canvas.Dispose()
