# Stato di sviluppo — 20 ottobre 2026

Implementate le funzioni audio approvate in C++20/JUCE: convoluzione mono,
dual mono, stereo e true stereo, matrice fino a 16 canali e 256 percorsi,
layout 5.1/7.1/7.1.2/7.1.4 e AmbiX ACN/SN3D ordini 1–3. La libreria WAV,
waveform/decadimento, predelay, EQ, width, trim/fade e preset sono integrati nella
GUI. Gli oggetti sono elaborati come canali esposti dall'host; metadati e
rendering Atmos restano nella DAW.

## Verifiche locali

- Build Release Linux VST3, CLAP e Standalone, con CMake e dipendenze fissate.
- 91 casi in 10 gruppi CTest: 45 core, 22 processor/editor, 21 Python e 3 test dei
  wrapper, incluso il rifiuto di un percorso inesistente.
- VST3 e CLAP caricati come moduli reali da un host di test, rispettivamente 9 e 25 configurazioni: factory,
  istanziazione, parametri, salvataggio/ripristino e audio verificati.
- Confronti con convoluzione diretta e verifica dei canali, LFE escluso per
  default, AmbiX preservato, cambi layout/generazioni e callback senza allocazioni
  intercettabili. WIDTH è disabilitato per AmbiX e layout senza coppie stereo.
- GUI verificata sotto Xvfb, matrice di 256 righe e anteprime stereo/immersiva.
- Setup cloud verificato sul percorso `.build`; gli script e i dati generati
  restano separati da `BUILD`, che contiene i plugin installabili.
- Staging con verifica dei formati/architetture, manifest, checksum, permessi,
  licenze e archivi con bundle completi. Provenienza non valida o test falliti
  bloccano la pubblicazione, conservando la consegna precedente.

I tempi misurati su questa macchina variano col carico: circa 400–510 ms per
101 blocchi da 256 campioni a 48 kHz su 16 canali con IR di 2 secondi, e circa
110–135 ms per 101 blocchi a 96 kHz stereo con IR di 30 secondi. La stima
conservativa di due motori per l'ultimo caso è 355 MiB. Queste misure non sono
una garanzia realtime; la stima non comprende la memoria totale del processo.

## Correzioni e compatibilità

La fase stereo aveva già corretto sette problemi importanti: eccezioni dei
worker, hash degli IR, ripristino senza IR, automazione del predelay, routing
sparso, coordinate del trim e persistenza delle preferenze. Questa fase risolve
anche i due problemi GUI rimasti: errore di salvataggio preset troppo breve e
vecchia preview modificabile durante una nuova selezione fallita/pendente. La revisione finale ha aggiunto il rifiuto delle interpretazioni salvate incompatibili, la conservazione dei gain delle matrici stereo, l’aggiornamento dei nomi canale al cambio bus e la correzione del flag MSVC FFTW. Lo staging controlla identità compilata, esecuzione fresca dei test, hash dei moduli ed entrypoint; un eseguibile generico non viene accettato come plugin.

Lo stato passa a versione 2 e legge la versione 1. Un'associazione esplicita
conserva l'identità del bus; un numero di canali uguale non autorizza una
reinterpretazione fra discreto, surround e AmbiX. Una matrice esplicita vuota
rimane wet silenziosa dopo il ripristino.

La build statica ha rivelato allocazioni FFTW che non erano intercettabili
all'interno della precedente libreria condivisa. Il planner ora esclude i
solver buffered tramite un adattamento riproducibile, documentato in
[fftw-realtime.md](fftw-realtime.md); tutti i controlli delle allocazioni restano
attivi. Le sorgenti upstream verificate non sono modificate.

## Piattaforme e limiti esterni

Il [run 39384746931](https://github.com/Lapowire/CONVOLUTION-HOST/actions/runs/39384746931#artifacts)
del commit `d49bc85` è **SUCCESS** su tutte le piattaforme e nell'assemblaggio:
91 casi per piattaforma, più controlli degli entrypoint e degli archivi estratti.

| Consegna | Stato verificato |
| --- | --- |
| Linux VST3/CLAP/Standalone x86_64 | Build, test e archivio verificati; copia locale in `BUILD/Linux/x86_64` |
| Windows VST3/CLAP/AAX/EXE x86_64 | Build, test, entrypoint PE e ZIP estratto verificati |
| macOS VST3/CLAP/AU/AAX/APP universal | Build, test su arm64, auval, due slice e archivio verificati |

Sono pubblicati i tre pacchetti e `convolution-host-all-platforms`, circa 62,6 MB,
con la cartella `BUILD` completa. [Download ed evidenze](native-validation.md).
Il download automatico tramite API dal cloud restituisce Forbidden; i pacchetti
Windows/macOS sono disponibili dal run GitHub con il proprio account.

Le verifiche native hanno corretto due problemi degli strumenti: la simulazione
OOM usa una soglia minima, compatibile con il padding MSVC, e il verificatore
ZIP canonicalizza la directory temporanea. AU legge i codici dal plist e attende
la registrazione, aggiornando il registrar del solo utente del runner GitHub.
Il wrapper CLAP espone [25 configurazioni](clap-bus-adaptation.md), mappe surround
e ACN/SN3D tramite una copia generata; VST3 verifica 9 layout canonici reali.

AAX supera compilazione e controllo degli entrypoint, ma non è firmato Avid/PACE
né provato in Pro Tools. Le build macOS non sono Developer ID firmate/notarizzate;
la slice Intel è compilata e controllata, senza esecuzione su un host Intel.
Nessuna prova in una sessione DAW con hardware audio o TSan. I manifest
distinguono build, test e validazione nella DAW.

Una [misura RSS locale successiva](resource-validation.md) registra 607,1 MiB
per l'intera suite core e 43,8 MiB per processor/editor, con 67/67 casi superati.
Il picco non è attribuito a un singolo scenario né al solo motore. Il caso a
16 canali impiega 459 ms per 539 ms di audio su questa macchina condivisa;
restano da verificare le scadenze dei singoli callback e il carico in una DAW.

Gli SDK AAX e VST3 sono inclusi nel JUCE fissato. Non sono presenti certificati o
credenziali di firma nelle consegne. Le [licenze](dependency-licenses.md)
accompagnano i pacchetti; la GPL originale è conservata. La revisione finale
congiunta è completata; tutte le correzioni e le decisioni sono raccolte in
[review-and-build-decisions.md](review-and-build-decisions.md).
