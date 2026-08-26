# Entscheidungsprotokoll — Sensormeter Display

## Mehrfach-Sensormeter-Ziele, Geraete-Info-Dialog, Static-IP (Erweiterung ueber lastenheft.txt 7.3 hinaus)

Nutzerwunsch, ueber das urspruengliche Lastenheft hinaus: statt genau eines
Sensormeter-Ziels sollen bis zu 5 (mindestens 1) abfragbar sein, je mit
automatisch generiertem Slide; zusaetzlich ein Info-Dialog fuers Display
selbst (Systemname/IP/DHCP-Static) und echte Static-IP-Konfiguration. Noch
NICHT an echter Hardware verifiziert (kein Sensormeter-Geraet zum Testen
erreichbar) - nur Build+Boot am Geraet bestaetigt. Bei Gelegenheit mit
echtem Sensormeter nachpruefen.

### PRO-Erkennung ueber den Typ-String, nicht ueber eine eigene Konfiguration
Das Sensormeter-Projekt leitet `systemType` automatisch aus `sensor2Enabled`
ab (`deriveSystemType()` in dessen `ConfigManager.cpp`: `"Sensormeter PRO"`
vs. `"Sensormeter"`) - kein separates Konfigurationsfeld. Die Display-Seite
prueft daher einfach auf den exakten String `"Sensormeter PRO"` (OID
`.1.3.0`) statt eine eigene Heuristik zu bauen. Sensor-2-Temperatur/-Feuchte
werden nur abgefragt, wenn dieser Typ erkannt wurde - beim normalen Typ
liefert das Sensormeter-Geraet dafuer ohnehin durchgehend `0` (kein Fehler),
das waere sonst nicht von einer echten Nullmessung unterscheidbar gewesen.

### SnmpClient um getString() erweitert (Refactor auf gemeinsamen Kern)
`SnmpClient` konnte bisher nur INTEGER-Werte lesen (Temperatur/Feuchte).
Fuer Systemname/-typ/Sensor-Namen (SNMP OCTET STRING) wurde die BER-
Response-Verarbeitung in eine gemeinsame private `getRaw()`-Methode
extrahiert (liefert Tag-gefiltert die Rohbytes des letzten passenden
TLV-Elements), `getInteger()`/`getString()` interpretieren die Bytes nur
noch unterschiedlich. Vermeidet Duplikation der Request-Kodierung/
Response-Iteration zwischen beiden Methoden.

### SensormeterManager: Identitaet einmalig pro Ziel, danach nur Messwerte
Pro Ziel wird zuerst Systemname+Typ+Sensor-Name(n) aufgeloest (mehrere
sequentielle SNMP-GETs in einem Rutsch - passiert nur einmal beim
Hinzufuegen/Aendern eines Ziels, ein laengerer Blockierzeitraum bei einem
gerade neu eingetragenen, noch nicht erreichbaren Ziel ist dafuer
akzeptabel, der Nutzer sitzt ohnehin in den Einstellungen). Danach werden
nur noch die eigentlichen Messwerte im Round-Robin aktualisiert (ein Ziel
pro `update()`-Aufruf, `kCheckIntervalMs=4000`) - analog zu
`PingManager::pingNextTarget()`, damit ein nicht erreichbares Ziel nicht
den Hauptloop blockiert.

### SensormeterView rotiert intern durch alle Sensor-Slides
Da "Sensormeter" im Static-Modus weiterhin ein einzelner Menuepunkt bleibt
(Nutzerentscheidung, keine 10 einzelnen Static-Eintraege), aber bis zu 10
Sensor-Slides (5 Ziele x 2 Sensoren bei PRO) anzeigen koennen muss, verwaltet
`SensormeterView` selbst einen Rotationsindex + eigenen `millis()`-Timer
(`kSubSlideIntervalMs=4000`) - unabhaengig davon, wie oft `draw()` vom
Hauptloop aufgerufen wird (gleiches Prinzip wie das WLAN-Blinksymbol in
StatusBar). `main.cpp` behandelt `DataSource::Sensormeter` deshalb wie
`Uhrzeit` als Quelle, die auch ohne neue Messung periodisch neu gezeichnet
werden muss (`periodicDue`), damit die interne Rotation ueberhaupt sichtbar
wird.

### Community bleibt touch-uneditierbar, nur ueber Web
Bestehende Entscheidung uebernommen: das Zifferntastenfeld deckt keine
Buchstaben ab, eine volle Bildschirmtastatur waere fuer dieses eine, selten
geaenderte, gemeinsame Feld unverhaeltnismaessig. Touch-UI zeigt die
Community nur lesend an, Aendern nur ueber das Webinterface.

### Mindestens 1 Sensormeter-Ziel: in SettingsManager statt nur in der UI erzwungen
`removeSensormeterTarget()` verweigert das Entfernen, wenn nur noch ein
Ziel uebrig ist - zentral in `SettingsManager` statt getrennt in Touch- und
Web-UI, damit beide Oberflaechen nicht unabhaengig voneinander auf diese
Regel achten muessen. Ein frisch geflashtes Geraet startet trotzdem bei 0
Zielen (wie bei den Ping-Zielen) - die Regel greift erst, sobald das erste
Ziel hinzugefuegt wurde.

### UiHelpers::drawCloseButton() extrahiert (zweiter Verbraucher: InfoUI)
Der "X"-Schliessen-Button existierte bisher nur lokal in `SettingsUI.cpp`.
Mit `InfoUI` als zweitem Verbraucher nach `UiHelpers.h/.cpp` verschoben -
gleiches Muster wie zuvor bei `hitRect`/`waitForTapEvent` (dort ebenfalls
erst bei einem zweiten Verbraucher ausgelagert, nicht vorzeitig).

### Info-Dialog nutzt das bereits bestehende `deviceName()`, keine neue Einstellung
Die urspruengliche Frage war, ob fuer den Info-Dialog ein neuer
"Systemname" fuer das Display selbst noetig ist. `SettingsManager` hatte
bereits ein `deviceName()`-Feld (Default "Sensormeter Display", ueber das
Webinterface als "Systemname" editierbar, bisher nur fuer Seitentitel/
Ueberschrift der Weboberflaeche genutzt) - dieses wird jetzt zusaetzlich im
Info-Dialog angezeigt, keine neue Einstellung noetig.

### Static-IP: eigenes Formular mit sofortigem Neustart
Netzwerkeinstellungen (`WiFi.config()`) wirken erst bei der naechsten
Verbindung, nicht auf eine bereits bestehende Session - das Speichern der
Static-IP-Einstellungen im Webinterface loest daher bewusst einen
sofortigen `ESP.restart()` aus (wie beim OTA-Update), statt den Nutzer im
Unklaren zu lassen, warum die Aenderung "nicht wirkt". Gespeichert im
selben NVS-Namespace wie die WLAN-Zugangsdaten (`WlanManager`, nicht
`SettingsManager`), da es sich um Verbindungskonfiguration handelt, nicht
um eine App-Einstellung.

## RGB-Status-LED leuchtete dauerhaft trotz "aus"

### Bug gefunden und behoben: LED-Polaritaet umgekehrt (gemeinsame Anode statt Kathode)
Nutzer meldete, die RGB-Status-LED leuchte durchgehend, obwohl kein
Ping-Alarm aktiv war. `LedManager` ging bisher von gemeinsamer Kathode aus
(HIGH=an, siehe fruehere, im Code selbst als unverifiziert markierte
Annahme aus P7/P8). Tatsaechlich gemeinsame Anode (LOW=an) - im
vermeintlichen "aus"-Zustand (alle drei Pins LOW) waren dadurch alle drei
Farbkanaele gleichzeitig an. Behoben durch Invertieren der Pegel in
`LedManager::setColor()`; am Geraet verifiziert (LED jetzt tatsaechlich aus
ohne aktiven Alarm).

## Farbtest (Rot/Gelb/Blau/Gruen/Weiss): echte Ursache gefunden, RGB/BGR-Diagnose war eine Sackgasse

### Korrektur zum Abschnitt "TFT_RGB_ORDER falsch konfiguriert" weiter unten
Die dortige Diagnose und der "Fix" (`TFT_RGB_ORDER=TFT_RGB`) waren **falsch**.
Systematischer Test mit vollflaechigem Hintergrund in Rot/Gelb/Gruen/Blau/
Weiss deckte auf:

1. `TFT_RGB_ORDER=TFT_RGB`/`TFT_BGR` als Build-Flag ist bei
   `USER_SETUP_LOADED=1` wirkungslos: `User_Setup_Select.h` definiert die
   Token `TFT_RGB`/`TFT_BGR` (1/0) nur, wenn `USER_SETUP_LOADED` NICHT
   gesetzt ist - bei uns aber schon. Der Praeprozessor wertet
   `TFT_RGB_ORDER == 1` dadurch immer als `false` (undefiniertes Token = 0),
   **unabhaengig davon, ob dort `TFT_RGB` oder `TFT_BGR` steht** - verifiziert
   per `gcc -E`. Der fruehere "Fix" auf `TFT_RGB` hat also nie etwas
   veraendert, die MADCTL-Farbreihenfolge war die ganze Zeit BGR.
2. Nachdem das behoben wurde (numerischer Wert `TFT_RGB_ORDER=1` statt
   Token), zeigte sich: Rot->Gelb, Gelb->Rot, Gruen->Rosa, Cyan->Blau,
   Magenta->Gruen, Blau->Blau (korrekt) - UND **Weiss->Schwarz**. Ein
   Rechenmodell aus den Messpunkten (`OutR = NICHT B`, `OutB = NICHT R`,
   `OutG = R UND NICHT G`) erklaerte alle sechs Buntfarb-Messungen exakt,
   sagte aber auch voraus, dass **kein** Eingabewert reines Weiss erzeugen
   kann - bestaetigt durch den Test. Reines Zuruecksetzen auf
   `TFT_RGB_ORDER=0` (BGR, numerisch diesmal korrekt wirksam) behob das
   Weiss-Problem NICHT (weiterhin Schwarz) - die MADCTL-Reihenfolge war also
   nie die eigentliche Ursache, nur ein Nebenschauplatz.

### Bug gefunden und behoben: TFT_eSPI schaltet INVON standardmaessig ein
Tatsaechliche Ursache: `TFT_Drivers/ST7789_Init.h` sendet im Standard-
Initialisierungsablauf unbedingt den Befehl `ST7789_INVON` (Farbinvertierung
an). Dieses konkrete Panel braucht das nicht/nicht so - mit `INVON` aktiv
wurde Weiss zu Schwarz und alle Buntfarben verfaelscht dargestellt. Behoben
mit einem expliziten `tft.invertDisplay(false)` direkt nach `tft.init()` in
`DisplayManager::begin()`. Nach dem Fix (kombiniert mit `TFT_RGB_ORDER=0`,
siehe oben) wurden Weiss, Schwarz, Rot, Gruen und Blau alle korrekt am
echten Geraet verifiziert - keine Farbsubstitutionen mehr noetig,
`TFT_RED`/`TFT_GREEN`/etc. koennen unveraendert verwendet werden.

**Fuer spaetere Fehlersuche:** Dieser Bug erklaert rueckwirkend vermutlich
auch den weiter unten dokumentierten "TFT_LIGHTGREY praktisch unsichtbar"-
Befund - mit aktivem `INVON` waere der (vermeintlich weisse) Hintergrund in
Wirklichkeit schwarz gewesen, und invertiertes Hellgrau (~definitionsgemaess
dunkel) haette auf einem tatsaechlich schwarzen Hintergrund ebenfalls kaum
Kontrast gehabt - ein in sich konsistentes Bild, nur mit vertauschter
Ausgangsannahme (es war nie "Hellgrau auf Weiss", sondern vermutlich
"dunkles Grau auf Schwarz").

### Statusleisten-Symbolfarbe: zurueck zu Hellgrau moeglich, dann auf Dunkelgrau angepasst
Mit behobener Invertierung wurde `TFT_LIGHTGREY` (lastenheft-konform) erneut
getestet und war diesmal tatsaechlich sichtbar (nicht mehr mit dem
Hintergrund verschmolzen) - bestaetigt, dass der fruehere "Schwarz"-Kompromiss
durch den INVON-Bug verursacht war, nicht durch eine generelle
Panel-Schwaeche. Nutzer fand den Kontrast trotzdem noch zu schwach; auf
`TFT_DARKGREY` (128,128,128) gewechselt - naeher am Lastenheft-Wortlaut
"hellgrau" als das vorherige Schwarz, aber mit ausreichend Kontrast auf
Weiss.

### Zahnrad-Symbol: erst Schraubenschluessel probiert, dann verworfen
Auf Nutzerwunsch testweise durch ein stilisiertes Schraubenschluessel-Symbol
ersetzt (diagonaler Schaft mit zwei unterschiedlich grossen Ringkoepfen,
angelehnt an eine vom Nutzer bereitgestellte Referenzgrafik). Bei der
kleinen Symbolgroesse (~22px) wirkte das Ergebnis nicht ueberzeugend genug
und wurde auf Nutzerwunsch wieder verworfen. Stattdessen ein kraeftigeres
Zahnrad: gefuellter Ring (vorher duenne Kreislinien) mit ausgeschnittener
Mitte (`fillCircle` in `bgColor` ueber dem massiven Ring) und Zaehnen als
gefuellte Dreiecke statt duenner Linien - Ring-Innenradius `r-5` (nach
Nutzer-Iteration ueber `r-4`, `r-7`) fuer eine "fleischigere" Optik.

## Bildschirm "zitterte": bedingungslose Full-Redraws statt Redraw-nur-bei-Aenderung

### Bug gefunden und behoben: sichtbares Flackern durch unnoetige Redraws
Nach dem Statusleisten-Sichtbarkeits-Fix meldete der Nutzer sichtbares
Zittern/Flackern des Displays im Normalbetrieb. Ursache: mehrere
Zeichenfunktionen fuehrten bei jedem Aufruf bedingungslos einen vollen
`fillRect` + Neuzeichnen aus, auch wenn sich der angezeigte Wert seit dem
letzten Aufruf gar nicht geaendert hatte - der fillRect-dann-Text-Ablauf
brauchte messbar lange genug (SPI-Uebertragung), um als kurzes Aufblitzen
sichtbar zu sein:

- `StatusBar::draw()` wurde alle 300ms aufgerufen (noetig fuers WLAN-Blink-
  Symbol, siehe aeltere Notiz oben in dieser Datei), zeichnete aber bei
  jedem Aufruf blind neu, auch wenn WLAN-Balken/Temperatur/Uhrzeit exakt
  gleich blieben.
- Der Hauptloop erzwang zusaetzlich alle 5s (`kPeriodicRedrawIntervalMs`)
  einen Redraw des Inhaltsbereichs unabhaengig von der aktiven Datenquelle -
  noetig eigentlich nur fuer die Uhrzeit-Ansicht (deren Anzeige sich ohne
  neue Messung rein zeitgesteuert aendert), traf aber auch Graph/Ping-
  Ansichten, die bereits ueber tatsaechliche Poll-Ereignisse redraw-getriggert
  werden.
- `PingView::drawAverage()`/`drawTargetList()` (Ping-Poll alle 2s) und
  `GraphManager::drawFullScreen()` (DHT11-Poll alle 5s) zeichneten bei jedem
  Poll neu, auch wenn der gerundete Anzeigewert unveraendert war - bei Ping
  alle 2s am staerksten wahrnehmbar.

Behoben nach demselben Muster in allen vier Stellen: letzten gezeichneten
Zustand zwischenspeichern (gerundete Werte bzw. eine zusammengesetzte
Signatur bei der Ping-Zielliste) und den eigentlichen Redraw nur ausfuehren,
wenn sich etwas tatsaechlich geaendert hat. Fuer `StatusBar` bleibt die
WLAN-Blink-Animation (`bars < 0`) als Ausnahme bestehen, die weiterhin bei
jedem 300ms-Tick neu zeichnet. `periodicDue` im Hauptloop ist jetzt auf
`activeSource == DataSource::Uhrzeit` beschraenkt.

**Noch nicht angefasst:** `SensormeterView::draw()` hat denselben
bedingungslosen Redraw-bei-jedem-Poll-Aufbau, aber bei einem 30s-Poll-
Intervall (`SensormeterClient::kPollIntervalMs`) faellt das kaum auf -
bewusst nicht ungetestet geaendert, da aktuell kein Sensormeter-Geraet zum
Verifizieren erreichbar war. Bei Gelegenheit nachziehen, falls dort
ebenfalls Flackern auffaellt.

## Erster echter Test im Hauptbetrieb (nach WLAN-Verbindung): Statusleisten unsichtbar

### Bug gefunden und behoben: TFT_RGB_ORDER falsch konfiguriert
Nach erfolgreicher WLAN-Verbindung (erster Durchlauf des Hauptloops auf
echter Hardware) war die obere UND untere Statusleiste komplett
unsichtbar/schwarz, obwohl `StatusBar::draw()` nachweislich korrekt und mit
plausiblen Werten aufgerufen wurde (per Serial-Debug verifiziert). Test mit
knallrotem statt hellgrauem Symbol zur Eingrenzung: `TFT_RED` erschien auf
dem echten Panel als **Türkis**, nicht Rot - klassischer Hinweis auf
vertauschte Farbkanal-Reihenfolge. `platformio.ini` hatte
`TFT_RGB_ORDER=TFT_BGR` gesetzt, das Panel ist aber tatsaechlich RGB-nativ.
Behoben durch Aendern auf `TFT_RGB_ORDER=TFT_RGB`.

**Wichtig:** Dieser Fehler war nicht auf die Statusleiste beschraenkt,
sondern haette jede explizite Farbe mit ungleichen R/G/B-Anteilen falsch
dargestellt - u. a. den roten Alarm-Hintergrund bei Ping-Ausfall
(lastenheft.txt Abschnitt 9) sowie die Graph-Farben (Temperatur rot,
Luftfeuchte blau). Reine Grautoene (R≈G≈B) sind von diesem Fehler nicht
sichtbar betroffen, weshalb er beim allgemeinen Boot-Bildschirm-Test zuvor
nicht auffiel.

