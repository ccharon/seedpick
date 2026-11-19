# Seedpick – Konzept

> Arbeitstitel „Seedpick“. Der Name ist an einer Stelle zentral definiert (CMake `PROJECT_NAME`, App-ID, Binary) und lässt sich vor dem ersten Release ändern.

## 1. Ziel

Ein KDE-Programm (Qt6/KF6/Kirigami), mit dem man aus einem Verzeichnis voller ähnlicher Bilder die **n besten** auswählt, typischerweise KI-Bilder mit gleichem Prompt und anderem Seed.

Kernidee: **Paarvergleich per Tastatur.** Zwei Bilder stehen nebeneinander, ← oder → wählt das bessere. Ein statistisches Modell entscheidet, welche Paare als Nächstes verglichen werden. Am Ende werden die Dateien in Ergebnisordner verschoben.

**Fairness ist eine Kernanforderung:** Kein Bild fliegt durch eine einzelne unglückliche Paarung raus. Reihenfolge, Seite (links/rechts) und Startposition dürfen das Ergebnis nicht systematisch beeinflussen.

## 2. Ablauf

```
Einlesen ──► Phase 1: Sichtung ──► Phase 2: Duelle ──► Phase 3: Übersicht & Anwenden
```

### Phase 0 – Einlesen
- Verzeichnis per Kommandozeile oder Dialog.
- Unterstützte Formate: alles, was `QImageReader` inkl. KImageFormats liest (PNG, JPEG, WebP, AVIF, JXL …).
- **Ergebnisordner werden ignoriert**, damit ein erneuter Lauf auf demselben Verzeichnis funktioniert.
- Nicht rekursiv (Option für später).
- Gibt es eine Sitzungsdatei, wird angeboten, **fortzusetzen** (siehe §4).

### Phase 1 – Sichtung (schneller Vorfilter)
Ein Bild im Vollbild, eine Taste pro Bild:

| Taste | Aktion |
|---|---|
| → / Leertaste | in Ordnung, nimmt an Duellen teil |
| ↓ / Entf | Ausschuss (technisch fehlerhaft) |
| Backspace / Strg+Z | rückgängig |
| ← | vorheriges Bild ansehen (ohne Entscheidung zu ändern) |

Die Phase lässt sich überspringen (Konfig/Taste). Das ist für sauber generierte Serien sinnvoll.

### Phase 2 – Duelle
Zwei Bilder nebeneinander, gleich skaliert:

| Taste | Aktion |
|---|---|
| ← | linkes Bild besser |
| → | rechtes Bild besser |
| ↑ | gleich gut (Unentschieden) |
| 1 / 2 | linkes/rechtes Bild nachträglich zu Ausschuss |
| Backspace / Strg+Z | rückgängig (beliebig tief) |
| Z oder Mausrad | synchroner Zoom/Pan in beiden Bildern (Details vergleichen) |
| F | Vollbild |
| I | Metadaten/Prompt einblenden (später) |
| Esc | pausieren, Sitzung speichern |

Anzeige: Fortschritt als Schätzung („ca. 40 Vergleiche verbleibend“) und die aktuelle Top-n-Leiste als Thumbnails.

Sonderfälle:
- Gilt nach der Sichtung **N ≤ n**, gibt es keine Duelle, alle Bilder kommen direkt in die Auswahl.
- Der Nutzer kann Phase 2 jederzeit mit „jetzt beenden“ abschließen. Dann zählt die aktuelle Rangfolge.

### Phase 3 – Übersicht & Anwenden
- Drei Spalten mit Thumbnails (Auswahl / Rest / Ausschuss) und Zählern.
- Manuelle Korrektur per Drag & Drop oder Kontextmenü.
- Button **„Anwenden“** verschiebt die Dateien (§5). Bis dahin wird **nichts** im Dateisystem verändert, außer der Sitzungsdatei.

## 3. Algorithmus

### 3.1 Modell
**Bradley-Terry mit Gauß-Prior (MAP-Schätzung):**
- Jedes Bild *i* hat eine Stärke μᵢ. P(i schlägt j) = σ(μᵢ − μⱼ), σ = logistische Funktion.
- Prior μᵢ ~ N(0, τ²), τ ≈ 1.5. Der Prior regularisiert und hält Bilder ohne Niederlagen endlich.
- Unentschieden zählt als ½ Sieg für beide.
- **Nach jedem Vergleich wird aus dem kompletten Log neu berechnet** (Newton/MM-Iteration). Bei N ≤ ~1000 sind das Millisekunden. Vorteil: Das Ergebnis hängt nicht von der Reihenfolge der Vergleiche ab, und Undo heißt einfach „letzten Log-Eintrag entfernen“.
- Unsicherheit σᵢ per Laplace-Näherung (Diagonale der inversen Hesse-Matrix, oder vereinfacht 1/√(Hᵢᵢ)).

