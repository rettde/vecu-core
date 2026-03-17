# ADR-003: vHsm Integration & Guarantees (Level-3 vECU)

## Status

**Accepted**

## Date

2026-03-17

## Related Decisions

- **[ADR-001](ADR-001-level3-vecu-architecture.md)** -- Level-3 vECU Architecture with 3rd-Party AUTOSAR BaseLayer (Vector MICROSAR / Eclipse OpenBSW)
- **[ADR-002](ADR-002-virtual-mcal-scope-guarantees.md)** -- Virtual-MCAL Scope & Guarantees

---

## Context

In der Level-3-vECU-Architektur laeuft der vollstaendige Serien-ECU-Code
(Applikation, RTE, BSW) unveraendert auf dem Host.
Dies schliesst sicherheits- und kryptorelevante Funktionen ein, die im
Serien-ECU-Setup ueber ein Hardware Security Module (HSM) bereitgestellt werden.

Typische Serien-Setups nutzen:

- Vector vHsm
- `Crypto_30_vHsm`
- IPC-basierte Kommunikation zwischen Applikation/BSW und dediziertem HSM-Core

Fuer eine Level-3-Virtualisierung muss diese Architektur funktional und
semantisch korrekt abgebildet werden, ohne:

- echte Hardware,
- echte Schluesselmaterialien,
- oder HW-Timing-Abhaengigkeiten.

---

## Decision

### Einfuehrung einer vHsm-Integrationsschicht als Teil der Level-3-vECU-Architektur.

Die vHsm-Integration:

- stellt Serien-kompatible HSM-Semantik bereit,
- ist API- und verhaltenskompatibel zu `Crypto_30_vHsm`,
- basiert auf IPC / Shared-Memory-Kommunikation,
- ist deterministisch, skalierbar und host-faehig.

Die vHsm-Integration ist kein Hardware-Ersatz, sondern eine funktionale
Abbildung fuer BSW- und Diagnose-Flows.

---

## Target Architecture (vHsm-Integration)

```
+------------------------------------------------------+
|                    vecu-core Runtime                 |
|  - Prozess- & Lifecycle-Steuerung                    |
|  - Shared Memory / IPC                               |
+------------------------------^-----------------------+
                               |
+------------------------------|-----------------------+
|        vHsm Adapter / Proxy Layer                    |
|  - Crypto_30_vHsm API-kompatibel                     |
|  - Job-Dispatch                                      |
|  - Error- & Status-Propagation                       |
+------------------------------^-----------------------+
                               |
+------------------------------|-----------------------+
|              vHsm Service (Host)                     |
|  - Kryptografische Operationen                       |
|  - deterministisches Verhalten                       |
|  - kein echtes Schluesselmaterial                     |
+------------------------------^-----------------------+
                               |
+------------------------------|-----------------------+
|      3rd-Party AUTOSAR BaseLayer                     |
|      (Vector MICROSAR / Eclipse OpenBSW)             |
|  - Csm / CryIf / Crypto Stack                        |
+------------------------------^-----------------------+
                               |
+------------------------------|-----------------------+
|                 RTE / Application                    |
+------------------------------------------------------+
```

---

## vHsm Scope (Level-3)

### Abgedeckte Funktionen (Mindest-Scope)

Die vHsm-Integration MUSS unterstuetzen:

#### Kryptografische Primitive

- AES (Encrypt / Decrypt)
- Hash (z. B. SHA-2)
- MAC / CMAC

#### Key-basierte Operationen

- Key-Handles / Key-IDs
- deterministische Dummy-Keys

#### Diagnose-relevante Security-Flows

- Seed & Key
- Secure Diagnostics
- Authentifizierungs-Flows

#### Job-basierte Verarbeitung

- synchron / asynchron (aus Sicht des BSW)

---

## Guarantees (Was vHsm garantiert)

### 1. API-Kompatibilitaet

- Volle Kompatibilitaet zu:
  - `Crypto_30_vHsm`
  - `Csm_*`
  - `CryIf_*`
- Keine Anpassung des Serien-BSW erforderlich

---

### 2. BSW-Semantik-Kompatibilitaet

Die vHsm-Integration garantiert:

- korrektes Job-Handling
- korrekte Rueckgabecodes
- konsistentes Fehlerverhalten
- identische Reaktionen der BSW-State-Maschinen

**Diagnose- und Security-Flows muessen identisch reagieren wie auf dem Real-Target.**

---

### 3. Determinismus & Reproduzierbarkeit

- deterministisches Verhalten
- reproduzierbare Testergebnisse
- kontrollierbare Latenzen (keine HW-Abhaengigkeit)

---

### 4. Isolierung

- vHsm laeuft logisch getrennt von Applikation
- Kommunikation ausschliesslich ueber:
  - IPC / Shared Memory
- Kein direkter Zugriff auf Runtime-Interna

---

### 5. Sicherheitsabgrenzung

- Keine echten Serien-Keys
- Keine sicherheitskritischen Assets
- Verwendung von:
  - Dummy-Keys
  - Test-Keys
  - konfigurierbaren Platzhaltern

**Die vECU ist kein Sicherheitsprodukt, sondern ein Vorintegrations-Artefakt.**

---

## Explicit Non-Goals

Die vHsm-Integration macht explizit NICHT:

- Hardware-basierte Security
- Schutz gegen physische Angriffe
- Secure Boot
- Secure Storage
- Zertifizierbare Security (ISO 21434 / FIPS / CC)
- Level-4-HSM-Emulation

> **Diese Punkte sind bewusst ausgeschlossen.**

---

## Interaction with Virtual-MCAL

- vHsm ist **kein** Teil des Virtual-MCAL
- Klare Trennung:
  - **MCAL** -- Hardware-nahe Peripherie
  - **vHsm** -- Security-Services
- Beide werden parallel, aber unabhaengig virtualisiert

---

## Consequences

### Positive

- Realistische Security- und Diagnose-Tests
- Keine Anpassung des Serien-BSW
- Hohe Skalierbarkeit in CI/CD
- Klare Abgrenzung zu Sicherheits-Zertifizierung

### Trade-offs

- Keine echte Security-Guarantee
- Kein Ersatz fuer HW-basierte Security-Tests
- Fokus klar auf BSW-Vorintegration

---

## Rationale (Kurzfassung)

> **Fuer Level 3 ist nicht entscheidend, wie sicher ein HSM ist,
> sondern dass sich das BSW so verhaelt, als waere eines vorhanden.
> Genau das garantiert die vHsm-Integration -- nicht mehr und nicht weniger.**

---

## Final Statement

> **The vHsm integration provides functional and behavioral compatibility
> for security-relevant BSW and diagnostic flows in Level-3 vECUs,
> while explicitly excluding hardware-based security guarantees.**
