# ADR-004: OS-Semantics Mapping & Guarantees (Level-3 vECU)

## Status

**Accepted**

## Date

2026-03-17

## Related Decisions

- **[ADR-001](ADR-001-level3-vecu-architecture.md)** -- Level-3 vECU Architecture with 3rd-Party AUTOSAR BaseLayer (Vector MICROSAR / Eclipse OpenBSW)
- **[ADR-002](ADR-002-virtual-mcal-scope-guarantees.md)** -- Virtual-MCAL Scope & Guarantees
- **[ADR-003](ADR-003-vhsm-integration-guarantees.md)** -- vHsm Integration & Guarantees

---

## Context

In der Level-3-vECU-Architektur laeuft der vollstaendige Serien-ECU-Code
(Applikation, RTE, BSW) unveraendert auf dem Host.
Dazu gehoert auch ein AUTOSAR-OS, das im Serien-ECU-Setup:

- Tasks,
- Alarme,
- Counter,
- Events,
- sowie Init- und Shutdown-Sequenzen

steuert und damit massgeblich das Verhalten der BSW-State-Maschinen beeinflusst.

Fuer eine Level-3-Virtualisierung ist keine OS-Emulation erforderlich.
Erforderlich ist jedoch eine semantisch korrekte Abbildung der OS-Effekte, sodass:

- BSW-Module (EcuM, BswM, Com, Dcm, NvM, WdgM, ...)
- und die RTE

identisch reagieren wie im Serien-ECU-Betrieb.

---

## Decision

### Einfuehrung eines OS-Semantics-Mapping-Layers als Teil der Level-3-vECU-Architektur.

Dieses Mapping:

- ersetzt keine AUTOSAR-OS-Implementierung,
- emuliert keine Scheduler- oder Interrupt-Mechanismen,
- bildet jedoch die relevanten OS-Semantiken deterministisch auf die vecu-core Runtime ab.

Der Fokus liegt ausschliesslich auf:

- BSW-Realismus
- reproduzierbarem Verhalten
- Vorintegration

---

## Target Architecture (OS-Semantics Mapping)

```
+------------------------------------------------------+
|                    vecu-core Runtime                 |
|  - deterministische Tick-Engine                      |
|  - Haupt-Event-Loop                                  |
+------------------------------^-----------------------+
                               |
+------------------------------|-----------------------+
|          OS-Semantics Mapping Layer                  |
|  - Init- & Shutdown-Phasen                           |
|  - Task-Aktivierung (zyklisch / ereignisbasiert)     |
|  - Counter & Alarm Mapping                           |
|  - Event-Trigger                                     |
+------------------------------^-----------------------+
                               |
+------------------------------|-----------------------+
|      AUTOSAR OS (3rd Party)                          |
|      (Vector / OpenBSW OS)                           |
|  - Task-Definitionen                                 |
|  - Alarm- & Counter-Definitionen                     |
+------------------------------^-----------------------+
                               |
+------------------------------|-----------------------+
|      BSW / RTE / Application                        |
+------------------------------------------------------+
```

---

## Scope: Abgebildete OS-Semantiken (Level-3)

### 1. ECU-Lifecycle-Phasen (Pflicht)

- Startup-Sequenz
- Init-Phasen (EcuM)
- Run-Phase
- Shutdown / Reset

EcuM- und BswM-State-Maschinen muessen identisch durchlaufen werden.

### 2. Task-Aktivierung

Unterstuetzt werden:

- zyklische Tasks (z. B. 1 ms / 10 ms / 100 ms)
- ereignisgetriggerte Tasks (funktional)

Abbildung:

- Tasks werden deterministisch im vecu-core Tick-Loop aufgerufen
- Reihenfolge ist stabil und reproduzierbar

### 3. Counter & Alarm-Semantik

- AUTOSAR Counter werden auf virtuelle Zeit abgebildet
- Alarme triggern:
  - Task-Aktivierungen
  - Callback-Funktionen

Keine echte Zeitmessung, sondern Sim-Zeit-getrieben.

### 4. Event-Semantik

- OS-Events werden funktional unterstuetzt
- Mapping auf:
  - Flags
  - deterministische Trigger im Tick-Loop

---

## Guarantees (Was das OS-Semantics Mapping garantiert)

### 1. BSW-Verhaltenskompatibilitaet

- BSW-State-Maschinen reagieren identisch
- identische Timeout- und Retry-Logik
- identische Diagnose-Flows

**Das BSW darf nicht erkennen, dass es nicht auf echter Hardware laeuft.**

---

### 2. Determinismus & Reproduzierbarkeit

- Vollstaendig deterministisches Scheduling
- Identisches Verhalten ueber Laeufe hinweg
- CI-tauglich

---

### 3. Reihenfolge-Garantie

- Stabile Task-Aufrufreihenfolge
- Keine nicht-deterministischen Race-Conditions

---

### 4. Abstraktion von Parallelitaet

- Parallele Tasks werden funktional serialisiert
- Core-Affinitaeten bleiben logisch erhalten

Fokus liegt auf funktionaler Korrektheit, nicht auf Parallelitaets-Validierung.

---

## Explicit Non-Goals

Das OS-Semantics Mapping macht explizit NICHT:

- AUTOSAR-OS-Emulation
- Preemptive Scheduling
- Interrupt-Simulation
- Cycle-Accuracy
- Thread-per-Task-Modelle
- Level-4-Timing-Validierung

> **Diese Punkte sind bewusst ausgeschlossen.**

---

## Multi-Core Handling

- Multi-Core wird **nicht emuliert**
- Unterstuetzung:
  - Single-Core-Fallback
  - Core-ID-bewusste Logikpfade
- Keine echte Parallelitaet

---

## Interaction with Virtual-MCAL & vHsm

- OS-Semantics Mapping ist orthogonal zu:
  - Virtual-MCAL (ADR-002)
  - vHsm-Integration (ADR-003)
- Klare Trennung:
  - **OS-Semantik** -- Ablauf
  - **MCAL** -- Peripherie-Abstraktion
  - **vHsm** -- Security-Services

---

## Consequences

### Positive

- Realistisches BSW-Verhalten
- Kein OS-Porting noetig
- Hohe Skalierbarkeit
- Saubere CI-Integration
- Klare Abgrenzung zu Level 4

### Trade-offs

- Kein echtes Timing
- Kein Parallelitaets-Stress-Test
- Kein Ersatz fuer HiL / vHiL

---

## Rationale (Kurzfassung)

> **Fuer Level 3 ist entscheidend, dass sich das BSW korrekt verhaelt --
> nicht, dass ein echtes Betriebssystem laeuft.
> OS-Semantics Mapping liefert genau diese Abstraktion.**

---

## Final Statement

> **OS-Semantics Mapping guarantees functional and behavioral compatibility
> for BSW and RTE execution in Level-3 vECUs,
> while explicitly excluding OS emulation and timing accuracy.**
