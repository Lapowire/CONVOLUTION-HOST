# Parametric EQ and Damping Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rendere EQ e decadimento modificabili tramite curve interattive, con A/B e undo, conservando sessioni, routing e sicurezza realtime.

**Architecture:** EQ parametrico sul percorso wet in tempo reale; damping spettrale dell'IR sul worker, prima dell'analisi e della preparazione del motore. Modelli a dimensione fissa separano DSP, parametri, stato e interfaccia; l'editor condivide le formule della risposta con il DSP.

**Tech Stack:** C++20, JUCE 8.0.6, CMake ≥3.22, Python ≥3.12 per test e consegna; dipendenze fissate esistenti.

**Spec:** `docs/superpowers/specs/2026-10-20-parametric-eq-damping-design.md` — approvata; questo piano attende revisione.

## Global Constraints

- Usare il checkout esistente, `.build/` per gli intermedi e `BUILD/` per le consegne; parallelismo di build 2.
- Dieci bande EQ, 20–20000 Hz, Q 0,1–18, gain -12/+12 dB; bell, low shelf, high shelf, notch; default identici al vecchio EQ, Q 1,4.
- Sei nodi damping, 0–60 dB/s; protezione 0–200 ms, default 20 ms; curva neutra evita la trasformazione.
- Callback senza I/O, allocazioni, distruzioni onerose, mutex o attese. Conservare backend FFTW_NO_BUFFERING statico e vDSP su Apple.
- Preservare tutti gli identificatori, indici, range e significati dei parametri già pubblicati; aggiungere parametri in coda.
- Damping uguale su tutti i canali, dopo trim/fade/normalizzazione comune; nessuna compensazione automatica del livello rimosso.
- Budget motori 512 MiB; buffer temporanei limitati separatamente; motore valido conservato in caso di errore o richieste superate.
- Damping asincrono, non sample-accurate; applicare prima dell'esportazione per render ripetibili. Non allungare né sintetizzare code.
- A/B persistente; undo locale all'istanza e non serializzato; automazione DAW esclusa dalla cronologia locale.
- Tutti i formati e layout esistenti rimangono supportati; firma e collaudo DAW restano verifiche distinte.

## Review Focus

- Ripristinare un vecchio preset sopra un'istanza già modificata deve azzerare i nuovi controlli ai default: Task 3.
- Automazione rapida, anche con editor chiuso e cambi del sample rate, non deve produrre instabilità, callback allocanti o attivazioni obsolete: Task 1 e Task 3.
- Modifica damping seguita da IR mancante, errore o cambio bus deve mantenere il motore valido e indicare chiaramente richiesta e stato attivo: Task 3 e Task 5.
- Nodi damping coincidenti, invertiti o non finiti nello stato non devono produrre divisioni per zero né sostituire lo stato valido: Task 2 e Task 3.
- Automazione DAW durante una gesture o dopo undo non deve essere riscritta accidentalmente da un pannello aperto o da uno snapshot A/B obsoleto: Task 4 e Task 5.

## Struttura dei file

- `src/audio/ParametricEq.h/.cpp`: tipi delle bande, coefficienti, risposta analitica e filtri realtime a memoria fissa.
- `src/ir/SpectralDamping.h/.cpp`: curva damping validabile e trasformazione STFT sul worker.
- `src/state/ToneState.h/.cpp`: configurazione EQ/damping, memorie A/B e validazione indipendente dalla GUI.
- `src/plugin/ToneController.h/.cpp`: gesture, notifiche host, A/B e cronologia locale del processor.
- `src/ui/CurveEditor.h/.cpp`: disegno e interazione dei nodi, senza gestione dei file o preparazione DSP.
- `src/ui/TonePanel.h/.cpp`: selezione banda, controlli numerici, toolbar, stato richiesto/attivo.
- File esistenti modificati solo per integrare i moduli; test aggiunti ai runner attuali, senza nuove dipendenze.

## Task 1: EQ parametrico stabile e compatibile

**Files:** creare `src/audio/ParametricEq.h/.cpp`, `tests/ParametricEqTests.cpp`; modificare `src/audio/ReverbProcessor.h/.cpp`, `CMakeLists.txt`.

