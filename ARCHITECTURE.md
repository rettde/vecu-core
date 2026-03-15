# vECU Architecture

This document defines the architecture of the vECU Execution System.
It is normative and implementation‑binding.

## Overview

The system executes virtual ECUs in a deterministic, tool‑independent and
cross‑platform way. All ECU logic is implemented in user‑defined modules.
The system is compatible with OpenSUT semantics but has no dependency
on proprietary tooling.

---

## Architecture Decisions

### ADR‑001: Cross‑Platform vECU Loader für eigenentwickelte APPL/HSM Module

**(OpenSUT‑kompatibel, ohne VTT‑Abhängigkeit)**

**Status:** Accepted

#### Kontext

Das vECU Runtime Environment existiert bereits und stellt bereit:

- Ausführungsumgebung für virtuelle ECUs
- deterministische Simulation (Tick‑basiert)
- Shared‑Memory‑basierte Interaktion
- OpenSUT‑kompatible Semantik für vECU‑Zugriff
- optionale Anbindung an verteilte Umgebungen (z. B. SilKit)

Historisch wurden vECUs als VTT‑Artefakte (`appl.dll`, `hsm.dll`) betrachtet.
Diese Annahme ist **nicht mehr gültig**.

**Neue Ausgangslage:**

- APPL und HSM sind vollständig eigenentwickelte Module
- Es existiert keine Abhängigkeit zu Vector VTT (weder Build, ABI, Tooling)
- Die Begriffe APPL und HSM beschreiben fachliche Rollen, keine Tool‑Artefakte
- OpenSUT‑Kompatibilität bedeutet semantische Kompatibilität, nicht Binär‑Kompatibilität

Was fehlt, ist eine klar spezifizierte Loader‑Komponente, die:

- diese Eigenentwicklungen plattformneutral lädt,
- die ABI‑Grenze formalisiert,
- deterministische Orchestrierung sicherstellt,
- und das vECU Runtime Environment korrekt initialisiert.

#### Entscheidung

Wir spezifizieren einen eigenständigen, plattformneutralen vECU Loader, der:

- ausschließlich eigenentwickelte APPL/HSM Module lädt
- keinerlei Abhängigkeit zu VTT oder proprietären Toolchains besitzt
- nur über eine definierte, stabile C‑ABI mit Modulen spricht
- OpenSUT‑kompatible Ausführung ermöglicht
- identisch auf Windows, Linux und macOS funktioniert
- rein orchestrierend ist (keine ECU‑Logik)

**Der Loader ist der einzige OS‑ und Plattform‑abhängige Teil der Lösung.**

#### Begriffsdefinition

| Begriff              | Bedeutung                                                      |
| -------------------- | -------------------------------------------------------------- |
| **APPL**             | Eigenentwickeltes vECU‑Applikationsmodul                       |
| **HSM**              | Eigenentwickeltes vECU‑Security‑/Crypto‑Modul                  |
| **Loader**           | Orchestrator, Lifecycle‑Owner, ABI‑Boundary                    |
| **Runtime Env**      | Existierende Ausführungslogik (nicht Teil dieses ADRs)         |
| **OpenSUT‑kompatibel** | Gleiches Interaktionsmodell, nicht gleiche Binärschnittstelle |

#### Verantwortlichkeiten des Loaders

**Der Loader ist verantwortlich für:**

- CLI / Konfigurationsverarbeitung (`config.yaml`)
- Laden der APPL/HSM Module
- ABI‑Version‑ und Capability‑Negotiation
- Initialisierung des vECU Runtime Environments
- Allokation und Übergabe von Shared Memory
- deterministische Tick‑Orchestrierung
- Weiterleitung von IO zwischen Runtime ↔ Modulen

**Der Loader ist nicht verantwortlich für:**

- ECU‑Funktionalität
- Bus‑Simulation
- Diagnose‑Logik
- Security‑Policies
- Tool‑Integration

➡️ **Der Loader kennt keine ECU‑Details.**

#### Plattformneutralität (harte Designentscheidung)

Der Loader muss ohne Codeänderung laufen auf:

| Plattform | Mechanismus            |
| --------- | ---------------------- |
| Windows   | `.exe` + `.dll`        |
| Linux     | ELF Binary + `.so`     |
| macOS     | Mach‑O Binary + `.dylib` |

Alle OS‑Unterschiede sind intern gekapselt.

#### Loader‑CLI (Code‑relevant)

```text
vecu-loader
  --appl <path>
  --hsm <path>
  --config <config.yaml>
  [--mode standalone|distributed]
```

