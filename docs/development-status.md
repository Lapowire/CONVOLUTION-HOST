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

| Consegna | Stato verificato in questa sessione |
| --- | --- |
| Linux VST3/CLAP/Standalone | Compilati e testati; raccolta in `BUILD/Linux/x86_64` |
| Windows VST3/CLAP/AAX/EXE | Target e runner predisposti; esecuzione nativa non verificata |
| macOS VST3/CLAP/AU/AAX/APP universal | Target e runner predisposti; esecuzione nativa e auval non verificati |

La prima esecuzione nativa [39379201602](https://github.com/Lapowire/CONVOLUTION-HOST/actions/runs/39379201602) ha prodotto il pacchetto Linux. Windows si è fermato nei test CTest e macOS in auval, dopo la compilazione dei target. L’API restituisce `Forbidden` e i log dettagliati richiedono accesso; le annotazioni pubbliche indicano le fasi fallite. La pipeline include ora diagnostiche limitate alle uscite dei test per individuare le cause al tentativo successivo. Non sono stati generati in Linux file AU,
AAX o applicazioni Windows/macOS tramite rinomina di binari.

Gli SDK AAX e VST3 sono inclusi nella dipendenza JUCE fissata. Non sono presenti
certificati o credenziali di firma. AAX richiede ancora il processo Avid/PACE per
la distribuzione nella normale versione di Pro Tools. Nessuna prova è stata
eseguita in una DAW con hardware audio; neppure TSan o misure del picco di
memoria del processo. Le [licenze effettive](dependency-licenses.md) accompagnano
le consegne; la GPL dei sorgenti originali è conservata.

La seconda esecuzione [39380863250](https://github.com/Lapowire/CONVOLUTION-HOST/actions/runs/39380863250), sul commit `323f236`, ha confermato 88 casi Linux verdi. Windows ha isolato il fallimento nella simulazione di memoria esaurita: MSVC aggiunge padding alle grandi allocazioni, rendendo fragile una dimensione esatta. Il fault injector ora usa una soglia minima e verifica anche una richiesta con padding. Su macOS i test passano, ma auval non rileva il componente (`-50`); lo script legge i codici dal plist, attende la registrazione e aggiorna il registrar del solo utente del runner GitHub. La terza esecuzione [39382584807](https://github.com/Lapowire/CONVOLUTION-HOST/actions/runs/39382584807), commit `aa729cc`, conferma il recupero Windows e il successo macOS completo, incluso auval e archivio. Windows si arresta soltanto nel controllo dei percorsi ZIP temporanei: la radice viene ora canonicalizzata, con regressione tramite un alias di directory. La conferma Windows di questo ultimo controllo resta al job successivo. Gli archivi vengono inoltre estratti e controllati dopo il packaging.

La revisione finale congiunta è completata. Le correzioni sono documentate nelle [decisioni di build e revisione](review-and-build-decisions.md); la validazione nativa Windows/macOS resta aperta.

Il wrapper CLAP fissato esponeva solo lo stereo iniziale. Una [copia generata adattata](clap-bus-adaptation.md) offre ora 25 configurazioni e metadati surround/AmbiX; il modulo reale le supera tutte su Linux. Anche VST3 passa 9 layout canonici reali. Le consegne native finali devono includere questo aggiornamento.
