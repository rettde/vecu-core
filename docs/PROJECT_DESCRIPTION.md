# Projektbeschreibung: vecu-core

## Zusammenfassung

**vecu-core** ist eine quelloffene, plattformübergreifende Laufzeitumgebung
für die Ausführung von AUTOSAR-basiertem ECU-C-Code auf dem Host-PC. Das
System ermöglicht Software-in-the-Loop-Tests (SiL) ohne Eval-Board, ohne
Target-Compiler und ohne proprietäre Tooling-Lizenzen.

Der bestehende ECU-Applikationscode (SWCs) wird unverändert als Shared
Library kompiliert und in einer deterministischen, tick-basierten
Rust-Runtime ausgeführt. Ein mitgelieferter AUTOSAR BaseLayer stellt
24 BSW-Modul-Stubs bereit (Com, Dcm, NvM, Csm, CanTp, DoIP u.a.),
sodass der Applikationscode gegen die gewohnten AUTOSAR-APIs kompiliert.
Alternativ kann auch der echte Vector AUTOSAR BSW aus DaVinci Configurator
oder [Eclipse OpenBSW](https://github.com/esrlabs/openbsw) (Apache-2.0)
eingebaut werden.

---

## Problemstellung

Bisher erfordern SiL-Tests für AUTOSAR-ECUs entweder:

- **Vector VTT** — proprietär, Windows-only, lizenzpflichtig pro Sitz,
  keine CI-Fähigkeit ohne CANoe-Lizenz auf dem Build-Server.
- **Target-Hardware** — langsam, teuer, nicht parallelisierbar, schlecht
  in CI integrierbar.

Beides bremst die Entwicklungsgeschwindigkeit und verhindert frühzeitiges
Testen der Applikationslogik.

---

## Lösung

vecu-core entkoppelt die Applikationslogik von der Target-Hardware:

1. **ECU-Code als Shared Library** — Die SWCs und RTE-Header werden mit
   einem Standard-Host-Compiler (gcc/clang/MSVC) als `.so`/`.dylib`/`.dll`
   kompiliert.

2. **AUTOSAR BaseLayer** — 24 BSW-Module als C11-Stubs: Lifecycle (EcuM,
   SchM), Kommunikation (Com, PduR, CanIf), Diagnostik (Dcm mit 9 UDS-
   Services, Dem), Speicher (NvM, Fee), Kryptografie (Csm → HSM) und
   Transport (CanTp, DoIP).

3. **Echtes HSM** — Keine Crypto-Stubs: AES-128 ECB/CBC, CMAC, CSPRNG
   und CMAC-basierter SecurityAccess (UDS 0x27) über ein SHE-kompatibles
   Hardware Security Module.

4. **Vector SIL Kit Integration** — Co-Simulation mit CANoe, Silver oder
   DTS.monaco über CAN, Ethernet, LIN und FlexRay. Alternativ:
   Standalone-Modus ohne externe Abhängigkeiten.

5. **CI-fähig** — Kein proprietäres Tooling auf dem Build-Server nötig.
   Die gesamte Testsuite läuft mit `cargo test` und `cmake`.

---

## Architektur (Kurzübersicht)

Das System besteht aus vier Schichten:

- **vecu-loader** — CLI-Tool, das die Simulation orchestriert: lädt
  Plugins, verhandelt die ABI-Version, steuert den Tick-Takt.

- **vecu-appl** — Rust-Bridge, die den C-BaseLayer und den ECU-Code zur
  Laufzeit als Shared Libraries lädt und über Callbacks (Logging,
  Frame-I/O, HSM) verbindet.

- **BaseLayer** — 24 AUTOSAR-BSW-Stubs in reinem C11. Konfiguration über
  C-Structs (Signale, PDUs, DIDs, NvM-Blöcke, CanTp-Kanäle). Kann durch
  den echten Vector BSW oder Eclipse OpenBSW ersetzt werden.

- **vecu-hsm** — SHE-kompatibles HSM mit 20-Slot AES-128 Key-Store, das
  über Shared Memory mit dem BaseLayer kommuniziert.

```
┌──────────────────────────────────────────────────────────┐
│  vECU Runtime (Rust)                     vecu-loader     │
│                                                          │
│  ┌────────────────────────────────────────────────────┐  │
│  │  APPL Module (vecu-appl)                           │  │
│  │  ┌──────────────────────┐  ┌────────────────────┐  │  │
│  │  │  ECU C-Code (SWCs)   │  │  BaseLayer         │  │  │
│  │  │  libappl_ecu.so      │→ │  libbase.so        │  │  │
│  │  │                      │  │  24 BSW-Module      │  │  │
│  │  └──────────────────────┘  └────────────────────┘  │  │
│  └────────────────────────────────────────────────────┘  │
│  ┌────────────────────┐  ┌─────────────────────────────┐ │
│  │  HSM (vecu-hsm)    │  │  Bus: SIL Kit / Standalone  │ │
│  │  AES-128, CMAC,    │  │  CAN, ETH, LIN, FlexRay    │ │
│  │  SecurityAccess     │  │  (vecu-silkit)              │ │
│  └────────────────────┘  └─────────────────────────────┘ │
│               ↕ SHM (vecu-shm)                           │
└──────────────────────────────────────────────────────────┘
```

Die Bus-Anbindung erfolgt über den **OpenSutApi**-Trait: eine
Abstraktionsschicht, die SIL Kit, Standalone oder eigene
Implementierungen unterstützt.

---

## Drei BaseLayer-Varianten

| BaseLayer | Lizenz | Einsatz |
|-----------|--------|---------|
| **Unsere Stubs** (Default) | MIT / Apache-2.0 | Prototyping, Unit-Tests, CI |
| **Vector AUTOSAR BSW** | Proprietär | Produktionsnahe Validierung (Kunden mit Lizenz) |
| **Eclipse OpenBSW** | Apache-2.0 | Vollständig offener Stack mit echten Zustandsmaschinen |

Der SWC-Code bleibt bei allen drei Varianten identisch — nur die
BaseLayer-Library (`libbase.so`) wird ausgetauscht.

---

## Für wen ist das relevant?

| Rolle | Nutzen |
|-------|--------|
| **Teamleiter / Projektleiter** | Kürzere Testzyklen, keine VTT-Lizenzkosten, CI-Integration ab Tag 1, plattformunabhängig |
| **SWC-Entwickler** | Applikationslogik lokal auf dem Laptop testen, ohne Eval-Board oder CANoe — Compile → Run → Debug in Sekunden |
| **Integratoren** | Gleicher SWC-Code läuft auf Target und in SiL; Vector BSW oder OpenBSW optional einbaubar |
| **Tester / QA** | UDS-Diagnostik (9 Services), SecurityAccess und NvM-Persistenz sind out-of-the-box testbar |
| **CI/DevOps** | Gesamte Suite läuft mit `cmake` + `cargo test` auf Linux/macOS/Windows ohne GUI oder Lizenzdateien |

---

## Technologieentscheidungen

- **Rust** für die Runtime: Speichersicherheit, kein `unsafe`,
  clippy-pedantic, Cross-Platform.
- **C11** für den BaseLayer: Kompatibilität mit bestehendem ECU-Code,
  keine externen Dependencies.
- **Dynamisches Linking** statt statisches: BaseLayer und ECU-Code können
  unabhängig gebaut und ausgetauscht werden.
- **SIL Kit per dynamischem FFI**: Kein Build-Time-Linking gegen die
  SIL Kit-Library — die API wird zur Laufzeit geladen.
- **MIT / Apache-2.0 Dual-Lizenz**: Keine proprietären Abhängigkeiten,
  frei einsetzbar.

---

## Status

Alle geplanten Phasen (P1–P7) sind abgeschlossen:

- ✅ C/Rust ABI-Kontrakt und Plugin-System
- ✅ 24 AUTOSAR BSW-Module (Lifecycle, Kommunikation, Diagnostik,
  Speicher, Krypto, Transport)
- ✅ SHE-kompatibles HSM mit echtem AES-128
- ✅ Vector SIL Kit Multi-Bus (CAN, ETH, LIN, FlexRay)
- ✅ Referenz-ECU-Projekt (3 SWCs) als Vorlage
- ✅ CI-Pipeline (GitHub Actions)
- ✅ 133 Tests, clippy-pedantic clean

---

## Roadmap

- [ ] **Eclipse OpenBSW Shim-Layer** — AUTOSAR-API-Wrapper über
  [OpenBSW](https://github.com/esrlabs/openbsw) als separates Repo
- [ ] **Pilot-Integration** eines echten ECU-Projekts
- [ ] **ARXML → C-Struct-Konverter** für DaVinci-generierte
  Konfigurationen
- [ ] **Web-Dashboard** für Simulationsergebnisse

---

## Weiterführende Dokumentation

- [README.md](../README.md) — Technische Übersicht (EN + DE)
- [HOWTO_ECU_INTEGRATION.md](../HOWTO_ECU_INTEGRATION.md) — Schritt-für-
  Schritt-Anleitung zur Integration eines ECU-Projekts
- [examples/sample_ecu/](../examples/sample_ecu/) — Funktionsfähiges
  Referenzprojekt mit 3 SWCs