### Symbolfarbe von hellgrau auf schwarz geaendert (Abweichung vom Lastenheft)
`lastenheft.txt` Abschnitt 4 verlangt "hellgrau" fuer die Statusleisten-
Symbole. Auch nach dem RGB/BGR-Fix blieb die Original-`TFT_LIGHTGREY`-Farbe
auf dem echten 2,8"-ST7789P3-Panel praktisch nicht von Weiss unterscheidbar
(Hardware-Befund, vermutlich begrenzte Graustufen-Differenzierung dieses
guenstigen Panels). Auf schwarz umgestellt fuer tatsaechliche Lesbarkeit -
bewusste Abweichung vom woertlichen Lastenheft-Wortlaut zugunsten der
dahinterliegenden Absicht (eine erkennbare Statusleiste). Interessanter
Nebenbefund: `TFT_BLACK` erscheint auf diesem Panel selbst als **weiss**
(vermutlich Interaktion mit der Panel-eigenen Inversionseinstellung) - das
Ergebnis ist trotzdem gut lesbar (weisse Symbole, siehe Test durch Nutzer),
daher nicht weiter verfolgt. Graph-Linienfarben (rot/blau) und der rote
Alarm-Hintergrund sind nach dem RGB/BGR-Fix noch nicht am echten Geraet
verifiziert (noch nicht genug DHT11-Messwerte fuer den Graph vorhanden) -
bei Gelegenheit nachpruefen.

## Erster echter Touch-Test: X/Y-Kanal vertauscht

### Bug gefunden und behoben: Touch-Controller liefert X/Y vertauscht
Beim ersten Test der neuen 4-Punkt-Kalibrierung auf echter Hardware kollabierten
`xMin`/`xMax` bzw. `yMin`/`yMax` immer wieder auf nahezu denselben Wert
(wenige Rohwert-Einheiten Unterschied), unabhaengig von Tippgenauigkeit,
Zielgroesse oder Haltedauer der Beruehrung - drei nacheinander ausprobierte
Erklaerungen (unzureichend eingeschwungener Rohwert, zu kleines/schwer
treffbares Kalibrierziel, zu kurzer Tipp) brachten keine Besserung.

Per-Sample-Debug-Log (`docs`/Kommentare in `TouchManager.cpp` zwischenzeitlich
entfernt, siehe Git-Historie) zeigte den eigentlichen Grund: Kommando 0xD0
(`kCmdReadX`) liefert auf dieser Verkabelung tatsaechlich die **vertikale**
Position, Kommando 0x90 (`kCmdReadY`) die **horizontale** - die beiden Kanaele
sind gegenueber der XPT2046-Standardannahme vertauscht. Die 4-Punkt-Mittelung
mittelte dadurch z. B. "oben-links + unten-links" (fuer `xMin` gedacht), was
auf dieser Verkabelung in Wirklichkeit eine hohe und eine niedrige
*vertikale* Ablesung sind - kollabiert folgerichtig auf einen bedeutungslosen
Mittelwert, komplett unabhaengig von der echten X-Position.

Behoben durch Vertauschen der Kanalzuweisung in `TouchManager::readRaw()`
(`rawX = readChannel(kCmdReadY)`, `rawY = readChannel(kCmdReadX)`) statt die
Kommandokonstanten selbst umzubenennen - die Namen `kCmdReadX`/`kCmdReadY`
bleiben an den XPT2046-Standardnamen orientiert, nicht an dieser
konkreten Verkabelung. Nach dem Fix stimmen alle vier Kalibrierecken und
freie Tipps praezise mit der angetippten Bildschirmposition ueberein
(auf echter Hardware verifiziert).

Die waehrend der Fehlersuche ergaenzte Setzzeit+Mittelung in `waitForTap()`
sowie das groessere Fadenkreuz-Kalibrierziel (statt 4px-Punkt) waren nicht
die Ursache, bleiben aber als sinnvolle Robustheit gegen einzelne Ausreisser
bzw. schlecht treffbare Ziele bestehen.

## Webserver + OTA (Zusatzfunktion nach P0-P8)

### Async statt synchron (ESPAsyncWebServer/AsyncTCP)
Wie im Sensormeter-Projekt: async, damit HTTP-Anfragen den Hauptloop
(Touch, DHT11/Sensormeter/Ping-Polling, Snake) nicht blockieren. Dieselben,
dort bereits bewaehrten Bibliotheksversionen uebernommen
(`esp32async/ESPAsyncWebServer@^3.11.2`, `esp32async/AsyncTCP@^3.4.10`).

### Kein REST-API/JSON, kein HTTPS-Client
Anders als das Sensormeter-Projekt (dort: Dashboard + REST-API) geht es
hier nur um ein Einstellungsformular - ArduinoJson wurde daher bewusst
NICHT hinzugefuegt (spart Flash, keine funktionale Notwendigkeit). Ebenso
kein HTTPS-Client fuer einen Remote-Versionscheck (hätte laut Sensormeter-
Projekt allein ca. 168 KB gekostet) - Firmware-Update nur per lokalem
`.bin`-Upload, dafuer ein Link zu den GitHub-Releases zum manuellen
Herunterladen (identisches Vorgehen wie dort).

### Custom-Partitionstabelle fuer OTA (partitions_ota.csv) - Korrektur, war unnoetig

**Korrektur (nachtraeglich, beim Entwurf des Sensormeter-WLAN-Projekts
aufgefallen):** Die Aussage unten, das PlatformIO-Standardschema habe nur
eine einzelne App-Partition, war **falsch** und wurde nie tatsaechlich
gegen den unveraenderten Standard geprueft - nur angenommen. Ein
tatsaechlicher Build des Sensormeter-Projekts (WT32-ETH01, identisches
`board = esp32dev`, keine eigene `partitions.csv`) zeigt per
`gen_esp32part.py`, dass das Standardschema fuer dieses Board bereits
`ota_0`/`ota_1` (je 1280K) mitbringt, dazu `spiffs`/LittleFS (1408K) und
eine `coredump`-Partition (64K) - OTA haette also auch ohne eigene Tabelle
funktioniert.

Die eigene `partitions_ota.csv` (nvs 20K, otadata 8K, app0/app1 je 1280K,
spiffs 1472K) bleibt trotzdem bestehen, da sie funktional gleichwertig ist
und bereits produktiv verifiziert wurde (siehe unten) - der einzige
tatsaechliche Unterschied zum Standard ist der Verzicht auf die
64K-Coredump-Partition zugunsten von 64K mehr LittleFS, was fuer die
24-Zeilen-`history.csv` ohnehin irrelevant ist. Eine Ruecknahme auf den
Standard wuerde keinen Vorteil bringen und nur unnoetiges Risiko fuer
bereits geflashte Geraete schaffen.

Urspruengliche (nicht mehr zutreffende) Begruendung, zur Nachvollziehbarkeit
belassen: "OTA braucht zwei App-Slots (ota_0/ota_1) statt der einen
App-Partition im PlatformIO-Standardschema." Eigene Tabelle auf dem
4-MB-Flash (32 Mbit laut Datenblatt): nvs 20K, otadata 8K, app0 1280K,
app1 1280K, spiffs (LittleFS) 1472K. app0/app1 sind genauso gross wie die
bisherige einzelne App-Partition (aktuelle Firmware liegt bei ~73%,
~350 KB Reserve je Slot). Verifiziert mit `gen_esp32part.py` gegen die
tatsaechlich generierte `partitions.bin` - Ausgabe zeigt exakt die
erwarteten ota_0/ota_1/spiffs-Eintraege (nur die falsche Praemisse, dass
dies noetig sei, wurde nicht hinterfragt).

**Wichtig fuer spaetere Firmware-Updates:** Ein Geraet, das bereits mit der
alten (Single-App-)Partitionstabelle geflasht wurde, braucht vor dem ersten
Flash dieser Version einen vollstaendigen Flash-Loeschvorgang
(`pio run --target erase` bzw. `esptool.py erase_flash`), da sich NVS-/
App-Offsets verschoben haben. Bisher ist noch kein reales Geraet mit einer
Vorversion geflasht worden, betrifft also aktuell niemanden praktisch.

### Bug gefunden und behoben: Datenrennen auf SettingsManager (fehlender Mutex)
ESPAsyncWebServer verarbeitet Anfragen in einem eigenen FreeRTOS-Task, NICHT
im Arduino-Hauptloop. `SettingsManager` wird aber sowohl vom Hauptloop/
Touch-UI als auch von den Web-Handlern gelesen und geschrieben (z. B.
Sensormeter-Ziel, Ping-Ziele) - ohne Sperre waere das ein Datenrennen auf
den `String`-Feldern (Arduino-Strings sind nicht thread-sicher), potenziell
mit Heap-Korruption oder Absturz. Behoben nach demselben Muster wie
`DataManager` im Sensormeter-Projekt: ein `SemaphoreHandle_t`-Mutex,
Getter/Setter nehmen ihn vor jedem Zugriff. `BacklightManager` (einfacher
LEDC-Registerzugriff) und `OtaManager` (nur waehrend eines einzelnen
Uploads genutzt) wurden bewusst NICHT zusaetzlich abgesichert - das
Risiko/der Nutzen einer Sperre steht dort in keinem Verhaeltnis zum
Aufwand.

### Web-Passwort statt fester Zugangsdaten, aber kein HTTPS
HTTP-Basic-Auth (Benutzername fest "admin", Passwort einstellbar, Default
"admin") wie im Sensormeter-Projekt. Das Passwort wird im Formular selbst
im Klartext vorausgefuellt (`value="..."`) - identische Kompromiss-
Entscheidung wie beim PSK-Eingabefeld in der WLAN-Ersteinrichtung (P1):
einfache Kontrolle wichtiger als Sichtschutz auf einem lokalen
Konfigurationsgeraet, zumal ein Angreifer mit Zugriff auf die
authentifizierte Seite das Passwort ohnehin schon kennt/kennen muss.

### Ressourcenverbrauch: +63 KB Flash, +400 Byte RAM
Vorab abgeschaetzt (Analogie zum Sensormeter-Projekt): ca. 97 KB allein fuer
ESPAsyncWebServer/AsyncTCP/ArduinoJson. Tatsaechlich (ohne ArduinoJson,
siehe oben): Flash 73,3% (960.453/1.310.720 Byte je App-Slot), RAM 14,4%
(47.240/327.680 Byte) - deutlich guenstiger als angenommen.

## P6 — Snake (optional)

### Touch-Drittel-Ueberschneidung: vertikale Zonen haben Prioritaet
`lastenheft.txt` Abschnitt 6.3 beschreibt vier Touch-Zonen (oberes/unteres/
linkes/rechtes Drittel), legt aber nicht fest, wie die Ecken aufgeloest
werden, wo sich z. B. "oberes Drittel" und "linkes Drittel" ueberlappen.
`SnakeGame::zoneForTouch()` prueft zuerst oben/unten, dann links/rechts -
ein Tipp in der oberen linken Ecke zaehlt also als "hoch". Einfachste,
deterministische Aufloesung ohne zusaetzliche UI-Elemente.

### Raster 16px-Zellen, 20x13 Felder, Kopfzeile fuer Punktestand
Bildschirm 320x240, 24px oben fuer die Punkteanzeige reserviert, Rest
(216px) in 16px-Zellen aufgeteilt -> 13 Reihen (208px genutzt, 8px Rand
unten). 16px-Zellen sind auf dem 2,8"-Display gut antippbar/erkennbar und
ergeben ein handhabbares Feld (260 Zellen max. Schlangenlaenge).

### Nicht-blockierender Spiel-Loop (Touch-Poll + fester Tick nebeneinander)
Anders als alle bisherigen Screens (Onboarding, Einstellungen, Tastenfeld),
die auf einen vollstaendigen Tipp-Zyklus warten (`waitForTapEvent`), braucht
Snake einen laufenden Takt unabhaengig vom Touch. `SnakeGame::run()` fragt
Touch alle ~15ms ab (aktualisiert nur die gewuenschte Richtung) und bewegt
die Schlange unabhaengig davon alle 200ms (`kTickIntervalMs`) - Muster
uebernommen aus dem bereits etablierten Hauptloop (`main.cpp`), nicht neu
erfunden.

### Highscore in eigenem NVS-Namespace statt ueber SettingsManager
`SnakeGame` verwaltet seinen Highscore direkt ueber einen eigenen
`Preferences`-Namespace ("snake") statt den Umweg ueber `SettingsManager`
zu nehmen - konsistent damit, wie auch `TouchManager` (Kalibrierung) und
`WlanManager` (Zugangsdaten) jeweils ihre eigene NVS-Namespace verwalten,
statt alle Einstellungen zentral zu buendeln.

### Ressourcenverbrauch geringer als geschaetzt
Vor der Umsetzung geschaetzt: +10-20 KB Flash. Tatsaechlich: **+2,4 KB
Flash, +136 Byte RAM** (896.165/1.310.720 Flash, 46.840/327.680 RAM) - da
Snake keine neue Bibliothek braucht und ausschliesslich bereits gelinkte
Bausteine (TFT_eSPI-Primitiven, Preferences, `random()`) wiederverwendet.

## P7/P8 — Sensormeter-Anbindung (SNMP), Ping-Überwachung

### OID-Kodierung im Sensormeter-Projekt verifiziert statt angenommen
`lastenheft.txt` Abschnitt 7.3 nennt die OID-Struktur, aber nicht die
Kodierung. Im Quellcode des Sensormeter-Projekts nachgesehen
(`SNMPManager.cpp`): Temperatur/Luftfeuchte sind als `INTEGER x10`
kodiert (z. B. 235 = 23,5 °C), nicht als Float oder Integer ohne
Skalierung. `SensormeterClient` teilt entsprechend durch 10.

### Eigener, minimaler SNMP-v1-GET-Client statt Bibliothek
Es existiert keine gaengige Arduino/PlatformIO-Bibliothek für einen
SNMP-**Client** (nur für SNMP-**Agenten**, wie die im Sensormeter-Projekt
selbst verwendete). Da nur zwei feste, bekannte skalare Integer-OIDs alle
30s abgefragt werden, wurde ein sehr schlanker, hart auf diesen Anwendungsfall
zugeschnittener BER-Encoder/Decoder geschrieben (`SnmpClient`) statt eine
vollstaendige SNMP-Implementierung zu bauen oder zu vendorn. Einschraenkungen:
nur Short-Form-Laengen (Pakete hier immer klein genug), keine explizite
Auswertung von `errorStatus` (siehe Kommentar in `SnmpClient.cpp`) - im
Fehlerfall (falsche OID) koennte theoretisch `errorIndex` faelschlich als
Wert interpretiert werden. Unkritisch, da die OIDs hart einprogrammiert und
bekannt korrekt sind; realistisches Fehlerbild ist "keine Antwort"
(Geraet nicht erreichbar), nicht "falsche OID".

### ESP32Ping als Bibliothek statt eigenem ICMP-Code
Anders als bei SNMP wurde fuer den Ping-Client (P8) eine etablierte
Bibliothek (`marian-craciunescu/ESP32Ping`) verwendet statt eigenem
ICMP-Code: Rohes ICMP erfordert tieferen lwIP-Zugriff, den die Bibliothek
bereits robust kapselt - hier lohnt sich das Vendoring/Selbstschreiben
nicht wie bei SNMP (dort ist das Protokoll trivial und die Bibliothekslage
duenn).

### Ping-Zyklus entzerrt (max. 1 Google-Ping + 1 Ziel-Ping pro update())
Ein Ping-Versuch kann bei Nichterreichbarkeit bis zu ~1s blockieren. Bei
bis zu 5 zusaetzlichen Zielen haette ein "alle auf einmal"-Ansatz den
Hauptloop und damit die Touch-Reaktion um mehrere Sekunden blockieren
koennen. Stattdessen: pro `update()`-Aufruf (alle 2s) genau ein
Google-Ping UND round-robin genau ein Zusatzziel - begrenzt die
Blockierzeit pro Aufruf auf ca. 2 Ping-Timeouts.

### Alarmzustand (rote Anzeige) als bgColor-Parameter durchgereicht
"Ändere die Display-Farbe in rot" (lastenheft.txt Abschnitt 9) gilt
bildschirmweit, unabhaengig von der aktuell aktiven Datenquelle. Statt
eines globalen Zustands, den jede View selbst abfragt, wird die
Hintergrundfarbe als expliziter Parameter an `GraphManager::drawFullScreen`,
`ClockView::draw`, `SensormeterView::draw`, `PingView::draw*` und
`StatusBar::draw` durchgereicht - haelt die Views unabhaengig testbar und
macht die Abhaengigkeit im Funktionssignatur sichtbar. Textfarbe bleibt
Schwarz (nicht explizit gefordert, Kontrast auf Rot bleibt ausreichend).

### RGB-LED-Polaritaet angenommen (HIGH = an)
Das Datenblatt spezifiziert nicht, ob die RGB-LED gemeinsame Anode oder
Kathode hat. `LedManager` nimmt HIGH=an (gemeinsame Kathode) an - am
eigenen Board zu verifizieren; falls falsch, ist nur die Polaritaet in
`LedManager::setColor` zu invertieren.

### Sensormeter-Ziel und Ping-Ziele in Systemeinstellungen platziert
`lastenheft.txt` beschreibt nicht, ueber welchen Bildschirm die
Sensormeter-IP oder die Ping-Ziel-Liste eingegeben werden. Da
Systemeinstellungen bereits WLAN-Neukonfiguration (ebenfalls
netzwerkbezogen) enthaelt, wurden beide dort als zusaetzliche
Menuepunkte ergaenzt statt einen weiteren Menue-Bereich einzufuehren.

### Numerisches Tastenfeld statt Bildschirmtastatur fuer IP-Eingaben
IP-Adressen brauchen nur Ziffern und Punkt. Die vorhandene
Bildschirmtastatur (`WifiOnboarding`, P1) ist auf PSK-Eingabe zugeschnitten
(Buchstaben+Sonderzeichen) und unnoetig komplex fuer diesen Zweck - stattdessen
ein eigenes, einfaches Telefon-Tastenfeld (`NumericKeypad`), wiederverwendet
fuer Sensormeter-Ziel UND Ping-Ziele.

