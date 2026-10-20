# Formati nativi e cartella BUILD — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Compilare i formati richiesti sulle piattaforme corrette e raccogliere gli artefatti reali in `BUILD`, distinguendo build, test, validazione e firma.

**Architecture:** Opzioni CMake esplicite per formato, wrapper CLAP fissato a commit e una matrice CI nativa Linux/Windows/macOS. Un unico script di staging raccoglie solo target compilati per il commit corrente e produce manifest, checksum e archivi che mantengono bundle e permessi. Completare prima il piano multicanale; non presentare la build stereo corrente come consegna finale.

**Tech Stack:** C++20, JUCE 8.0.6, CMake ≥3.22, Python 3 standard library per staging, GitHub Actions e runner nativi. CLAP tramite free-audio/clap-juce-extensions.

**Spec:** `docs/superpowers/specs/2026-10-19-convolution-host-design.md`, fase 3; richiesta utente del 19 ottobre 2026 per `BUILD` con AAX, VST3, AU, CLAP e standalone Windows/macOS/Linux.

## Global Constraints

- JUCE resta al commit `51a8a6d7aeae7326956d747737ccf1575e61e209` con SHA-256 già fissato.
- CLAP wrapper al commit `7adee3a1bd4684d4caa5601100e364abccff4b4d`, verificato sul repository upstream durante la pianificazione; submoduli fissati dai gitlink di quel commit.
- AU solo macOS; AAX solo Windows/macOS. Richieste di formato non disponibile devono fallire esplicitamente, senza esclusioni silenziose.
- La copia SDK AAX inclusa nel JUCE fissato è presente e ha licenza GPL v3 alternativa alle condizioni commerciali Avid; nessun nuovo SDK proprietario va pubblicato nel repository.
- I binari AAX non firmati sono artefatti di sviluppo; non equivalgono a plugin caricabili nella normale versione di Pro Tools.
- Non pubblicare segreti, certificati o credenziali di firma. Firma Windows, Developer ID/notarizzazione e firma Avid/PACE restano requisiti esterni al rilascio firmato.
- Conservare GPL-3.0 e avvisi delle dipendenze; nessun IR commerciale incluso.
- `.build/` contiene intermedi; `BUILD/` contiene esclusivamente consegne, manifest, licenze e documentazione. Migrare i comandi dal precedente `build/` per evitare collisioni sui filesystem non sensibili alle maiuscole.

## Review Focus

1. AU o AAX richiesti su Linux: Task 1 verifica errore di configurazione anziché una build parziale apparentemente riuscita.
2. Binario con dipendenze assolute da `/workspace/toolchain`: Task 1 verifica backend FFT e dipendenze runtime redistribuibili.
3. Bundle, permessi o slice arm64 persi durante il packaging: Task 3 verifica estrazione e tipi binari su ciascuna piattaforma.
4. Artefatto vecchio raccolto dopo build fallita: Task 3 verifica provenienza/versione/commit, fallisce senza sovrascrivere l'ultima consegna valida.
5. Firma o validazione in host assente: Task 3 rende questi stati espliciti nel manifest; non li deduce dal successo della compilazione.

---

## Task 1: Target CMake e FFT riproducibile su Windows

**Files:** modificare `CMakeLists.txt`, `cmake/Dependencies.cmake`, `scripts/setup-cloud.sh`, `.gitignore`, README e AGENTS; creare `cmake/PluginFormats.cmake`, `cmake/FftBackend.cmake`, `tests/cmake/FormatOptions.cmake`.

**Interfaces:**

- Opzioni `CH_BUILD_VST3`, `CH_BUILD_STANDALONE`, `CH_BUILD_CLAP` attive di default; `CH_BUILD_AU` attiva solo su Apple e `CH_BUILD_AAX` attiva solo su Apple/Windows. `CH_BUILD_PLUGIN=OFF` esclude tutti i wrapper esplicitamente.
- `CH_AAX_SDK_PATH` opzionale consente un SDK esterno; senza override usare quello incluso nel JUCE fissato, previa verifica di `Interfaces/ACF` e `Libs/AAXLibrary`.
- `cmake/PluginFormats.cmake` produce `CH_JUCE_FORMATS`; registra ogni target richiesto. La presenza dei target deve essere verificata dopo la creazione del wrapper CLAP.
- Su non-Apple, target importato `ch_fft_backend` collega direttamente FFTW3f, con `JUCE_DSP_USE_STATIC_FFTW=1`. Prevedere `CH_FFTW_PROVIDER=system|bundled`, default system su Linux, bundled su Windows; bundled usa FFTW 3.3.10 tag al commit `7184fc796279cfa70e4ba62519ac2938054584e6`, singola precisione statica PIC, thread/OpenMP/test FFTW disattivati. Su Apple usare vDSP.