➡️ Keine impliziten Pfade, keine Tool‑Defaults.

#### Modul‑Ladekonzept (ohne VTT)

**Grundsatz:**

- APPL und HSM sind normale dynamische Libraries
- Namenskonvention ist rein betriebssystembedingt, nicht toolabhängig
- Der Loader kennt nur **ein** Symbol

**Einziger verpflichtender Entry‑Point:**

```c
vecu_status_t vecu_get_api(
    vecu_abi_version_t requested,
    vecu_plugin_api_t* out_api
);
```

➡️ Kein zweites Symbol, kein magisches Verhalten.

#### ABI‑Vertrag (Loader ↔ Module)

**ABI‑Prinzipien:**

- C‑ABI (kein C++, kein Rust‑ABI)
- Function‑Table‑Pattern
- explizite Versionierung
- keine Allokation über Modulgrenzen
- keine globalen Zustände

➡️ Ziel: stabil, sprachneutral, testbar

#### Initialisierungssequenz (exakt, implementierbar)

Der Loader muss folgende Sequenz einhalten:

1. Lade APPL‑Modul
2. Lade HSM‑Modul
3. Rufe `vecu_get_api()` auf beiden Modulen auf
4. Prüfe:
   - ABI‑Version
   - Modul‑Rolle (APPL/HSM)
   - Capability‑Flags
5. Initialisiere vECU Runtime Environment
6. Allokiere Shared Memory
7. Übergib Runtime‑Context an beide Module (`init`)

➡️ Kein Modul sieht das andere direkt.

#### Deterministische Orchestrierung (zwingend)

Der Loader definiert die **einzige gültige Aufrufreihenfolge**:

```text
for each simulation tick:
  1. Loader → APPL:  push_frame(inbound)   [0..N]
  2. Loader → HSM:   step(tick)
  3. Loader → APPL:  step(tick)
  4. Loader ← APPL:  poll_frame(outbound)  [0..N]
```

> **Hinweis:** Die Inbound‑Frames (Schritt 1) stammen logisch aus dem
> vECU Runtime Environment (Bus, OpenSUT, externe Quellen). Der Loader
> leitet sie als Orchestrator an das APPL‑Modul weiter. Ebenso sammelt
> der Loader in Schritt 4 die Outbound‑Frames ein und übergibt sie an
> die Runtime. Module sehen nur den Loader, nie die Runtime direkt.

**Eigenschaften:**

- Single‑threaded ABI
- reproduzierbar
- CI‑fähig
- identisches Verhalten auf allen OS

➡️ **Determinismus ist Teil der ABI.**

#### Shared Memory Ownership

- Shared Memory wird **ausschließlich vom Loader** erzeugt
- Module erhalten:
  - Base‑Pointer
  - Größe
  - Header mit Offsets
- Module:
  - dürfen lesen/schreiben
  - dürfen **nicht** re‑allokieren
  - dürfen keine Pointer untereinander austauschen

➡️ Verhindert versteckte Kopplung.

#### OpenSUT‑Kompatibilität (klar abgegrenzt)

Der Loader:

- erfüllt die **semantischen** Erwartungen von OpenSUT:
  - vECU start/stop
  - deterministische Ausführung
  - klar definierte Interaktion
- ist **kein** OpenSUT‑Framework
- kann durch einen OpenSUT‑Adapter genutzt werden

➡️ Kompatibel, aber nicht abhängig.

#### Fehler‑ und Exit‑Semantik

**Der Loader muss abbrechen, wenn:**

- Modul nicht geladen werden kann
- `vecu_get_api` fehlt
- ABI inkompatibel
- Initialisierung fehlschlägt

**Der Loader darf weiterlaufen, wenn:**

- optionale Capabilities fehlen
- Module keine Frames liefern

#### Implementierungsleitplanken

**Sprache:**

- Loader: Rust (oder C++17)
- Module: C / C++ / Rust

**Regeln:**

- keine OS‑APIs außerhalb des Loader‑Pakets
- keine Threads im ABI
- keine Exceptions über ABI
- Logging nur über Callback

#### Abgelehnte Alternativen

| Alternative                        | Grund                        |
| ---------------------------------- | ---------------------------- |
| VTT‑kompatible Binärschnittstelle  | unnötig, Tool‑Bindung        |
| Windows‑DLL‑Only                   | widerspricht Linux/macOS Ziel |
| C++‑ABI                            | instabil                     |
| Callback‑basierte Orchestrierung   | nicht deterministisch        |

---

