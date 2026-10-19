# Stereo Plugin Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Consegnare la prima fase della specifica: plugin mono/stereo realmente compilabile e verificato, con IR WAV, controlli DSP, libreria e GUI.

**Architecture:** Separare caricamento e analisi asincroni dal motore real-time e dalla GUI JUCE. Il routing esplicito descrive anche i quattro percorsi true stereo; le estensioni immersive useranno lo stesso confine fra dati preparati e motore.

**Tech Stack:** C++20, CMake ≥3.22, JUCE 8.0.6 risolto a commit esatto prima dell'integrazione, CTest, GCC/Clang, target VST3 e Standalone.

**Spec:** `docs/superpowers/specs/2026-10-19-convolution-host-design.md`

## Global Constraints

- C++20, JUCE e CMake; dipendenze a revisioni esatte, TLS e verifiche d'integrità conservate.
- Nessun I/O, allocazione, distruzione onerosa o attesa nel callback audio.
- IR fino a 30 secondi a 96 kHz; limite dei motori 512 MiB inclusa sostituzione; crossfade 50 ms.
- GUI iniziale 1180×760, minimo 960×640; EQ 10 bande ±12 dB; predelay 0…500 ms; WIDTH 0…200%.
- La specifica resta vincolante per intervalli, default e catena DSP; nessun renderer Atmos o gestione metadati oggetti.
- Usare il checkout esistente. Non creare worktree. Conservare licenza e file dell'utente.
- Questa prima consegna non abilita bus oltre stereo: multicanale, AmbiX, CLAP, AU e AAX restano requisiti delle fasi successive, con piani dedicati e prove sulle piattaforme necessarie.

## Review Focus

- Cambio sample rate durante caricamento: nessun motore preparato per la configurazione precedente può attivarsi (Task 3).
- WAV di quattro canali senza mapping: rifiutare l'interpretazione automatica true stereo (Task 1–2).
- Stato salvato con file spostato: nessuna sostituzione con un WAV omonimo diverso (Task 5).
- Automazione predelay e IR lunghi: memoria limitata e transizioni finite senza discontinuità (Task 3–4).
- Editor chiuso durante scansione/caricamento: nessuna callback su componenti distrutti (Task 5–6).

## Task 1: Routing e base di build verificabile

**Files:** `CMakeLists.txt`, `cmake/Dependencies.cmake`, `.gitignore`, `src/audio/RoutingModel.h`, `src/audio/RoutingModel.cpp`, `tests/RoutingTests.cpp`, `tests/TestMain.cpp`.

**Interfaces:** `Route { int input, output, irChannel; float gain; }`; `RoutingModel::validate(int inputs, int outputs, int irChannels) const -> juce::Result`; modello immutabile dopo validazione.

- [ ] Verificare risorse e toolchain; installare CMake e dipendenze Linux JUCE mancanti con verifica attiva. Risolvere il tag JUCE a SHA, salvarlo in `Dependencies.cmake` e verificare il checkout.
- [ ] Configurare una libreria core e runner CTest senza dispositivo audio. Scrivere test per mono, dual mono, stereo e quattro percorsi espliciti; rifiutare indici fuori intervallo, gain non finiti e IR 4ch senza mapping.
- [ ] Eseguire il runner con implementazione iniziale che rifiuta i modelli; osservare i fallimenti dei casi validi, quindi implementare la validazione e rieseguire.
- [ ] Verificare con `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`, `cmake --build build --parallel 2`, `ctest --test-dir build --output-on-failure`; commit del task solo con test verdi.

## Task 2: Decodifica WAV e analisi dell'IR

**Files:** `src/ir/PreparedIr.h`, `src/ir/IrDecoder.h`, `src/ir/IrDecoder.cpp`, `tests/IrDecoderTests.cpp`, `tests/WavFixtures.h`.

**Interfaces:** `IrRequest` contiene file, sample rate target, trim, fade e normalizzazione; `PreparedIr` contiene campioni, metadati, hash del contenuto, waveform e decadimento; `IrDecoder::decode(const IrRequest&) -> DecodeResult` con dati oppure errore.

