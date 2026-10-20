# CONVOLUTION HOST — specifica di progetto

## Scopo e accordi

Plugin di riverbero a convoluzione in C++20 e JUCE per utilizzare librerie di impulse response WAV di stanze, ambienti e processori d'effetto. Il prodotto offre un'interfaccia dedicata alla selezione, visualizzazione e modifica non distruttiva degli impulsi. Build con CMake; formati previsti: VST3, AU, AAX e CLAP, oltre a un'applicazione standalone per sviluppo e verifica.

L'utente ha confermato il supporto dei canali audio di bus immersivi, bed e oggetti esposti dalla DAW. La DAW e il renderer conservano posizione, traiettorie, metadati e rendering Dolby Atmos. Il plugin non costituisce un renderer o encoder Atmos. La disponibilità di un layout o di un inserto su oggetto dipende dalle capacità del formato e dell'host.

## Consegna incrementale

1. Base utilizzabile: build riproducibile, standalone e VST3, convoluzione mono, dual mono, stereo e true stereo, librerie, GUI completa, parametri e stato persistente.
2. Estensione dello stesso motore: routing multicanale a matrice, layout immersivi e ambisonici, editor di associazione degli IR e verifiche dei canali.
3. Integrazione e validazione dei wrapper CLAP, AU e AAX sui sistemi e SDK disponibili.

Queste fasi definiscono l'ordine, non eliminano i requisiti finali. Ogni formato e layout sarà dichiarato verificato soltanto dopo una prova specifica. I test disponibili su Linux non certificano le build macOS o AAX.

## Architettura

- `IrLibrary`: indicizza directory della libreria e dell'utente, riconosce WAV, ricerca e preferiti; non dipende dall'editor audio.
- `IrLoader`: decodifica, valida, ricampiona e prepara gli impulsi su worker; produce dati immutabili con identificatore di richiesta.
- `RoutingModel`: descrive layout, etichette di canale e percorsi ingresso–uscita–IR. Valida la compatibilità prima dell'attivazione.
- `ConvolutionEngine`: esegue convoluzione FFT partizionata e somma i percorsi; usa JUCE per i percorsi mono/stereo e un'astrazione di matrice per l'estensione multicanale.
- `ReverbProcessor`: integra dry/wet, predelay, EQ, livelli, bypass e smoothing; espone parametri tramite AudioProcessorValueTreeState.
- `PluginEditor`: visualizza libreria, analisi dell'impulso, controlli e routing, senza effettuare scansioni o decodifiche nel paint o nel callback audio.

Il callback audio non esegue I/O, allocazioni, distruzioni onerose o attese su lock. Gli impulsi preparati vengono trasferiti al motore tramite un meccanismo di ownership con recupero sul worker. Le richieste superate non sostituiscono quella più recente. I buffer, le code e i motori attivi sono dimensionati prima dell'elaborazione.

## Libreria e caricamento

Directory multiple configurabili, distinte fra libreria e file utente; scansione asincrona, ricerca per nome e percorso, preferiti persistenti, selezione con file chooser e drag-and-drop WAV. Nessuna modifica ai WAV originali. I metadati includono frequenza di campionamento, numero di canali, durata e percorso.

La libreria e le preferenze globali sono salvate nella directory applicativa dell'utente. Lo stato della sessione conserva riferimento all'IR, associazioni di routing e parametri. Gli IR non sono incorporati nei preset iniziali: il plugin prova percorso originale e percorsi relativi alle radici configurate, verificando l'identità del contenuto prima di una sostituzione automatica. Se il file manca, conserva il riferimento e propone la rilocalizzazione.

Sono richiesti WAV PCM e float decodificabili da JUCE. File vuoti, corrotti, non finiti o oltre i limiti di risorse producono un errore leggibile senza interrompere l'audio. La baseline supporta IR fino a 30 secondi a 96 kHz e 16 canali di bus; l'ammissione di una matrice dipende anche da memoria e numero di percorsi. Il limite di memoria stimata dei motori è 512 MiB per istanza, inclusa la sostituzione in crossfade: un caricamento che lo supera viene rifiutato prima dell'attivazione. Le stime includono buffer e dati FFT, non solo i campioni del file.