**Interfaces:**
- `enum class EqType { bell, lowShelf, highShelf, notch };`
- `EqBandShape { float frequencyHz, q; EqType type; bool enabled; }`; default delle dieci frequenze esistenti, Q 1,4, bell, enabled true.
- `EqSettings { std::array<float, 10> gainDb; std::array<EqBandShape, 10> bands; bool bypass; }`.
- `ParametricEq::prepare(double sampleRate, int channels)`, `reset() noexcept`, `setTargets(const EqSettings&) noexcept`, `process(juce::AudioBuffer<float>&) noexcept`.
- `static double ParametricEq::magnitude(const EqSettings&, double sampleRate, double frequencyHz) noexcept`; include bypass e bande disattivate, esclude HP/LP esterni.
- `ParameterSnapshot::eq` resta la sorgente dei dieci gain esistenti; aggiungere `eqShape` e `eqBypass` e costruire `EqSettings` nel processor DSP.

- [ ] Scrivere `parametric_eq_matches_legacy_and_analytic_response`: vecchi default e gain ±12 dB conservano la risposta bell Q 1,4; risposta sinusoidale entro 0,15 dB dalla formula indipendente a 44,1/48/96 kHz; shelf ai due estremi e notch alla frequenza centrale.
- [ ] Scrivere `parametric_eq_extreme_automation_is_finite_and_allocation_free`: tutti i tipi, Q 0,1/18, frequenze 20/20000, sample rate 8–96 kHz, salti ripetuti, bypass, blocchi 1/37/64/256/1024, 16 canali. Campioni finiti, nessun crossfeed, nessuna allocazione intercettabile, nessun impulso spurio con ingresso silenzioso.
- [ ] Eseguire build e `.build/ch_tests`; registrare RED dei nuovi test prima dell'implementazione. Comandi assunti preceduti da `source /workspace/toolchain/activate.sh`.
- [ ] Implementare coefficienti biquad normalizzati e risposta condivisa. Smussare frequenza e Q in dominio logaritmico e gain in dB per 20 ms; ricalcolare coefficienti da parametri validi, evitando interpolazione arbitraria dei poli. Per cambi tipo/bypass usare due banchi preallocati con crossfade 20 ms e coalescenza dell'ultimo target; non azzerare il banco udibile.
- [ ] Integrare l'EQ wet prima degli HP/LP esistenti, separando questi ultimi dai dieci vecchi filtri; `reset` pulisce entrambi i banchi. Fuori Nyquist usare identità e mantenere il valore salvato.
- [ ] Eseguire `.build/ch_tests` e `DISPLAY=:99 .build/ch_plugin_tests`; nuovi e vecchi casi verdi. Commit `feat: add stable realtime parametric wet equalizer`.

## Task 2: Damping spettrale dell'impulso

**Files:** creare `src/ir/SpectralDamping.h/.cpp`, `tests/SpectralDampingTests.cpp`; modificare `src/ir/PreparedIr.h`, `src/ir/IrDecoder.h/.cpp`, `CMakeLists.txt`.

**Interfaces:**
- `DampingNode { float frequencyHz, attenuationDbPerSecond; }`.
- `DampingSettings { std::array<DampingNode, 6> nodes; float protectMs = 20; bool bypass = false; }`; frequenze iniziali 63/250/1000/4000/8000/16000 Hz, attenuazioni zero.
- `bool DampingSettings::isValid() const noexcept`, `bool isNeutral() const noexcept`, `float attenuationAt(double frequencyHz) const noexcept`.
- `static juce::Result SpectralDamping::apply(juce::AudioBuffer<float>&, double sampleRate, const DampingSettings&, const std::function<bool()>& cancelled)`; cancellazione e validazione restituiscono errore senza pubblicare l'IR parziale.
- `static std::size_t SpectralDamping::scratchBytes(double sampleRate) noexcept`; limite scratch 16 MiB, indipendente dalla durata e dal numero di canali; processo sequenziale per canale. `IrRequest` e `PreparedIr` conservano rispettivamente damping richiesto e applicato.
- `DecodeResult IrDecoder::decode(const IrRequest&, const std::function<bool()>& cancelled = {}) const` conserva le chiamate precedenti e propaga la cancellazione ai child true stereo e al damping. Il loader fornisce il predicate del job.