- [ ] Scrivere fixture WAV PCM/float sintetiche e test: impulso noto, ricampionamento 44,1→48 kHz, trim/fade, normalizzazione comune, file vuoto/corrotto, campioni non finiti, oltre 30 s e stima oltre budget. Verificare decadimento monotono e finito anche per silenzio.
- [ ] Eseguire i test con decoder che restituisce errore di funzionalità assente; verificare che i casi validi falliscano.
- [ ] Implementare decodifica e preparazione sincrone destinate esclusivamente al worker. Validare header e dimensioni prima di allocare; stimare anche FFT e doppio motore. Non attribuire significato automatico ai canali di un WAV multicanale.
- [ ] Eseguire tutta la suite CTest, registrare i risultati e creare commit.

## Task 3: Motore di convoluzione e sostituzione asincrona

**Files:** `src/audio/ConvolutionEngine.h`, `src/audio/ConvolutionEngine.cpp`, `src/ir/IrLoader.h`, `src/ir/IrLoader.cpp`, `tests/ConvolutionTests.cpp`, `tests/IrLoaderTests.cpp`.

**Interfaces:** `ConvolutionEngine::prepare(double sampleRate, int maxBlockSize, int channels)`, `process(juce::AudioBuffer<float>&)`, `reset()`, `latencySamples() const -> int`; `IrLoader::request(IrRequest, RoutingModel) -> uint64_t`. Loader produce motori preparati identificati da richiesta e generazione della configurazione.

- [ ] Scrivere test di convoluzione contro riferimento diretto con IR asimmetrici, tolleranza assoluta 1e-4, mono/stereo/true stereo, sample rate 44,1/48/96 kHz e blocchi 64/256/1024 e variabili. Un percorso incrociato errato deve far fallire il test.
- [ ] Scrivere test per ultima richiesta vincente, cambio sample rate durante caricamento, conservazione del motore dopo errore e crossfade 50 ms. Fallire esplicitamente con motore iniziale pass-through.
- [ ] Implementare percorsi JUCE DSP mono preallocati, somma della matrice, proprietà esclusiva dei motori e trasferimento tramite code bounded. Il worker raccoglie i motori ritirati; nessuna distruzione nel callback. Richieste obsolete sono scartate sul worker. Gestire preparazione quando il processing è sospeso e code piene senza bloccare.
- [ ] Verificare latenza, coda e reset, tracciare allocazioni del thread audio nel runner; eseguire CTest e commit.

## Task 4: Catena audio e parametri automatizzabili

**Files:** `src/audio/ReverbProcessor.h`, `src/audio/ReverbProcessor.cpp`, `src/plugin/Parameters.h`, `src/plugin/Parameters.cpp`, `tests/ReverbTests.cpp`.

**Interfaces:** `ReverbProcessor::prepare(double, int, int)`, `process(juce::AudioBuffer<float>&, const ParameterSnapshot&)`, `reset()`; `createParameterLayout() -> juce::AudioProcessorValueTreeState::ParameterLayout`. Identificatori stabili `inputGain`, `outputGain`, `wetGain`, `mix`, `predelay`, `width`, `eq0`…`eq9`, `highpass`, `lowpass`, `filtersEnabled`, `bypass`.

- [ ] Scrivere test per dry senza IR, mix 0/100%, ritardo noto, width 0/100/200%, gain e curva EQ a ogni centro banda. Verificare filtri vicino a Nyquist, bypass allineato e campioni finiti durante automazione.
- [ ] Eseguire i test con catena pass-through e confermare i fallimenti attesi.
- [ ] Implementare catena e default della specifica; usare buffer e coefficienti preallocati, smoothing e tap predelay in crossfade. Non allocare coefficienti IIR per aggiornare i filtri dal callback. Conservare i rapporti dei canali.
- [ ] Eseguire CTest, audit del callback e misura CPU/memoria con IR lungo; commit riportando macchina e condizioni di misura.

## Task 5: Processor plugin, libreria e stato persistente