### ADR‑002: Eigenentwickelte APPL‑ und HSM‑Module für vECU Loader

**(1:1 kompatibel zur Loader‑ABI, OpenSUT‑kompatibel, plattformneutral)**

**Status:** Accepted

**Bezug:** Dieses ADR ist bindend in Kombination mit ADR‑001.
Alle hier beschriebenen Module müssen die dort definierte Loader‑ABI erfüllen.
Abweichungen sind nicht zulässig, da sonst Determinismus, Portabilität oder
Austauschbarkeit verloren gehen.

#### Kontext

Im vECU Runtime Environment werden zwei fachlich getrennte, eigenentwickelte
Module ausgeführt:

- **APPL‑Modul:** Fahrzeug‑/ECU‑Applikationslogik
- **HSM‑Modul:** Security‑, Crypto‑ und Schutzfunktionen

Diese Module:

- sind keine Tool‑Artefakte
- sind keine VTT‑DLLs
- sind vollständig unter eigener Kontrolle
- werden ausschließlich über den vECU Loader orchestriert

Historische Tool‑Grenzen (z. B. CANoe/VTT) spielen keine Rolle mehr.
Die Begriffe APPL und HSM bezeichnen **logische Rollen**, nicht Herkunft.

#### Entscheidung

APPL und HSM werden als plattformneutrale, dynamisch ladbare Module definiert, die:

- exakt dieselbe Loader‑ABI implementieren (ADR‑001)
- keine direkte Abhängigkeit zueinander haben
- nur über Shared Memory und definierte ABI‑Calls interagieren
- keine OS‑, Tool‑ oder Framework‑Kenntnis besitzen
- deterministisch und reproduzierbar arbeiten

**Der Loader ist der einzige Orchestrator.**

#### Gemeinsame Grundsätze für APPL und HSM

##### 1. ABI‑Pflicht

Jedes Modul muss:

- `vecu_get_api()` exportieren
- eine gültige `vecu_plugin_api_t` zurückliefern
- `common.init` / `step` / `shutdown` implementieren
- keine weiteren Exports bereitstellen

➡️ **Ein Symbol. Ein Vertrag.**

##### 2. Plattformneutralität

Ein Modul:

- darf keine OS‑APIs verwenden
- darf keine Threads starten
- darf keine dynamische Ladung durchführen
- darf keine Speicherbereiche außerhalb des Loaders allokieren

➡️ Ein Modul ist reiner Logik‑Code.

##### 3. Speicher‑ und Lebenszyklusregeln

- Module besitzen keinen Speicher
- Alle Ressourcen kommen vom Loader:
  - Shared Memory
  - Allocator
  - Logger
- Ein Modul:
  - initialisiert sich in `init()`
  - arbeitet nur in `step()`
  - gibt Ressourcen in `shutdown()` frei

➡️ Kein globaler Zustand außerhalb des Moduls.

#### APPL‑Modul (Application Logic)

##### Rolle

Das APPL‑Modul implementiert:

- ECU‑/Fahrzeug‑Applikationslogik
- Zustandsautomaten
- Kommunikation mit der Außenwelt (Frames, Events)
- fachliche Reaktion auf Diagnose‑ und Steuerbefehle

Es ist der **funktionale Kern** der vECU.

##### Pflicht‑Capabilities (ABI)

Das APPL‑Modul muss folgende Flags setzen:

```c
VECU_CAP_FRAME_IO
```

Optional:

```c
VECU_CAP_DIAGNOSTICS
```

##### Pflicht‑Funktionen (APPL‑API)

Das APPL‑Modul muss folgende Callbacks implementieren:

```c
appl.push_frame(const vecu_frame_t* in);   // Inbound
appl.poll_frame(vecu_frame_t* out);         // Outbound
```

Bedeutung:

- **`push_frame`** — wird vom Loader aufgerufen, liefert Inbound‑Events (Bus, Runtime, OpenSUT)
- **`poll_frame`** — wird vom Loader gepollt, liefert Outbound‑Events (Bus, Runtime)

➡️ APPL ist event‑getrieben, nicht aktiv sendend.

##### APPL‑Ausführungsmodell (bindend)

APPL‑Code **darf nur:**

- Shared Memory lesen/schreiben
- interne Zustände aktualisieren
- Frames in interne Queues legen

APPL‑Code **darf nicht:**

- Zeit selbst messen
- `sleep()` aufrufen
- andere Module direkt aufrufen
- Systemzustand abfragen

➡️ Zeit kommt ausschließlich über den Loader‑Tick.