- [ ] Scrivere `neutral_damping_preserves_every_sample`: bypass o tutti zero restituiscono campioni bit-identici, nessuna trasformazione FFT; controllo delle frequenze 20–20000 e nodi strettamente crescenti, Q assente.
- [ ] Scrivere `damping_changes_decay_by_frequency`: due bande sintetiche separate, 0 e 12 dB/s, misure in finestre dopo 0,25/0,75 s; differenza coerente con la pendenza entro 1,5 dB; usare segnali e finestre lontani dai bordi STFT.
- [ ] Scrivere `damping_reconstructs_edges_and_preserves_channel_relations`: impulsi a inizio/fine, IR più corto della finestra, protezione 0/200 ms, 44,1/48/96 kHz, 16 canali proporzionali. Nessuna variazione di lunghezza/ordine; errore dei rapporti <2e-4; nessun campione non finito. Damping piatto confrontato anche con inviluppo temporale nominale, entro tolleranza ai bordi dichiarata.
- [ ] Scrivere `damping_rejects_invalid_or_cancelled_jobs`: NaN/Inf, rate illegale, nodi uguali/invertiti, budget scratch superato e cancellazione a metà. Eseguire RED.
- [ ] Implementare STFT complessa con finestra sqrt-Hann, hop N/4 e normalizzazione overlap-add; FFT power-of-two più vicina a 2048*sampleRate/48000, limitata 512–8192. Padding centrato e compensazione dei bordi; fase preservata. Riutilizzare scratch per ogni canale e controllare cancellazione a ogni frame.
- [ ] Integrare dopo la normalizzazione e prima delle analisi waveform/Schroeder. Usare tempo dal trim, nessun gain comune successivo. Nel percorso quattro file mono applicare solo dopo assemblaggio, evitando damping ripetuto dei child.
- [ ] Eseguire core, regressioni decoder e misura scratch; commit `feat: shape impulse decay with offline spectral damping`.

## Task 3: Parametri, worker e migrazione dello stato

**Files:** creare `src/state/ToneState.h/.cpp`, `tests/ToneStateTests.cpp`; modificare `src/plugin/Parameters.h/.cpp`, `src/plugin/PluginProcessor.h/.cpp`, `src/ir/IrLoader.h/.cpp`, `src/state/StateStore.h/.cpp`, `tests/ProcessorTests.cpp`, `tests/IrLoaderTests.cpp`, `tests/StateTests.cpp`, `CMakeLists.txt`.

**Interfaces:**
- `ToneState { EqSettings eq; DampingSettings damping; }`, `ToneMemories { std::array<ToneState, 2> slots; int active = 0; }`; validazione e conversione `juce::ValueTree` in `ToneState.h/.cpp`.
- Aggiungere in coda `peq{0..9}Freq/Q/Type/Enabled`, `eqBypass`, `damp{0..5}Freq/Rate`, `dampProtect`, `dampBypass`; gain continua a usare `eq0`–`eq9`. Memorie A/B non diventano parametri duplicati.
- `ParameterRefs::read()` restituisce anche shape EQ e bypass; `DampingSettings ParameterRefs::readDamping() const noexcept` usa soli atomici.
- Stato versione 3: `SavedState` aggiunge `ToneMemories`; lettura versioni 1/2 con default aggiuntivi espliciti. Parametri assenti ripristinati al default prima di `replaceState`; rifiutare enum fuori range, duplicati e valori non finiti.
- `IrLoader::Status` espone damping richiesto/preparato con identificatori; distinguere `activeRequest()` dalla preparazione pronta. Passare predicate di cancellazione dal job fino alla trasformazione.