## Formati e routing audio

- Mono: un percorso 1→1.
- Dual mono: L→L e R→R indipendenti, senza crossfeed.
- Stereo: ingresso stereo con IR mono applicato separatamente ai due canali oppure WAV stereo con L→L e R→R.
- True stereo: quattro percorsi espliciti LL, LR, RL e RR; l'utente associa quattro IR mono o i canali di un WAV multicanale. Nessuna deduzione silenziosa dell'ordine.
- Multicanale: matrice sparsa con percorso, indice del canale IR e guadagno definiti dall'utente. Nessun downmix implicito.
- Immersivo: layout simmetrici 5.1, 7.1, 7.1.2 e 7.1.4 quando supportati dall'host. Canali identificati tramite layout JUCE, non soltanto per indice. Il percorso wet LFE è disattivato inizialmente e può essere attivato esplicitamente; il dry segue il mix.
- Oggetti: elaborazione mono/stereo o multicanale secondo i bus che la DAW espone; nessuna ricezione o modifica dei metadati di posizione.
- Ambisonico: ordini 1–3, rispettivamente 4, 9 e 16 canali, ACN/SN3D (AmbiX). Modalità per canale o matrice esplicita, senza decodifica binaurale, rotazione o conversione automatica di convenzione.

Il numero di canali da solo non distingue un bus ambisonico da uno discreto: si usa il layout dichiarato dall'host e, dove necessario, una selezione esplicita. Configurazioni incompatibili vengono rifiutate o segnalate, mai reinterpretate silenziosamente. WIDTH modifica soltanto coppie stereo designate e viene disabilitato negli altri layout; una matrice ambisonica arbitraria non implica conservazione dell'invarianza rotazionale.

## Catena DSP e parametri

Ingresso → gain → separazione dry/wet. Ramo wet: predelay → convoluzione → EQ → width sulle coppie configurate → wet gain. Somma dry/wet → output gain → meter. La modifica di trim e inviluppo prepara un nuovo IR sul worker.

Controlli iniziali:

| Parametro | Intervallo / comportamento |
| --- | --- |
| Input e output | −24…+24 dB, default 0 dB |
| Mix | 0…100%, default 25%, legge lineare |
| Wet gain | −60…+12 dB, default 0 dB |
| Predelay | 0…500 ms, default 0 ms |
| Width | 0…200%, default 100%; mid/side sulle coppie definite |
| EQ grafico | 10 bande: 31,5, 63, 125, 250, 500 Hz, 1, 2, 4, 8, 16 kHz; ±12 dB |
| Filtri wet | passa-alto 20…2000 Hz e passa-basso 1000…20000 Hz; bypass iniziale |
| Trim IR | inizio/fine in millisecondi, entro la durata del file |
| Inviluppo | fade-in 0…500 ms, fade-out 0…5000 ms, limitati alla selezione |
| Normalizzazione IR | opzionale, disattivata inizialmente; scalatura comune dei canali per preservare rapporti spaziali |
| Bypass | transizione smussata, percorso dry con compensazione della latenza |

Le frequenze dei filtri rispettano Nyquist; le bande non utilizzabili vengono disabilitate e indicate nella GUI. Guadagni e mix sono smussati; predelay e sostituzioni IR usano transizioni senza discontinuità. Si dichiara all'host la latenza effettiva del motore, si allinea il dry e si riporta la coda in base a predelay e durata effettiva dell'IR. Il cambio IR usa crossfade di 50 ms fra motori preparati. Nessun limiter nascosto: i meter segnalano il clipping.

Senza IR valido il plugin lascia passare l'ingresso a guadagno unitario e mostra lo stato. Se un nuovo caricamento fallisce, il motore precedente continua a funzionare.

## Interfaccia

GUI ridimensionabile, dimensione iniziale 1180×760, minimo 960×640. Tema grafite, accenti turchesi e ambra, contrasto leggibile e font scalabili. Rendering JUCE nativo, senza dipendenza obbligatoria da OpenGL o webview.

