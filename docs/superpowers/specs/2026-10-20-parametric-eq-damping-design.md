# EQ parametrico e damping grafico — specifica approvata

Approvata dall'utente con «procedi» dopo la presentazione della specifica.

## Obiettivo

L'utente richiede di proseguire lo sviluppo e desidera modificare EQ e damping
attraverso curve interattive, prendendo FabFilter Pro-R 2 come riferimento
funzionale. L'interfaccia avrà un disegno originale, coerente con CONVOLUTION
HOST. Questa estensione mantiene C++20/JUCE, tutti i formati e i layout esistenti.

L'esito richiesto è poter modellare separatamente il timbro wet e il decadimento
alle diverse frequenze, con risposta grafica comprensibile, automazione e
richiamo affidabile delle sessioni. Le funzioni accessorie selezionate sono
A/B, undo/redo delle modifiche locali e controlli numerici precisi dei nodi.

## Scelta del metodo

1. **Raccomandato: EQ realtime più damping dell'IR sul worker.** L'EQ modifica
   immediatamente il segnale wet; il damping prepara un nuovo impulso con
   attenuazione dipendente da frequenza e tempo, poi usa il crossfade esistente.
   Il damping ha un'attesa di preparazione indicata nell'interfaccia.
2. **Damping tramite filtri sull'uscita.** Meno lavoro di preparazione, ma cambia
   il colore di tutta la coda senza controllarne il decadimento. Non soddisfa
   il significato di damping concordato per questa proposta.
3. **Motore riverbero ibrido/algoritmico.** Permetterebbe allungamenti e code
   sintetiche, ma richiederebbe un altro motore e altererebbe la fedeltà agli IR.
   Escluso da questa estensione.

## Post EQ

- Dieci bande parametriche: frequenza, gain, Q, tipo e attivazione individuale.
- Tipi: bell, low shelf, high shelf e notch. Restano disponibili i filtri
  passa-alto e passa-basso già presenti.
- Frequenza 20–20000 Hz, limitata alla regione utilizzabile al sample rate
  corrente; Q 0,1–18; gain -12/+12 dB per conservare i range già pubblicati.
  Il notch non usa il gain; per gli shelf Q controlla la risonanza secondo
  le formule del filtro, con limiti verificati per stabilità e campioni finiti.
- Nodi trascinabili: orizzontale = frequenza, verticale = gain; rotella = Q.
  Click seleziona; doppio click sul nodo azzera il gain; i campi numerici e la
  tastiera consentono la stessa modifica. Il notch ha profondità fissa.
- Grafico logaritmico con curva totale calcolata dai coefficienti realmente
  usati, curve delle singole bande e frequenze oltre Nyquist non modificabili.
- Bypass della singola banda e dell'EQ con transizione smussata. Nessuna
  allocazione, acquisizione di lock o distruzione onerosa nel callback.
- Impostazioni iniziali equivalenti all'EQ attuale: frequenze originali, gain
  zero, bell, Q 1,4. Lo smoothing deve essere stabile anche durante grandi salti
  di frequenza, Q, tipo e bypass; non basta interpolare coefficienti senza
  verificarne il comportamento.

## Decay / Damping

- Secondo grafico con sei nodi, asse frequenza logaritmico e asse verticale in
  **dB/s di attenuazione aggiuntiva**, da 0 a 60. Zero conserva il decadimento
  originale; valori crescenti accorciano la coda nella regione selezionata.
  Non è una misura o una promessa di RT60 e non allunga l'IR.
- Frequenze dei nodi modificabili mantenendo ordine e separazione positiva;
  interpolazione lineare dell'attenuazione rispetto al logaritmo della frequenza,
  costante oltre i nodi estremi. Stato iniziale: tutte le attenuazioni a zero.
- Protezione iniziale regolabile 0–200 ms, inizialmente 20 ms, misurata dall'inizio
  dell'IR selezionato. Gain spettrale nominale dopo tale intervallo:
  `10 ^ (-attenuazioneDbAlSecondo(f) * tempoSuccessivo / 20)`.
- Elaborazione spettrale STFT/overlap-add offline, con finestre e compensazione
  ai bordi verificate. La protezione è nominale alla risoluzione temporale della
  finestra: non promette isolamento esatto al singolo campione. La curva neutra
  evita completamente questa trasformazione, conservando l'IR esistente.
