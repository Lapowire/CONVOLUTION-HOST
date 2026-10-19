# Istruzioni per gli agenti

## Scopo

Sviluppare CONVOLUTION HOST in C++20 con JUCE e CMake: plugin di riverbero a convoluzione basato su IR WAV, con GUI, librerie configurabili e routing mono, stereo, multicanale, immersivo e ambisonico.

Leggere `docs/superpowers/specs/2026-10-19-convolution-host-design.md` prima di modificare architettura o comportamento. Finché la specifica e il piano non sono approvati, non considerarli autorizzazione a iniziare l'implementazione. Segnalare conflitti tra requisiti prima di scegliere un comportamento incompatibile.

## Correttezza audio

- Nel callback audio non effettuare I/O, scansioni, logging, allocazioni, deallocazioni onerose, attese o acquisizioni di mutex.
- Preparare gli IR e le matrici fuori dal thread audio; trasferire ownership e recuperare gli oggetti senza distruggerli nel callback.
- Gestire sample rate, blocchi variabili, reset, bypass, latenza e coda. Smussare i parametri e applicare transizioni ai cambi IR.
- Non reinterpretare implicitamente ordine dei canali, layout, convenzioni ambisoniche o canali di un WAV true stereo.
- Validare dimensioni, indici, campioni non finiti e limiti di memoria prima dell'attivazione. Un caricamento fallito deve conservare il motore precedente.
- Conservare metadati e rapporti di livello spaziali; nessun downmix, normalizzazione per canale o limiter nascosto.

## Architettura e interfaccia

- Separare libreria, loader, routing, DSP, stato e GUI; evitare dipendenze del motore dall'editor.
- Usare parametri con identificatori stabili, gesture di automazione e stato versionato. Non cambiare gli identificatori pubblicati senza migrazione.
- Rendere espliciti stato di caricamento, errori, file mancanti e layout non supportati. Non modificare i WAV originali.
- Mantenere GUI ridimensionabile, controlli numerici leggibili, navigazione da tastiera e nomi accessibili.
- Conservare i file e le modifiche dell'utente. Non incorporare segreti, SDK proprietari o librerie IR prive di autorizzazione.

## Build e dipendenze

- Fissare le revisioni delle dipendenze; conservare TLS e verifiche di integrità. Non seguire branch mobili per build riproducibili.
- Usare il checkout esistente: le attività cloud sono già isolate. Non creare worktree senza una richiesta dell'utente.
- Rendere opzionali i target vincolati a piattaforma o SDK; un target richiesto esplicitamente deve fallire con una diagnosi se manca un prerequisito, senza essere escluso silenziosamente.
- AU richiede macOS. AAX richiede SDK e condizioni Avid. CLAP richiede la sua integrazione dedicata. Non dichiarare un formato verificato perché un altro compila.
- Elaborare audio di bed/oggetti esposti dall'host non equivale a implementare un renderer Atmos o a gestire metadati degli oggetti.
- Conservare la licenza esistente e verificare le condizioni delle dipendenze prima della distribuzione.

## Verifica e comunicazione

- Per modifiche DSP usare test offline con segnali noti e confronto con convoluzione di riferimento, includendo routing, sample rate, latenza e dimensioni variabili dei blocchi.
- Verificare caricamento asincrono, errori e ripristino dello stato; misurare le risorse su casi rappresentativi.
- Per la GUI compilare il target interessato ed eseguire uno smoke test quando l'ambiente lo consente.
- Riportare comandi eseguiti e risultati reali; distinguere test superati, falliti, saltati e non eseguiti. Non sostituire test funzionali con controlli banali.
- Diagnosticare i problemi prima di correggerli. Non disabilitare test o controlli di sicurezza per ottenere un risultato positivo.
- Non garantire assenza assoluta di errori. Dichiarare limiti e prerequisiti non verificati con precisione.

## Stato iniziale

Non esistono ancora sorgenti, target CMake o test. Non inventare comandi di build come se fossero già disponibili. Aggiornare questa sezione con i comandi realmente verificati quando vengono implementati.
