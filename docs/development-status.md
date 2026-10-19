# Prima versione di sviluppo — 19 ottobre 2026

Implementata la prima fase approvata in C++20/JUCE: convoluzione mono/stereo,
quattro percorsi true stereo, libreria WAV, preset e GUI ridimensionabile.
I target Linux sono VST3 e Standalone. Per utilizzo e compilazione vedere il
[README](../README.md).

## Verifiche

- Build Release dei due target su Debian Linux.
- CTest: 37 casi core e 10 casi processor/GUI, tutti superati sotto Xvfb.
- Confronto con convoluzione diretta, routing, ricampionamento, EQ, predelay,
  blocchi variabili, caricamento asincrono, stato e rilocalizzazione degli IR.
- Controlli delle allocazioni sul thread audio, inclusi blocchi grandi; FFTW3f
  collegato direttamente su Linux per evitare il backend FFT di ripiego.
- Avvio della finestra Standalone e anteprima dell'editor generata dai test.
- Setup cloud senza root eseguito due volte; installazione e istruzioni di avvio
  salvate nella bozza dell'ambiente, ancora da pubblicare.

IR stereo di 30 secondi a 96 kHz: circa 100–125 ms per elaborare 101 blocchi da 256
campioni nelle misure Release effettuate su questa macchina. Budget stimato per
due motori: 355 MiB; non rappresenta la memoria totale del processo né una
garanzia di prestazioni su altre macchine.

## Revisione finale

Una revisione indipendente ha individuato sette problemi importanti. Ogni caso
è stato riprodotto con un test fallito prima della correzione:

1. Eccezioni nei worker: gestione dell'esaurimento di memoria senza terminare
   l'host, preservando il motore attivo e permettendo un nuovo tentativo.
2. Identità degli IR: conservazione degli hash durante richieste successive
   pendenti o fallite, inclusi i quattro WAV separati.
3. Preset senza IR: cancellazione delle richieste e disattivazione dei vecchi
   motori senza distruggerli nel callback audio.
4. Predelay: continuità durante cambi rapidi, completando la transizione dei
   segnali prima di avviare l'ultimo valore richiesto.
5. Routing parziale: ripristino per coppia ingresso/uscita e aggiornamento
   dell'editor quando cambia lo stato.
6. Trim: coordinate riferite al file originale anche dopo un primo ritaglio.
7. Preferenze: verifica della scrittura, conservazione delle directory e
   visibilità degli errori anche dopo una nuova scansione.

Restano due problemi minori dell'interfaccia, da affrontare nella prossima fase:
l'errore di salvataggio di un preset può essere visualizzato troppo brevemente;
durante un caricamento pendente o fallito la waveform può ancora mostrare l'IR
precedente. Verificare la creazione del file preset e lo stato di caricamento
prima di fare affidamento sull'anteprima.

## Ambito successivo

Multicanale immersivo e canali di bed/oggetti, AmbiX, CLAP, AU/macOS e AAX
richiedono ancora implementazione e verifiche dedicate. Non è implementato un
renderer Atmos. Non sono state eseguite prove in una DAW, con hardware audio,
su Windows/macOS, né analisi TSan o misure del picco di memoria del processo.
La workflow GitHub è predisposta; il risultato locale non implica che la CI
remota sia già passata.