> Warum nicht Elo/Glicko? Diese aktualisieren inkrementell und sind reihenfolgeabhängig. Mit vollständigem Neu-Fit ist das Modell fairer und Undo trivial. TrueSkill ist markenrechtlich geschützt und unnötig komplex.

### 3.2 Paarauswahl (aktiv, Fokus auf die Top-n-Grenze)
Gesucht ist nicht die komplette Rangfolge, sondern nur **wer zu den Top n gehört**.

1. Nach μ sortieren. Grenze b = (μ₍ₙ₎ + μ₍ₙ₊₁₎) / 2.
2. Mehrdeutigkeit jedes Bildes: aᵢ = Φ(−|μᵢ − b| / σᵢ), also die Wahrscheinlichkeit, dass es auf der falschen Seite der Grenze steht.
3. **Fairness-Regel zuerst:** Hat ein Bild weniger als `minDuels` Vergleiche (Standard 2), hat es Vorrang.
4. Sonst wird das Bild A mit der höchsten Mehrdeutigkeit gewählt.
5. Partner B: ein Bild mit möglichst ähnlichem μ, bevorzugt auf der anderen Seite der Grenze, mit hoher Mehrdeutigkeit.
6. Nebenbedingungen:
   - kein Bild in zwei aufeinanderfolgenden Duellen (wenn N ≥ 4),
   - ein Paar höchstens `maxRepeatsPerPair` mal (Standard 1, außer bei Unentschieden),
   - **links/rechts zufällig** (Seed protokolliert, damit reproduzierbar).
7. Startrunde: Alle Bilder werden zufällig gepaart (wie Runde 1 im Schweizer System). So hat jedes Bild ab dem ersten Durchgang Daten.

### 3.3 Abbruchkriterium
Fertig, wenn alle Bilder `minDuels` erfüllt haben **und** max(aᵢ) < `confidenceThreshold` (Standard 0.05). Zusätzlich gibt es ein weiches Budget (`maxComparisons`, Standard 4·N), danach kommt der Hinweis „gut genug – beenden?“.

### 3.4 Verifikation per Simulation (Pflicht-Test)
Ein Test-Harness simuliert einen Nutzer:
- Jedes Bild bekommt eine verborgene „wahre Qualität“.
- Die simulierte Entscheidung ist verrauscht (logistisch, mit einstellbarer Fehlerrate und optionalem Links-Bias).
- Gemessen werden die Trefferquote der Top n und die Anzahl Vergleiche, für N ∈ {20, 50, 100, 300} und n ∈ {1, 3, 5, 10}.
- Akzeptanzkriterium als Startwert: Bei N=100, n=5 und 10 % Fehlerrate liegt die Trefferquote im Mittel ≥ 0.9 mit < 250 Vergleichen.
- Der Harness ist gleichzeitig das Werkzeug zum Tunen von τ, `minDuels` und Schwellen.

## 4. Sitzung & Persistenz

Datei `.seedpick-session.json` im Arbeitsverzeichnis, versioniertes Schema:

```json
{
  "schema": 1,
  "createdAt": "…", "updatedAt": "…",
  "config": { "n": 5, "minDuels": 2, "rngSeed": 123456, "skipScreening": false },
  "images": [ { "file": "img_0001.png", "size": 1234567, "mtime": "…" } ],
  "events": [
    { "t": "screen", "at": "2026-11-19T10:15:02.120Z", "file": "img_0001.png", "verdict": "ok" },
    { "t": "screen", "at": "…", "file": "img_0002.png", "verdict": "reject" },
    { "t": "duel", "at": "…", "left": "img_0001.png", "right": "img_0007.png", "result": "left" },
    { "t": "duel", "at": "…", "left": "…", "right": "…", "result": "tie" },
    { "t": "reject", "at": "…", "file": "…" },
    { "t": "finish", "at": "…" },
    { "t": "override", "at": "…", "file": "…", "target": "keep" }
  ]
}
```

