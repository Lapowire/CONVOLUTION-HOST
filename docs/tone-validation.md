# Verifica dell'estensione EQ/damping 0.3

La build Release Linux e i dieci gruppi CTest passano: 56 casi core, 34
processor/editor, 21 Python e tre wrapper, per 114 casi complessivi. I moduli
VST3/CLAP reali espongono i nuovi parametri; VST3 verifica anche il richiamo del
valore della frequenza. Rimangono verificati nove layout VST3 e 25 configurazioni
CLAP. Il test audio del processor verifica un notch a 2 kHz impostato tramite i
nuovi parametri, conservando il segnale a 500 Hz sull'altro canale.

Sono coperti: confronto con il vecchio EQ e riferimenti analitici, shelf/notch,
Q/frequenze estreme, blocchi 1/37/64/256/1024, callback senza allocazioni
intercettate, damping neutro bit-identico e selettivo, protezione/bordi e IR corti,
rapporti dei canali, cancellazione, caricamenti falliti, automazione senza editor,
migrazione v1/v2 sopra istanze modificate e nuovo stato v3. A/B e copy sono
persistenti; undo viene verificato per gesture, conflitti esterni e limite 128.
Le curve sono renderizzate e ispezionate a 960×640 e 1180×760.

## Revisione indipendente

Una revisione finale separata ha riprodotto due problemi importanti, entrambi
corretti con test RED→GREEN e suite completa verde:

- Una freccia della tastiera poteva riscrivere l'altro asse usando il valore
  visualizzato prima di un'automazione recente. Ora ogni tasto scrive solo il
  parametro interessato e legge lo stato corrente prima della variazione.
- RESET applicava uno snapshot di entrambi i pannelli, potendo sovrascrivere
  un intervento concorrente nell'altro. Ora ogni reset scrive esclusivamente
  i parametri del proprio pannello.

Un rilievo minore resta aperto: con callback audio sospesi, waveform/Schroeder
possono mostrare l'IR appena preparato prima che sia accettato dal thread audio.
Il pannello damping continua a indicare PENDING e il motore precedente rimane
attivo fino al callback. Non è una sostituzione prematura del motore audio.

## Risorse locali

Nella misura del processo core: picco RSS 621856 KiB, circa 607,3 MiB, includendo
fixture e preparazioni. Il solo EQ automatizzato a 16 canali/48 kHz/256 campioni
ha richiesto circa 0,37 ms nel callback peggiore osservato; altre esecuzioni
hanno riportato 0,38–0,45 ms. Questo non include il costo della convoluzione.

Damping stereo 30 s/96 kHz: circa 368–383 ms di preparazione; caso 16 canali,
2 s/48 kHz: circa 85–102 ms. La stima conservativa dello scratch è rispettivamente
327680 e 196608 byte e non comprende IR, motori o memoria totale del processo.
Sono misure della macchina cloud condivisa, non soglie garantite per una DAW.
Il riferimento precedente è in [resource-validation.md](resource-validation.md).

## Consegna e limiti

La verifica Linux iniziale del commit `65f31a1` ha prodotto i tre formati,
verificato l'archivio estratto con 29 checksum e ricaricato i moduli da un percorso
con spazi senza LD_LIBRARY_PATH. I pacchetti di consegna devono essere rigenerati
sul commit successivo contenente le due correzioni della revisione; gli esiti
nativi definitivi sono registrati in [native-validation.md](native-validation.md).

Non si dichiara una prova esaustiva della stabilità per ogni modulazione o della
concorrenza sotto ogni carico. La revisione e i test non sostituiscono sessioni
DAW con hardware, TSan, firma PACE, firma Developer ID o notarizzazione.

## Decisioni di esecuzione

- Ledger compilato manualmente perché gli helper della skill cloud non erano
  eseguibili localmente; rischio di omissioni compensato dal confronto col piano.
- Posizioni originali delle tre schede conservate, aggiungendo Damping in fondo;
  il costo è avere EQ e damping non adiacenti.
- Alcuni casi di accettazione aggiunti dopo il primo RED/GREEN del modulo;
  prova test-first meno forte per quei singoli casi, compensata da verifiche
  funzionali di integrazione e revisione indipendente.
- Copertura rappresentativa di stabilità/concorrenza accettata, senza affermare
  una prova esaustiva; restano possibili difetti rari di automazione o timing host.
