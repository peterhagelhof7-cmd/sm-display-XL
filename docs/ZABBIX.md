# Zabbix-Integration

Anders als das [Sensormeter](https://github.com/peterhagelhof7-cmd/sensormeter)-Projekt
betreibt das Display **keinen SNMP-Agenten und keinen Webserver** — es ist ein
reiner SNMP-/Ping-**Client** (siehe `docs/lastenheft.txt` Abschnitt 7.3/7.4).
Die auf dem Display angezeigten Werte (DHT11, Sensormeter-Abfrage,
Ping-Ergebnisse) werden nicht über das Netzwerk bereitgestellt — es gibt
dafür nichts, was Zabbix abfragen könnte.

Das mitgelieferte Template beschränkt sich daher bewusst auf **agentenlose
ICMP-Erreichbarkeitsüberwachung** (Zabbix-Simple-Checks, kein Zabbix-Agent
auf dem Display nötig): ist das Display erreichbar, wie hoch ist die
Antwortzeit, gibt es Paketverlust. Das reicht, um einen WLAN-Ausfall oder
einen abgestürzten/eingefrorenen Bootzustand des Geräts zu erkennen.

## Template importieren

1. In Zabbix: **Data collection → Templates → Import**
2. Datei [`zabbix-template-sensormeter-display.yaml`](zabbix-template-sensormeter-display.yaml) auswählen
3. Import bestätigen

## Host anlegen

1. **Data collection → Hosts → Create host**
2. Name vergeben (z. B. "Display Wohnzimmer")
3. Template **"Sensormeter Display (HW-458B)"** zuweisen
4. Agent-Interface hinzufügen, IP-Adresse des Displays eintragen — dient nur
   als Zieladresse für die ICMP-Simple-Checks, es muss dort kein
   Zabbix-Agent laufen

## Mitgelieferte Trigger

Schwellwerte über Host-Makros anpassbar (`{$ICMP_LOSS_MAX_PERCENT}`,
`{$ICMP_RESPONSE_MAX_SEC}`):

- Display nicht erreichbar (ICMP)
- Hoher Paketverlust
- Hohe Antwortzeit

## Falls das Display später einen eigenen Agenten/SNMP bekommt

Sollte künftig entschieden werden, dass das Display selbst überwachbare
Werte bereitstellen soll (z. B. eigener Sensormeter-Systemstatus,
Ping-Ergebnisse der Zusatzziele), müsste dafür erst eine Server-Gegenstelle
in der Firmware ergänzt werden (SNMP-Agent oder kleine REST-API, analog zum
Sensormeter-Projekt) — das ist aktuell nicht Teil des Lastenhefts und daher
nicht umgesetzt.

## Testen ohne Zabbix

```
ping <display-ip>
```