- Testata: nome IR, preset, stato di caricamento e formato del bus.
- Colonna sinistra: radici configurate, ricerca, preferiti e risultati.
- Area centrale: waveform multicanale, selezione di trim e vista decadimento energetico Schroeder in dB calcolato sul worker. Il grafico rappresenta l'IR, non una misura acustica certificata.
- Sezione inferiore: predelay, mix, width, livelli, filtri ed EQ grafico interattivo.
- Pagina Routing: ingressi, uscite, assegnazione dei canali IR e livelli dei percorsi.
- Barra di stato: frequenza, durata, canali, latenza, meter ingresso/uscita ed errori con azione di recupero.

Controlli con valore numerico modificabile, unità, tooltip, focus da tastiera e nomi accessibili. Ogni modifica di parametro passa per le gesture dell'host. Scansioni e caricamenti espongono progresso/stato senza bloccare l'interfaccia.

## Build, dipendenze e licenze

CMake con versioni JUCE e integrazione CLAP fissate a revisioni esatte nell'implementazione, senza seguire branch mobili. VST3 e standalone sono i target di base; AU è condizionale a macOS, AAX a Windows/macOS tramite lo SDK incluso nel JUCE fissato con alternativa GPLv3, oppure uno SDK esterno esplicitamente configurato, CLAP all'integrazione del wrapper dedicato. Gli SDK proprietari non vengono inclusi nel repository. Nessun download deve disattivare verifica TLS, firme o checksum.

Linux cloud: compilazione dei target compatibili e test DSP senza dispositivo audio. macOS: build AU/VST3/CLAP e verifica in host. Windows: build VST3/CLAP/AAX ove disponibile. Firma, notarizzazione, packaging commerciale e autorizzazioni Avid sono attività di rilascio successive. La licenza GPL già presente va conservata; prima di distribuire occorre verificare compatibilità delle dipendenze e condizioni JUCE/Avid per il modello di distribuzione scelto.

## Verifica e criteri di accettazione

Test DSP offline con WAV sintetici generati dal runner, senza librerie IR esterne:

1. Impulso unitario e IR noti: uscita uguale alla convoluzione di riferimento entro tolleranza numerica documentata, compensando la latenza dichiarata.
2. Mono, dual mono, stereo e quattro percorsi true stereo: verifica del segnale atteso e assenza di crossfeed indesiderato.
3. Matrici multicanale e layout immersivi: impulsi distinti su ogni ingresso, corretta destinazione e politica LFE.
4. AmbiX: ordine dei canali preservato e comportamento per canale verificato per ordini 1–3.
5. Predelay, mix, EQ, width, trim e normalizzazione: risposta attesa, canali isolati e campioni finiti.
6. Frequenze 44,1/48/96 kHz e blocchi 64/256/1024, inclusi blocchi di dimensione variabile e ricampionamento degli IR.
7. Caricamenti concorrenti, file mancanti/corrotti, limite di risorse e ripristino dello stato: nessun crash, motore precedente conservato e ultima richiesta rispettata.
8. Sostituzione e automazione: continuità su segnali controllati; audit del callback per allocazioni e blocchi. Misure CPU/memoria riportate con macchina, layout e IR, senza promettere soglie universali.
9. Build reale dei wrapper disponibili e smoke test di istanziazione, stato e audio; GUI sotto display virtuale dove disponibile. La sola compilazione non certifica il funzionamento nella DAW.

L'esito finale distingue test eseguiti, falliti, esclusi e dipendenti da piattaforme/SDK esterni. Nessun formato viene presentato come pronto alla distribuzione senza le sue verifiche.

## Confini iniziali

Esclusi: libreria commerciale di IR inclusa, acquisizione di risposte tramite sweep, time stretching dell'IR, gestione DRM, renderer Atmos, decoder ambisonico e distribuzione firmata. Restano inclusi tutti i controlli e i layout sopra descritti. Le restrizioni dell'onboarding sui sorgenti non si applicano alla successiva implementazione richiesta dall'utente; eventuali script di ambiente dovranno comunque essere riproducibili e non distruttivi.