**Files:** `src/plugin/PluginProcessor.h`, `src/plugin/PluginProcessor.cpp`, `src/ir/IrLibrary.h`, `src/ir/IrLibrary.cpp`, `src/state/StateStore.h`, `src/state/StateStore.cpp`, `tests/LibraryTests.cpp`, `tests/StateTests.cpp`, `tests/ProcessorTests.cpp`.

**Interfaces:** processor JUCE con APVTS e supporto bus mono/stereo simmetrici; `IrLibrary::scanAsync()` e snapshot dei risultati; `StateStore` serializza versione 1, parametri, hash, percorso e mapping. Preferenze globali separate tramite directory applicativa JUCE.

- [ ] Scrivere test con directory temporanee per radici multiple, ricerca, preferiti, symlink ciclici e file non leggibili; test stato round-trip e rilocalizzazione con hash, rifiutando omonimi con contenuto diverso.
- [ ] Scrivere test di istanziazione processor, layout rifiutati, audio, coda, latenza e ripristino prima/dopo `prepareToPlay`. Confermare fallimenti prima dell'implementazione.
- [ ] Implementare scansione e caricamento cancellabili; notifiche GUI con lifetime sicuro, senza dipendere dall'esistenza dell'editor. Parser dello stato con versione e limiti; nessun accesso al disco sincrono nel callback di elaborazione.
- [ ] Generare target VST3/Standalone con codici produttore/plugin stabili e documentati. Eseguire build, CTest e commit.

## Task 6: GUI funzionante e consegna della prima fase

**Files:** `src/ui/PluginEditor.h`, `src/ui/PluginEditor.cpp`, `src/ui/Theme.h`, `src/ui/IrDisplay.h`, `src/ui/IrDisplay.cpp`, `src/ui/LibraryPanel.h`, `src/ui/LibraryPanel.cpp`, `src/ui/RoutingPanel.h`, `src/ui/RoutingPanel.cpp`, `tests/EditorTests.cpp`, `README.md`, `AGENTS.md`.

**Interfaces:** editor dipendente dal processor, APVTS attachments, snapshot immutabili per waveform e decadimento. `RoutingPanel` permette selezione esplicita dei quattro percorsi true stereo già supportati dal motore.

- [ ] Scrivere smoke test dell'editor con parametri reali: controlli associati, ridimensionamento minimo, chiusura durante attività asincrona e riapertura senza perdita di stato. Eseguire il runner sotto Xvfb e osservare le funzionalità mancanti.
- [ ] Implementare tema grafite/turchese/ambra, browser con radici e ricerca, waveform/decadimento, trim/fade/normalizzazione, predelay, mix, width, EQ e meter. File chooser asincrono, drag-and-drop, errori con rilocalizzazione; accessibilità e gesture host.
- [ ] Compilare VST3 e Standalone, eseguire CTest e smoke GUI sotto display virtuale. Verificare un caricamento WAV e un rendering audio dal processor reale; documentare separatamente ciò che richiede ancora una DAW fisica.
- [ ] Aggiornare README e AGENTS con comandi effettivamente verificati; riepilogare funzioni reali e limiti. Salvare installazione/avvio nell'ambiente cloud se necessario, verificando i comandi prima di salvarli. Commit della consegna e pubblicazione sul remoto richiesto.

## Fasi successive preservate

Terminata la base stereo, preparare piani separati per: matrice fino a 16 canali e routing LFE, AmbiX ordini 1–3 con test canale per canale, integrazione CLAP fissata a SHA e verificata su Linux, build AU/macOS e AAX con SDK fornito. Queste funzionalità non sono certificate o simulate nella prima fase. Mantenere l'architettura di Route e motori preparati senza limitazioni pubbliche arbitrarie a due canali.

## Revisione del piano

Coperti tutti i requisiti della prima fase della specifica. Le fasi 2–3 restano esplicitamente separate; non sono omissioni della roadmap. I rischi di review sono associati ai test dei task indicati. Esecuzione proposta: inline nella sessione, mantenendo checkpoint e commit per ogni task; nessun subagente richiesto per l'implementazione.
