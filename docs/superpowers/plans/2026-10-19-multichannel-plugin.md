# Multicanale immersivo e AmbiX — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Completare la fase 2 approvata: bus fino a 16 canali, matrice sparsa, layout immersivi e AmbiX ordini 1–3, mantenendo compatibilità con le sessioni stereo.

**Architecture:** Separare descrizione semantica del bus, selezione del routing e matrice preparata. Il processor confronta il layout dell'host con lo stato salvato prima di inviare il lavoro al loader. La GUI modifica una copia del routing, attivata soltanto dopo validazione; il callback usa strutture preparate e di dimensione fissa.

**Tech Stack:** C++20, JUCE 8.0.6, CMake ≥3.22, FFTW3f su Linux/Windows, vDSP su macOS, runner di test esistenti.

**Spec:** `docs/superpowers/specs/2026-10-19-convolution-host-design.md`, fase 2. Il piano continua la specifica già approvata, senza introdurre un renderer Atmos.

## Global Constraints

- Massimo 16 canali e 256 percorsi; IR fino a 30 secondi a 96 kHz, ammissione entro 512 MiB stimati inclusi due motori.
- Nessun I/O, lock, allocazione o distruzione onerosa nel callback audio.
- ACN/SN3D (AmbiX), ordini 1–3: 4, 9 e 16 canali; nessuna conversione automatica di convenzione.
- Immersivo: layout simmetrici 5.1, 7.1, 7.1.2 e 7.1.4; LFE wet inizialmente escluso, dry secondo il mix.
- WIDTH modifica soltanto coppie stereo designate; disabilitato nei layout privi di coppie, sempre disabilitato per AmbiX.
- Crossfade IR di 50 ms; parametri pubblicati e identificatori produttore/plugin esistenti restano stabili.
- Stato versione 1 leggibile; uno stato incompatibile non deve reinterpretare gli indici per un altro layout.
- Usare il checkout isolato esistente, senza creare worktree.

## Review Focus

1. Bus discreto di quattro canali non è automaticamente AmbiX: Task 1 verifica identità diverse e selezione esplicita.
2. Sessione ripristinata su un bus con stesso numero ma diverso significato: Task 2 rifiuta il routing incompatibile conservando il riferimento salvato.
3. Matrice con tutti i percorsi spenti: Task 2 verifica silenzio wet, senza ripristinare il routing automatico.
4. Cambio layout mentre il loader prepara un motore: Task 2 verifica invalidazione delle generazioni e assenza di attivazione del motore precedente.
5. GUI multicanale al minimo di 960×640: Task 3 verifica scrolling, etichette e mantenimento delle modifiche non ancora applicate.

---

## Task 1: Descrizione del bus e politica di routing

**Files:** creare `src/audio/BusLayout.h`, `src/audio/BusLayout.cpp`, `tests/BusLayoutTests.cpp`; modificare `src/audio/RoutingModel.h`, `src/audio/RoutingModel.cpp`, `CMakeLists.txt`.

**Interfaces:**

- `enum class BusInterpretation { followHost, discrete, ambix };`
- `struct StereoPair { int left, right; };`
- `struct BusLayout` conserva `juce::AudioChannelSet channels`, interpretazione, etichette, indice LFE e coppie designate.
- `static std::optional<BusLayout> BusLayout::fromHost(const juce::AudioChannelSet&, BusInterpretation)` costruisce solo layout supportati. `juce::String identity() const` distingue tipo, canali e convenzione con identificatori stabili definiti in questo file.
- `RoutingModel` conserva routes, `bool automatic = true`, `bool lfeWetEnabled = false`, `juce::String sourceLayout`; `static RoutingModel forLayout(const BusLayout&, int irChannels, bool lfeWetEnabled)` costruisce l'associazione automatica. La variante `diagonal` rimane disponibile ai test core esistenti.
- Il costruttore predefinito di `RoutingModel` seleziona il modo automatico; `RoutingModel(std::vector<Route>)` seleziona il modo matrice anche con vettore vuoto. Conservare la sintassi esistente dei percorsi espliciti tramite questo costruttore; non ignorare percorsi presenti perché un nuovo flag ha un default diverso.