- **Das Event-Log ist die einzige Wahrheit.** Der Zustand (Phase, Ratings, Zuordnung) wird immer daraus abgeleitet. Undo entfernt das letzte wirksame Event. Events zu Dateien, die nicht mehr vorhanden sind, bleiben im Log, wirken aber nicht.
- `at` ist der Zeitpunkt des Events (ISO 8601, UTC, Millisekunden). Das Feld ist optional und beeinflusst den abgeleiteten Zustand nicht.
- `finish` beendet Phase 2 vorzeitig („jetzt beenden“). Undo nimmt es zurück.
- `skipScreening` gehört zur Sitzung, weil es bestimmt, ob ungesichtete Bilder an Duellen teilnehmen.
- Unbekannte Felder werden beim Lesen ignoriert. Unbekannte oder fehlerhafte Events lassen das Laden scheitern, damit beim nächsten Speichern nichts verloren geht.
- Gespeichert wird nach jedem Event, atomar (`QSaveFile`).
- Beim Fortsetzen: Fehlende Dateien werden aus der Wertung entfernt. Neue Dateien kommen als ungesichtet dazu. Geänderte Dateien (size/mtime) erzeugen eine Warnung.
- Nach „Anwenden“ wird die Sitzungsdatei in `.seedpick-session.done.json` umbenannt (Nachvollziehbarkeit) oder gelöscht (Konfig).

## 5. Dateioperationen

### Ergebnisordner (Standard, konfigurierbar)
| Rolle | Standardname | Inhalt |
|---|---|---|
| keep | `_keep` | Top n |
| rest | `_rest` | in Duellen unterlegen |
| reject | `_reject` | in Sichtung/Duell als fehlerhaft markiert |

Der Unterstrich sorgt dafür, dass die Ordner in Dateimanagern oben stehen und als programmgeneriert erkennbar sind. Die Defaults sind englisch, weil das Repo öffentlich ist, und lassen sich frei umbenennen (z. B. `_auswahl/_rest/_ausschuss`).

Option `rejectToTrash`: Ausschuss in den Papierkorb (freedesktop Trash) statt in einen Ordner.

### Regeln
- **Begleitdateien mitverschieben:** gleiche Basis mit anderer Endung (`img.txt`, `img.json`, `img.yaml`) sowie `img.png.xmp` / `img.png.json`. Die Endungsliste ist konfigurierbar.
- **Nie überschreiben:** Bei Kollision wird `-1`, `-2` … vor der Endung angehängt, bei Begleitdateien konsistent mit dem Hauptbild.
- Ordner nur anlegen, wenn sie gebraucht werden.
- Verschieben über Dateisystemgrenzen: rename, sonst copy + verify + remove.
- Ein **Apply-Log** `.seedpick-apply.log.json` (Quelle → Ziel) ermöglicht `seedpick --undo-apply <dir>`. Das ist für einen späteren Meilenstein, aber das Log wird von Anfang an geschrieben.
- Fehler bei einzelnen Dateien brechen nicht alles ab. Am Ende kommt ein Bericht.

### Abstraktion
Interface `FileMover` im Core, ohne GUI- oder KIO-Abhängigkeit:
- `LocalFileMover` (Qt Core / std::filesystem) als Default.
- `KioFileMover` optional, für Trash und Fortschrittsanzeige im Plasma-Benachrichtigungssystem.

> **Risiko AppImage + KIO:** KIO-Worker laufen teils als eigene Prozesse und sind in AppImages fummelig. Deshalb gilt: Der Core hängt nicht von KIO ab, und der Trash-Pfad wird früh in der AppImage getestet. Notfalls wird der freedesktop-Trash-Spec selbst implementiert, er ist klein.

## 6. Konfiguration

KConfigXT (`seedpick.kcfg` + `.kcfgc`), Einstellungsdialog in Kirigami. Alle Werte lassen sich per CLI für den Lauf überschreiben.

| Schlüssel | Typ | Default | CLI |
|---|---|---|---|
| `TopN` | int | 5 | `-n, --top` |
| `KeepDirName` | string | `_keep` | `--keep-dir` |
| `RestDirName` | string | `_rest` | `--rest-dir` |
| `RejectDirName` | string | `_reject` | `--reject-dir` |
| `RejectToTrash` | bool | false | `--reject-to-trash` |
| `SkipScreening` | bool | false | `--skip-screening` |
| `MinDuels` | int | 2 | |
| `ConfidenceThreshold` | double | 0.05 | |
| `SidecarExtensions` | stringlist | `txt,json,yaml,yml,xmp,caption` | |
| `KeepSessionAfterApply` | bool | true | |

CLI-Aufruf: `seedpick [optionen] [verzeichnis]` über `QCommandLineParser` + `KAboutData`.

## 7. Architektur