- [ ] Scrivere `legacy_state_restores_new_defaults_on_modified_instance`: fixture versioni 1/2 sopra EQ/damping già modificati, indici dei vecchi parametri identici, risposta bell originale e damping neutro.
- [ ] Scrivere `tone_state_v3_roundtrip_and_invalid_restore_are_atomic`: due slot A/B distinti, stato attivo coerente coi parametri, versioni future/enum/nodi/NaN rifiutati senza cambiare stato corrente; dimensione limite preset esistente rispettata.
- [ ] Scrivere `damping_automation_with_closed_editor_coalesces_latest`: modifiche continue, IR mancante, restore durante preparazione, cambio bus/sample rate, bypass e zero curve; motori obsoleti mai attivati, precedente motore valido conservato, callback senza allocazioni.
- [ ] Eseguire RED, poi integrare parametri aggiunti in coda, default/migrazione e worker. Debounce trailing 150 ms usando il timer processor esistente; `parameterChanged` imposta soltanto atomici. `servicePendingChanges()` applica l'ultima configurazione e funziona con editor chiuso.
- [ ] Estendere cancellazione ai job superati e controllo separato scratch/motori, senza spostare distruzioni sul callback. Salvare stato richiesto anche quando preparazione fallisce.
- [ ] Eseguire i due runner sotto Xvfb e verificare fixture, routing e restore pre/post prepare; commit `feat: automate and persist parametric tone shaping compatibly`.

## Task 4: Gesture, A/B e undo locali

**Files:** creare `src/plugin/ToneController.h/.cpp`, `tests/ToneControllerTests.cpp`; modificare `src/plugin/PluginProcessor.h/.cpp`, `src/state/ToneState.h/.cpp`, `CMakeLists.txt`.

**Interfaces:**
- Controller di proprietà del processor, utilizzabile solo sul thread di controllo; storia massima 128 gesture di `ToneState`, senza file o motori.
- `beginGesture()`, `setParameter(const juce::String& id, float plainValue)`, `endGesture()`, `undo()`, `redo()`, `selectSlot(int)`, `copySlot(int from, int to)`, `resetEq()`, `resetDamping()`.
- `ToneMemories snapshot() const`, `restore(const ToneMemories&)`, `bool canUndo() const`, `bool canRedo() const`; accesso GUI tramite `PluginProcessor::toneController()`.

- [ ] Scrivere `tone_drag_is_one_undo_and_host_gestures_are_balanced`: cento spostamenti producono un solo passo; callback host ricevono begin/end per ogni parametro toccato e valori coerenti; gesto senza cambiamenti non aggiunge storia.
- [ ] Scrivere `tone_ab_copy_restore_and_undo_preserve_unrelated_parameters`: EQ/damping scambiati e persistenti, routing/mix/IR invariati; modifiche nel preset attivo aggiornano solo quello slot; 129 gesture rispettano limite 128.
- [ ] Scrivere `external_automation_during_gesture_is_not_overwritten`: automazione non aggiunge undo; undo locale modifica solo i parametri toccati localmente e non sovrascrive modifiche esterne successive; redo invalidato dopo interventi esterni sui medesimi parametri. Restore chiude gesture e azzera la storia.
- [ ] Eseguire RED; implementare delta per parametro e revisioni di conflitto, anziché ripristinare snapshot globali indiscriminati. A/B è una singola azione locale con notifiche host e debounce damping condiviso.
- [ ] Eseguire test processor/editor; commit `feat: add host-aware tone comparison and gesture undo`.

## Task 5: Curve interattive e pannelli grafici

**Files:** creare `src/ui/CurveEditor.h/.cpp`, `src/ui/TonePanel.h/.cpp`, `tests/CurveEditorTests.cpp`; modificare `src/ui/PluginEditor.h/.cpp`, `src/ui/Theme.h`, `tests/EditorTests.cpp`, `CMakeLists.txt`.

**Interfaces:**
- `enum class CurveMode { postEq, damping };`.
- `CurveEditor(ToneController&, CurveMode)`, `setDisplayState(const ToneState& requested, const ToneState& applied, double sampleRate, bool pending)`, `int selectedNode() const`.
- `TonePanel(PluginProcessor&, CurveMode)` possiede curva, controlli della banda selezionata e toolbar; timer legge soltanto snapshot e stato loader, nessun submit o scrittura automatica dei parametri.