### Bug gefunden und behoben: Static-Quellenliste lief ueber den Bildschirmrand hinaus
Die Datenquellenliste in den Static-Einstellungen ist mit P7/P8 von 2 auf
5 Eintraege gewachsen, nutzte aber weiterhin die fuer das 4-Punkt-Hauptmenu
bemessene Zeilenhoehe (44px) - der 5. Eintrag (Ping-Ziele) waere teilweise
unterhalb von y=240 gelandet und damit weder sichtbar noch antippbar.
Behoben mit eigener, kompakterer Zeilenhoehe (40px) nur fuer diese Liste;
zusaetzlich die Datenquellen-Labels gekuerzt ("Innentemperatur (DHT11)" u. ae.
waeren in der schmalen Zeile ohnehin abgeschnitten worden).

## P4/P5 — Uhrzeit-Datenquelle, Einstellungen/Betriebsarten

### LEDC-API der installierten Arduino-ESP32-Version ist die alte, kanalbasierte
`ledcAttach(pin, freq, bits)` (neue, pin-basierte API aus Arduino-ESP32 3.x)
existiert in der hier per PlatformIO installierten Framework-Version nicht -
der Compiler schlug `ledcAttachPin` als Alternative vor. `BacklightManager`
nutzt daher die aeltere Kanal-API (`ledcSetup` + `ledcAttachPin` + `ledcWrite(channel,...)`,
Kanal 4 gewaehlt, da 0-3 oft von anderen Peripherien/Beispielen belegt sind).

### Nur DHT11 und Uhrzeit in der Static-Quellenauswahl
`DataSource` (siehe `DataSource.h`) enthaelt aktuell nur die beiden bereits
implementierten Quellen. Sensormeter (P7) und Ping (P8) werden dort ergaenzt,
sobald sie existieren - die Auswahlliste in `SettingsUI` iteriert bereits
generisch ueber `kAvailableDataSources`, erfordert also keine strukturelle
Aenderung, nur einen weiteren Eintrag.

### Statusleisten-Redraw wird nach dem Schliessen der Einstellungen erzwungen
Der Einstellungen-Dialog zeichnet vollflaechig ueber die normale Anzeige.
Nach dessen Rueckkehr werden `contentDirty=true` und `lastStatusBarMs=0`
gesetzt, damit im naechsten `loop()`-Durchlauf sofort alles neu gezeichnet
wird statt bis zum naechsten regulaeren Timer zu warten (bis zu 5s
Uhrzeit-Redraw-Intervall waeren sonst als eingefrorener Bildschirm
wahrgenommen worden).

**Korrektur (siehe "Fünf Touch-UI-Fixes" weiter unten):** Dieser Mechanismus
erzwang nur den naechsten `StatusBar::draw()`-**Aufruf**, nicht dass darin
auch tatsaechlich neu gezeichnet wird - `draw()` hat seinen eigenen
Diff-Cache (siehe Abschnitt "Bildschirm 'zitterte'" oben), der bei
unveraenderten Werten trotzdem nichts zeichnet. Blieb lange unbemerkt, weil
Zeit/WLAN-Balken/Temperatur sich meistens innerhalb der paar Sekunden bis
zum naechsten echten Wertwechsel sowieso aendern - auf echter Hardware nach
mehrfachem Oeffnen/Schliessen aber reproduzierbar als "obere Leiste bleibt
manchmal leer" aufgefallen. Fix: `StatusBar::forceRedraw()`.

### Snake (P6) als Platzhalter in der Modus-Liste
Der Menuepunkt "Snake" ist bereits Teil des Einstellungen-Dialogs (wie im
Lastenheft verlangt: Slide/Static/Snake/Systemeinstellungen sind EIN
gemeinsames Auswahlmenu), zeigt aber nur einen Hinweistext und kehrt zurueck,
da das eigentliche Spiel erst P6 ist. Vermeidet eine spaetere Restrukturierung
des Menues.

### Gemeinsame Touch-UI-Bausteine ausgelagert (UiHelpers)
`hitRect`/`waitForTapEvent` gab es zunaechst nur lokal in
`WifiOnboarding.cpp` (P1). Mit `SettingsUI` als zweitem Verbraucher wurden
sie nach `UiHelpers.h/.cpp` verschoben, um Code-Duplikation zu vermeiden -
bewusst nicht schon in P1 vorgezogen, da zu dem Zeitpunkt noch kein zweiter
Verbraucher absehbar war.

## P2/P3 — Statusleiste, DHT11-Datenquelle

### NTP-Sync vorgezogen aus P4
Die permanente Statusleiste (P2) zeigt laut Lastenheft auch Uhrzeit/Datum
an - ohne funktionierende Zeit waere dieses Element sinnlos. Daher wurde
ein schlankes `TimeSync`-Modul (NTP via `de.pool.ntp.org`, deutsche
Zeitzone inkl. Sommerzeit ueber POSIX-TZ-String `CET-1CEST,M3.5.0,M10.5.0/3`)
bereits jetzt ergaenzt statt komplett auf P4 zu warten. Der volle
`TimeManager` in P4 (grosse Uhrzeit-Datenquellen-Ansicht) baut auf diesen
Funktionen auf, ersetzt sie nicht.

### Statusleisten-Layout: zwei Leisten a 36px (11,25% der langen Kante)
Bildschirmaufteilung zentral in `Layout.h`: Statusleiste oben (Zahnrad,
WLAN-Balken, DHT11-Werte) und unten (Uhrzeit/Datum), je 36px von 320px
langer Kante = 11,25%, haelt die 15%-Vorgabe aus `lastenheft.txt` Abschnitt 4
mit Reserve ein. Der verbleibende Inhaltsbereich (168px) zeigt die aktive
Datenquelle - fuer P2/P3 fest die DHT11-Ansicht, Umschaltbarkeit folgt mit
Slide/Static in P5.

### Zahnrad-Symbol vorerst nicht antippbar
Das Zahnrad wird gezeichnet, reagiert aber noch nicht auf Touch - der
Einstellungen-Dialog selbst ist erst P5. Antippen aller Statusleisten-Symbole
wird zusammen mit der Einstellungen-Funktion verdrahtet, um Touch-Zonen
nicht doppelt zu definieren.

### Bug gefunden und behoben: WLAN-Blink-Symbol haette bei fester 1s-Redrawrate nie geblinkt
Erster Entwurf redrawte die gesamte UI im festen 1000ms-Takt; das
WLAN-Blinksymbol schaltet alle 500ms um. 1000ms ist ein exaktes Vielfaches
von 500ms, d. h. bei jedem Redraw waere immer dieselbe Blink-Phase
abgetastet worden (Aliasing) - das Symbol haette real nie sichtbar
geblinkt. Behoben, indem die Statusleiste unabhaengig vom Inhaltsbereich
alle 300ms neu gezeichnet wird (kein ganzzahliges Vielfaches von 500ms).
Der Inhaltsbereich (Graph/Werte) wird zusaetzlich nur bei tatsaechlich
neuer DHT11-Messung neu gezeichnet statt starr im Sekundentakt, spart
unnoetige Redraws.

### Graph ohne Gitternetz, nur Min/Max-Achsenbeschriftung
Fuer 24 Punkte auf 168px Inhaltshoehe wurde bewusst auf ein volles
Gitternetz mit Zwischenwerten verzichtet - zwei Polylinien (Temperatur rot,
Luftfeuchte blau) plus Min/Max-Werte an den Achsenenden reichen fuer die
geforderte Aufloesung (volle °C/%) und sparen Bildschirmplatz auf einem
kleinen Display.

### Ringpuffer-Persistenz: komplette Datei bei jedem neuen Eintrag neu schreiben
`history.csv` hat maximal 24 Zeilen und wird hoechstens alle 30 Minuten neu
geschrieben - Flash-Verschleiss ist bei dieser Frequenz irrelevant, ein
komplexeres Append-Only-Format mit Offset-Verwaltung haette sich nicht
gelohnt.

## P1 — WLAN-Ersteinrichtung

### Bildschirmtastatur: gewichtsbasiertes Zeilenlayout, keine PSK-Maskierung
Die Tastatur (`WifiOnboarding.cpp`) berechnet Tastenbreiten aus relativen
Gewichten je Zeile (z. B. Leertaste = 4x Normalbreite) statt fester
Pixelkoordinaten — vermeidet manuelle Off-by-one-Fehler bei Anpassungen.
Zwei Seiten: Buchstaben (mit Umschalt/CAPS) und Sonderzeichen (SYM/ABC zum
Umschalten), Zeichensatz auf gaengige WPA2-taugliche Sonderzeichen begrenzt.

Die eingegebene PSK wird im Klartext angezeigt, nicht maskiert: Auf einem
Geraet ohne physische Tastatur ist die Kontrolle der Eingabe wichtiger als
Sichtschutz, zumal es sich um ein lokales Konfigurationsgeraet handelt.

### Nur ein gespeichertes WLAN
Es wird genau ein Netz (SSID+PSK) in NVS gespeichert, kein Multi-WLAN mit
Prioritaetsliste. Deckt sich mit der fruehreren Entscheidung, die
Mehr-Netzwerk-/Timeout-/Retry-Details der Vorgaenger-Lastenhefte bewusst
nicht zu uebernehmen (siehe `lastenheft.txt` Abschnitt 11). "WLAN neu
waehlen" in den Systemeinstellungen ueberschreibt die gespeicherten Daten.

### Netzliste ohne Scroll-Funktion
`WlanManager::scan()` liefert maximal 6 Netze (nach Signalstaerke sortiert,
SSID-Duplikate zusammengefasst) — mehr passt ohnehin nicht auf den 240px
hohen Bildschirm ohne Scrollen. Weitere Netze werden nicht angezeigt; bei
Bedarf spaeter durch Scroll-Geste erweiterbar.

### Fehlgeschlagene Verbindung: zurueck zur gecachten Liste
Nach einem gescheiterten Verbindungsversuch wird die zuvor gescannte
Netzliste erneut angezeigt statt automatisch neu zu scannen (WLAN-Scan
dauert mehrere Sekunden) — ein manueller "Aktualisieren"-Tap scannt bei
Bedarf neu.

## P0 — Display- und Touch-Grundgerüst

### TFT-Pinbelegung nicht im Datenblatt enthalten
`CBAA0055-008_DE.pdf` (Hersteller-Datenblatt) listet nur die Touch-Pins
(TP_CLK/CS/DIN/OUT/IRQ) sowie den Backlight-Pin (IO21), aber keine eigenen
SPI-Pins für das ST7789P3-TFT selbst (MOSI/MISO/SCLK/CS/DC/RST). Die vier
GPIOs 12/13/14/15 tauchen im gesamten Pin-Diagramm nirgends auf — ein
starker Hinweis, dass sie intern für das TFT reserviert sind.

Gefunden über das exakt identische Referenzdesign LCDWIKI E32R28T/E32N28T
(`2.8inch_ESP32-32E-7789`): dessen Touch-Pinbelegung (IO25/32/33/36/39) und
Backlight-Pin (IO21) stimmen 1:1 mit unserem Datenblatt überein — hohes
Vertrauen, dass auch die TFT-Pins identisch sind:

```
TFT_MOSI = 13   TFT_MISO = 12   TFT_SCLK = 14
TFT_CS   = 15   TFT_DC   = 2    TFT_RST  = -1 (an EN, kein eigener Pin)
TFT_BL   = 21
```

**Wichtiger Nebenfund:** GPIO2 ist damit die TFT-DC-Leitung (Data/Command)
des Displays — bestätigt die frühere Entscheidung, DHT11 nicht auf
literal GPIO2 zu legen (siehe `lastenheft.txt` Abschnitt 2), sondern auf
GPIO22 über den Steckverbinder "Erweiterungsanschluss IO2". GPIO2 wäre
ohnehin belegt gewesen, unabhängig vom Boot-Strapping-Argument.

**Noch offen:** Diese Pinbelegung ist nicht am eigenen Board nachgemessen,
sondern über ein passendes Referenzdesign erschlossen. Beim ersten
Flash-Versuch auf echter Hardware verifizieren (funktioniert das Display
nicht, zuerst TFT_CS/DC/SCLK/MOSI in `platformio.ini` gegeneinander
tauschen).

### Touch-Ansteuerung: Bit-Bang statt XPT2046-Bibliothek
Verfügbare XPT2046-Arduino-Bibliotheken (u. a. `paulstoffregen/XPT2046_Touchscreen`,
in PlatformIO nur als alte Alpha-Version verfügbar) nutzen intern den
globalen Arduino-`SPI`-Bus. Der ist hier aber bereits vom TFT belegt
(eigene Pinbelegung, siehe oben), und die verfügbare Alpha-Version bietet
keine Möglichkeit, einen alternativen `SPIClass`-Bus zu übergeben.

Statt einen zweiten Hardware-SPI-Bus zu konfigurieren (unnötiger Aufwand
für ein einfaches Protokoll), wird der XPT2046-artige Touch-Controller
direkt per Bit-Bang auf den TP_-Pins angesteuert (`TouchManager.cpp`):
Kommandobyte MSB-first raus, 12-Bit-Ergebnis rein, `TP_IRQ` zeigt per
Low-Pegel eine aktive Berührung an. Timing ist Standard-XPT2046, aber noch
nicht an echter Hardware verifiziert.

### Kalibrierung: 2-Punkt, gespeichert in NVS (Preferences)
Resistive Touchscreens sind in der Regel linear, daher genügt eine
Zwei-Punkt-Kalibrierung (oben-links, unten-rechts antippen) statt einer
aufwendigeren 4- oder 9-Punkt-Prozedur. Speicherung über die ESP32-eigene
`Preferences`-Bibliothek (NVS-Partition), Namespace `touch` — kein eigenes
Dateiformat nötig für vier Ganzzahlen plus ein Flag.

### Schriftart: Font 7 (7-Segment) für Zahlen, Font 4 für Text
`lastenheft.txt` fordert eine systemweite Schriftart "ähnlich einer
7-Segment-Anzeige". TFT_eSPI bringt mit Font 7 einen eingebauten
7-Segment-Stil-Font mit, der aber nur Ziffern, Doppelpunkt, Punkt und Minus
darstellen kann — keine Buchstaben. Für Zahlenwerte (Temperatur, Uhrzeit)
wird Font 7 verwendet; für Text (WLAN-Liste, Bildschirmtastatur, Wochentag)
ein lesbarer Standard-Font (Font 4), da ein reiner 7-Segment-Font dafür
technisch nicht ausreicht.

## Versionierung

Anders als die beiden Schwesterprojekte hatte dieses Repo bislang **gar
keine** Firmware-Versionskonstante - keine `config.h`/`config.h.example`,
kein `DEVICE_FIRMWARE_VERSION`, keine Anzeige der Version an irgendeiner
Stelle (weder Touch-UI noch Webserver), obwohl die Weboberfläche bereits
einen Link auf "Releases auf GitHub" zeigt.

**Nachgezogen, analog zu Sensormeter und Sensormeter WLAN:**
- `firmware/include/config.h.example` (+lokale `config.h`, per
  `.gitignore` bereits vorbereitet) neu angelegt mit
  `DEVICE_FIRMWARE_VERSION "0.9.0-rc1"` (Beta) - gleicher Stand wie die
  beiden anderen Projekte
- **Touch-UI**: Version nur im Geräte-Info-Dialog (`InfoUI.cpp`, hinter dem
  i-Symbol) als vierte Zeile ergänzt - bewusst nicht auf anderen Screens,
  da dort kein Platz/Bedarf für eine reine Diagnose-Info besteht
  (Nutzerentscheidung)
- **Webserver**: im Bereich „Firmware-Update" als „Aktuell installiert:
  …" ergänzt (`WebServerManager.cpp`)

Versionsnummer zusätzlich in README, One-Pager und Admin-Guide vermerkt,
wie bei den beiden anderen Projekten. Git-Tags/GitHub-Releases mit
`.bin`-Artefakt sind - wie dort auch - noch nicht Teil dieser Änderung.

### Nachtrag: v0.9.0-rc2 + Doku-Aufräumen nach den Warnschwellwert-/Dashboard-Nachträgen
Nach den zahlreichen Nachträgen oben (Warnschwellwerte, öffentliches
Dashboard, Kalibrierkorrektur, PRO-Sensor-Schwellwerte, diverser
Feinschliff) enthielten mehrere Dokumente noch den vorherigen Stand:

- `lastenheft.txt` Abschnitt 11 behauptete "kein Status-Dashboard" - seit
  dem öffentlichen Dashboard falsch, korrigiert (Dashboard-Inhalt
  ergänzt, Abschnitt 9 um Blink-Verhalten/Mehrfach-Alarm erweitert)
- `pflichtenheft.txt` 2.25 (WebServerManager) und Abschnitt 8 nannten nur
  das alte Einstellungsformular, nicht die Seitenaufteilung "/" +
  "/settings" - ergänzt, dazu ein neuer Eintrag 2.26 (`AlertEvaluator`)
- README.md behauptete noch "Noch nicht verifiziert: reale Hardware" -
  seit den zahlreichen echten Flash-/Testzyklen in dieser Session falsch,
  korrigiert
- `docs/admin-guide.pdf` und `docs/sensormeter-display-onepager.pdf`:
  HTML-Quellen aus der Git-Historie wiederhergestellt (waren nach dem
  PDF-Export gelöscht worden, siehe Commit "HTML-Quelldokumente entfernt,
  wo bereits eine PDF existiert"), aktualisiert (neue Version, neuer
  Dashboard-Abschnitt im Admin-Guide, `AlertEvaluator`, Warnschwellwerte,
  Kalibrierung), per Headless-Chrome (`--headless --print-to-pdf`) neu
  als PDF exportiert, HTML-Quellen danach wieder entfernt (Konvention
  beibehalten)
- Admin-Guide Abschnitt 8.1 (jetzt 9.1): der seit 2026-07-08
  zurückgestellte OTA-Wortlaut ("von den GitHub-Releases") korrigiert -
  es existiert weiterhin kein GitHub-Release mit `.bin`-Anhang für dieses
  Repo, Wortlaut sagt jetzt klar, dass die `.bin` selbst gebaut werden muss