```
seedpick/
├── src/
│   ├── core/                 # libseedpickcore – nur Qt6::Core, keine GUI
│   │   ├── imagescanner.*    # Verzeichnis lesen, Filter, Ergebnisordner ignorieren
│   │   ├── event.*           # Event-Typen + JSON (de)serialisierung
│   │   ├── session.*         # Event-Log, abgeleiteter Zustand, Undo
│   │   ├── sessionstore.*    # Laden/Speichern (QSaveFile), Schema-Migration
│   │   ├── btmodel.*         # Bradley-Terry MAP-Fit + Unsicherheit
│   │   ├── pairscheduler.*   # Paarauswahl, Abbruchkriterium
│   │   ├── sidecars.*        # Begleitdateien finden
│   │   ├── applyplan.*       # Plan: Quelle → Ziel, Kollisionen auflösen
│   │   └── filemover.*       # Interface + LocalFileMover
│   ├── app/                  # Qt Quick / Kirigami
│   │   ├── main.cpp
│   │   ├── controller.*      # QML_ELEMENT, bindet Core an QML
│   │   ├── imagecache.*      # Vorladen der nächsten Paare (QQuickImageProvider / async)
│   │   └── qml/              # Main.qml, ScreeningPage, DuelPage, SummaryPage, SettingsPage
│   └── seedpick.kcfg
├── tests/
│   ├── tst_btmodel.cpp
│   ├── tst_pairscheduler.cpp
│   ├── tst_session.cpp       # Undo, Replay, Schema
│   ├── tst_applyplan.cpp     # Kollisionen, Begleitdateien (QTemporaryDir)
│   └── sim/                  # Simulations-Harness (§3.4), eigenes Executable
├── packaging/
│   ├── appimage/             # Build-Skript, AppRun-Anpassungen
│   └── gentoo/media-gfx/seedpick/
│       ├── seedpick-9999.ebuild
│       └── metadata.xml
├── data/
│   ├── io.github.USER.seedpick.desktop
│   ├── io.github.USER.seedpick.metainfo.xml
│   └── icons/
├── po/                       # i18n (ki18n), später
├── .github/workflows/        # CI + Release
├── CMakeLists.txt
├── CMakePresets.json
├── KONZEPT.md
├── LICENSES/                 # REUSE
└── README.md
```

Prinzipien:
- **Core ist ohne GUI vollständig testbar.** Die gesamte Logik (inkl. Simulation) läuft headless.
- QML enthält keine Logik, nur Darstellung und Tastenbindung an `Controller`-Slots.
- Bilder werden mit `sourceSize` passend zur Anzeige geladen, das nächste Paar wird im Hintergrund vorgeladen. Ziel: Tastendruck → nächstes Paar in < 50 ms.
- Code, Bezeichner und Kommentare auf Englisch, UI-Strings über `i18n()` (Englisch als Quelle, Deutsch als erste Übersetzung).

## 8. Build & Abhängigkeiten

- CMake ≥ 3.22, **ECM** (extra-cmake-modules), Ninja
- Qt 6 (Core, Gui, Quick, QuickControls2, Test, Concurrent)
- KF6: CoreAddons, I18n, Config, Kirigami; optional KIO; Laufzeit: kimageformats, qqc2-desktop-style
- Mindestversionen beim Gerüstbau an den aktuellen Gentoo-Stand anpassen und konservativ wählen (AppImage-Basis!).

Gentoo-Pakete (Orientierung): `dev-qt/qtbase`, `dev-qt/qtdeclarative`, `kde-frameworks/extra-cmake-modules`, `kde-frameworks/{kcoreaddons,ki18n,kconfig,kirigami,kio,kimageformats}`, `kde-frameworks/qqc2-desktop-style`.

`CMakePresets.json` mit Presets `dev` (Debug, Ninja, `CMAKE_EXPORT_COMPILE_COMMANDS=ON`, ASan/UBSan) und `release`. CLion erkennt Presets direkt.

Version: einzige Quelle ist `project(seedpick VERSION x.y.z)`. Sie wird über `KAboutData` und eine generierte `version.h` verteilt. Git-Builds hängen `-git+<shorthash>` an (ECM `ecm_setup_version` + `git describe`, falls `.git` vorhanden).

## 9. Versionierung, Releases, Ebuild, AppImage

### Versionen
- SemVer, Git-Tags `vX.Y.Z`. `main` ist immer baubar.
- `CHANGELOG.md` (Keep a Changelog).
- Release-Ablauf (Skript `packaging/release.sh`): Version in CMake setzen → Changelog → Commit → Tag → Push. Die CI erledigt den Rest.