- [ ] Scrivere `curve_drag_numeric_and_keyboard_edit_same_parameters`: coordinate logaritmiche, drag gain/frequenza, rotella Q, tastiera, doppio click reset, shelf/notch; damping non attraversa i vicini. Una gesture resta un passo undo anche modificando due parametri.
- [ ] Scrivere `curve_response_matches_dsp_and_nyquist_is_explicit`: risposta totale di bande + HP/LP, band individuale, EQ bypass; sample rate 8 kHz disabilita i nodi fuori banda senza cambiare valori salvati, aggiornamento automazione senza feedback.
- [ ] Scrivere `pending_or_failed_damping_keeps_active_curve_visible`: richiesta e applicato distinti durante preparazione, swap/crossfade ed errore; editor chiuso/riaperto mostra configurazione attuale, non snapshot precedente.
- [ ] Scrivere regressioni accessibilità e layout a 960x640 e 1180x760: nomi/focus dei controlli, campi numerici visibili, curve non sovrapposte alla toolbar; adattare test della vecchia pagina EQ conservando shape e routing.
- [ ] Eseguire RED; sostituire la pagina GRAPHIC EQ con POST EQ e aggiungere DECAY/DAMPING, usando componenti originali e griglia leggibile. Conservare HP/LP, waveform, libreria e controlli principali; niente codice DSP nel paint.
- [ ] Eseguire `DISPLAY=:99 .build/ch_plugin_tests`, renderizzare e ispezionare entrambe le schede a dimensione minima e normale; commit `feat: edit wet EQ and decay through interactive curves`.

## Task 6: Verifica finale e consegne native aggiornate

**Files:** modificare `tests/WrapperSmoke.cpp`, `README.md`, `BUILD/README.md`, `AGENTS.md`, `docs/development-status.md`, `docs/native-validation.md`, `docs/resource-validation.md`, `CMakeLists.txt`; aggiornare `.github/workflows/build.yml` solo se i nuovi artefatti GUI richiedono raccolta.

- [ ] Estendere gli smoke test dei moduli reali: parametri nuovi presenti e richiamabili, stato roundtrip, EQ non neutro verificato su segnale noto; conservare i 9 layout VST3 e 25 CLAP. Eseguire RED prima delle nuove verifiche non ancora soddisfatte.
- [ ] Portare versione progetto a 0.3.0 e documentare unità damping, latenza di applicazione, bypass, A/B, tastiera, migrazione e limitazioni. Non cambiare UUID/formato/identità del plugin.
- [ ] Eseguire build Release completa con parallelismo 2, suite fresca tramite `scripts/test-report.py --run`, verifica dei contatori effettivi e audit allocazioni; non riutilizzare i vecchi risultati da 91 casi.
- [ ] Misurare RSS e preparazione del damping con IR lunghi e 16 canali, confrontare tempi core con le misure precedenti; registrare anche peggior callback del caso parametrico automatizzato senza dedurre garanzie DAW.
- [ ] Eseguire una revisione finale indipendente secondo `executing-plans`, correggere i problemi importanti con regressioni e ripetere soltanto le verifiche interessate più il controllo finale necessario per lo staging.
- [ ] Commit pulito delle modifiche; ricompilare con identità definitiva e rieseguire il rapporto fresco vincolato ai binari, quindi `scripts/stage-build.py` e `scripts/verify-delivery.py` per Linux. Avviare anche i moduli estratti da un percorso contenente spazi.
- [ ] Push su main già autorizzato; seguire CI Linux/Windows/macOS, verificare AU con auval, architetture/entrypoint e archivi. Se API download rimane Forbidden, usare gli esiti pubblici senza inventare copie locali Windows/macOS.
- [ ] Pubblicare il link agli artefatti aggiornati, commit dei binari, conteggi e limiti reali; distinguere build native, firma e collaudo DAW. Aggiornare il piano con i risultati, senza dichiarare completata una funzione non verificata.

## Self-review del piano

Ogni sezione della specifica è assegnata: Post EQ → Task 1/3/5; damping → Task
2/3/5; interazione, A/B e undo → Task 4/5; compatibilità → Task 3; verifica e
consegna → Task 6. I cinque casi di Review Focus hanno test espliciti nelle
task indicate. Le strutture DSP sono a dimensione fissa; stato e controllo non
trasferiscono snapshot allocanti nel callback. Non sono previsti nuovi SDK,
servizi, worktree o credenziali. Metodo conservato: esecuzione diretta nella
sessione, una revisione finale indipendente. Prima dell'implementazione serve
la revisione di questo piano da parte dell'utente.
