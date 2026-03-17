# ADR-006: Validation & Acceptance Criteria (Level-3 vECU)

## Status

**Accepted**

## Date

2026-03-17

## Related Decisions

- **[ADR-001](ADR-001-level3-vecu-architecture.md)** -- Level-3 vECU Architecture with 3rd-Party AUTOSAR BaseLayer (Vector MICROSAR / Eclipse OpenBSW)
- **[ADR-002](ADR-002-virtual-mcal-scope-guarantees.md)** -- Virtual-MCAL Scope & Guarantees
- **[ADR-003](ADR-003-vhsm-integration-guarantees.md)** -- vHsm Integration & Guarantees
- **[ADR-004](ADR-004-os-semantics-mapping.md)** -- OS-Semantics Mapping & Guarantees
- **[ADR-005](ADR-005-build-toolchain-integration.md)** -- Build- & Toolchain-Integration (DaVinci -> vECU)

---

## Context

Level-3-Virtualisierung ist kein Selbstzweck.
Sie ist nur dann wertvoll, wenn sie nachweisbar:

- das Serien-BSW-Verhalten abbildet,
- fruehzeitig Integrationsfehler sichtbar macht,
- und reproduzierbare Ergebnisse liefert.

Diese ADR definiert objektive Validierungs- und Akzeptanzkriterien, mit denen
eindeutig entschieden werden kann:

**Ist eine vECU wirklich Level 3 -- oder nur Level 2.5?**

---

## Decision

### Einfuehrung eines formalen, mehrstufigen Validation- & Acceptance-Frameworks fuer Level-3 vECUs.

Eine vECU gilt nur dann als akzeptiert, wenn alle Pflichtkriterien dieser ADR
erfuellt sind.

---

## Acceptance Criteria (Pflichtkriterien)

### 1. Code-Artefakt-Identitaet (Hard Gate)

**Pflicht:**

- Applikation, RTE und BSW stammen identisch aus dem Serien-Build
- Keine Code-Forks
- Keine manuelle Anpassung fuer vECU-Zwecke

**Nicht akzeptiert:**

- dedizierte "vECU-BSW-Versionen"
- funktionale Anpassungen im Serien-Code

---

### 2. BSW-Verhaltensgleichheit (Hard Gate)

Die vECU MUSS:

- identische BSW-State-Maschinen durchlaufen
- identische Init-, Run-, Shutdown-Sequenzen zeigen
- identische Timeout-, Retry- und Error-Pfad-Logik ausfuehren

**Nachweis:**

- Vergleichbare Logs
- identische Zustandsuebergaenge
- identische Diagnose-Responses

---

### 3. Diagnose-Aequivalenz (Hard Gate)

Mindestens folgende UDS-Flows muessen identisch funktionieren:

- Session Control
- ReadDataByIdentifier
- WriteDataByIdentifier
- Routine Control
- DTC Read / Clear
- SecurityAccess (Seed & Key)

**Nachweis:**

- identische Responses gegenueber:
  - CANoe
  - OpenSOVD
  - anderen UDS-Testern

---

### 4. Kommunikations-Aequivalenz (Hard Gate)

Die vECU MUSS:

- identische PDUs senden / empfangen
- identische Signal- und Signal-Group-Semantik zeigen
- identische Update-Bit- und Trigger-Logik ausfuehren

**Nachweis:**

- Vergleich von Bus-Frames
- identisches Verhalten in Cluster-Setups

---

### 5. vHsm-Verhaltensgleichheit (Hard Gate)

**Pflicht:**

- Security-relevante Diagnose-Flows funktionieren vollstaendig
- Job-Handling identisch
- Fehler-Propagation identisch

**Nicht gefordert:**

- echte Hardware-Security
- Zertifizierungs-Nachweise

---

### 6. OS-Semantics-Aequivalenz (Hard Gate)

**Pflicht:**

- korrekte Init- und Task-Aktivierungs-Reihenfolge
- korrekte Alarm- und Counter-Semantik
- deterministische Ausfuehrung

**Nicht gefordert:**

- Preemptive Scheduling
- Cycle-Accuracy

---

### 7. Determinismus & Reproduzierbarkeit (Hard Gate)

**Pflicht:**

- identische Ergebnisse bei wiederholten Laeufen
- identische Logs bei identischem Input
- CI-faehig

---

## Optional Acceptance Criteria (Nice-to-Have)

Diese Kriterien sind nicht verpflichtend, erhoehen aber den Reifegrad:

- parallele Ausfuehrung mehrerer vECUs (Cluster)
- Fault-Injection (MCAL / Kommunikation)
- Jitter-Injection (kontrolliert)
- Replay-Faehigkeit von Testlaeufen

---

## Validation Strategy

### 1. Referenz-Vergleich

- Vergleich vECU <-> Serien-ECU
- Vergleich vECU <-> VTT (falls vorhanden)

### 2. Automatisierte Tests (Pflicht)

- Smoke-Tests in CI
- Diagnose-Regression
- Kommunikations-Regression

### 3. Tool-Unabhaengigkeit

Die Akzeptanz darf nicht von einem spezifischen Tool abhaengen
(z. B. CANoe-Only-Tests sind nicht ausreichend).

---

## Explicit Non-Acceptance Criteria

Eine vECU wird **nicht** akzeptiert, wenn:

- Serien-BSW angepasst werden musste
- Stub-Baselayer aktiv ist
- MCAL nicht explizit virtualisiert ist
- Verhalten nur "aehnlich", aber nicht reproduzierbar ist
- Ergebnisse nicht deterministisch sind

---

## Consequences

### Positive

- Objektive Entscheidungsgrundlage
- Weniger Architektur-Diskussionen
- Klare Abgrenzung zu Level 2/2.5
- Hohe Glaubwuerdigkeit gegenueber OEMs & Partnern

### Trade-offs

- Hoeherer Initialaufwand
- Strikte Disziplin bei Toolchain & Tests notwendig

---

## Rationale (Kurzfassung)

> **Level-3 ist keine Marketing-Kategorie.
> Level-3 ist eine ueberpruefbare Eigenschaft.
> Diese ADR definiert genau, wann sie erreicht ist.**

---

## Final Statement

> **A Level-3 vECU is accepted only if it demonstrates
> reproducible, deterministic and behavior-equivalent execution
> of the full series ECU software stack.**