- [x] Scrivere configurazioni negative: `CH_BUILD_AU=ON` su non-Apple, `CH_BUILD_AAX=ON` su Linux, SDK override incompleto e provider FFT sconosciuto. Aspettarsi errore nominando prerequisito e formato.
- [x] Eseguire i casi negativi contro il CMake corrente e confermare RED: oggi quelle opzioni non sono gestite.
- [x] Implementare i moduli sopra. Per CLAP usare `clap_juce_extensions_plugin(TARGET ConvolutionHost CLAP_ID com.lapowire.convolutionhost CLAP_FEATURES audio-effect reverb)` dell'integrazione fissata, verificandone la firma nel sorgente upstream prima dell'uso. Conservare gli identificatori JUCE pubblicati.
- [x] Migrare script, workflow, documentazione operativa e comandi correnti a `cmake -S . -B .build`, senza eliminare la vecchia build locale dell'utente. Ignorare `/.build/` e i soli artefatti generati sotto `/BUILD/`; rimuovere il precedente pattern `/build*/` per non rischiare di ignorare la cartella di consegna su filesystem non sensibili alle maiuscole. Verificare che `BUILD/README.md` resti tracciato.
- [x] Compilare il backend bundled su Linux come prova del percorso Windows senza dipendere da pkg-config; rieseguire i test grandi blocchi/allocazioni per impedire il fallback FFT. Controllare `ldd`/`otool -L`/`dumpbin /dependents` sui rispettivi runner; nessuna dipendenza da percorsi del workspace.
- [x] Configurare e compilare Linux VST3/CLAP/Standalone e CTest. Verificare i target prodotti e i casi negativi verdi. Commit `feat: add native AU AAX and CLAP build targets`.

## Task 2: Verifica reale dei wrapper e build native

**Files:** creare `tests/WrapperSmoke.cpp`, `scripts/check-native-artifacts.py`; modificare CMake, `tests/RealtimeGuard.cpp` se richiesto dal compilatore nativo e `.github/workflows/build.yml`.

**Interfaces:**

- `ch_wrapper_smoke --format clap|vst3 --plugin <path>` carica il modulo con API nativa, trova factory/entry, istanzia, prepara, elabora e salva/ripristina stato. I test di processor esistenti restano separati da questi test del modulo.
- `scripts/check-native-artifacts.py --platform linux|windows|macos --artifacts <dir>` verifica estensioni, bundle e architetture native con strumenti del runner, senza chiamare un ELF `.exe` o un bundle AU.
- CI nativa: Linux x86_64, Windows x86_64, macOS universal `arm64;x86_64`; Release e test su ogni runner. Il programma macOS universale esegue i test sulla slice del runner; il manifest registra quali architetture sono solo compilate.

- [x] Scrivere smoke wrapper: CLAP entry/factory e VST3 factory, un'istanza effetto, parametri, callback con silenzio, restore e distruzione ordinata. Errori di percorso o factory devono dare exit code diverso da zero. Eseguire RED prima del runner.
- [x] Implementare il runner, leggendo le API dalle dipendenze fissate; i test audio multicanale restano quelli del processor e non vengono sostituiti da una sola istanziazione.
- [x] Scrivere controlli artefatti: ELF x86_64 per Linux, PE x64 per Windows, Mach-O con entrambe le slice nei bundle macOS. AAX e AU devono avere il layout nativo JUCE e i rispettivi entry point.
- [x] Aggiungere job nativi con azioni fissate a SHA; Linux usa Xvfb, Windows e macOS i toolchain nativi. Non usare certificati né dati segreti. Richieste manuali `workflow_dispatch` oltre a push/PR.
- [x] Su macOS eseguire `auval -v aufx Cvht Lpwr` dopo installazione nella sola directory utente temporanea del runner, ripristinando qualsiasi plugin preesistente; registrare output. AAX: verifica binario compilato; prova in Pro Tools Developer solo se effettivamente disponibile, altrimenti esplicito `not_run`.
- [x] Eseguire i job disponibili e raccogliere esiti reali, log e limiti. Commit `ci: build and smoke test native plugin formats on three platforms`.

## Task 3: Staging in BUILD, manifest e archivi finali

**Files:** creare `scripts/stage-build.py`, `tests/test_stage_build.py`, `cmake/BuildProvenance.cmake`; aggiornare `BUILD/README.md`, `.gitignore`, workflow, README, AGENTS e `docs/development-status.md`.

**Interfaces:**