#### HSM‑Modul (Security / Crypto)

##### Rolle

Das HSM‑Modul implementiert:

- kryptografische Primitive
- SecurityAccess (Seed/Key)
- Signatur / Verifikation
- Schutzlogik für APPL

Es ist **kein echtes Hardware‑HSM**, sondern eine deterministische
Security‑Abstraktion für vECUs.

##### Pflicht‑Capabilities (ABI)

Das HSM‑Modul muss mindestens setzen:

```c
VECU_CAP_HSM_SEED_KEY
```

Optional:

```c
VECU_CAP_SIGN_VERIFY
```

##### Pflicht‑Funktionen (HSM‑API)

Das HSM‑Modul muss implementieren:

```c
hsm.seed(...)
hsm.key(...)
```

Optional:

```c
hsm.sign(...)
hsm.verify(...)
```

➡️ Alle Funktionen sind rein funktional (keine Seiteneffekte außerhalb des Moduls).

##### HSM‑Ausführungsmodell

- HSM hat keine Kenntnis von Bussen
- HSM kommuniziert nicht direkt mit APPL
- Interaktion erfolgt:
  - über Shared Memory
  - oder über explizite Loader‑Aufrufe

➡️ HSM ist rein dienstleistend.

#### Gemeinsame Orchestrierung (APPL + HSM)

##### Verbindliche Tick‑Sequenz

Der Loader ruft Module **immer** in folgender Reihenfolge auf:

```text
1. APPL.push_frame(...)  [0..N]
2. HSM.step(tick)
3. APPL.step(tick)
4. APPL.poll_frame(...)   [0..N]
```

Bedeutung:

- APPL kann Security‑Anfragen vorbereiten
- HSM verarbeitet Security‑Logik
- APPL nutzt Ergebnisse im selben Tick

➡️ **Determinismus garantiert.**

##### Shared Memory Nutzung

APPL und HSM:

- erhalten denselben Shared‑Memory‑Bereich
- kennen nur Offsets, keine Pointer untereinander
- dürfen keine Ownership an Speicher beanspruchen

➡️ Synchronisation erfolgt implizit durch Loader‑Sequenz, nicht durch Locks.

#### OpenSUT‑Kompatibilität

APPL/HSM sind OpenSUT‑kompatibel, weil:

- sie eine vECU darstellen
- sie deterministisch start/stop‑bar sind
- sie über klar definierte Schnittstellen interagieren

Sie:

- implementieren kein OpenSUT
- kennen kein OpenSUT‑Protokoll

➡️ Der Loader fungiert als OpenSUT‑Execution‑Backend.

#### Fehler‑Semantik

Ein Modul darf Fehler signalisieren, aber:

- darf den Prozess **nicht** beenden
- darf keine Exception über ABI werfen
- darf keine undefinierten Zustände erzeugen

Der Loader entscheidet über Abbruch oder Fortsetzung.

#### Implementierungsleitplanken (direkt code‑able)

**Empfohlene Sprachen:**

- Rust (`cdylib`) → bevorzugt
- C / C++

**Zwingende Regeln:**

- `extern "C"`
- `#[repr(C)]`
- keine Panics über ABI
- kein Threading
- kein I/O

#### Abgelehnte Alternativen

| Alternative                   | Grund                    |
| ----------------------------- | ------------------------ |
| VTT‑Binärkompatibilität       | unnötig                  |
| Direktaufruf APPL ↔ HSM       | zerstört Kapselung       |
| Thread‑basierte Module        | nicht deterministisch    |
| Tool‑abhängige APIs           | Lock‑in                  |

#### Ergebnis

APPL und HSM sind definiert als:

> **Deterministische, plattformneutrale Fachmodule, die ausschließlich
> durch den vECU Loader orchestriert werden.**

In Kombination mit ADR‑001 ist die Architektur:

- vollständig spezifiziert
- tool‑frei
- plattformneutral
- direkt implementierbar

### ADR‑003: Shared‑Memory‑Layout & Runtime‑Interaction Contract

**Status:** Accepted

**Bezug:** Dieses ADR ist bindend zusammen mit:

- ADR‑001: vECU Loader
- ADR‑002: APPL & HSM Module

Ohne Einhaltung dieses ADRs sind Determinismus, Austauschbarkeit und
OpenSUT‑Kompatibilität nicht gewährleistet.

#### Kontext

Das vECU Runtime Environment nutzt Shared Memory als:

- einzigen gemeinsamen Datenraum zwischen Loader, APPL und HSM
- Entkopplungsmechanismus (keine direkten Funktionsaufrufe APPL↔HSM)
- Grundlage für deterministische Simulation

Bisher war Shared Memory nur implizit dargestellt (Slide 3).
Dieses ADR macht es **explizit, versioniert und implementierbar**.

#### Entscheidung

Wir definieren ein offset‑basiertes, versioniertes Shared‑Memory‑Layout, das:

- vom Loader allokiert und initialisiert wird
- von APPL und HSM nur gelesen/geschrieben wird
- keine Pointer‑Übergabe zwischen Modulen erlaubt
- deterministisch ist (keine Locks notwendig)
- ABI‑stabil über Versionen bleibt

#### Grundprinzipien

- **Offsets statt Struct‑Wachstum** → ABI‑stabil
- **Single‑Writer‑Regeln pro Bereich** → kein Locking
- **Loader orchestriert** → implizite Synchronisation
- **Header mit Magic + Version** → robust gegen Fehlkonfiguration

#### Shared‑Memory Top‑Level‑Layout

```text
+----------------------------------------------------+
| vecu_shm_header                                    |
+----------------------------------------------------+
| RX Frame Queue   (Runtime → APPL)                  |
+----------------------------------------------------+
| TX Frame Queue   (APPL → Runtime)                  |
+----------------------------------------------------+
| Diagnostic Mailbox (APPL ↔ HSM ↔ Runtime)          |
+----------------------------------------------------+
| Variable / State Block (APPL & HSM)                |
+----------------------------------------------------+
| Reserved / Future Extensions                       |
+----------------------------------------------------+
```

#### Verbindlicher Header (`vecu_shm_header_t`)

```c
typedef struct vecu_shm_header_t {
    uint32_t magic;          // 'VECU'
    uint16_t abi_major;
    uint16_t abi_minor;
    uint64_t total_size;

    // Offsets (from base)
    uint64_t off_rx_frames;
    uint64_t off_tx_frames;
    uint64_t off_diag_mb;
    uint64_t off_vars;

    // Sizes
    uint32_t size_rx_frames;
    uint32_t size_tx_frames;
    uint32_t size_diag_mb;
    uint32_t size_vars;

    uint32_t flags;          // future use
    uint32_t reserved;
} vecu_shm_header_t;
```

**Regeln:**

- `magic` muss validiert werden
- Offsets dürfen sich nie ändern, nur erweitert werden
- Versionen steuern Backward‑Compatibility

#### RX / TX Frame Queues

**Modell:**

- Ringbuffer
- Single Writer / Single Reader
- Keine Locks notwendig

**Ownership:**

| Bereich   | Writer           | Reader           |
| --------- | ---------------- | ---------------- |
| RX Frames | Runtime / Loader | APPL             |
| TX Frames | APPL             | Runtime / Loader |

#### Diagnostic Mailbox

**Zweck:**

- Diagnose‑Requests
- Security‑Anfragen (Seed/Key)
- Status‑Antworten

**Interaktion:**

- APPL schreibt Anfrage
- HSM verarbeitet
- Ergebnis wird im selben Bereich abgelegt
- Loader garantiert Tick‑Reihenfolge

➡️ Kein direkter Funktionsaufruf APPL→HSM.

#### Variable / State Block

**Zweck:**

- ECU‑interner Zustand
- Debug‑/Trace‑Informationen
- DIO‑/Signal‑Simulation

**Regeln:**

- Struktur ist projektspezifisch
- Layout ist Version‑abhängig
- Loader kennt Inhalt nicht

#### Synchronisationsmodell

- Keine Mutexes
- Keine Atomics notwendig
- Synchronisation erfolgt ausschließlich über:
  - Loader‑Tick‑Sequenz (ADR‑001)
  - klare Ownership pro Bereich

➡️ **Determinismus ist garantiert.**

#### Fehlerfälle

Der Loader muss abbrechen, wenn:

- `magic` falsch
- ABI‑Version inkompatibel
- Offsets außerhalb der Shared‑Memory‑Größe liegen

#### Ergebnis

Shared Memory ist definiert als:

> **Stabiler, versionierter Datenaustauschraum, der APPL und HSM
> vollständig entkoppelt und deterministische Simulation ermöglicht.**

---

## C4 Architecture

### L1: System Context

