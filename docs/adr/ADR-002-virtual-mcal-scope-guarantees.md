# ADR-002: Virtual-MCAL Scope & Guarantees (Level-3 vECU)

## Status

**Accepted**

## Date

2026-03-17

## Related Decisions

- **[ADR-001](ADR-001-level3-vecu-architecture.md)** -- Level-3 vECU Architecture with 3rd-Party AUTOSAR BaseLayer (Vector MICROSAR / Eclipse OpenBSW)

---

## Context

Im Rahmen der Level-3-vECU-Architektur (siehe ADR-001) wird der **vollstaendige Serien-ECU-Code**
(Applikation, RTE, BSW) **unveraendert** auf dem Host ausgefuehrt.

Die **einzige virtualisierte Schicht** ist die **MCAL (Microcontroller Abstraction Layer)**.

Diese Entscheidung definiert:

- den **Scope** des Virtual-MCAL,
- die **funktionalen Garantien** gegenueber dem darueberliegenden BSW,
- sowie die **expliziten Nicht-Ziele**, um Abgrenzung zu Level 4 sicherzustellen.

---

## Decision

### Einfuehrung eines dedizierten Virtual-MCAL-Layers als einzige Virtualisierungsschicht unterhalb des 3rd-Party Baselayers.

Der Virtual-MCAL:

- ersetzt **ausschliesslich** die Hardware-gebundenen MCAL-Module,
- stellt **API- und Semantik-Kompatibilitaet** sicher,
- ist **host-kompatibel**, **deterministisch** und **konfigurierbar**,
- emuliert **keine Hardware**, sondern **BSW-relevantes Verhalten**.

---

## Virtual-MCAL Scope

### 1. Abgedeckte MCAL-Module (Mindest-Scope fuer Level 3)

Der Virtual-MCAL MUSS folgende Module bereitstellen:

#### Kommunikation

- `Can_30_*`
- `Fr_*`
- `Eth_*` (funktional reduziert, Frame-basiert)

#### IO & Peripherie

- `Dio_*` (RAM-backed)
- `Port_*` (Init-Semantik)
- `Spi_*` (No-Op / Loopback / Stub-Device)

#### Zeit & System

- `Gpt_*` (Sim-Zeit-basiert)
- `Mcu_*` (Clock / Reset / Init-Semantik)

#### Speicher

- `Fls_*` (Shared-Memory-backed)

> **Hinweis:**
> Der Scope ist **erweiterbar**, aber diese Module sind **nicht optional** fuer Level 3.

---

## Guarantees (Was der Virtual-MCAL garantiert)

### 1. API-Kompatibilitaet

- Identische Funktionssignaturen
- Identische Rueckgabewerte
- Identische Error-Codes

Der darueberliegende **Vector MICROSAR / OpenBSW** darf **nicht angepasst** werden muessen.

---

### 2. BSW-Semantik-Kompatibilitaet

Der Virtual-MCAL garantiert:

- korrekte Zustandsuebergaenge
- erwartetes Timeout-Verhalten
- konsistente Error-Propagation
- reproduzierbares Verhalten ueber Laeufe hinweg

**BSW-State-Maschinen muessen identisch reagieren wie auf dem Real-Target.**

---

### 3. Determinismus

- Vollstaendig deterministisch
- Zeitmodell basiert auf vecu-core Tick-Engine
- Kontrollierbarer Jitter (optional, explizit)

---

### 4. Konfigurierbarkeit

- Ableitung aus Serien-MCAL-Konfiguration
- Uebernahme:
  - IDs
  - Channels
  - Controller-Zuordnung
- Klare Regeln:
  - was ignoriert wird
  - was virtualisiert wird

---

### 5. Runtime-Unabhaengigkeit

- Keine direkte Abhaengigkeit zu vecu-core internals
- Saubere Schnittstelle:

```
BSW <-> Virtual-MCAL <-> Runtime
```

---

## Explicit Non-Goals (sehr wichtig)

Der Virtual-MCAL **macht explizit NICHT**:

- Hardware-Emulation
- Register-Simulation
- Interrupt-Genauigkeit
- Cycle-Accuracy
- Ausfuehrung von originalem MCAL-Code
- Level-4-Funktionalitaet (HEX, QEMU, Renode)

> **Diese Punkte sind bewusst ausgeschlossen.**

---

## Multi-Core Semantics

- Multi-Core wird **nicht emuliert**
- Virtual-MCAL unterstuetzt:
  - **Single-Core-Fallback**
  - Core-affine Logikpfade
- Parallelitaet wird **funktional serialisiert**

Fokus bleibt auf **BSW-Vorintegration**, nicht Timing-Validierung.

---

## vHsm Interaction (Abgrenzung)

- Virtual-MCAL **enthaelt kein HSM**
- HSM-Interaktion erfolgt ueber:
  - `Crypto_30_vHsm`-kompatiblen Adapter
  - IPC / Shared Memory
- Ziel: korrekte Diagnose- und Security-Flows

---

## Consequences

### Positive

- Klarer Verantwortungsschnitt
- Reproduzierbare Tests
- Hohe Skalierbarkeit
- Kompatibel mit Vector MICROSAR und Eclipse OpenBSW
- Saubere Abgrenzung zu Level 4

### Trade-offs

- Kein HW-Timing
- Kein echtes Parallelverhalten
- Kein Ersatz fuer HiL / vHiL

---

## Rationale (Kurzfassung)

> **Level-3-Virtualisierung lebt von BSW-Realismus,
> nicht von Hardware-Genauigkeit.
> Der Virtual-MCAL existiert ausschliesslich,
> um dem BSW eine glaubwuerdige Umwelt zu liefern.**

---

## Final Statement

> **The Virtual-MCAL is the only virtualization layer in the Level-3 vECU.
> It guarantees API and behavioral compatibility for the BSW,
> but explicitly avoids hardware and timing emulation.**