**Bei dieser Durchsicht gefunden, aber NICHT behoben (siehe
pflichtenheft.txt Abschnitt 9 für die vollständige Beschreibung):**
`WebServerManager` liest seit dem Dashboard auch `SensorManager`,
`PingManager`, `SensormeterManager` und `GraphManager` - alle vier ohne
Mutex, anders als `SettingsManager`. Theoretisches Risiko bei
Arduino-`String`-Rückgaben (nicht thread-sicher) unter echter Nebenläufigkeit
zwischen Hauptloop und Web-Task. Bisher ohne beobachtete Auswirkung,
bewusst zurückgestellt statt im Rahmen dieses Aufräum-Durchgangs
mitgezogen (separater, planbarer Umbau: Mutex je Objekt nach demselben
Muster wie `SettingsManager`).

## Fünf Touch-UI-Fixes (Nutzer-Feedback nach erstem Hardware-Test)

### Redraw-Cache blieb nach InfoUI/SettingsUI stehen
`GraphManager::drawFullScreen()` und `PingView::drawAverage()`/
`drawTargetList()` haben (wie `StatusBar`) einen Diff-Cache, der nur bei
geänderten Anzeigewerten neu zeichnet, um Flackern zu vermeiden (siehe
oben "Redraw-Cache" bei StatusBar). InfoUI/SettingsUI überschreiben aber
per `fillScreen()` den kompletten Bildschirm - kehrt man danach zu einer
Ansicht zurück, deren letzter gecachter Wert sich zufällig nicht geändert
hat, zeichnet der Cache gar nichts, der Screen bleibt leer bis zur
nächsten echten Wertänderung. `main.cpp` setzte für `StatusBar` bereits
`lastStatusBarMs = 0` als Workaround, aber `GraphManager`/`PingView`
hatten kein Äquivalent. Fix: beide bekommen ein `forceRedraw()`
(`everDrawn`-Flag zurücksetzen), aufgerufen direkt nach
`InfoUI::run()`/`settingsUI.run()` in `main.cpp`. `ClockView` und
`SensormeterView` zeichnen ohnehin unbedingt bei jedem Aufruf, brauchen
also keinen Fix.