```text
+------------------------------------------------------------------+
|                                                                  |
|  User / CI / OpenSUT Client                                      |
|  --------------------------------------------------------------  |
|  - Developer                                                     |
|  - CI Pipeline                                                   |
|  - OpenSUT-compatible Tooling                                    |
|                                                                  |
|                |                                                 |
|                | executes / controls                              |
|                v                                                 |
|                                                                  |
|  +------------------------------------------------------------+  |
|  |                                                            |  |
|  |  vECU Execution System                                     |  |
|  |  --------------------------------------------------------  |  |
|  |  Deterministic, cross-platform execution of virtual ECUs   |  |
|  |  OpenSUT-compatible                                        |  |
|  |  No proprietary tool dependencies                           |  |
|  |                                                            |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

### L2: Container Diagram

```text
+----------------------------------------------------------------------------------+
|                                                                                  |
|  vECU Execution System                                                           |
|                                                                                  |
|  +---------------------------+        +---------------------------+              |
|  |                           |        |                           |              |
|  |  vECU Loader              |------->|  vECU Runtime Environment |              |
|  |                           |        |                           |              |
|  |  - CLI / config.yaml      |        |  - Simulation loop        |              |
|  |  - Dynamic module loading |        |  - Bus abstraction        |              |
|  |  - ABI negotiation        |        |  - OpenSUT semantics      |              |
|  |  - Tick orchestration     |        |                           |              |
|  |                           |        +---------------------------+              |
|  +---------------------------+                                                   |
|           |          |                                                           |
|           | loads    | loads                                                     |
|           v          v                                                           |
|                                                                                  |
|  +---------------------------+        +---------------------------+              |
|  |                           |        |                           |              |
|  |  APPL Module              |<------>|  Shared Memory            |              |
|  |                           |        |                           |              |
|  |  - ECU application logic  |        |  - RX frame queue         |              |
|  |  - State machines         |        |  - TX frame queue         |              |
|  |  - Event handling         |        |  - Diagnostic mailbox     |              |
|  |                           |        |  - Variable/state block   |              |
|  +---------------------------+        +---------------------------+              |
|                                                ^                                 |
|                                                | reads/writes                    |
|                                                |                                 |
|  +---------------------------+                 |                                  |
|  |                           |-----------------+                                 |
|  |  HSM Module               |                                                   |
|  |                           |  (no direct APPL↔HSM dependency;                  |
|  |  - Crypto primitives      |   interaction only via Shared Memory)             |
|  |  - SecurityAccess         |                                                   |
|  |  - Sign / verify          |                                                   |
|  |                           |                                                   |
|  +---------------------------+                                                   |
|                                                                                  |
+----------------------------------------------------------------------------------+
```

### L3: Component Diagram – vECU Loader

```text
+--------------------------------------------------------------------------+
|                                                                          |
|  vECU Loader                                                             |
|                                                                          |
|  +----------------------+                                                |
|  | CLI / Config Parser  |                                                |
|  |----------------------|                                                |
|  | - parses config.yaml |                                                |
|  | - resolves module    |                                                |
|  |   paths              |                                                |
|  +----------------------+                                                |
|              |                                                           |
|              v                                                           |
|  +----------------------+                                                |
|  | Dynamic Module Loader|                                                |
|  |----------------------|                                                |
|  | - LoadLibrary /      |                                                |
|  |   dlopen             |                                                |
|  | - resolves           |                                                |
|  |   vecu_get_api       |                                                |
|  +----------------------+                                                |
|              |                                                           |
|              v                                                           |
|  +----------------------+                                                |
|  | ABI Negotiator       |                                                |
|  |----------------------|                                                |
|  | - checks ABI version |                                                |
|  | - validates role     |                                                |
|  | - reads capabilities |                                                |
|  +----------------------+                                                |
|              |                                                           |
|              v                                                           |
|  +----------------------+                                                |
|  | Shared Memory Manager|                                                |
|  |----------------------|                                                |
|  | - allocates memory   |                                                |
|  | - initializes header |                                                |
|  | - validates layout   |                                                |
|  +----------------------+                                                |
|              |                                                           |
|              v                                                           |
|  +----------------------+                                                |
|  | Tick Orchestrator    |                                                |
|  |----------------------|                                                |
|  | - deterministic loop |                                                |
|  | - enforces call      |                                                |
|  |   order              |                                                |
|  +----------------------+                                                |
|              |                                                           |
|              v                                                           |
|  +----------------------+                                                |
|  | Runtime Adapter      |                                                |
|  |----------------------|                                                |
|  | - connects to vECU   |                                                |
|  |   Runtime Env        |                                                |
|  | - OpenSUT semantics  |                                                |
|  +----------------------+                                                |
|                                                                          |
+--------------------------------------------------------------------------+
```

### L4: Component Diagram – APPL & HSM Modules

**APPL Module:**

```text
+------------------------------------------------------+
|                                                      |
|  APPL Module                                        |
|                                                      |
|  +--------------------------+                        |
|  | Event Ingress             |                        |
|  |--------------------------|                        |
|  | - push_frame()            |                        |
|  | - inbound events          |                        |
|  +--------------------------+                        |
|              |                                       |
|              v                                       |
|  +--------------------------+                        |
|  | Application Logic         |                        |
|  |--------------------------|                        |
|  | - state machines          |                        |
|  | - ECU behavior            |                        |
|  +--------------------------+                        |
|              |                                       |
|              v                                       |
|  +--------------------------+                        |
|  | Event Egress              |                        |
|  |--------------------------|                        |
|  | - poll_frame()            |                        |
|  | - outbound events         |                        |
|  +--------------------------+                        |
|                                                      |
+------------------------------------------------------+
```

**HSM Module:**

```text
+------------------------------------------------------+
|                                                      |
|  HSM Module                                         |
|                                                      |
|  +--------------------------+                        |
|  | Security Interface        |                        |
|  |--------------------------|                        |
|  | - seed()                  |                        |
|  | - key()                   |                        |
|  +--------------------------+                        |
|              |                                       |
|              v                                       |
|  +--------------------------+                        |
|  | Crypto Engine              |                        |
|  |--------------------------|                        |
|  | - signing                 |                        |
|  | - verification            |                        |
|  +--------------------------+                        |
|                                                      |
+------------------------------------------------------+
```

### C4 Zusammenfassung

- **L1** zeigt: Wer nutzt das System und wofür
- **L2** zeigt: Welche Container existieren und wie sie interagieren
- **L3** zeigt: Wie der Loader intern strukturiert ist
- **L4** zeigt: Wie APPL und HSM intern logisch aufgebaut sind

Alle Ebenen:

- sind tool‑frei
- sind plattformneutral
- sind deterministisch
- entsprechen exakt ADR‑001 bis ADR‑003

---

## ADR‑004: OpenSUT API Abstraction & Runtime Adapter Traits

**Status:** Accepted

### Kontext

Das vECU Runtime Environment benötigt zwei orthogonale Abstraktionen:

1. **Tick‑Quelle:** Wann werden Ticks ausgelöst? (Timer‑Loop vs. SIL Kit Virtual Time Sync)
2. **Frame‑Routing:** Wohin werden Frames gesendet? (SHM‑Queues, SIL Kit CAN, Hardware)

Bisher waren beide Aspekte implizit im Loader/Runtime gekoppelt. Für OpenSUT‑kompatible
Co‑Simulation und alternative Backends müssen sie **explizit getrennt** sein.

### Entscheidung

Zwei unabhängige Traits in `vecu-runtime`:

#### `RuntimeAdapter` – Tick‑Quelle

```rust
pub trait RuntimeAdapter {
    fn run(&mut self, runtime: &mut Runtime) -> Result<(), RuntimeError>;
}
```

| Implementierung     | Crate           | Beschreibung                          |
|---------------------|-----------------|---------------------------------------|
| `StandaloneAdapter` | `vecu-runtime`  | Feste Tick‑Anzahl in Tight‑Loop       |
| `SilKitAdapter`     | `vecu-silkit`   | SIL Kit Lifecycle + `TimeSyncService` |

#### `OpenSutApi` – Frame‑Routing (Bus‑Abstraktion)

```rust
pub trait OpenSutApi: Send {
    fn recv_inbound(&mut self, out: &mut Vec<VecuFrame>) -> Result<(), RuntimeError>;
    fn dispatch_outbound(&mut self, frames: &[VecuFrame]) -> Result<(), RuntimeError>;
    fn on_start(&mut self) -> Result<(), RuntimeError>;
    fn on_stop(&mut self) -> Result<(), RuntimeError>;
}
```

| Implementierung        | Crate          | Beschreibung                                     |
|------------------------|----------------|--------------------------------------------------|
| `NullBus`              | `vecu-runtime` | No‑op (Test / Standalone ohne externe Busse)     |
| `SilKitBus`            | `vecu-silkit`  | SIL Kit CAN‑Controller via Shared RX Buffer      |
| *(SHM‑Fallback)*       | `vecu-runtime` | Wenn kein Bus gesetzt: SHM RX/TX Queues direkt   |

#### Per‑Modul Bus‑Zuweisung

Jedes Modul kann seinen **eigenen** `OpenSutApi`‑Bus erhalten:

```rust
runtime.set_appl_bus(Box::new(silkit_bus));   // APPL ↔ CAN+ETH+LIN+FR
runtime.set_hsm_bus(Box::new(rbs_bus));       // HSM  ↔ RBS
```

- `set_appl_bus()` – Bus für das APPL‑Modul (SHM‑Fallback wenn nicht gesetzt)
- `set_hsm_bus()` – Bus für das HSM‑Modul (kein Fallback: ohne Bus kein Frame‑I/O für HSM)
- `set_bus()` – Convenience‑Alias für `set_appl_bus()` (abwärtskompatibel)

#### Integration in `Runtime::tick()`

```text
for each simulation tick:
  1a. appl_bus.recv_inbound()      ──→ inbound_buf   (oder SHM RX Fallback)
  1b. APPL.push_frame(inbound)     ─── inbound_buf
  2a. hsm_bus.recv_inbound()       ──→ inbound_buf   (nur wenn hsm_bus gesetzt)
  2b. HSM.push_frame(inbound)      ─── inbound_buf
  3.  HSM.step(tick)
  4.  APPL.step(tick)
  5a. HSM.poll_frame()             ──→ outbound_buf
  5b. hsm_bus.dispatch_outbound()  ─── outbound_buf  (nur wenn hsm_bus gesetzt)
  6a. APPL.poll_frame()            ──→ outbound_buf
  6b. appl_bus.dispatch_outbound() ─── outbound_buf  (oder SHM TX Fallback)