- CMake genera `.build/build-provenance.json` con versione, commit, piattaforma, architettura, configurazione e target richiesti del build corrente.
- `python3 scripts/stage-build.py --artifacts <dir> --provenance <json> --test-results <json> --output BUILD` raccoglie SOLO la piattaforma di provenienza; controlla i file richiesti, copia in temporaneo e sostituisce solo la destinazione di quella piattaforma dopo successo completo. Non cancella consegne di altre piattaforme.
- Il manifest versione 1 registra commit, versione, piattaforma, architetture compilate/testate, configurazione, formato, percorso, dimensioni, checksum, `built`, `automated_tests`, `host_validation`, `signed`, `notarized`. Gli stati test usano `passed|failed|not_run` con motivazione per gli ultimi due.
- Destinazioni: `BUILD/Linux/x86_64`, `BUILD/Windows/x86_64`, `BUILD/macOS/universal`; elenco formati nel contratto `BUILD/README.md`. `SHA256SUMS` copre ciascun file del bundle con percorso relativo; symlink interni leciti conservati, link esterni rifiutati.
- Archivi piattaforma sotto `BUILD/archives` con radice `BUILD/<piattaforma>/<architettura>/`; `.tar.gz` Linux/macOS, `.zip` Windows. I binari e gli archivi generati sono ignorati in Git; documentazione tracciata. CI carica gli archivi come download con percorso `BUILD` preservato.

- [x] Scrivere test Python temporanei: bundle completi, formato mancante, hash, nomi con spazi, directory aggiuntive ignorate, symlink esterni, provenienza mancante/commit errato, esito test fallito, conservazione dell'ultima consegna in caso di errore. Confermare RED prima dell'implementazione.
- [x] Implementare staging e manifest; generare anche avvisi/licenze GPL/JUCE/FFTW/CLAP/AAX applicabili, recuperati dalle dipendenze effettive senza aggiungere SDK nel repository.
- [x] Testare permessi eseguibili dopo tar/estrazione su Linux e macOS, entrambe le slice dopo packaging su macOS e PE x64 dopo unzip su Windows. Verificare checksum dopo estrazione.
- [x] Collegare staging alla CI solo DOPO build e test verdi; un formato richiesto mancante fa fallire il job. Unire gli archivi scaricabili sotto `BUILD` senza dichiarare un formato generato se il suo job non è eseguito.
- [x] Dopo il completamento multicanale, eseguire build Release, tutta la suite e staging Linux in questo workspace. Scaricare e verificare gli archivi Windows/macOS dai job se l'accesso remoto lo permette; in caso contrario registrare precisamente l'impedimento senza inventare file.
- [x] Dopo aver verificato il setup con `.build/`, aggiornare la bozza cloud install/start affinché usi il nuovo percorso; preservare le altre impostazioni dell'ambiente.
- [x] Verificare sezione BUILD e manifest finali, aggiornare limiti solo con evidenza. Commit e push non forzato sul ramo autorizzato; pubblicazione di una release nominata solo se richiesta. Commit `build: collect verified native artifacts in BUILD`.

## Prerequisiti e stato rilevati

- Ambiente locale: Linux x86_64; VST3 e Standalone esistenti della prima fase. Nessun toolchain nativo macOS/Windows è disponibile in questo workspace.
- SDK AAX sorgente presente nella dipendenza JUCE, con licenza GPL v3 alternativa rilevata nel suo `LICENSE.txt`; manca la possibilità locale di testare/firma Avid/PACE o Pro Tools.
- `gh api` e `gh run list` per il repository restituiscono `Forbidden` in questa sessione. Il push Git precedentemente funzionava; non si può dedurne la visibilità o il successo delle esecuzioni Actions. Non modificare le impostazioni di sicurezza del repository per aggirare il limite.
- Se i runner GitHub non sono abilitati/accessibili, i file macOS/Windows richiedono un runner nativo accessibile. Questa è una dipendenza reale per completare la generazione richiesta, non una ragione per dichiarare pronti binari assenti.

## Verifica del piano

Coperti tutti i formati richiesti e le tre applicazioni standalone; distingue
macOS AU da Linux/Windows, e build AAX da firma/validazione. Il piano multicanale
precedente copre le funzioni audio ancora mancanti. Nessun installer commerciale,
servizio di aggiornamento o gestione licenze viene aggiunto senza richiesta.

**Stato:** implementazione locale e revisione finale completate; 91 casi Linux passano in 10 gruppi. La prima esecuzione CI ha prodotto il pacchetto Linux, mentre Windows si è fermato nei test e macOS in auval. Le diagnostiche per i tentativi successivi sono integrate; la consegna Windows/macOS richiede ancora test/auval verdi. API `Forbidden`, annotazioni pubbliche consultabili.

**Conferma finale:** run `39384746931`, commit `d49bc85`, tutti i job SUCCESS. 91 casi per piattaforma; AUval verde, archivi estratti verificati su tre OS e pacchetto aggregato pubblicato. Il download dal cloud è impedito da API Forbidden; i file sono disponibili dal run GitHub. Firma e prove Pro Tools/DAW restano esplicitamente non eseguite.