### Ping-Ziele: Latenz pro Ziel
`PingManager::TargetState` speicherte bisher nur `ok`/`checked`, keine
Zeit. Ergänzt um `lastLatencyMs` (aus `Ping.averageTime()` bei Erfolg) und
`hasLatency` (unterscheidet "noch nie erfolgreich gepingt" von "0ms
gemessen"). Bei Fehlschlag bleibt der zuletzt erfolgreiche Wert stehen
statt auf 0 zurückzufallen - wirkt weniger sprunghaft als bei jedem
Timeout "0ms" anzuzeigen. `PingView`s Redraw-Signature musste um die
Latenz erweitert werden, sonst hätte sich eine reine Latenzänderung (ohne
OK/Fehler-Wechsel) gar nicht im Diff-Cache niedergeschlagen.

### Ansichtswechsel per Tippen links/rechts: nur im Slide-Modus
Im Static-Modus zeigt das Gerät genau eine vom Nutzer fest gewählte
Datenquelle (`settings.staticSource()`) - es gibt dort kein sinnvolles
"nächste/vorige Ansicht". Die neue Tap-Navigation (linke Bildschirmhälfte
= vorige, rechte = nächste) wirkt daher nur im Slide-Modus und verändert
dort denselben `slideIndex`, den auch der automatische Timer nutzt;
zusätzlich wird `slideLastSwitchMs` beim manuellen Wechsel zurückgesetzt,
damit der Auto-Wechsel nicht unmittelbar danach nochmal weiterspringt.
Falls das nicht der Erwartung entspricht (z. B. auch im Static-Modus
gewünscht), bitte melden.

### WLAN-Signalstärke: Hysterese gegen Flackern
`WlanManager::signalBars()` mappte den RSSI-Wert bisher ungefiltert auf
feste Schwellen (-60dBm/-75dBm) - lag der Messwert nahe an einer Schwelle,
sorgte normales Funkrauschen für ständiges Umspringen zwischen z. B. 2 und
3 Balken bei jeder 300ms-StatusBar-Aktualisierung. Fix: exponentiell
geglätteter RSSI (`smoothedRssi = smoothedRssi*0.8 + rssi*0.2`) plus
Hysterese von 4dB beim Schwellenwechsel (rauf UND runter je 4dB Abstand
vom Grenzwert nötig, bevor die Balkenanzahl wechselt) - direkt am
ursprünglichen Grenzwert bleibt die zuletzt gezeigte Stufe stabil.

### Temperaturverlauf: Zeitachse beschriftet
`GraphManager::drawGraph()` zeigte X-Achse (Zeit) bisher ganz ohne
Beschriftung, nur die Y-Achsen (Temperatur links, Feuchte rechts) hatten
Min/Max-Werte. Ergänzt um Uhrzeit-Labels am linken/rechten Rand des Graphen
(ältester/neuester Messpunkt aus dem Ringpuffer), nur sichtbar wenn
`TimeSync::isValid()` (ohne NTP-Sync sind die gespeicherten Zeitstempel
nicht aussagekräftig).

Alle fünf Fixes mit `pio run` gebaut, erfolgreich kompiliert und per
`pio run --target upload` auf das echte Board geflasht (Boot/WLAN-Verbindung
im Serial-Log bestätigt).

## Warnschwellwerte (DHT11 intern, Sensormeter-Ziele, Ping)

Nutzerwunsch: konfigurierbare Warnschwellwerte für den internen DHT11-
Sensor, jedes Sensormeter-Ziel und Ping-Latenz. Vier offene Design-Fragen
per Rückfrage geklärt, bevor implementiert wurde:

- **Auslöseverhalten:** ganzer Bildschirm rot bei Über-/Unterschreitung -
  bewusst derselbe Mechanismus wie der bestehende Ping-Ausfall-Alarm
  (`alertActive` → `bgColor=TFT_RED`, siehe P7/P8-Abschnitt oben), keine
  neue visuelle Sprache nötig. Als Nebeneffekt löst ein überschrittener
  Schwellwert jetzt auch die rot blinkende Status-LED aus (`led.update()`
  hängt bereits an `alertActive`) - konsistent mit lastenheft.txt
  Abschnitt 9, das LED und Rot-Bildschirm als gemeinsamen "Alarmzustand"
  behandelt.
- **Richtung:** sowohl Maximal- als auch Minimalwerte (zu heiß/kalt, zu
  feucht/trocken) für Temperatur/Feuchte. Bei Ping-Latenz nur ein
  Maximalwert - eine "Mindestlatenz" ist kein sinnvolles Konzept.
- **Ping-Granularität:** pro Ziel einzeln, google.com bekommt einen eigenen,
  davon unabhängigen Schwellwert (`googlePingMaxLatencyMs()` getrennt von
  den indizierten `pingMaxLatencyMs(i)`).
- **Konfigurationsort:** ausschließlich Webserver, keine Touch-UI-Screens
  nötig - reduziert den Umsetzungsaufwand erheblich (kein neues
  Zifferntastenfeld-Formular am Gerät).

### Sentinel-Werte statt zusätzlicher bool-Flags
Um "kein Schwellwert gesetzt" ohne einen zusätzlichen bool pro Wert
abzubilden: Temperatur/Feuchte nutzen `INT16_MIN`
(`SettingsManager::kThresholdDisabled`) als Sentinel, da 0°C/0% reale
Messwerte sein können und daher nicht als "aus" missverstanden werden
dürfen. Ping-Latenz nutzt dagegen `0` als Sentinel - `Ping.averageTime()`
liefert nie exakt 0ms, ein echter Schwellwert von 0 wäre ohnehin bei jedem
Ping ausgelöst worden und damit sinnlos. Webformular-Felder: leer =
gespeichert als Sentinel, ausgefüllt = echter Wert.

### Schwellwerte pro Ziel-Slot, nicht pro Einzelsensor
Ein Sensormeter-PRO-Ziel liefert zwei Sensor-Slides (Sensor 1+2, siehe
`SensormeterView`), bekommt aber nur EINEN gemeinsamen Schwellwertsatz -
kein separater Schwellwert je Einzelsensor. Vereinfacht die
Weboberfläche (ein Formular pro Ziel statt zwei) und deckt den
naheliegenden Anwendungsfall (beide Sensoren am selben Ziel/Standort)
ausreichend ab.

### Bug im Entwurf gefunden und behoben: Slot-Wiederverwendung hätte alte Schwellwerte "vererbt"
Ping-/Sensormeter-Ziele werden intern als Arrays mit fortlaufendem Index
verwaltet; beim Entfernen eines Ziels rutschen alle nachfolgenden Einträge
eine Position nach vorne (`removePingTarget()`/`removeSensormeterTarget()`).
Ohne Gegenmaßnahme wäre der dadurch frei werdende letzte Array-Slot mit
seinem alten Schwellwert stehen geblieben - ein später dort neu
hinzugefügtes Ziel (`addPingTarget()`/`addSensormeterTarget()` schreiben
immer an Index `count_++`) hätte fälschlich die Schwellwerte des vorher
entfernten Ziels übernommen. Behoben: beide Remove-Methoden setzen den
frei werdenden Slot zusätzlich explizit auf den Sentinel-Wert zurück (und
persistieren das), bevor er wiederverwendet werden kann.

### Webformular-Layout: separate Mini-Formulare pro Ziel statt im Hauptformular
Die Sensormeter-/Ping-Zielliste ist dynamisch (0-5 Einträge). Statt die
Schwellwertfelder in das eine große `/save`-Hauptformular zu packen
(hätte bei jeder Ziel-Änderung eine variable Feldanzahl im selben Formular
bedeutet), bekommt jedes Ziel sein eigenes kleines Formular
(`/sensormeter/thresholds`, `/ping/threshold`) mit eigenem
Speichern-Button - konsistent mit dem bereits bestehenden Muster, dass
Ziel-Hinzufügen/Entfernen ebenfalls eigene Formulare/Endpunkte sind. DHT11
(intern) und der google.com-Schwellwert sind dagegen fest (nicht
listenbasiert) und bleiben daher im Hauptformular.

## Nachtrag: Blink-Alarm mit Richtung/Quelle statt statischem Rot-Bildschirm

Nutzer-Feedback nach dem ersten Test der Warnschwellwerte: die
Sensormeter-Zielliste im Webformular zeigte pro Ziel nur unbeschriftete
Eingabefelder (nur Platzhaltertext, der beim Ausfüllen verschwindet) -
bei mehreren Zielen nicht mehr erkennbar, welches Feld zu welchem Ziel
gehört. Außerdem drei Verhaltensänderungen gewünscht: Bildschirm blinkt
(nicht dauerhaft) rot bei Überschreitung, blau bei Unterschreitung; die
betroffene Quelle (Intern/Sensormeter/Ping) wird in der Statusleiste
angezeigt; im DHT11-Graph werden die Schwellwerte als Linien eingezeichnet.

### Webformular: CSS-Tabelle statt <table>, da eine Zeile ein <form> ist
Jede Sensormeter-Zielzeile ist funktional ein eigenes `<form>` (POST an
`/sensormeter/thresholds`) - ein `<form>` als direktes Kind eines
`<table>`/`<tr>` ist ungültiges HTML. Gelöst über CSS-"Tabellen" (`display:
table`/`table-row`/`table-cell` auf normalen `<div>`/`<form>`-Elementen)
statt echter `<table>`-Tags - das `<form>` selbst wird zur Tabellenzeile
und darf trotzdem als Formular fungieren. Spaltenköpfe (Ziel, Temp
min/max, Feuchte min/max) sorgen jetzt für eindeutige Zuordnung auch bei
mehreren Zielen.

### Speichern und Entfernen im selben Formular (per `formaction`)
Um pro Tabellenzeile bei nur einem `<form>` trotzdem zwei verschiedene
Aktionen (Schwellwerte speichern vs. Ziel entfernen) anzubieten, nutzt der
"Entfernen"-Button `formaction="/sensormeter/remove"` (überschreibt die
Standard-`action` des Formulars nur für diesen Button). Der Remove-Handler
liest ohnehin nur das Feld `i` und ignoriert die mitgesendeten
Schwellwert-Felder - keine Änderung an `handleSensormeterRemove()` nötig.

### Blinken statt Dauerzustand: 1s-Taktwechsel, Farbe zeigt Richtung
Der bisherige Ping-Ausfall-Alarm hielt den Bildschirm dauerhaft rot,
solange die Störung anhielt. Jetzt blinkt der Hintergrund (1s Taktwechsel,
`(millis()/1000)%2`) - Farbe zeigt die Richtung: Rot für Überschreitung
eines Max-Werts ODER einen Ping-Ausfall (beides "zu viel"/Störung), Blau
für Unterschreitung eines Min-Werts (Ping hat keine sinnvolle
"Unterschreitung", daher immer Rot). Der Redraw-Mechanismus musste
entsprechend angepasst werden: statt eines einzelnen `alertActive`-Bool-
Wechsels erzwingt jetzt jede `bgColor`-Änderung ein Neuzeichnen
(`bgColorChanged`), da sich die Farbe durch das Blinken bereits ohne
An-/Abklingen des Alarms laufend ändert. Die Status-LED (`LedManager`)
bekam dieselbe Rot/Blau-Unterscheidung, blinkt aber weiterhin mit ihrem
eigenen, unveränderten 500ms-Takt.

### Alarmquelle: nur EINE Quelle gleichzeitig anzeigbar, feste Prioritätsreihenfolge
Mehrere Schwellwertverletzungen können theoretisch gleichzeitig aktiv
sein (z.B. DHT11 zu warm UND ein Sensormeter-Ziel zu kalt). Die
Statusleiste hat nur Platz für ein kurzes Label, daher gewinnt bei
`computeAlertInfo()` der erste Treffer in fester Reihenfolge Intern →
Sensormeter → Ping - keine inhaltliche Rangfolge, nur zur
deterministischen, platzsparenden Anzeige. Das Label wird in der
Statusleiste zwischen Info-Symbol und DHT11-Werten platziert (Platz dort
bisher ungenutzt) und bleibt - anders als der Bildschirmhintergrund -
durchgehend sichtbar (nicht blinkend), damit die Ursache auch während der
"weißen" Blink-Phase ablesbar bleibt.

### Graph-Linien: gestrichelt statt durchgezogen, auf Plotbereich geklemmt
Schwellwertlinien im DHT11-Graph nutzen dieselben Farben wie die
Messwert-Polylinien (Temperatur rot, Feuchte blau), aber gestrichelt
(eigene `drawDashedHLine()`-Hilfsfunktion, TFT_eSPI kennt keine
gestrichelten Linien nativ) - unterscheidet sie klar von den
durchgezogenen Messdaten. Liegt ein Schwellwert außerhalb des aktuell
sichtbaren (autoskalierten) Wertebereichs, wird die Linie auf den
Plotbereichsrand geklemmt statt sie (unsichtbar) außerhalb zu zeichnen -
zeigt an "Schwellwert noch nicht erreicht" statt sie ersatzlos wegfallen
zu lassen. Nur für DHT11 (intern) umgesetzt, da nur diese Ansicht einen
Verlaufsgraphen hat - Sensormeter-Ziele zeigen nur den aktuellen Wert.

Erneut mit `pio run` gebaut (erfolgreich) und per `pio run --target
upload` geflasht.

## Nachtrag 3: Öffentliches Status-Dashboard + Design aus dem Admin-Guide

Nutzerwunsch: eine der Einstellungsseite vorgelagerte, NICHT
passwortgeschützte Webseite mit allen aktuellen Werten, Auto-Refresh alle
30s - plus die Übernahme der Farbpalette/Typografie aus
`docs/admin-guide.pdf` (Navy-Cover, Orange-Akzent) auf beide Webseiten.

### Sicherheitsabwägung explizit angesprochen, vom Nutzer akzeptiert
Ein unauthentifizierter Endpunkt zeigt Sensormesswerte sowie Ping-/
Sensormeter-Ziel-IPs jedem im selben Netzwerk, ohne Login. Das wurde vor
der Umsetzung ausdrücklich als Kompromiss benannt (siehe Rückfrage) - für
ein privates LAN akzeptiert. Die Einstellungsseite selbst bleibt
vollständig passwortgeschützt; das Dashboard ist rein lesend (keine
Formulare, keine Schreibaktionen).

### Routen getrennt: `/` (öffentlich) und `/settings` (Login)
`/` liefert jetzt `buildDashboardPage()` ohne `checkAuth()`-Aufruf, die
bisherige Einstellungsseite (`buildSettingsPage()`, vormals `buildPage()`)
liegt auf `/settings` und bleibt per HTTP-Basic-Auth geschützt. Alle
Formular-Handler (`handleSave`, `handlePingAdd/Remove`,
`handleSensormeterAdd/Remove/Thresholds`, `handlePingThreshold`)
redirecten jetzt auf `/settings` statt `/`, damit man nach dem Speichern
wieder auf der Einstellungsseite landet statt auf dem öffentlichen
Dashboard.

### Alarm-Logik ausgelagert (AlertEvaluator.h/.cpp) statt dupliziert
Das Dashboard muss dieselbe Warnschwellwert-Auswertung zeigen wie die
Touch-UI (`main.cpp`) - ein zweites Mal von Hand nachbauen hätte das
Risiko geschaffen, dass beide Stellen bei einer künftigen Änderung
auseinanderlaufen (genau das Muster, das im Sensormeter-WLAN-Gehäuse-
Projekt bereits einmal zu einem Bug durch doppelt gepflegte Formeln
geführt hat). Die `AlertInfo`-Struct und `computeAlertInfo()` wurden
daher aus `main.cpp` in ein eigenes Modul verschoben, das explizite
Referenzen (`SensorManager`, `SensormeterManager`, `PingManager`,
`SettingsManager`) statt globaler Variablen entgegennimmt - dadurch aus
`main.cpp`'s `loop()` UND aus `WebServerManager::buildDashboardPage()`
gleichermaßen aufrufbar.

### Design aus dem Admin-Guide übernommen: Navy-Banner, Orange-Akzent, warmes Creme
Palette 1:1 aus `docs/admin-guide.pdf` übernommen: Navy `#0f1f3d` für die
Kopfzeile (analog zum Cover des PDFs), Orange `#c8622a` für Buttons/
Akzentlinien, warmes Creme `#f2f0e9` für Tabellenköpfe/Kartenrahmen
`#e4e1d8` - dieselbe Systemschriftart-Stack
(`-apple-system,'Segoe UI',Roboto,...`). Als gemeinsame `sharedCss()`-
Methode statt doppelt gepflegtem CSS in beiden Seiten-Buildern. Rot-Braun
`#a63d2e` (Warn-Callout-Farbe aus dem Guide) für Ausfall/Überschreitung,
dazu passend ein neues Blau `#2a5ba0` für Unterschreitung (im Guide selbst
nicht vorhanden, farblich zur Navy passend ergänzt) - konsistent mit der
Rot/Blau-Unterscheidung aus dem Blink-Alarm der Touch-UI.

### Bestehende Einstellungsseite: nur CSS/Wrapper geändert, Formulare unverändert
Um das Risiko einer Restrukturierung zu vermeiden, wurde die
Formular-Struktur der Einstellungsseite (alle `<fieldset><legend>`-Blöcke)
unverändert gelassen - nur `fieldset`/`legend` bekamen per CSS das
Karten-Aussehen (weißer Hintergrund, Rahmen, Radius) verpasst, plus ein
neuer Banner-Header oben. Dadurch bleibt das Risiko einer Regression in
der Formularlogik (Sensormeter-/Ping-Ziele, Schwellwerte, Netzwerk, OTA)
minimal.

Mit `pio run` gebaut (erfolgreich, Flash 75,8 % / 993.069 B, RAM 14,7 % /
48.056 B) und per `pio run --target upload` geflasht.

## Nachtrag 4: Vier Feinschliff-Punkte am Dashboard/Einstellungen

Nutzer-Feedback nach dem ersten Blick aufs neue Design: Verlaufsgraph auch
im Dashboard für den internen Sensor, Sensormeter-Ziel 1 muss ohne
Entfernen neu setzbar sein, einheitliche Rahmenbreite (Banner war breiter
als die Karten), und die aktuelle NTP-Zeit mit Sync-Zeitpunkt auf der
Statusseite.

### DHT11-Verlaufsgraph im Dashboard: inline SVG statt Canvas/JS
Der Touch-Display-Graph (`GraphManager::drawGraph()`) hat kein Web-
Äquivalent gehabt - das Dashboard zeigte bisher nur den aktuellen Wert.
Ergänzt um `WebServerManager::dhtGraphSvg()`: liest den Ringpuffer über
neue, rein lesende `GraphManager`-Zugriffsmethoden (`entryCount()`/
`entryTs()`/`entryTempC()`/`entryHumidityPct()`) aus und rendert ihn als
inline `<svg>` (Polylinien, kein Canvas/JavaScript nötig - passt zum
bisherigen reinen-HTML-Stil des Webservers). Warnschwellwerte als
gestrichelte Linien in derselben Farblogik wie auf dem Display (rot
Temperatur, blau Feuchte), auf den sichtbaren Wertebereich geklemmt.
`WebServerManager` bekam dafür eine weitere lesende Referenz
(`const GraphManager &graph_`).

### Sensormeter-Ziel-IP direkt änderbar statt nur entfernen+neu anlegen
`removeSensormeterTarget()` verweigert das Entfernen des letzten
verbleibenden Ziels (siehe weiter oben) - ohne weitere Änderung wäre ein
einzelnes, falsch eingetragenes Ziel dadurch permanent falsch geblieben.
Neue Methode `SettingsManager::setSensormeterTargetIp()` ändert die IP
in-place (Schwellwerte bleiben erhalten). In der Web-Tabelle ist das
Ziel-Feld der ersten Sensor-Zeile jetzt ein Eingabefeld statt reinem Text;
der bestehende "Speichern"-Button aktualisiert IP und Schwellwerte in
einem Schritt. Bei PRO-Zielen bleibt die IP nur in der Sensor-1-Zeile
editierbar (gehört dem ganzen Ziel, nicht dem einzelnen Sensor - gleiches
Prinzip wie beim "Entfernen"-Button, der ebenfalls nur dort erscheint).

### Rahmenbreite vereinheitlicht: Banner-Bleed entfernt
Der Banner-Header hatte einen negativen Rand (`margin:0 -14px`), der ihn
bis an den Viewport-Rand "bluten" ließ - dadurch war er sichtbar breiter
als die Fieldsets/Karten darunter, die innerhalb der `.wrap`-Polsterung
blieben. Bleed entfernt, Banner bekam stattdessen denselben Randradius wie
Karten/Fieldsets (`border-radius:6px`) - jetzt sind alle Boxen exakt gleich
breit.

### NTP-Sync-Zeitpunkt: echter SNTP-Callback statt nur aktuelle Uhrzeit
"Letzter Sync" ist etwas anderes als "aktuelle Uhrzeit" - letztere läuft
zwischen zwei Sync-Ereignissen über die interne RTC/`millis()` weiter und
sagt nichts darüber aus, wann tatsächlich zuletzt erfolgreich synchronisiert
wurde. `TimeSync::begin()` registriert daher `sntp_set_time_sync_notification_cb()`
(ESP-IDF-SNTP-API, `esp_sntp.h`) - der Callback speichert den Zeitpunkt
jedes erfolgreichen Sync-Ereignisses in einer modul-internen Variable.
Neue Methode `TimeSync::formatLastSync()` formatiert das als
"TT.MM.JJ HH:MM" (2-stelliges Jahr, wie vom Nutzer gewünscht), leerer
String vor dem ersten erfolgreichen Sync. Im Dashboard-Banner unter dem
"Aktualisiert automatisch"-Hinweis angezeigt.

Mit `pio run` gebaut (erfolgreich, Flash 76,2 % / 998.385 B, RAM 14,7 % /
48.064 B) und per `pio run --target upload` geflasht.

## Nachtrag 5: Ping-Ziel-Gruppierung + Achsenbeschriftung mit 1°-Inkrementen

Nutzer-Feedback: Ping-Ziel-IP und deren Schwellwert-Formular in den
Einstellungen wirkten bei mehreren Zielen wie zwei unabhängige Zeilen statt
als zusammengehörige Gruppe; der DHT11-Graph im Dashboard hatte trotz
Nachtrag 4 immer noch keine echte Achsenbeschriftung. Rückfrage zum Umfang
(Touch-Display vs. Webseiten) ergab: **nur Webseiten**.

### Ping-Ziele: `.pingrow-group`-Wrapper mit Trennlinie
IP-Zeile + Schwellwert-Formular jedes Ziels stecken jetzt in einem
gemeinsamen `<div class="pingrow-group">` mit `border-bottom`, der optisch
zum nächsten Ziel abgrenzt - keine Formular-Restrukturierung nötig, nur
ein zusätzlicher Wrapper plus eine CSS-Regel.

### Achsenbeschriftung: 1°/1%-Schritte statt fester Rundwert-Raster
`dhtGraphSvg()` bekam echte Tick-Beschriftungen an beiden Hochachsen
(links Temperatur in °C/rot, rechts Feuchte in %/blau) - eine Zeile pro
ganzzahligem Grad/Prozent zwischen dem jeweiligen Min- und Maxwert des
aktuellen 12h-Fensters. Bewusst **kein** festes Rundwert-Raster (z.B.
0/5/10/...), sondern das Fenster selbst (`tempMin`...`tempMax` bzw.
`humMin`...`humMax`) als Skala - dadurch landen die tatsächlich erfassten
Extremwerte immer exakt auf einem beschrifteten Strich, nie zwischen zwei
Rundwert-Stufen ("Sliding Window", da sich Min/Max mit jedem neuen
Messwert und jedem aus dem Ringpuffer herausfallenden alten Wert
verschieben können). `kMarginY` von 10 auf 16px erhöht, um Platz für die
`°C`/`%`-Achsentitel über der Grafik zu schaffen.

Mit `pio run` gebaut (erfolgreich, Flash 76,3 % / 1.000.389 B, RAM 14,7 % /
48.064 B) und per `pio run --target upload` geflasht.

## Nachtrag 6: Erfassungszeitpunkt des DHT11-Werts in derselben Zeile

Nutzerwunsch: neben den aktuellen DHT11-Werten im Dashboard soll erkennbar
sein, WANN dieser Wert erfasst wurde - in derselben Zeile, nicht separat.

### Erfassungszeitpunkt, nicht Anzeigezeitpunkt
Wichtig: das ist NICHT dieselbe Information wie der bereits vorhandene
"Letzter NTP-Sync" im Banner (Nachtrag 4) - dort ging es um die
Systemuhr-Genauigkeit, hier um das Alter des angezeigten Messwerts selbst.
`SensorManager` bekam dafür `lastReadTs` (`time(nullptr)`, gesetzt nur bei
einer tatsächlich PLAUSIBLEN Messung in `update()` - bei einer
implausiblen Messung bleiben sowohl der alte Wert als auch dessen
Zeitstempel unverändert stehen, konsistent zueinander) und einen neuen
Getter `lastReadTime()`. Das Dashboard hängt "(Stand HH:MM Uhr)" direkt an
die Temperatur-/Feuchte-Zeile an, `<span class="hint">` (dezent, kein
eigener Absatz).

Mit `pio run` gebaut (erfolgreich, Flash 76,4 % / 1.000.841 B, RAM 14,7 % /
48.072 B) und per `pio run --target upload` geflasht.

## Nachtrag 7: Kalibrierkorrektur für den internen DHT11-Sensor

Nutzerwunsch: der eingebaute DHT11 weicht am eigenen Standort systematisch
von einem Referenzsensor ab (Beispiel: -1°C) - ein fester, ganzzahliger
Korrekturwert (°C/%, positiv oder negativ) soll über das Webinterface
einstellbar sein.

### Korrektur direkt in SensorManager angewendet, nicht an jeder Anzeigestelle
Die Korrektur wird direkt in `SensorManager::update()` auf den validierten
Rohmesswert addiert, bevor er in `lastTempC`/`lastHumidityPct` landet -
dafür bekommt `update()` jetzt eine `const SettingsManager&`-Referenz
(gleiches Muster wie `SensormeterManager::update(settings)` und
`PingManager::update(settings)` bereits vorher). Dadurch sehen ALLE
Verbraucher (Touch-UI-Statusleiste/-Graph, Webserver-Dashboard,
`computeAlertInfo()`-Warnschwellwert-Auswertung) automatisch den bereits
korrigierten Wert - eine Alternative (Korrektur nur an der Web-Anzeige)
hätte das Risiko geschaffen, dass Touch-Display und Web unterschiedliche
Werte zeigen und Warnschwellwerte auf den falschen (unkorrigierten) Wert
reagieren.

### Plausibilitätsprüfung bleibt auf dem Rohwert
`isPlausible()` prüft weiterhin den ROHEN Sensorwert, nicht den
korrigierten - die Korrektur ist eine kleine Kalibrierkonstante (wenige
Grad/Prozent), kein Mittel zur Fehlerkompensation, und soll den
Garbage-Filter (DHT11 liefert bei Lesefehlern u.a. NaN oder Werte weit
außerhalb des Sensor-Messbereichs) nicht verfälschen. Luftfeuchte wird
nach der Korrektur auf [0, 100] geklemmt (Temperatur bewusst nicht - kein
technisch begründetes Limit für einen kalibrierten Innenraumwert).

### Bereits aufgezeichnete Verlaufsdaten bleiben unkorrigiert
`history.csv`-Einträge, die vor dem Setzen der Korrektur aufgezeichnet
wurden, werden NICHT rückwirkend angepasst (die Rohwerte sind nicht mehr
rekonstruierbar, nur die bereits gerundeten/gespeicherten Werte) - ab dem
Setzen der Korrektur sind alle NEUEN Verlaufspunkte korrigiert, ältere
bleiben auf dem alten Stand. Für eine Kalibrierkorrektur (typischerweise
einmalig beim Einrichten gesetzt) ein akzeptabler Kompromiss.

Mit `pio run` gebaut (erfolgreich, Flash 76,5 % / 1.002.237 B, RAM 14,7 % /
48.072 B) und per `pio run --target upload` geflasht.

## Nachtrag 8: Vier eigene Verbesserungsvorschläge umgesetzt

Nach Rückfrage "fallen dir noch Verbesserungen ein?" vier Vorschläge
gemacht, der Nutzer wollte 4 der 6 umgesetzt haben (Ping-Timeout-Risiko
und CSV-Export bewusst nicht - ersteres groesserer Umbau, zweites ohne
konkreten Bedarf).

### Mehrfach-Alarm-Hinweis: computeAlertInfo() zaehlt jetzt alle Kategorien
Bisher brach `computeAlertInfo()` bei der ERSTEN gefundenen Verletzung ab
(Prioritaet Intern -> Sensormeter -> Ping) - eine zweite, gleichzeitig
aktive Kategorie blieb komplett unsichtbar. Umgebaut auf drei vollstaendige
Kategorie-Checks (`checkIntern()`/`checkSensormeter()`/`checkPing()`, alle
in einer neuen anonymen Namespace-Sektion in `AlertEvaluator.cpp`), die
IMMER alle drei auswerten statt frueh abzubrechen. `AlertInfo` bekam ein
neues Feld `extraCount` (Anzahl weiterer, gleichzeitig aktiver Kategorien)
- Statusleiste haengt "+N" an die Quelle an, das Dashboard schreibt einen
ausführlicheren Satz ("außerdem N weitere Quellen betroffen"). Bewusst nur
auf Kategorie-Ebene gezaehlt (nicht z.B. pro einzelnem Sensormeter-Sensor),
da ohnehin nur eine Quelle gleichzeitig anzeigbar ist.

### WLAN-Balken im Dashboard statt Text
`wifiBarsHtml()` erzeugt drei CSS-Balken (ansteigende Hoehe, gefuellt bis
zur aktuellen Empfangsstaerke) statt des bisherigen "Signal 2/3"-Texts -
optisch konsistent zum WLAN-Symbol in der Touch-UI-Statusleiste
(`StatusBar::drawWifiIcon()`).

### Speichern-Bestätigung per Redirect-Parameter statt JavaScript
Ohne JS (bewusste Entscheidung von Anfang an, siehe Webserver-Abschnitt
oben) geht eine Erfolgsmeldung nach einem POST nur über einen Redirect-
Parameter: alle Formular-Handler redirecten jetzt auf
`/settings?saved=1` statt nur `/settings`; `buildSettingsPage()` bekam
einen `bool saved`-Parameter und zeigt bei `true` einen grünen
"Gespeichert."-Hinweis oben auf der Seite.

### Favicon als inline SVG-Daten-URI
Browser fragen bei fehlendem Favicon automatisch `/favicon.ico` an - ohne
eigene Route landete das als 404 im (nicht vorhandenen) Server-Log, aber
sichtbar als fehlendes Tab-Icon. Kleines Navy-Quadrat mit Orange-Punkt
(passend zur übrigen Palette) als `data:image/svg+xml`-URI direkt im
`<head>` beider Seiten - kein zusätzlicher Dateizugriff/Endpunkt nötig.

Mit `pio run` gebaut (erfolgreich, Flash 76,6 % / 1.003.921 B, RAM 14,7 % /
48.072 B) und per `pio run --target upload` geflasht.

## Nachtrag 2: Schwellwerte pro Sensor statt pro Ziel (PRO-Geräte)

Rückfrage des Nutzers deckte eine Lücke auf: bei einem "Sensormeter PRO"-
Gerät (2 Sensoren) hätte die ursprüngliche Umsetzung beide Sensoren
denselben Schwellwertsatz teilen lassen. Auf Nachfrage entschieden:
getrennte Schwellwerte pro Sensor, mit eigener Zeile in der Web-Tabelle.

### Datenmodell: zweite Dimension statt Verdopplung der Zielanzahl
`smTempMin_[kMaxSensormeterTargets]` (int16_t) wurde zu
`smTempMin_[kMaxSensormeterTargets][kMaxSensorsPerTarget]` (analog für
Max/Hum), mit `kMaxSensorsPerTarget=2` (Sensor 1 immer, Sensor 2 nur bei
PRO). NVS-Keys um eine zweite Ziffer erweitert (`smTn` + Ziel-Index +
Sensor-Index, z.B. `smTn01` = Ziel 1, Sensor 2) - beide Indizes sind durch
die festen Obergrenzen (5 Ziele, 2 Sensoren) garantiert einstellig, daher
ohne Trennzeichen eindeutig. `removeSensormeterTarget()` muss beim
Verschieben der Ziel-Slots jetzt beide Sensor-Unterindizes mitverschieben
und zuruecksetzen (gleiches Prinzip wie zuvor, nur eine Dimension mehr).

### Webserver braucht jetzt Lesezugriff auf SensormeterManager
Um zu wissen, ob ein Ziel tatsächlich ein PRO-Gerät ist (und damit zwei
Zeilen statt einer bekommt), muss `WebServerManager` erstmals auf
`SensormeterManager` zugreifen (bisher nur `SettingsManager` +
`BacklightManager` + `OtaManager` + `WlanManager`) - als zusätzliche
`const`-Referenz im Konstruktor, rein lesend (`isResolved()`/`isPro()`/
`sensorName()`). Für ein frisch hinzugefügtes, noch nicht aufgelöstes
Ziel ist unklar, ob PRO - bis zur ersten erfolgreichen SNMP-Identitäts-
Auflösung wird nur eine Zeile (Sensor 1) angezeigt, die zweite erscheint
danach automatisch.

### Entfernen-Button nur in der ersten Sensor-Zeile
"Entfernen" bezieht sich auf das ganze Ziel (beide Sensoren), nicht auf
einen einzelnen Sensor - der Button erscheint daher nur in der Sensor-1-
Zeile, auch wenn das Ziel PRO ist und zwei Zeilen hat.

Erneut mit `pio run` gebaut (erfolgreich) und per `pio run --target
upload` geflasht.

### Nachtrag: StatusBar hatte denselben Redraw-Cache-Bug, urspruenglich uebersehen
Nutzer meldete nach dem ersten Hardware-Test: die obere Statusleiste wird
nach dem Schliessen eines Menüs (Zahnrad/Info) nicht zuverlässig neu
gezeichnet. Ursache identisch zum oben behobenen `GraphManager`/`PingView`-
Bug, nur bei `StatusBar` selbst nicht mit-behoben: `lastStatusBarMs = 0`
in `main.cpp` erzwingt zwar den naechsten `draw()`-Aufruf, aber `draw()`
hat einen eigenen Diff-Cache (`everDrawn`/`lastBars`/...) und zeichnet bei
unveraenderten Werten trotzdem nichts (siehe Korrektur-Hinweis beim
ursprünglichen P4/P5-Eintrag oben). Fix: `StatusBar::forceRedraw()`
ergänzt, nach `InfoUI::run()`/`settingsUI.run()` aufgerufen, analog zu
`graph.forceRedraw()`/`pingView.forceRedraw()`. Erneut geflasht, Nutzer hat
am Geraet bestätigt: "sieht gut aus".

## Sicherheits-Feature: Ping-Check vor statischer IP-Vergabe

Beim Speichern der Netzwerkeinstellungen wird jetzt vor dem Übernehmen
einer neu gesetzten statischen IP per Ping geprüft, ob im Netz bereits ein
anderes Gerät unter dieser Adresse antwortet (`ipRespondsToPing()` in
`WebServerManager.cpp`, nutzt die bereits vorhandene `ESP32Ping`-Library,
die hier schon für die Ping-Zielüberwachung eingebunden ist). Ist das der
Fall, wird die neue Konfiguration nicht übernommen (kein Neustart, alte
Konfiguration bleibt aktiv) und eine Fehlermeldung angezeigt - anders als
bei den Geschwisterprojekten (dort ein zusammenhängendes Formular mit
vielen weiteren Feldern) betrifft das hier ausschließlich
`handleNetworkSave()`, da die Netzwerkeinstellungen bereits ein eigener,
isolierter Speichern-Endpunkt sind. Check läuft nur, wenn sich die
eingetragene IP von der aktuell aktiven `WiFi.localIP()` unterscheidet
(verhindert einen falschen Alarm bei Bestätigung der eigenen Adresse).
Identisches Grundmuster wie bei Sensormeter und Sensormeter WLAN.

## Werks-Passwort auf "installer" vereinheitlicht

Projektübergreifende Prüfung ergab: Sensormeter und Sensormeter WLAN
nutzten bereits beide `installer` als Web-Passwort-Default, hier war es
abweichend `admin` (`SettingsManager.h`/`.cpp`, sowohl Feld-Default als
auch der Preferences-/Leerstring-Fallback). Auf `installer` angeglichen.
Wirkt sich nur auf fabrikneu geflashte oder per NVS-Löschung
zurückgesetzte Geräte aus - ein bereits konfiguriertes Gerät behält sein
gespeichertes Passwort unverändert bei. Admin-Guide entsprechend ergänzt
(inkl. Hinweis auf die Umstellung).

Mit `pio run` gebaut und verifiziert (erfolgreich, Flash 79,2 % /
1.038.413 B, RAM 15,3 % / 50.008 B).

## DHCP-Lease-Test vor Übernahme der Netzwerkeinstellungen

Analog zum bereits vorhandenen Ping-Check bei statischer IP (siehe oben)
jetzt auch für den DHCP-Zweig in `handleNetworkSave()`: `WiFi.config(0,0,0)`
erzwingt auf der bestehenden Verbindung einen neuen DHCP-Lauf, OHNE die
WLAN-Assoziation zu trennen (reiner L3-Vorgang) - erst bei tatsächlich
erhaltener Lease (`WiFi.localIP() != 0.0.0.0`, bis zu 8s Wartezeit) gilt
der Test als erfolgreich. Schlägt er fehl, wird die zuvor gespeicherte
(ggf. statische) Konfiguration auf der laufenden Verbindung
wiederhergestellt, NICHTS gespeichert und KEIN Neustart ausgelöst -
identisches Verhaltensmuster wie bei Sensormeter und Sensormeter WLAN
(dort als eigener "IP-Einstellungen übernehmen"-Button neben dem
allgemeinen Speichern-Button; hier direkt in den bereits bestehenden,
isolierten `/network/save`-Endpunkt integriert, da Sensormeter Display
diese Trennung schon vorher hatte - kein neuer Button nötig).

Wartezeit-Konstante `kDhcpTestTimeoutMs = 8000` - noch nicht auf echter
Hardware verifiziert, ob das den Async-Webserver-Task spürbar blockiert
(gleicher offener Punkt wie bei den Schwesterprojekten). Admin-Guide
(Abschnitt 7.4, neuer Warnhinweis + Troubleshooting-Zeile) und
Einstellungsseite (neuer Hinweistext unter dem Speichern-Button)
entsprechend ergänzt.

## Admin-Guide um das Live-Status-Dashboard ergänzt (nachgezogen)

Der öffentliche, passwortlose Dashboard-Screen auf `/` (Auto-Refresh 30s,
zeigt DHT11/WLAN/Sensormeter-Ziele/Ping-Ziele + Warnleiste) war bereits
seit v0.9.0-rc2 Teil der Firmware, aber nie im Admin-Guide dokumentiert -
beim Wiederherstellen der HTML-Quelle für diese Runde aufgefallen und als
neuer Abschnitt 7.4 nachgezogen (nur diese eine Lücke geschlossen, keine
darüber hinausgehende Vollständigkeitsprüfung der übrigen v0.9.0-rc2-
Änderungen).

## Mutex-Schutz auf SensorManager/PingManager/SensormeterManager/GraphManager ausgeweitet

Bereits als TODO vermerkt (siehe frühere Notiz zu diesem Projekt): seit
dem Live-Status-Dashboard (v0.9.0-rc2) liest der asynchrone Webserver-Task
`SensorManager`, `PingManager`, `SensormeterManager` und `GraphManager`
gleichzeitig zum Hauptloop, der sie schreibt - keine der vier hatte bisher
eine Sperre (anders als `SettingsManager`, das dieses Muster von Anfang an
nutzt). Risiko konzentriert auf `String`-Felder (z. B.
`PingManager::targetIp()`, `SensormeterManager::systemName()`/
`sensorName()`) - `Arduino::String` ist nicht thread-sicher, ein
gleichzeitiger Lese-/Schreibzugriff auf denselben internen Puffer kann zu
Speicherfehlern/Abstürzen führen.

**Umsetzung** (identisches Muster wie `SettingsManager`: `SemaphoreHandle_t
mutex_`, jeder öffentliche Getter/Setter nimmt/gibt die Sperre, private
`save()`-artige Helfer setzen voraus, dass der Aufrufer sie bereits
hält):
- `SensorManager`: unproblematisch komplett gesperrt - `dht.readTemperature()`/
  `readHumidity()` sind kurze 1-Wire-Zugriffe, kein Netzwerk.
- `GraphManager`: nur die vier öffentlichen `entry*()`-Getter und
  `appendEntry()` gesperrt - die private Zeichenlogik
  (`drawFullScreen()`/`drawGraph()`) läuft ausschließlich im selben Task
  wie die Schreibzugriffe und braucht daher keine Sperre.
- `PingManager`/`SensormeterManager`: die eigentliche, blockierende
  Netzwerk-I/O (`Ping.ping()` bis zu ~1s, SNMP-Rundreisen bis zu mehrere
  Sekunden bei Nichterreichbarkeit) läuft bewusst AUSSERHALB der Sperre -
  ein naives "ganzes update() sperren" hätte den Webserver-Task bei jedem
  Dashboard-Aufruf um bis zu mehrere Sekunden blockiert. Stattdessen:
  Ergebnis in lokale Variablen schreiben, dann nur das kurze Übernehmen in
  die Member-Felder sperren. Bei `SensormeterManager::resolveIdentity()`/
  `refreshReadings()` zusätzlich eine IP-Übereinstimmungsprüfung beim
  Übernehmen (Absicherung gegen den seltenen Fall, dass sich die Zielliste
  während der mehrere Sekunden dauernden SNMP-Rundreise über die
  Einstellungen geändert hat).
- `SensormeterManager` brauchte dafür erstmals eine eigene `begin()`-Methode
  (Mutex-Erzeugung) - in `main.cpp` ergänzt.

Mit `pio run` gebaut und verifiziert (erfolgreich, Flash 79,5 % /
1.041.757 B, RAM 15,3 % / 50.024 B). Noch NICHT auf echter Hardware
geflasht/getestet in dieser Runde - insbesondere die Nebenläufigkeit
selbst (Dashboard-Aufruf waehrend eines laufenden Ping/SNMP-Zyklus) ist
ohne ein zweites, aktiv abfragendes Geraet schwer gezielt zu provozieren.
Firmware-Version auf `0.9.0-rc3` angehoben (`config.h`/`config.h.example`,
Admin-Guide, One-Pager).

## Anbieter-Branding (Weisslabel) umgesetzt - erstes Farb-Display der Familie

Letztes der vier Sensormeter-Projekte, das das bei Sensormeter WLAN
eingeführte Branding-Feature (siehe dessen `docs/entscheidungen.md`)
bekommt - hier aber eine echte Neuentwicklung statt einer reinen Portierung,
da dieses Board als einziges ein Farb-TFT (ST7789P3) statt eines
monochromen OLEDs hat. Kein `ConfigManager`/`config.xml` wie bei den
Geschwistern, sondern das projekteigene `SettingsManager`/NVS-Schema.

- **Vendor-Name**: neues Feld in `SettingsManager` (NVS-Key `brandName`),
  identisches Muster zu `deviceName_`/`webPassword_` - anders als
  `deviceName()` OHNE Default-Fallback (leer = Feature inaktiv).
- **Logo NICHT in NVS**: ein 16.384-Byte-Blob waere fuer einen einzelnen
  NVS-Eintrag ungeeignet gross - stattdessen wie bei den Geschwistern eine
  eigene Datei auf LittleFS (`/branding-logo.bin`), verwaltet durch die
  neue Klasse `BrandingManager` (eigener Mutex, Tmp-Datei-plus-Umbenennen-
  Upload wie `ConfigManager::save()` bzw. die OLED-Geschwister, RAM-Cache
  fuer `hasLogo()` statt wiederholtem `LittleFS.exists()` - letzteres von
  Anfang an vermieden, siehe der bei Sensormeter WLAN gefundene Logquirk).
- **Neue Datenquelle statt eigenem Sonderfall**: `DataSource::Branding`
  als sechster Wert in `kAvailableDataSources` - bewusst IMMER Teil der
  Slide-Rotation/Static-Auswahl, auch unkonfiguriert (zeigt dann einen
  Platzhalter "Kein Branding konfiguriert", exakt wie
  `PingView::drawTargetList()` bei leerer Zielliste) - ANDERS als bei den
  OLED-Geschwistern (dort dynamisches Ein-/Ausblenden), weil dieses Projekt
  ohnehin nie nach Konfigurationsstand filtert (Sensormeter-/Ping-Ziele
  bleiben ebenfalls immer in der Rotation).
- **Logo-Format 128x64** (nicht z. B. 200x100 wie zunaechst entworfen):
  bewusst dieselbe Aufloesung wie die drei OLED-Geschwisterprojekte
  gewaehlt, nur mit RGB565 (2 Byte/Pixel) statt 1bpp - damit
  `scripts/convert-logo.ps1` fuer alle vier Projekte dieselbe Zielgroesse
  nutzt und nur die Farbtiefe je Display umschaltet. Ergebnis: 16.384 Byte
  Rohdaten statt der zunaechst veranschlagten 40.000 Byte.
- **Web-Auslieferung als 24-Bit-BMP** (nicht 16-Bit): eine korrekte
  16-Bit-BMP braucht eine BITFIELDS-Farbmaskenerweiterung (mehr
  Fehlerpotential, weniger portabel) - der zusaetzliche Speicher fuer
  24 Bit faellt bei dieser Logo-Groesse nicht ins Gewicht.
- **Echter Bug gefunden und behoben, bevor er ausgeliefert wurde**: die
  erste Fassung von `handleBrandingLogoBmp()` allozierte den BMP-Puffer
  auf dem Heap und gab ihn UNMITTELBAR nach `request->send()` wieder frei
  - ein Use-after-free, da ESPAsyncWebServers `beginResponse(code, type,
  const uint8_t*, len)`-Ueberladung (`AsyncProgmemResponse`) nur den
  rohen Pointer haelt und asynchron ERST NACH Rueckkehr aus dieser
  Funktion tatsaechlich daraus liest (siehe `WebResponseImpl.h` in der
  Bibliothek - `_content` ist dort ein roher `const uint8_t*`, keine
  Kopie). Behoben durch Umstellung auf statische Puffer (analog zum
  kleineren statischen BMP-Puffer bei den OLED-Geschwistern) - dabei
  gleich die Logo-Groesse von 200x100 auf 128x64 reduziert (siehe oben),
  um die beiden nun permanent reservierten Puffer (Roh-Logo 16.384 Byte +
  BMP 24.630 Byte, zusammen ~41 KB) in einem vertretbaren Rahmen zu
  halten.
- **Farbkanal-Vorbehalt, bewusst NICHT vorab kompensiert**: `platformio.ini`
  setzt `TFT_RGB_ORDER=0` (BGR) fuer dieses Panel, und selbst unter dieser
  gewaehlten Einstellung mussten einzelne benannte Farben empirisch
  kompensiert werden (`TFT_YELLOW` statt `TFT_RED`, siehe weiter oben in
  diesem Protokoll) - der MADCTL-Farbkanaltausch verhaelt sich auf diesem
  Panel/Klon also nicht rein spezifikationsgemaess. Logos werden daher
  bewusst in Standard-RGB565 gepackt (wie `TFT_eSPI::color565()` es auch
  tut) statt blind zu kompensieren, um nicht falsch herum zu tauschen.
  `scripts/convert-logo.ps1 -Display display -SwapRedBlue` als
  Escape-Hatch, falls sich auf echter Hardware zeigt, dass doch getauscht
  werden muss.
- **Neue Web-UI-Elemente**: Branding-Banner (Logo + Name) unterhalb des
  Haupt-Banners auf Dashboard UND Einstellungsseite (nur ausgegeben, wenn
  etwas konfiguriert ist), eigenes Fieldset "Anbieter-Branding"
  (Vendor-Name im Haupt-`/save`-Formular) sowie "Branding-Logo" (eigenes
  Multipart-Formular, analog Firmware-Update) mit Lösch-Button.

**Unabhängig verifiziert, nicht auf echter Hardware**: sowohl das
PC-seitige `convert-logo.ps1`-Packen als auch die Firmware-seitige
`buildLogoBmp()`-Logik wurden 1:1 nach Python portiert, gegen dieselbe
128x64-Testdatei (Kreis/Rechteck-Testmuster) laufen gelassen und mit
Pillow geöffnet - beide erzeugen pixelgenau dasselbe Bild wie die
Ausgangsdatei. Das eigentliche Rendering auf dem TFT (`pushImage()`,
insbesondere der Farbkanal-Vorbehalt oben) sowie der Upload-Web-Flow
selbst sind mangels angeschlossenem Board in dieser Session NICHT auf
echter Hardware bestätigt.

Mit `pio run` gebaut und verifiziert (erfolgreich, Flash 80,1 % /
1.050.265 B, RAM 27,8 % / 91.120 B - gegenüber dem Stand vor dieser
Änderung, 79,5 % / 1.041.757 B Flash, 15,3 % / 50.024 B RAM, ein Zuwachs
von +8.508 B Flash / +41.096 B RAM). Der RAM-Zuwachs ist deutlich größer
als bei den drei Geschwisterprojekten (dort jeweils nur wenige KB), da
hier zwei permanente statische Puffer für die Web-BMP-Auslieferung nötig
sind (siehe oben) - bei 327.680 B Gesamt-RAM bleiben trotzdem knapp 237 KB
(72 %) frei, unkritisch. Nicht geflasht - kein Board in dieser Session
angeschlossen.

## Verdrahtungsplan interaktiv: docs/verdrahtungsplan.html neu angelegt

Auf Anfrage, familienweit für alle vier Projekte. Im Unterschied zu den
drei Geschwisterprojekten gab es hier bisher **gar keinen**
Verdrahtungsplan (weder PDF noch HTML) - nur die Pin-Zuordnungen verstreut
in `firmware/include/pins.h` und `docs/entscheidungen.md`.

Besonderheit dieses Projekts: HW-458B ist ein integriertes Board (ESP-
WROOM-32 + ST7789P3-TFT + resistiver Touch bereits auf einer Platine
verlötet) - TFT- und Touch-Pins sind also feste PCB-Leiterbahnen, keine
vom Nutzer gesteckten Jumper-Kabel. Tatsächlich extern verkabelt wird nur
der DHT11-Sensor über den 4-poligen Erweiterungsanschluss "IO2". Die RGB-
Status-LED gilt laut Datenblatt ebenfalls als onboard.

Entscheidung: trotzdem alle vier Baugruppen (TFT, Touch, DHT11, RGB-LED)
als anklickbare Drähte im selben Schema zeigen, aber farblich/mit Rahmen
und Beschriftung klar zwischen "onboard, fest verdrahtet" (TFT blau, Touch
grün, RGB-LED lila umrandet) und "extern, tatsächlich zu verkabeln" (DHT11
orange umrandet) unterscheiden - liefert denselben Nachschlage-Nutzen
("welcher GPIO geht wohin") unabhängig davon, ob es sich um eine
Platinenleiterbahn oder ein Kabel handelt, ohne den Eindruck zu erwecken,
TFT/Touch müssten verkabelt werden. Zusätzlicher Callout-Absatz "Onboard
vs. extern" stellt das noch einmal explizit klar.

18 Drähte insgesamt (mehr als bei den Geschwisterprojekten mit 7-15, da
vier statt zwei-drei Baugruppen): 3V3→DHT11 VCC, GND→DHT11 GND, GND→RGB-
LED GND, sechs TFT-Signale (MOSI/MISO/SCLK/CS/DC/BL), fünf Touch-Signale
(CLK/CS/DIN/OUT/IRQ), DHT11 DATA, drei RGB-LED-Kanäle (R/G/B). `TFT_RST`
(-1, an EN gebunden) erscheint bewusst nicht als eigener Draht - kein
echter GPIO. Gleiches Interaktionsmuster wie bei den drei anderen
Projekten übernommen (Inline-SVG, unsichtbarer breiterer Hit-Pfad je
Draht, Info-Zeile "Von → Nach", Klick zum Ab-/Auswählen).

Pinbelegung 1:1 aus `firmware/include/pins.h` übernommen (bereits dort mit
Herkunftsnachweis dokumentiert: TFT/Touch-Pins über das baugleiche
Referenzdesign LCDWIKI E32R28T/E32N28T bestätigt, nicht am eigenen Board
nachgemessen) - keine neuen Zuordnungen erfunden. Ungenutzte, laut
Datenblatt aber vorhandene Pins (Audio, Lichtsensor, SD-Karte) als reine
Text-Auflistung am Ende ergänzt, nicht als Drähte (analog zu den
"Vermiedene Pins"-Abschnitten der Geschwisterprojekte).

Getestet mit Headless Chrome (`--dump-dom` + synthetischer Klick auf Draht
`w12`, IO32→Touch DIN): korrektes Hervorheben (1 aktiv, 17 gedimmt),
korrekter Info-Text. Zusätzlich Screenshot (`--screenshot`,
900×2600-Fenster) zur visuellen Kontrolle des neuen, deutlich dichteren
4-Baugruppen-Layouts angefertigt - keine Überlappungen, alle vier Module
und alle 18 Drahtlinien sauber lesbar. Kein Board nötig, rein
clientseitiges HTML/JS ohne Firmware-Bezug.

## Verdrahtungsplan auf DHT11-Draht reduziert, offizielles Produktbild als Quelle ergänzt

Nutzer wies auf ein offizielles Verkäufer-Produktbild der HW-458B-Platine
hin (in `docs/hw-458b-pinout-datenblatt.jpg` übernommen) und bat darum, im
interaktiven Verdrahtungsplan nur noch den DHT11-Anschluss als Drähte zu
zeichnen - TFT, Touch und RGB-LED sind ja bereits vorhanden (onboard),
dafür keine eigene "Verkabelung" im Sinne des Schemas nötig.

**Schema reduziert**: von 18 auf 3 Drähte (3.3V→DHT11 VCC, GND→DHT11 GND,
IO22→DHT11 DATA). Die vollständige Pinbelegung aller Baugruppen bleibt in
der Tabelle "Pinbelegung" erhalten (reine Referenz, keine Drähte mehr) -
nur das interaktive SVG-Schema selbst zeigt jetzt ausschließlich die eine
tatsächliche Handverkabelung.

**Produktbild als zusätzliche Quelle**: das Bild bestätigt direkt (nicht
nur über ein ähnliches Referenzdesign) Touch-Pins (IO25/33/32/39/36),
Backlight (IO21), RGB-LED (IO17/4/16), Audio (IO26), Lichtsensor/
Fotowiderstand (IO34), SD-Kartenslot (CD/DAT3=IO5, CMD=IO23, CLK=IO18,
DATA0=IO19) sowie den Erweiterungsanschluss "IO2" (GND/IO22/IO27/3.3V,
exakt wie in `pins.h` dokumentiert) - alles deckungsgleich mit den
bisherigen Annahmen, keine Korrektur nötig. Einzige Ausnahme: die
TFT-SPI-Pins (MOSI/MISO/SCLK/CS/DC) sind auf dem Bild nicht abgebildet
(nur "Touchscreen chip" beschriftet, nicht der TFT-Treiber) - dafür gilt
weiterhin nur die schwächere Bestätigung über das Referenzdesign LCDWIKI
E32R28T/E32N28T. `firmware/include/pins.h` und
`docs/verdrahtungsplan.html` entsprechend präzisiert (welche Pin-Gruppe
über welche Quelle bestätigt ist).

Bild unter `docs/hw-458b-pinout-datenblatt.jpg` ins Repo übernommen und im
Verdrahtungsplan als `<figure>` mit Quellenangabe eingebunden.

Getestet mit Headless Chrome (`--dump-dom` + synthetischer Klick auf Draht
`w3`, IO22→DHT11 DATA): korrektes Hervorheben (1 aktiv, 2 gedimmt).
Zusätzlich Screenshot zur visuellen Kontrolle des vereinfachten Schemas
sowie des eingebetteten Produktbilds - beides sauber ohne Layoutfehler.

## Architekturübersicht auf vier Familienmitglieder erweitert, Sensormeter PoE ergänzt

`docs/projektfamilie.html` (identische Kopie in allen fünf
Sensormeter-Repos) zeigte bisher nur drei Karten (Sensormeter, Sensormeter
WLAN, Sensormeter Display) - Sensormeter PoE fehlte komplett, obwohl es
seit einiger Zeit ein vollwertiges viertes Familienmitglied ist. Nutzer
wies darauf hin, dass im Family-Repo die Übersicht sogar ganz fehlte.

Diagramm neu aufgebaut: drei Karten oben (WLAN/Sensormeter/PoE, Sensormeter
als Ursprung in der Mitte, dünne durchgezogene Linien zu den beiden
Varianten), Sensormeter Display darunter zentriert mit gestrichelten
SNMP-Linien zu allen drei. Neue Akzentfarbe `--rust` (Blitz-Icon) für PoE
ergänzt - fehlte bisher in `projektfamilie.html` (existierte nur in
`implementierungsplan.html`, dort wiederum ohne "-strong"-Variante).

Positionierung nicht mehr aus dem alten 3-Karten-Layout übernommen,
sondern per Skript (`getBoundingClientRect()` auf einer Kopie mit
angehängtem Mess-Script, da echte Karteninhalte wegen Zeilenumbruch bei
PoE deutlich höher sind als angenommen - 372px statt der ursprünglich
angenommenen ~240px) neu vermessen, um Überlappung mit den gestrichelten
SNMP-Linien zu vermeiden.

`docs/projektfamilie-light.png`/`-dark.png` (statische Vorschaubilder fürs
GitHub-README, da GitHub keine CSS-Media-Queries im Markdown auswertet)
aus dem aktualisierten Inhalt neu gerendert. README-Text ("wie die drei
Sensormeter-Projekte") und Bild-Alt-Text ebenfalls auf vier Projekte
korrigiert.

Getestet: Headless-Chrome-Screenshots hell/dunkel, `getBoundingClientRect()`-
Vermessung aller vier Karten (keine Überlappung mehr). Rein statisches
HTML/CSS/SVG ohne Firmware-Bezug, kein Board nötig.

## Werksreset-Umfangsauswahl + Serial-Kommandozeile neu gebaut (nicht portiert), Version auf 0.9.0-rc4

Beide Features wurden zuerst in Sensormeter WLAN gebaut und dort auf echter
Hardware verifiziert, dann auf die drei Geschwisterprojekte (Sensormeter,
Sensormeter PoE) portiert - siehe deren jeweilige `entscheidungen.md`. Bei
Sensormeter Display war ein reiner Port NICHT möglich: dieses Projekt hatte
bisher **überhaupt keinen Werksreset** (weder Taster noch Web) und **keine
XML-Konfiguration** - Einstellungen liegen einzeln in NVS/Preferences
(`SettingsManager`, `WlanManager`) statt in einer `DeviceConfig`-Struktur
mit `exportXml()`/`importXml()`. Rücksprache mit dem Nutzer ergab: vollen
Funktionsumfang bauen, aber OHNE `dump`/`upload` (kein XML-Dokument, das
sich sinnvoll ex-/importieren ließe - ein neues JSON-Format nur für dieses
eine Projekt wäre unverhältnismäßiger Aufwand gegenüber dem Nutzen).

**Neue Reset-Primitive** (vorher nicht vorhanden):
- `SettingsManager::resetConfig(bool keepBrandingName)`: leert den kompletten
  NVS-Namespace "settings" (Modus, Helligkeit, Sensormeter-/Ping-Ziele samt
  Schwellwerten, DHT11-Kalibrierung, Geräte­name, Web-Passwort) und lädt die
  eingebauten Defaults in den RAM-Zustand zurück; `keepBrandingName` schreibt
  den Anbieternamen nach dem `clear()` wieder zurück (Umfang "Konfiguration").
- `WlanManager::clearCredentials()`: leert den kompletten NVS-Namespace
  "wifi" (SSID/PSK UND statische IP) - ein Neustart landet danach wieder im
  Onboarding.
- `GraphManager::reset()`: leert den 12h-Ringpuffer und löscht `history.csv`.

Alle drei sind bewusst getrennte, gezielte Operationen statt eines
einzelnen "alles"-Resets - der Aufrufer (`WebServerManager::
handleFactoryReset()`, `main.cpp::handleSerialCommands()`) kombiniert sie
je nach gewähltem Umfang (Alles/Konfiguration/Messwerte/Branding), analog
zum `DeviceConfig()`-Default-Konstruktor-Muster der drei Geschwisterprojekte.

**Bewusst NICHT angerührt**: die Touch-Kalibrierung (`TouchManager`, NVS-
Namespace "touch") - das sind physische Rohwert-Extrema des
Touch-Controllers, kein Konfigurationswert im Sinne des Lastenhefts. Bei
keinem der drei Geschwisterprojekte gibt es eine vergleichbare
Hardware-Kalibrierung, die ein Werksreset ausließe (deren
Sensor-Temperatur-/Feuchte-Offsets sind dagegen echte Konfigurationswerte
und werden mitzurückgesetzt - hier ebenso, `dhtTempOffsetC`/
`dhtHumOffsetPct` sind Teil von `resetConfig()`).

**Web** (`WebServerManager.cpp`): neues Fieldset "Werksreset" auf der
Einstellungsseite mit Auswahlmenü (Alles/Konfiguration/Messwerte/Branding)
und JS-Bestätigungsdialog (`confirmReset()`) - erstes `<script>`-Element
in diesem Projekt überhaupt, da die Einstellungsseite bisher rein
serverseitig gerendert wurde (keine AJAX-Formulare wie bei den
Geschwisterprojekten). Dafür musste `graph_` in `WebServerManager` von
`const GraphManager&` auf `GraphManager&` geändert werden (Konstruktor +
Header + `main.cpp`-Instanziierung), da `reset()` mutierend ist.

**Serial** (`main.cpp`): `status`, `dhcp`, `ip <ip> <maske> <gateway>`
(kein DNS-Feld - `WlanManager` kennt keine separate DNS-Konfiguration),
`wifi <ssid> <passwort>`, `reset`/`reset all`. Nur ein Netzwerk-Interface
(WLAN, kein LAN) wie bei Sensormeter WLAN, daher ohne Interface-Argument
bei `dhcp`/`ip` - anders als bei Sensormeter/Sensormeter PoE (LAN+WLAN).
`status` hat kein "Zustand:"-Feld wie die Geschwisterprojekte (kein
`SystemState`-Konzept in diesem Projekt), zeigt stattdessen WLAN-Status/IP/
Signal (direkt über `WiFi.localIP()`/`WiFi.RSSI()`, da `WlanManager` dafür
keine eigenen Getter hat) und den DHT11-Messwert.

Version auf **`0.9.0-rc4`** angehoben (vorher `0.9.0-rc3`) - passend zum
gemeinsamen Stand aller vier Sensormeter-Projekte, siehe "## Versionierung"
weiter oben (dieses Projekt führte das SemVer-Schema für die Familie
zuerst ein).

Nur per `pio run` gebaut (kein Board für Sensormeter Display in dieser
Sitzung angeschlossen), Build erfolgreich (Flash 80,7 %, RAM 27,8 %).
Noch nicht auf echter Hardware verifiziert - anders als der übrige, bereits
verifizierte Funktionsumfang dieses Projekts (siehe README, Abschnitt
"Auf echter Hardware verifiziert").

## Familien-Standardlogo automatisch provisioniert, außer eigenes Logo bereits vorhanden

Analog zu Sensormeter WLAN (dort zuerst umgesetzt und auf echter Hardware
verifiziert, siehe dessen `entscheidungen.md`): das Standard-Branding-Logo
(Tri-Orbit + Dial Mark, siehe sensormeter-family) musste bisher nach jedem
Flash manuell über die Einstellungsseite hochgeladen werden. Jetzt
automatisch: `BrandingManager::begin()` prüft zuerst `checkLogoOnDisk()`
wie bisher, ruft bei fehlendem Logo aber zusätzlich neu
`provisionDefaultLogo()` auf, das das in `DefaultLogo.h` eingebettete
Rohbild (128×64, RGB565, 2 Byte/Pixel, exakt 16.384 Byte) einmalig auf
LittleFS schreibt - Tmp-Datei-plus-Umbenennen-Muster wie beim regulären
Web-Upload.

**Genau das erwartete Verhalten, kein Entweder-Oder:** Ist bereits ein
Logo vorhanden (eigenes hochgeladenes ODER schon einmal automatisch
provisioniertes), bleibt es unangetastet - `provisionDefaultLogo()` wird
dann gar nicht erst aufgerufen. Ein Kunden-eigenes Logo wird also nie
überschrieben, auch nicht bei einem erneuten Firmware-Flash (ein normaler
`pio run --target upload` rührt die LittleFS-Datenpartition ohnehin nicht
an). Der Anbietername selbst (in `SettingsManager`/NVS, unabhängig vom
Logo) ist von dieser Automatik nicht betroffen - bleibt weiterhin leer,
bis er manuell gesetzt wird.

`DefaultLogo.h` liegt im Repo (kein Klartext-Geheimnis, nur Bilddaten) und
wird aus der vorkonvertierten Datei in `sensormeter-family/logo/` generiert
- bei einem neuen Default-Logo einfach neu konvertieren und den
Array-Inhalt ersetzen, nicht von Hand editieren.

Gilt hier derselbe TFT-Farbkanal-Vorbehalt wie beim manuellen Logo-Upload
(Abschnitt 7.5 im Admin-Guide): das eingebettete Default-Logo ist in
Standard-RGB565 gepackt, nicht vorkompensiert für die BGR-Panel-Einstellung
dieses Boards - auf echter Hardware könnte es daher farblich vertauscht
erscheinen (Rot/Blau), bis das durch einen Hardware-Test bestätigt oder
widerlegt ist.

Nur per `pio run` gebaut (kein Board für Sensormeter Display in dieser
Sitzung angeschlossen) - beide Zweige (Logo fehlt → automatisch schreiben;
Logo vorhanden → nichts tun) nur per Code-Review verifiziert, nicht auf
echter Hardware getestet.

## `scripts/flash.sh`: Mac-/Linux-Unterstützung umgesetzt

Neues `scripts/flash.sh` (Bash-Pendant zu `flash.ps1` für macOS - nur
Apple Silicon/arm64 - und Linux, nur Flashen, kein `convert-logo`/
`snmp-load`-Äquivalent) identisch aus dem Sensormeter-Repo übernommen -
volle Begründung und Verifizierungsstand dort in `docs/entscheidungen.md`
("`scripts/flash.sh`: Mac-/Linux-Unterstützung umgesetzt"). Bei diesem
Projekt entfällt der `config.h`-Anlage-Schritt wie bei `flash.ps1` schon
(`HasConfigH`/`project_has_config_h` liefert hier `0`/false) - alle
Einstellungen liegen zur Laufzeit in NVS statt zur Kompilierzeit.

## 2026-07-16 — OTA-Upload: Projekt-/Versionspruefung gegen Verwechslungen

Direkter Anlass: die Frage, wie sichergestellt wird, dass niemand diese
Firmware versehentlich auf ein Sensormeter-PoE-Geraet (oder umgekehrt)
hochlaedt - `/ota/upload` pruefte bisher nur Basic-Auth, nicht den Inhalt
der `.bin`. Identischer Mechanismus in allen vier Projekten der Familie
umgesetzt, siehe Sensormeter-Eintrag vom selben Tag fuer die
vollstaendige Begruendung.

- Neues Feld `FIRMWARE_PROJECT_ID` (`"SENSORMETER-DISPLAY"`) neben
  `DEVICE_FIRMWARE_VERSION` in `include/config.h(.example)`. Beide
  zusammen ergeben den in `main.cpp` einkompilierten Marker
  `"SM-FW-ID:SENSORMETER-DISPLAY:0.9.0-rc4:SM-FW-END"`.
- **Abweichung von den drei Schwesterprojekten**: dort bindet `main.cpp`
  `config.h` neu per hartem `#error` bei Fehlen ein (identisches Muster
  zu deren bereits bestehendem Include). Hier war `config.h` bisher gar
  nicht in `main.cpp` eingebunden und ist projektweit **nicht**
  verpflichtend (siehe Eintrag oben, `project_has_config_h = false`) -
  daher stattdessen derselbe weiche Fallback wie in
  `WebServerManager.cpp` (`#ifndef DEVICE_FIRMWARE_VERSION #define ...
  "0.0.0"`, hier zusaetzlich `FIRMWARE_PROJECT_ID` -> `"UNKNOWN"`). Ein
  hartes `#error` haette den `config.h`-losen Kompilierpfad gebrochen,
  den `flash.sh`/`flash.ps1` fuer dieses Projekt bewusst offen lassen.
- `OtaManager` sucht diesen Marker chunk-uebergreifend im Byte-Stream
  eines Uploads, vergleicht Projekt-ID (exakt) und Version (Semver,
  `a.b.c[-rcN]`-Schema) gegen die eigenen Werte - `endLocalUpdate()`
  committet nur bei Uebereinstimmung, sonst `Update.abort()`. Mitglieds-
  namen hier mit Trailing-Underscore (`markerFound_` etc.) statt
  Leading-Underscore, passend zum bestehenden Stil dieses Projekts
  (`otaInProgress_`/`otaSuccess_`).
- Neue Checkbox "Downgrade erzwingen" im Firmware-Formular (bewusst VOR
  dem Datei-Feld wegen ESPAsyncWebServers Multipart-Parse-Reihenfolge).
- Vier unterscheidbare Fehlermeldungen statt einem generischen "Update
  fehlgeschlagen" (Schreibfehler / kein Marker / falsches Projekt / zu
  alte Version) - ohne Log-Eintrag, da dieses Projekt (anders als die
  drei Schwesterprojekte) keinen `pushLogEntry`-Mechanismus hat.
- Kein kryptografischer Schutz, nur Verwechslungs-Pruefung - siehe
  Sensormeter-Eintrag.

Getestet: `pio run` - baut sauber (Flash 82,3%/RAM 27,8%, unveraendert
gegenueber vorherigem Stand). Marker per Byte-Suche in `firmware.bin`
verifiziert (`SM-FW-ID:SENSORMETER-DISPLAY:0.9.0-rc4:SM-FW-END`). Nicht
getestet: echter OTA-Upload auf echter Hardware.

**Standing-Vorgabe**: dieser Mechanismus ist ab jetzt fester Bestandteil
dieses Projekts und laeuft bei kuenftigen Firmware-Versionen automatisch
mit (siehe Sensormeter-Eintrag).

## 2026-07-16 — OTA-Marker-Scan byte-sicher gemacht (Bug uebernommen aus Sensormeter)

Beim Debuggen eines Zeitzonen-Fixes im Schwesterprojekt `sensormeter`
wurde entdeckt, dass der dortige OTA-Marker-Scan (`OtaManager::
scanChunkForMarker()`) eine echte, gueltige `.bin`-Datei nie akzeptieren
konnte: die Suche nutzte Arduino `String`/`indexOf()` (intern
`strstr()`-basiert) auf den rohen Binaerdaten des Uploads - `strstr()`
bricht am ersten eingebetteten Null-Byte ab, ein kompiliertes
ESP32-Image hat sein erstes Null-Byte aber schon bei Byte 9
(Image-Header-Padding), lange vor dem Marker selbst. Dieses Projekt hat
denselben OTA-Marker-Mechanismus mit identischem Code-Muster (nur mit
diesem Projekts abweichender trailing-underscore-Namenskonvention,
`markerFound_` statt `_markerFound` - siehe `sensormeter`-Eintrag
"OTA-Marker-Scan fand echte .bin nie" fuer die volle Herleitung) - Bug
bestaetigt, gleiche Ursache.

Fix uebernommen (Namenskonvention dieses Projekts beibehalten):
`OtaManager::scanChunkForMarker()` auf rohe `uint8_t`-Puffer und eine
Null-Byte-sichere `findBytes()`-Suche (memcmp-basiert statt
strstr-basiert) umgestellt, `String tail_`/`capture_` durch feste
Byte-Puffer ersetzt. `pio run` erfolgreich (Flash 82,3%/RAM 27,9%,
praktisch unveraendert). Nicht getestet: echter OTA-Upload auf echter
Hardware - kein Board in dieser Sitzung angeschlossen.

## 2026-07-18 — OTA-Marker-Scan: identischer Chunkgroessen-Bug wie sensormeter, vorsorglich gefixt

Bei sensormeter deckte der erste echte End-to-End-OTA-Test (HTTP-Upload
ueber LAN, nicht nur `pio run`) auf, dass der obige Fix zwar das
urspruengliche NUL-Byte-Problem loeste, dabei aber einen neuen,
subtileren Bug einbaute: `scanChunkForMarker()` kopierte jeden Chunk in
einen auf `kTailCap_+512` = 528 Byte GEDECKELTEN Zwischenpuffer und
durchsuchte nur diesen - ein echter HTTP-Upload liefert aber
regelmaessig groessere Chunks, wodurch alles jenseits der Deckelung
STILLSCHWEIGEND uebersprungen wurde (weder gescannt noch als Tail
vorgemerkt). Vollstaendige Herleitung samt Live-Nachweis (Geraetelog
zeigt Ablehnung vor dem Fix, Erfolg danach) im `sensormeter`-Repo,
Eintrag "Task-Watchdog + Stack-Overflow-Fix erfolgreich seriell
geflasht; dabei zweiten OTA-Marker-Scan-Bug gefunden und live behoben".

Dieses Projekt hat exakt dieselbe `kJoinCap_ = kTailCap_ + 512`-Struktur
(nur mit der abweichenden trailing-underscore-Namenskonvention) - Bug
per Quellcode-Vergleich bestaetigt, nicht nur vermutet. Fix identisch
uebernommen: kein kopierter Zwischenpuffer mehr fuer den Chunk selbst -
`findBytes()` durchsucht `data`/`len` jetzt direkt, ein kleiner
Join-Puffer (`kTailCap_+kMarkerPrefixLen` = 25 Byte) wird nur noch fuer
den echten Tail-Grenzfall gebraucht. `pio run` erfolgreich (Flash/RAM
praktisch unveraendert). Noch nicht auf echter Hardware geflasht/per
echtem OTA-Upload getestet - Board wird in dieser Sitzung angeschlossen.

## 2026-07-21 — Jedes abgefragte sm bekommt einen eigenen Slide + neue Uebersichtsseite (Name+Uptime)

Nutzerbefund: bei mehreren konfigurierten Sensormeter-Zielen zeigte "der
Slide" im Slide-Modus faktisch nur eines - "Sensormeter" war EIN Slot in
der aeusseren Rotation (`kAvailableDataSources`), der intern per eigenem
4s-Timer (`SensormeterView::currentSlide_`) durch alle aufgeloesten Ziele
rotierte. Fuer den Betrachter, der diesen internen Takt nicht kennt, sah
das wie ein einzelnes, sich gelegentlich aenderndes Geraet aus, nicht wie
mehrere.

Fix: `main.cpp` baut jetzt bei jedem `loop()`-Durchlauf eine dynamische
Slide-Liste (`buildSlideEntries()`/`SlideEntry[]`) - jedes aufgeloeste Ziel
(und bei PRO-Geraeten Sensor 2 zusaetzlich) bekommt einen eigenen Eintrag
in der AEUSSEREN Rotation, `SensormeterView::draw()` bekommt dafuer einen
neuen optionalen `overrideTargetIndex`/`overrideSensorIndex`-Parameter
(Default -1 = alte interne Auto-Rotation, weiterhin fuer den Static-Modus
genutzt). Zusaetzlich eine neue Datenquelle `DataSource::SensormeterOverview`
(eigene Tabellen-Ansicht `SensormeterView::drawOverview()`: Systemname +
Uptime aller konfigurierten Ziele) - wird von `buildSlideEntries()` nach
den Geraete-Slides eingefuegt.

Uptime war bisher gar nicht abgefragt - SNMP-OID `.5.1.0` existiert auf der
Gegenstelle bereits (`sensormeter/repo/firmware/src/SNMPManager.cpp`), aber
als TimeTicks-Typ (ASN.1-Tag 0x43), nicht INTEGER (0x02) wie die bisher
einzige genutzten Typen. `SnmpClient::getTimeTicks()` neu ergaenzt,
`SensormeterManager::refreshReadings()` fragt sie ab (nicht in
`resolveIdentity()`, da sie sich laufend aendert).

Nebenbefund beim Bauen: `SettingsUI.cpp`s Static-Quellen-Liste hatte eine
fest verdrahtete Zeilenhoehe (40px) fuer 6 Eintraege - mit dem 7. Eintrag
waere die Liste ueber den 240px-Bildschirmrand hinausgelaufen. Auf 28px
reduziert. Web-Dashboard-Dropdown (`WebServerManager.cpp`) war bereits
generisch (HTML `<select>`), keine Anpassung noetig.

`pio run` erfolgreich (Flash 82.4%, RAM 27.9%). Noch nicht auf echter
Hardware getestet.

## 2026-07-21 — Vorgemerkt: Web-Graphen pro abgefragtem sm-Ziel, nach dem "sm-Modell"

Nutzeranfrage (nur Frage, noch nicht umgesetzt): im Web-Interface fuer
jedes per SNMP abgefragte Sensormeter-Ziel (sm/sm-poe/sm-wlan, verwaltet
von `SensormeterManager`) einen Verlaufsgraph anzeigen, analog zum
bereits vorhandenen Graph fuer den internen DHT11-Sensor auf der
Uebersichtsseite.

Feasibility-Check (siehe Recherche-Ergebnis dieses Tages):
- `SensormeterManager` speichert aktuell GAR KEINEN Verlauf - jeder
  SNMP-Poll ueberschreibt den vorherigen Wert direkt im `Target`-Struct.
  Muesste fuer dieses Feature erst einen Ringpuffer pro Ziel (und bei
  PRO-Geraeten pro Sensor) bekommen.
- Dieses Projekt (`sensormeter-display`) nutzt fuer seinen EIGENEN
  internen Graph (`GraphManager`) noch das einfache Modell: 24 Punkte,
  30-Min-Intervall, serverseitig als fertiges `<svg>` gerendert, kein
  Chart.js, keine JSON-API.
- Die drei anderen Familienmitglieder (`sensormeter`, `sensormeter-poe`,
  `sensormeter-wlan`) haben dagegen bereits UNTEREINANDER konsistent das
  bessere Modell: `DataManager` mit 168-Punkte/7-Tage-Ringpuffer (stuendlich),
  echte JSON-`/api/graph`-Route, clientseitiges Rendering per Chart.js
  (`WebServerManager.cpp`, `_server.on("/api/graph", ...)` + `fetch('/api/graph')...new Chart(...)`
  - identischer Code in allen drei Projekten).

**Entscheidung (vorgemerkt, noch nicht umgesetzt):** Wenn das Feature
kommt, NICHT das aktuelle einfache SVG-Modell dieses Projekts fuer die
neuen Pro-Ziel-Graphen kopieren, sondern das bereits an drei Stellen
(sm/sm-poe/sm-wlan) bewaehrte "sm-Modell" uebernehmen (Ringpuffer-Groesse/
-Intervall, JSON-API-Form, Chart.js-Einbindung) - sowohl fuer Konsistenz
in der Familie als auch weil es sich dort bereits als funktionierend und
speicherbudget-vertraeglich erwiesen hat. Ob dabei auch der bestehende
interne DHT11-Graph dieses Projekts (aktuell simples SVG) mit auf dasselbe
Modell umgestellt wird, ist noch offen.

## 2026-07-21 — Vorgemerkt (Ergaenzung): RAM-only Verlauf + Graph-Slide ab Uebersichtsseite auswaehlbar

Zwei weitere Praezisierungen zum Feature aus dem Eintrag "Web-Graphen pro
abgefragtem sm-Ziel" weiter oben (immer noch nur vorgemerkt, nicht
umgesetzt):