```

#### Lifecycle‑Hooks

- `on_start()` wird in `Runtime::init_all()` auf **beiden** Buses aufgerufen (nach Modul‑Init)
- `on_stop()` wird in `Runtime::shutdown_all()` auf **beiden** Buses aufgerufen (vor Modul‑Shutdown)

### Separation of Concerns

```text
┌─────────────────────────────────────────────────────────────┐
│  vECU Runtime Env                                           │
│                                                             │
│  RuntimeAdapter (WANN)                                      │
│  ┌──────────────────┐                                       │
│  │ StandaloneAdapter │                                       │
│  │ SilKitAdapter     │                                       │
│  └───────┴──────────┘                                       │
│          │                                                   │
│          └──────── Runtime::tick() ──────────────┐          │
│                    │                        │          │
│          ┌────────┴────────┐    ┌────────┴─────┐    │
│          │  APPL             │    │  HSM            │    │
│          └────────┴────────┘    └────────┴─────┘    │
│                  │                        │          │
│          ┌────────┴────────┐    ┌────────┴─────┐    │
│          │  appl_bus         │    │  hsm_bus        │    │
│          │  (OpenSutApi)     │    │  (OpenSutApi)   │    │
│          └─────────────────┘    └──────────────┘    │
│               │                     │               │
│          CAN/ETH/LIN/FR          CAN/LIN/FR           │
└─────────────────────────────────────────────────────────────┘
```

### Konsequenzen

- **Per‑Modul Bus‑Zuweisung**: APPL und HSM können jeweils eigene `OpenSutApi`‑Instanzen haben
- Neue Bus‑Backends (TAP Bridge, RBS, Hardware) implementieren nur `OpenSutApi`
- Neue Tick‑Quellen (HiL, Replay) implementieren nur `RuntimeAdapter`
- Bestehender Standalone‑Modus unverändert (SHM‑Fallback für APPL)
- HSM Frame‑I/O nur aktiv wenn `hsm_bus` gesetzt (kein SHM‑Fallback für HSM)
- `SilKitBus` nutzt `Arc<Mutex<Vec<VecuFrame>>>` als thread‑sicheren RX‑Buffer
- CAN‑only: ETH/LIN/FlexRay Controller als TODO dokumentiert

---

## Key Properties

- Cross‑platform (Windows / Linux / macOS)
- Deterministic execution
- OpenSUT‑compatible
- No proprietary dependencies
- CI‑ready
- Rust/C/C++ friendly

---

## Non‑Goals

- Binary compatibility with VTT
- Tool‑specific APIs
- Threaded execution in modules

---

## Summary

The vECU architecture cleanly separates:
- orchestration (Loader),
- execution (Runtime),
- tick source (RuntimeAdapter),
- bus I/O (OpenSutApi),
- logic (APPL),
- security (HSM),
using a stable ABI, shared memory and two orthogonal trait abstractions.

This enables scalable, license‑free and reproducible vECU execution
across standalone, SIL Kit co‑simulation and future hardware backends.