- [x] Scrivere `host_layout_semantics_are_explicit`: ammettere mono, stereo, discreti 1–16, i quattro layout immersivi e AmbiX 1–3; rifiutare bus disabilitati e >16 canali. Discreto 4 e AmbiX 1 hanno identità diverse; solo una selezione esplicita AmbiX su bus discreti 4/9/16 consente l'interpretazione AmbiX. Un bus surround nominale non può essere reinterpretato come AmbiX.
- [x] Scrivere `automatic_routes_preserve_channels_and_exclude_lfe`: IR mono o con numero canali uguale al bus generano solo diagonale; nessun percorso da/verso LFE per default. Con consenso esplicito LFE, includere il percorso diagonale. IR con conteggio incompatibile richiede matrice esplicita.
- [x] Eseguire `cmake --build build --parallel 2` e `build/ch_tests`; osservare il fallimento dei nuovi casi prima dell'implementazione.
- [x] Implementare le interfacce sopra, usando `AudioChannelSet` e i suoi ChannelType per etichette/LFE; non dedurre il layout dal solo conteggio. Per percorsi LFE espliciti richiedere `lfeWetEnabled` nella validazione semantica.
- [x] Ripetere i test core; confermare i nuovi casi verdi insieme ai precedenti. Commit `feat: describe immersive and AmbiX bus layouts explicitly`.

## Task 2: Processor multicanale, DSP e stato compatibile

**Files:** modificare `src/plugin/PluginProcessor.h/.cpp`, `src/audio/ReverbProcessor.h/.cpp`, `src/ir/IrLoader.h/.cpp`, `src/state/StateStore.h/.cpp`, `tests/ProcessorTests.cpp`, `tests/ConvolutionTests.cpp`, `tests/ReverbTests.cpp`, `tests/StateTests.cpp`, `tests/IrLoaderTests.cpp`.

**Interfaces:**

- `SavedState` aggiunge interpretazione del bus e routing di Task 1. Lo stato codificato passa a versione 2; versione 1 migra `automatic = routes.empty()`, mantenendo percorsi e parametri senza cambi numerici. Le vecchie matrici esplicite sono riferite a mono/stereo: non diventano matrici immersive automaticamente.
- `PluginProcessor::busLayout() const` restituisce descrizione off-thread; `void setBusInterpretation(BusInterpretation)` e `void setLfeWetEnabled(bool)` convalidano e risottomettono sul thread di controllo. Nessuna lettura di etichette nel callback.
- `ReverbProcessor::prepare(double, int, int)` resta compatibile. Aggiungere `void configureWidthPairs(std::span<const StereoPair>)` solo con audio sospeso; i dati diventano array fisso massimo 8 coppie con conteggio.
- Il loader riceve sempre una matrice risolta esplicita dal processor; `automatic` e matrice vuota esplicita non si confondono. Il motore gestisce una matrice esplicitamente vuota producendo wet silenzioso.

- [x] Scrivere test di layout del processor per ogni bus ammesso, ingressi/uscite asimmetrici, canali extra e bus disabilitati; aggiornare la precedente aspettativa di rifiuto 5.1.
- [x] Scrivere test audio con impulsi distinti per 4/6/8/9/10/12/16 canali, mappature sparse incrociate e confronto diretto con tolleranza assoluta `2e-4`. Coprire 44,1/48/96 kHz e blocchi 64/256/1024, incluse lunghezze variabili.
- [x] Scrivere `lfe_is_explicit_and_dry_remains_mix_dependent`: con mix 100 wet LFE silenzioso per default, mix 0 dry LFE invariato; abilitazione esplicita attiva il percorso atteso. Nessun crossfeed LFE verso altri canali per default.
- [x] Scrivere `ambix_preserves_acn_without_width_processing`: ordini 1–3, impulso su ciascun ingresso, nessuno scambio/normalizzazione per canale; WIDTH 0/100/200 non cambia AmbiX. Verificare una coppia stereo designata senza modificare gli altri canali e rifiutare coppie sovrapposte o fuori indice.
- [x] Scrivere test stato versione 1→2 e round-trip versione 2; rifiutare versioni future, interpretazioni sconosciute e layout incompatibili a uguale conteggio. Matrice esplicita vuota deve restare vuota e produrre wet silenzioso dopo riapertura.
- [x] Scrivere regressione cambio layout durante caricamento/crossfade, reset e restore prima/dopo prepare: motori di vecchie generazioni non si attivano; nessuna allocazione nel callback a 16 canali. Verificare rifiuto di matrice oltre budget e conservazione del motore compatibile precedente.
- [x] Eseguire i runner e osservare RED nei casi nuovi prima del codice.
- [x] Implementare negoziazione e validazione semantica nel processor, migrazione dello stato, copia delle coppie width nel prepare e routing risolto off-thread. Alla riconfigurazione invalidare i motori precedenti; una matrice incompatibile espone errore e conserva lo stato salvato per il recupero.
- [x] Eseguire build Release e CTest sotto Xvfb; misurare il caso 16 canali con IR corto e lungo ammesso dal budget, riportando tempi e stima senza promettere realtime universale. Commit `feat: process multichannel immersive and AmbiX buses`.

