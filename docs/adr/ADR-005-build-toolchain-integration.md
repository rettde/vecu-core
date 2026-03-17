# ADR-005: Build- & Toolchain-Integration (DaVinci -> vECU)

## Status

**Accepted**

## Date

2026-03-17

## Related Decisions

- **[ADR-001](ADR-001-level3-vecu-architecture.md)** -- Level-3 vECU Architecture with 3rd-Party AUTOSAR BaseLayer (Vector MICROSAR / Eclipse OpenBSW)
- **[ADR-002](ADR-002-virtual-mcal-scope-guarantees.md)** -- Virtual-MCAL Scope & Guarantees
- **[ADR-003](ADR-003-vhsm-integration-guarantees.md)** -- vHsm Integration & Guarantees
- **[ADR-004](ADR-004-os-semantics-mapping.md)** -- OS-Semantics Mapping & Guarantees

---

## Context

Fuer Level-3-Virtualisierung ist nicht nur die Zielarchitektur, sondern vor
allem ein reproduzierbarer Build- und Toolchain-Flow entscheidend.

In Serienprojekten wird die ECU-Software typischerweise mit:

- Vector DaVinci Configurator / Developer
- OEM-/Tier-1-spezifischen Toolchains
- Serien-MCAL + BSW

erstellt.

Fuer eine Level-3-vECU gilt:

- Die vECU muss aus denselben Artefakten gebaut werden wie die Serien-ECU,
- mit minimalen, expliziten Anpassungen fuer die Virtualisierung.

Anything else leads to:

- implizite Abweichungen,
- nicht reproduzierbares Verhalten,
- und faktisch wieder Level 2.5.

---

## Decision

### Einfuehrung eines definierten, deterministischen Build- & Toolchain-Flows von DaVinci zu Level-3-vECU, basierend auf 3rd-Party Baselayer + Virtual-MCAL.

Der Flow:

- nutzt Serien-Artefakte als Single Source of Truth,
- vermeidet Tool-Forks,
- ist CI-faehig,
- und trennt Serien-Build klar vom vECU-Build.

---

## Target Toolchain Architecture

```
DaVinci Configurator / Developer
        |
        |  (ARXML, BSW-Config, RTE)
        v
+-------------------------------+
|   Serien-Artefakte (SoT)      |
|  - ARXML                      |
|  - RTE-Code                   |
|  - BSW-Code (Vector/OpenBSW)  |
|  - MCAL-Config                |
+-------------------------------+
        |
        |  (Build Profile Switch)
        v
+-------------------------------+
|   vECU Build Profile          |
|  - Serien-BSW                 |
|  - Serien-RTE                 |
|  - Virtual-MCAL               |
|  - vHsm Adapter               |
|  - OS-Semantics Mapping       |
+-------------------------------+
        |
        v
+-------------------------------+
|   vECU Binary / Library       |
|  - Host-executable            |
|  - vecu-core Runtime          |
+-------------------------------+
```

---

## Build Strategy

### 1. Single Source of Truth (Pflicht)

Die Serien-Artefakte sind die einzige Quelle fuer:

- BSW-Konfiguration
- RTE-Schnittstellen
- Diagnose-Definitionen
- Task- und Timing-Definitionen

**Keine separate "vECU-Konfiguration".**

### 2. Explizites Build-Profil: TARGET vs VECU

Der Unterschied zwischen Real-ECU und vECU wird ausschliesslich ueber:

- Compiler-Defines
- Linker-Selektion
- Modul-Substitution

abgebildet.

Beispiel (konzeptionell):

```c
#if defined(VECU_BUILD)
  #include "VirtualMcal_Can.h"
#else
  #include "Can_30_Rh850.h"
#endif
```

**Kein Fork des Codes, nur ein definierter Build-Switch.**

### 3. Virtual-MCAL als Drop-in-Replacement

- Identische Header
- Identische API
- Link-time-Substitution des MCAL

**Serien-BSW bleibt unangetastet.**

### 4. vHsm Integration im Build

- `Crypto_30_vHsm` bleibt erhalten
- vHsm-Adapter wird nur im vECU-Build gelinkt
- IPC / SHM wird vom Runtime-Layer bereitgestellt

### 5. OS-Semantics Mapping als Runtime-Bindung

- AUTOSAR-OS-Artefakte bleiben erhalten
- Scheduling-Effekte werden zur Laufzeit gemappt
- Keine OS-Re-Generierung notwendig

---

## Guarantees (Build & Toolchain)

### 1. Reproduzierbarkeit

- Identische Inputs -> identisches vECU-Verhalten
- CI-faehig
- deterministisch

---

### 2. Serien-Naehe

- Keine funktionalen Aenderungen am Serien-Code
- BSW, RTE, Diagnose unveraendert
- Abweichungen sind explizit sichtbar (MCAL, HSM, OS-Mapping)

---

### 3. Tool-Kompatibilitaet

- Volle Kompatibilitaet mit:
  - Vector DaVinci (Configurator / Developer)
  - Serien-Build-Umgebungen
- Kein Vendor-Lock-in im Runtime-Layer

---

### 4. Debug- & Trace-Faehigkeit

- Debugging auf BSW- und Applikationsebene moeglich
- deterministische Replays
- reproduzierbare Fehlerbilder

---

## Explicit Non-Goals

Die Build- & Toolchain-Integration macht explizit NICHT:

- Ersetzen von DaVinci
- Eigene ARXML-Generatoren
- GUI-basierte Parallel-Toolchains
- Automatische "Fixups" von fehlerhaften Serien-Konfigurationen
- Abweichende Diagnose- oder Timing-Modelle

> **Diese Punkte sind bewusst ausgeschlossen.**

---

## Consequences

### Positive

- Klare Trennung Serien-ECU <-> vECU
- Hohe Akzeptanz bei OEMs & Tier-1s
- Weniger Diskussionen ueber "Abweichungen"
- CI-Skalierung moeglich

### Trade-offs

- Initialer Integrationsaufwand
- Strikte Disziplin bei Build-Profilen notwendig
- Keine "Abkuerzungen" fuer schlecht konfigurierte Serienprojekte

---

## Rationale (Kurzfassung)

> **Level-3 scheitert nicht an der Runtime,
> sondern an inkonsistenten Toolchains.
> Dieser ADR stellt sicher, dass vECUs
> aus exakt denselben Artefakten entstehen wie Serien-ECUs.**

---

## Final Statement

> **The Level-3 vECU build is derived from the exact same DaVinci-generated artifacts
> as the series ECU.
> Differences are introduced only through explicit build profiles
> and well-defined substitution layers (MCAL, vHsm, OS-Semantics).**
