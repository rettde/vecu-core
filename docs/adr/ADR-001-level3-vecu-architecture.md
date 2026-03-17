# ADR-001: Level-3 vECU Architecture with 3rd-Party AUTOSAR BaseLayer

**(Vector MICROSAR / Eclipse OpenBSW)**

| Field  | Value      |
|--------|------------|
| Status | Accepted   |
| Date   | 2026-03-17 |

---

## Decision Drivers

- Ziel: Level-3 ECU-nahe Virtualisierung (BSW-zentriert)
- Maximale Wiederverwendung von Serien-ECU-Code
- Realistische BSW-Vorintegration vor Hardware
- Skalierbarkeit und CI/CD-Fähigkeit
- Offenheit für proprietäre und Open-Source-Baselayer
- Klare Trennung von Verantwortlichkeiten (Runtime vs. AUTOSAR)

---

## Context

Das Projekt verfolgt das Ziel, virtuelle ECUs auf Level 3 bereitzustellen.
Level 3 bedeutet hier konkret:

- Der vollständige Serien-ECU-Code (Applikation, RTE, BSW) läuft unverändert auf dem Host.
- Die einzige virtualisierte Schicht ist MCAL.

Frühere Ansätze basierten teilweise auf einem Minimal-Stub-Baselayer, der
lediglich einen reduzierten AUTOSAR-API-Umfang bereitstellt.
Diese Variante ist nicht geeignet, um:

- originales BSW realistisch zu testen,
- projektspezifische Vector-APIs (z. B. Signal Groups, Trigger APIs) zu unterstützen,
- oder vollständige State-Maschinen und Timing-Semantik abzubilden.

Gleichzeitig ist vecu-core bewusst kein AUTOSAR-Stack, sondern ein
Runtime- und Orchestrierungs-Framework.

---

## Decision

**Die Zielarchitektur wird ausschließlich auf einem produktiven 3rd-Party
AUTOSAR BaseLayer aufgebaut:**

- Vector MICROSAR Classic
- Eclipse OpenBSW (Open-Source AUTOSAR Classic BaseLayer)

**Die Minimal-Stub-Baselayer-Variante wird nicht weiter ausgebaut und ist
kein Zielpfad für Level 3.**

Der Stub-Baselayer dient ausschließlich:

- als frühes Enablement,
- für Experimente,
- oder für stark reduzierte PoCs.

Er ist kein Produktiv- oder Ausbaupfad.

---

## Architecture (Target State)

### Logische Zielarchitektur

```
+------------------------------------------------------+
|                    vecu-core Runtime                 |
|  - deterministische Tick-Engine                      |
|  - Prozess- & Lifecycle-Steuerung                    |
|  - Frame-Transport (CAN / FR / ETH via SIL Kit)      |
|  - Shared Memory, Zeit, Logging                      |
|                                                      |
|  ! Keine AUTOSAR-Logik                               |
+------------------------------^-----------------------+
                               |
+------------------------------|--------- -------------+
|            Virtual-MCAL Layer (neu)                  |
|  - vollstaendige MCAL-API-Kompatibilitaet            |
|  - Host-kompatible Implementierungen                 |
|  - BSW-semantisch korrekt                            |
|  - konfigurierbar je ECU / Projekt                   |
+------------------------------^-----------------------+
                               |
+------------------------------|-----------------------+
|      3rd-Party AUTOSAR BaseLayer                     |
|      (Vector MICROSAR / Eclipse OpenBSW)             |
|                                                      |
|  - vollstaendige BSW-Module                          |
|    Com, PduR, CanIf, FrIf, EthIf                     |
|    Dcm, Dem, NvM, WdgM, EcuM, Os                     |
|                                                      |
|  - originale State-Machines                          |
|  - originale Timing- & Error-Semantik                |
+------------------------------^-----------------------+
                               |
+------------------------------|-----------------------+
|                 Original RTE                         |
|        (Vector RTE / OpenBSW RTE)                    |
+------------------------------^-----------------------+
                               |
+------------------------------|-----------------------+
|            ECU Applikation (SWCs)                    |
+------------------------------------------------------+
```

---

## Consequences

### Positive

- Echte Level-3-Virtualisierung
- Unveraenderte Serien-Artefakte (SWCs, RTE, BSW)
- Realistische BSW-State-Maschinen und Diagnose-Flows
- Wahlfreiheit:
  - Vector MICROSAR (OEM-/Serien-Setup)
  - Eclipse OpenBSW (Open-Source / Forschungs- & Plattform-Setups)
- vecu-core bleibt schlank, wartbar und AUTOSAR-agnostisch

### Negative / Trade-offs

- Hoeherer Initialaufwand (Integration 3rd-Party Baselayer)
- Notwendigkeit einer dedizierten Virtual-MCAL-Schicht
- Multi-Core wird funktional reduziert (Single-Core-Fallback)

---

## Explicit Non-Goals

- Ausbau oder Produktivnutzung des vecu-core Stub-Baselayers
- Re-Implementierung von BSW-Modulen
- Eigenes RTE oder "AUTOSAR-Light"
- Level-4-Emulation (HEX, CPU-Emulation, Cycle Accuracy)
- DSL-basierte oder generative MCAL-Ansaetze

---

## Open Items / Gaps to Target Architecture

To reach the target state, the following items are missing today:

### 1. Integration Path for 3rd-Party BaseLayer

- Build- und Link-Pfad fuer Vector MICROSAR und Eclipse OpenBSW
- Saubere ABI-Grenze zwischen BSW und Virtual-MCAL

### 2. Explicit Virtual-MCAL Layer

- Can / Fr / Eth / Dio / Gpt / Fls / Mcu
- Semantik-kompatibel, nicht HW-emuliert

### 3. MCAL Configuration Synchronization

- Ableitung aus Serien-MCAL-Konfiguration
- Klare Regeln: uebernehmen / ignorieren / virtualisieren

### 4. vHsm Integration

- `Crypto_30_vHsm`-kompatibler Adapter
- oder Open-Source-faehige HSM-Abstraktion

### 5. Minimal OS-Semantics Mapping

- Init-Phasen
- zyklische Tasks
- Counter / Alarm-Abbildung auf Tick

---

## Rationale (Kurzfassung)

> Level 3 wird ausschließlich durch Integration eines produktionsreifen
> 3rd-Party AUTOSAR BaseLayers erreicht -- proprietaer oder Open Source.
> Der Stub-Baselayer ist kein Ausbaupfad.
> Der einzige Virtualisierungspunkt ist MCAL.

---

## Final Statement

> Level-3 vECUs in this project are built exclusively with a 3rd-party
> AUTOSAR BaseLayer (Vector MICROSAR or Eclipse OpenBSW).
> The minimal stub base layer is not a future path and will not be extended.