## Task 3: Editor di matrice e completamento GUI

**Files:** modificare `src/ui/RoutingPanel.h/.cpp`, `src/ui/PluginEditor.h/.cpp`, `src/ui/IrDisplay.h/.cpp`, `tests/EditorTests.cpp`; creare `src/state/PresetFile.h/.cpp`, `tests/PresetFileTests.cpp`; aggiornare README, AGENTS e stato di sviluppo.

**Interfaces:**

- `RoutingPanel` mantiene il flusso true stereo esistente e aggiunge editor sparso con righe aggiungibili/rimuovibili, ingresso/uscita etichettati, canale IR, gain e polarità. `Apply routing` aggiorna il processor solo dopo validazione completa; massimo 256 righe in `Viewport`.
- `PresetFile::save(const juce::File&, const juce::MemoryBlock&) -> juce::Result` usa temporaneo, controlla scrittura e flush prima della sostituzione; `PresetFile::load(const juce::File&, juce::MemoryBlock&) -> juce::Result` limita a 4 MiB e convalida lo stato prima di consegnarlo al processor.
- L'editor conserva l'errore dell'ultima azione fino al successivo tentativo o chiusura esplicita; separa IR selezionato da IR ancora attivo. Disabilita il trim della preview precedente durante caricamento/errore del nuovo file, con indicazione testuale.

- [x] Scrivere test GUI: matrice 16 canali e 256 righe scrollabili a 960×640, aggiunta/rimozione, guadagni negativi, assegnazioni sparse conservate, refresh dopo restore senza cancellare edit non applicati per un semplice timer tick.
- [x] Scrivere test dei controlli layout/LFE e WIDTH: etichette coerenti con l'host, selezione AmbiX solo nei casi ammessi, WIDTH disabilitato senza coppie o in AmbiX. Test per tutti gli ingressi/uscite della matrice raggiungibili via GUI.
- [x] Scrivere regressioni per i due problemi minori della fase 1: errore preset visibile dopo più tick del timer e preview precedente non modificabile come se fosse il file appena selezionato.
- [x] Scrivere test preset con destinazione directory, parent non scrivibile, dati invalidi/oltre limite e successo round-trip; fallimento non modifica il preset valido precedente né lo stato del processor.
- [x] Eseguire i test e osservare i fallimenti; implementare i componenti sopra e mantenere waveform/decadimento sul tempo originale del file.
- [x] Eseguire tutta la suite e smoke reale Standalone; generare e ispezionare screenshot mono/stereo e 7.1.4/AmbiX. Aggiornare documentazione con le sole capacità verificate. Commit `feat: add immersive routing editor and reliable preset feedback`.

## Verifica del piano

La fase 2 della specifica è coperta dai tre task, inclusi LFE, AmbiX, width,
compatibilità dello stato e GUI. La generazione dei formati e gli artefatti
`BUILD` appartengono al piano successivo. Restano invariati i confini approvati:
nessun renderer Atmos, metadati oggetto, decoder binaurale o libreria IR inclusa.

**Stato:** implementato e verificato su Linux; revisione finale congiunta con il piano dei formati.