- Stessa trasformazione per tutti i canali e i quattro IR true stereo, senza
  normalizzazione per canale, downmix o variazione delle associazioni spaziali.
- Ordine: decode/ricampionamento, trim/fade e normalizzazione comune già
  esistenti, damping, analisi waveform/Schroeder, preparazione convoluzione.
  La normalizzazione non recupera il livello rimosso dal damping.
- Debounce delle modifiche e coalescenza sul percorso worker esistente. Il
  callback del parametro segnala soltanto la modifica; nessun caricamento nel
  thread audio. Solo l'ultima richiesta valida può diventare attiva.
- Vecchio motore mantenuto durante preparazione o errore; crossfade quando il
  nuovo è pronto. Stato visibile: in preparazione, applicato oppure errore.
  Il grafico distingue la curva richiesta da quella attiva se differiscono.
- I parametri di damping sono richiamabili e automatizzabili, ma l'applicazione
  asincrona non è sample-accurate. Esplicitare questa proprietà anche nel manuale;
  per esportazioni ripetibili la configurazione va applicata prima del render.
- I buffer temporanei STFT devono avere dimensione limitata e verificata prima
  dell'allocazione; il limite esistente dei motori resta 512 MiB. Misurare e
  documentare separatamente memoria temporanea e RSS complessivo.

## Interazione e funzioni accessorie

Due schede grandi, POST EQ e DECAY/DAMPING, con griglia leggibile, curve di
colori distinti e pannello della banda selezionata. La preview dell'IR rimane
disponibile e si aggiorna all'effettiva preparazione, senza rappresentare come
attivo un risultato pendente. GUI ridimensionabile, nomi accessibili, focus
visibile e controlli equivalenti da tastiera.

A/B scambia due configurazioni di EQ/damping; copy A→B e B→A, reset del pannello
e undo/redo delle modifiche locali permettono confronti ripetibili. Una gesture
di trascinamento vale un passo undo. L'automazione ricevuta dalla DAW aggiorna
la curva senza riempire la cronologia locale. A/B e undo notificano correttamente
i parametri alla DAW; non duplicano IR o motori e non modificano file originali.
Le memorie A/B sono salvate nel preset; la cronologia undo è solo della sessione
dell'istanza e non viene serializzata. Entrambi i pannelli hanno reset e bypass.

## Parametri e compatibilità

Conservare identificatori, ordine, range e significato di tutti i parametri
pubblicati, inclusi `eq0`–`eq9`; aggiungere i nuovi parametri in coda. Frequenza,
Q e tipo rendono parametriche le bande esistenti. Un vecchio preset richiama
valori aggiuntivi predefiniti, senza ereditare le modifiche dell'istanza attuale.

Versionare lo stato per memorie A/B e nuovi dati, continuando a leggere le
versioni precedenti. Validare indici, enum, frequenze, dati non finiti e dimensioni;
uno stato corrotto non deve sostituire quello valido. Ripristino, automazione
con editor chiuso e caricamento dei file mancanti conservano i controlli nuovi.

## Verifica e consegna

- Risposta EQ contro riferimenti analitici, notch/shelf, frequenze estreme,
  blocchi variabili, bypass e automazioni aggressive a 44,1/48/96 kHz.
- Damping neutro identico; segnali sintetici a decadimento noto in regioni
  spettrali diverse; attenuazione dipendente dal tempo e ricostruzione ai bordi.
- Stessa elaborazione sui canali, true stereo, AmbiX e politica LFE invariati;
  richieste superate, cancellazione, errori e budget senza sostituzioni errate.
- Preset precedenti, nuovo roundtrip, A/B, undo per gesture, automazione senza
  editor e campi accessibili; test e rendering della GUI sotto Xvfb.
- Audit allocazioni realtime e misure di tempo/memoria prima e dopo.
- Build e test nativi dei wrapper, staging in BUILD e nuovi archivi verificati.
  Firma, notarizzazione e prove DAW con hardware restano verifiche distinte.

Questa proposta non aggiunge stretching, cattura IR, freeze, modulazione,
renderer Atmos o nuovi motori. Le ulteriori funzioni si valutano dopo il collaudo
di EQ e damping, sulla base di un vantaggio concreto per questo plugin.