### Gentoo
- `packaging/gentoo/media-gfx/seedpick/seedpick-9999.ebuild`: EAPI 8, `inherit ecm git-r3`, baut `main`.
- Versionierte Ebuilds `seedpick-X.Y.Z.ebuild` erzeugt das Release-Skript aus einer Vorlage. Sie nutzen `SRC_URI` auf das Tag-Archiv (`…/archive/refs/tags/vX.Y.Z.tar.gz -> ${P}.tar.gz`).
- Das Verzeichnis `packaging/gentoo` ist als lokales Overlay einbindbar (`metadata/layout.conf` + `profiles/repo_name`) oder wird später in ein eigenes Overlay-Repo gespiegelt.
- Tests im Ebuild über `IUSE="test"` und `ecm_src_test`.

### AppImage
- Gebaut in der CI bei jedem Tag (Release-Asset) und optional nightly von `main` („continuous“-Release).
- Werkzeuge: `linuxdeploy` + `linuxdeploy-plugin-qt` (mit `QML_SOURCES_PATHS`, `EXTRA_QT_PLUGINS=imageformats`, Kirigami-QML-Module und kimageformats-Plugins mitnehmen). Alternative, falls das hakt: **KDE Craft**, das KDE selbst für AppImages nutzt.
- Basis: Ein Container mit ausreichend neuem Qt6/KF6. Abwägung: Je älter die glibc des Containers, desto mehr Systeme laufen. Ziel ist die älteste Basis, auf der die benötigten KF6-Versionen verfügbar sind (oder KF6 in der CI selbst bauen bzw. über Craft beziehen).
- Pflicht: `.desktop`, Icon, AppStream-Metainfo (wird in der CI mit `appstreamcli validate` geprüft).
- Smoke-Test in der CI: `./Seedpick-*.AppImage --version` und ein Headless-Lauf mit `QT_QPA_PLATFORM=offscreen`.

### CI (GitHub Actions; Codeberg/Forgejo analog)
- `ci.yml`: Build + Tests + Simulation (kurz) bei jedem Push/PR, plus `reuse lint`, clang-format-Check.
- `release.yml`: bei Tag `v*` AppImage bauen, Quell-Tarball + AppImage + SHA256SUMS als Release-Assets.

### Lizenz
GPL-2.0-or-later für den Code, CC0-1.0 für Metadaten, REUSE-konform (SPDX-Header in jeder Datei). Das passt zur KDE-Lizenzpolitik, falls das Projekt später zu KDE wandern soll.

## 10. Meilensteine

| # | Inhalt | Fertig, wenn … |
|---|---|---|
| M0 | Gerüst: CMake/ECM, Presets, leeres Kirigami-Fenster, Core-Lib, ein Dummy-Test, REUSE, README | `cmake --preset dev && cmake --build --preset dev && ctest` grün, Fenster öffnet sich |
| M1 | Core: Scanner, Events, Session mit Undo/Replay, SessionStore | Unit-Tests grün |
| M2 | BT-Modell + PairScheduler + Simulations-Harness | Akzeptanzkriterium §3.4 erfüllt, Tabelle der Ergebnisse im README/`docs/` |
| M3 | GUI Phase 1 + 2 mit Tastatursteuerung, Vorladen | Bedienbar mit echtem Ordner, flüssig |
| M4 | Phase 3 + ApplyPlan + LocalFileMover + Apply-Log | Tests mit `QTemporaryDir` inkl. Kollisionen/Begleitdateien grün |
| M5 | KConfigXT, Einstellungsseite, CLI-Optionen | alle Werte aus §6 wirksam |
| M6 | Gentoo: 9999-Ebuild + Vorlage + Release-Skript | `ebuild … install` lokal erfolgreich |
| M7 | CI + AppImage + Release-Workflow | Tag `v0.1.0` erzeugt lauffähiges AppImage |
| M8 | Feinschliff: synchroner Zoom, Metadaten/Prompt-Anzeige (PNG-tEXt, ComfyUI/A1111), i18n de, Trash, `--undo-apply` | nach Bedarf |

## 11. Offene Fragen (vor/während M0 klären)

- Endgültiger Name und App-ID (`io.github.<user>.seedpick` o. ä.), Hosting (GitHub vs. Codeberg).
- Soll n auch als Prozentsatz angebbar sein („beste 10 %“)?
- Rekursive Verzeichnisse / mehrere Prompts in einem Ordner gruppieren (z. B. nach Prompt aus den Metadaten)?
- Grid-Modus für die Sichtung (mehrere Thumbnails, schnelleres Aussortieren) zusätzlich zur Einzelansicht?