**RAM-only statt LittleFS-Persistenz:** Der dort geaeusserte Flash-
Verschleiss-Einwand (10 Ziele × Ganzdatei-Neuschreiben) entfaellt, wenn
der Verlauf NUR im RAM gehalten wird, nicht auf LittleFS persistiert.
Kostenschaetzung mit dem kompakten `GraphManager::Entry`-Format (8 Byte/
Punkt: `time_t` + 2× `int16_t`, kein Bedarf fuer die zusaetzlichen
Sensor2-Druck/Lux/CO2-Felder der `DataManager::HourValue`-Struktur aus
sm/sm-poe/sm-wlan, da per SNMP ohnehin nur Temp+Feuchte ankommen): bei
168 Punkten × bis zu 10 Ziel-Slots (5 Ziele × 2 Sensoren PRO) ~13,4 KB,
bei 24 Punkten (wie der aktuelle interne Graph) nur ~1,9 KB - beides
angesichts 279 KB freiem RAM voellig unkritisch. Tausch: Verlauf geht bei
Neustart verloren, dafuer kein zusaetzlicher Flash-Verschleiss/keine
Ganzdatei-Rewrites mehr - eingeschaetzt als vertretbar (baut sich nach
Neustart einfach neu auf).

**Bedienung am Geraet selbst (nicht nur im Web-Interface):** Von der
`SensormeterOverview`-Slide (Name+Uptime-Tabelle aller Ziele, siehe
frueherer Eintrag "Jeder abgefragte Sensormeter bekommt einen eigenen
Slide") aus soll man ein Ziel antippen/auswaehlen koennen, woraufhin ein
neuer Graph-Slide fuer genau dieses Ziel erscheint (Datenquelle: der
neue RAM-Ringpuffer aus obigem Punkt). Seitenlayout **exakt** wie der
bestehende interne DHT11-Graph-Slide (`GraphManager::drawFullScreen()`,
aktuell fuer `DataSource::Dht11`) - selbe Optik/Aufbau, nur mit den
Werten des gewaehlten Ziels statt des internen Sensors.

Offene Umsetzungsfragen fuer spaeter: wie die Zielauswahl in der
Touch-Bedienung genau verankert wird (analog zum bestehenden Tap-Muster
fuer Zahnrad-/Info-Icon in `main.cpp`, oder direktes Antippen einer Zeile
in der Uebersichtstabelle), und wie man von der Graph-Ansicht wieder
zurueck zur Uebersicht kommt (eigener State ausserhalb der normalen
Slide-Rotation, aehnlich wie die Settings-/Info-UI heute schon als
blockierende Sub-Screens funktionieren).

## 2026-07-21 — Umstellung SNMP v1 -> v2c (Client-Seite)

Nutzerentscheidung nach Machbarkeitspruefung (SNMPv3 zuerst geprueft und
verworfen - Server-Bibliothek `SNMP_Agent@2.1.0` bei sensormeter/-poe/
-wlan hat keinerlei v3/USM-Unterstuetzung, waere Neuentwicklung auf
beiden Seiten gewesen; siehe dortige Eintraege). v2c dagegen einfach:
die Agent-Bibliothek verhandelt die Version bereits pro eingehender
Anfrage automatisch (`SNMPPacket.cpp` liest sie direkt aus dem Paket,
kein fest programmierter Server-Wert) - keine der drei
`SNMPManager.cpp`-Dateien referenziert `SNMP_VERSION` ueberhaupt.

Aenderung ausschliesslich hier auf Client-Seite noetig:
`SnmpClient.cpp`, Message-Version-Feld von `encodeInteger(0, version)`
(v1) auf `encodeInteger(1, version)` (v2c). PDU-Form/Response-Parsing
bleiben unveraendert kompatibel, da GET/GetResponse in v1 und v2c
dieselben Tags nutzen und hier keine v2c-spezifischen Faehigkeiten
(GetBulk, Counter64) gebraucht werden.

**Kein Sicherheitsgewinn** - v2c nutzt weiterhin das reine
Community-String-Modell im Klartext, exakt wie v1. Diese Umstellung ist
rein pragmatisch (leichte technische Verbesserung), nicht
sicherheitsmotiviert.

`docs/lastenheft.txt` Abschnitt 7.3 entsprechend von "SNMP v1" auf
"SNMP v2c" aktualisiert. `pio run` erfolgreich, Speicherbedarf
unveraendert (91.400 B RAM / 1.080.493 B Flash). Noch nicht auf echter
Hardware getestet.

Betrifft auch `sensormeter`/`sensormeter-poe`/`sensormeter-wlan` (dort
keine Code-Aenderung noetig, nur Lastenheft-Anpassung - siehe jeweils
eigener Eintrag).

## 2026-07-31 — Übersicht-Drill-down (Halten), Static-Liste Font 2, Web-Display-Spiegel

Drei Punkte, alle auf realer Hardware (COM16, IP 192.168.77.7) geflasht
und geprueft; `pio run` erfolgreich (27,9 % RAM / 83,2 % Flash).

**1) Drill-down aus der Sensormeter-Uebersicht.** Bisher zeichnete die
Uebersicht (`SensormeterView::drawOverview`) nur eine Zeilenliste; ein Tap
loeste ausschliesslich das linke/rechte prev/next-Blaettern aus - ein
gezieltes "zeige nur dieses sm" war nicht implementiert. Neu:
`SensormeterView::overviewHitTest()` (teilt sich die Zeilen-Geometrie ueber
`overviewRowGeometry()` mit `drawOverview`, damit Tap-Flaeche und gezeichnete
Zeile deckungsgleich sind). In `main.cpp` wird ein Tap auf eine erreichbare
sm-Zeile zum Detail-Slide dieses Ziels und **haelt** dort (Auto-Rotation
gestoppt) bis irgendwo zurueckgetippt wird. Nutzerentscheidung: **nur im
Slide-Modus** (im Static-Modus bewusst inaktiv). Das gehaltene Ziel wird
ueber `heldTarget/heldSensor` stabil nachgefuehrt, falls sich die Slide-Liste
zwischen zwei loop()-Durchlaeufen umsortiert; wird es unerreichbar, loest
sich die Haltung automatisch und es geht zurueck zur Uebersicht.

