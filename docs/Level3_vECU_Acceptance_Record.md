# Windsurf -- Level-3 vECU Abnahmevorlage

| Feld          | Wert                                              |
|---------------|---------------------------------------------------|
| Dokumenttyp   | Formale Abnahmevorlage / Acceptance Record         |
| Gueltig fuer  | Level-3 vECUs gemaess ADR-001 ... ADR-006          |
| Version       | v1.0                                               |
| Datum         | 2026-03-17                                         |

---

## 1. Ziel der Abnahme

Diese Abnahme prueft, ob eine vECU:

- tatsaechlich Level-3-faehig ist
- die Zielarchitektur gemaess Windsurf einhaelt
- als Digital Twin EE fuer Vorintegration freigegeben werden kann

**Wichtig:**
Diese Abnahme entscheidet nicht, ob die vECU "gut genug fuer Tests" ist,
sondern ob sie architektonisch und technisch Level 3 ist.

---

## 2. Abnahme-Scope

Die Abnahme umfasst:

- Architektur
- Build & Toolchain
- Virtual-MCAL
- vHsm-Integration
- OS-Semantik
- Verhalten & Reproduzierbarkeit

**Nicht Bestandteil der Abnahme:**

- Performance-Optimierung
- Safety- oder Security-Zertifizierung
- Hardware-nahe Validierung (Level 4)

---

## 3. Architektur-Konformitaet (ADR-001)

### 3.1 Baselayer

| Kriterium | Status | Nachweis |
|-----------|--------|----------|
| Ausschliesslich 3rd-Party Baselayer (Vector MICROSAR oder Eclipse OpenBSW) | ☐ Pass ☐ Fail | Build-Artefakte |
| Kein Einsatz / Ausbau des Stub-Baselayers | ☐ Pass ☐ Fail | Repo-Analyse |
| Serien-BSW unveraendert | ☐ Pass ☐ Fail | Diff / Hash |

**Fail, wenn:**

- Stub-Baselayer aktiv ist
- BSW fuer vECU angepasst wurde

---

## 4. Build- & Toolchain-Konformitaet (ADR-005)

### 4.1 Single Source of Truth

| Kriterium | Status | Nachweis |
|-----------|--------|----------|
| DaVinci-Artefakte als einzige Konfigurationsquelle | ☐ Pass ☐ Fail | ARXML |
| Kein separater vECU-Konfigurationspfad | ☐ Pass ☐ Fail | Build-Flow |
| Expliziter Build-Switch (TARGET / VECU) | ☐ Pass ☐ Fail | Compiler-Flags |

### 4.2 Reproduzierbarkeit

| Kriterium | Status | Nachweis |
|-----------|--------|----------|
| Reproduzierbarer vECU-Build | ☐ Pass ☐ Fail | CI-Logs |
| Identischer Input -> identisches Verhalten | ☐ Pass ☐ Fail | Test-Runs |

---

## 5. Virtual-MCAL Abnahme (ADR-002)

### 5.1 Scope-Vollstaendigkeit

| MCAL-Modul | Implementiert | Semantisch korrekt |
|------------|---------------|-------------------|
| Can        | ☐             | ☐                 |
| Fr         | ☐             | ☐                 |
| Eth        | ☐             | ☐                 |
| Dio        | ☐             | ☐                 |
| Gpt        | ☐             | ☐                 |
| Mcu        | ☐             | ☐                 |
| Fls        | ☐             | ☐                 |

**Fail, wenn:**

- MCAL implizit "wegabstrahiert" ist
- API-Inkompatibilitaeten existieren

### 5.2 Guarantees

| Garantie | Status | Nachweis |
|----------|--------|----------|
| API-Kompatibilitaet | ☐ Pass ☐ Fail | Header-Vergleich |
| BSW-Semantik korrekt | ☐ Pass ☐ Fail | Log-Vergleich |
| Deterministisch | ☐ Pass ☐ Fail | Wiederholtests |

---

## 6. vHsm-Integration (ADR-003)

### 6.1 Funktionale Abnahme

| Kriterium | Status | Nachweis |
|-----------|--------|----------|
| Crypto_30_vHsm API kompatibel | ☐ Pass ☐ Fail | Build |
| SecurityAccess (Seed/Key) | ☐ Pass ☐ Fail | UDS-Test |
| Job-Handling korrekt | ☐ Pass ☐ Fail | Logs |

**Fail, wenn:**

- Security-Flows unvollstaendig
- BSW-Anpassungen noetig waren

### 6.2 Abgrenzung

| Kriterium | Status |
|-----------|--------|
| Keine echten Serien-Keys | ☐ Pass ☐ Fail |
| Keine HW-Security-Claims | ☐ Pass ☐ Fail |

---

## 7. OS-Semantics Abnahme (ADR-004)

### 7.1 Ablauf-Semantik

| Kriterium | Status | Nachweis |
|-----------|--------|----------|
| Init-Sequenzen korrekt | ☐ Pass ☐ Fail | Logs |
| Task-Reihenfolge stabil | ☐ Pass ☐ Fail | Trace |
| Alarm / Counter korrekt | ☐ Pass ☐ Fail | Tests |

**Fail, wenn:**

- BSW-State-Maschinen anders reagieren als auf Serien-ECU

---

## 8. Verhalten & Integration (ADR-006)

### 8.1 Diagnose-Aequivalenz (Pflicht)

| Service | Status |
|---------|--------|
| Session Control | ☐ |
| ReadDataByIdentifier | ☐ |
| WriteDataByIdentifier | ☐ |
| Routine Control | ☐ |
| DTC Read / Clear | ☐ |
| SecurityAccess | ☐ |

### 8.2 Kommunikation

| Kriterium | Status |
|-----------|--------|
| PDU-Verhalten identisch | ☐ |
| Signal-Groups korrekt | ☐ |
| Trigger-Logik korrekt | ☐ |

### 8.3 Determinismus

| Kriterium | Status |
|-----------|--------|
| Wiederholbare Ergebnisse | ☐ |
| CI-faehig | ☐ |

---

## 9. Gesamtentscheidung

### Abnahmeentscheidung

☐ **ACCEPTED** -- Level-3 vECU freigegeben

☐ **REJECTED** -- Kriterien nicht erfuellt

### Begruendung (Pflicht bei Reject)

```
[Freitext]
```

---

## 10. Freigabe

| Rolle | Name | Datum | Unterschrift |
|-------|------|-------|--------------|
| Architektur | | | |
| Entwicklung | | | |
| Test / Validation | | | |
| Windsurf | | | |

---

## 11. Klarer Windsurf-Merksatz

> **Level-3 ist erreicht, wenn niemand mehr argumentieren muss,
> ob es Level-3 ist.
> Diese Abnahme macht es eindeutig.**