**2) Static-Auswahlliste (Einstellungen > Static) ueberlappte.** Mit der 7.
Datenquelle war die Zeilenhoehe auf 28px (Box 22px) verkleinert worden, der
Font aber auf 4 (~26px) geblieben - der Text ragte aus den Boxen und die
Labels benachbarter Zeilen ueberlappten optisch. Fix: Listen-Labels in
`drawStaticSourceList()` auf **Font 2 (16px)**, konsistent mit den uebrigen
Listen (`drawSensormeterList`/Ping-Liste). Nur Font geaendert, Geometrie/
Hit-Test unveraendert.

**3) Web-Spiegel des Displays (Nutzeranforderung "B": echtes Nachbild).**
Ein Pixel-Screenshot ist auf diesem PSRAM-losen ESP-WROOM-32 nicht
praktikabel (kein Framebuffer im RAM; ein 320x240x16bit-Sprite = ~150 KB
laesst sich neben WLAN+Async-Webserver nicht zuverlaessig allokieren;
ST7789-Readback ueber MISO ist langsam/unzuverlaessig). Stattdessen
**Daten-Spiegel**: Der Hauptloop fuellt bei jedem Durchlauf einen
`DisplayMirrorState` (aktive Quelle, sm-Ziel/Sensor, held, Alarm, Modus);
neuer JSON-Endpunkt `/api/display` liefert genau den Datenblock der aktiven
Quelle, und `/display` (oeffentlich, vom Dashboard verlinkt) bildet als
320x240-Kachel per ~1s-Polling exakt das aktive Slide nach - inkl.
"gehalten"-Badge, Slide/Static-Anzeige und Alarm-Hintergrund (rot/blau).
