# Build native verificate — 20 ottobre 2026

La workflow [39384746931](https://github.com/Lapowire/CONVOLUTION-HOST/actions/runs/39384746931#artifacts)
è conclusa con **SUCCESS** sul commit dei binari
`d49bc85e3e2f90dbacfd1411df43fbf85b9332fc`. Tutti i job nativi e il job di
assemblaggio sono verdi. Ogni piattaforma ha eseguito la suite da 91 casi in
10 gruppi CTest prima della raccolta dei file.

| Piattaforma | Consegna prodotta | Verifica |
| --- | --- | --- |
| Linux x86_64 | VST3, CLAP, Standalone | Test, moduli reali e archivio estratto |
| Windows x86_64 | VST3, CLAP, AAX, EXE | Test, moduli/entrypoint PE e ZIP estratto |
| macOS universal arm64+x86_64 | VST3, CLAP, AU, AAX, APP | Test su arm64, entrypoint e due slice, auval e archivio estratto |

I moduli reali verificano 9 layout canonici VST3 e 25 configurazioni CLAP,
compresi canali discreti e metadati surround/AmbiX. Il DSP ha inoltre test con
convoluzione di riferimento, LFE, routing, transizioni e limiti di memoria.
La slice macOS x86_64 è compilata e controllata nel bundle; non è stata eseguita
su un secondo host Intel. La GUI del processor è testata sulle tre piattaforme;
la finestra Standalone è stata avviata e verificata anche nel workspace Linux.

## Download

Nel run GitHub aprire **Artifacts → convolution-host-all-platforms** (circa
62,6 MB). Lo ZIP scaricato contiene `convolution-host-all-platforms.tar.gz`;
estraendo anche questo archivio si ottiene:

```text
BUILD/
  Linux/x86_64/
  Windows/x86_64/
  macOS/universal/
```

Ogni cartella contiene i bundle completi nelle sottocartelle dei formati,
`INSTALL.txt`, `manifest.json`, `SHA256SUMS` e le licenze. Copiare il bundle
intero nella cartella scansionata dalla DAW, seguendo [BUILD](../BUILD/README.md).
Sono disponibili anche i tre artefatti separati, con risultati CTest e anteprime.

Digest SHA-256 dell'artefatto ZIP aggregato **riportato da GitHub**:
`945a68aee37732ad28e32fd756726ea290d801401b79ef2a7147558e68a817c6`.
Questo non è il checksum del TAR interno; i file estratti hanno i propri checksum.

Il download tramite `gh run download` dal cloud restituisce `Forbidden`.
L'esito e i quattro artefatti sono visibili nella pagina pubblica; per scaricarli
usare GitHub con il proprio account. Nel workspace locale `BUILD` contiene la
consegna Linux verificata del medesimo commit; Windows/macOS e l'archivio unico
sono stati generati e pubblicati dai runner, senza copie locali fittizie.

## Limiti di rilascio

Le build sono di sviluppo, senza firma di distribuzione o notarizzazione.
AAX ha superato compilazione e controllo degli entrypoint, ma richiede il
processo Avid/PACE per il normale Pro Tools. AU ha superato auval; nessun formato
è stato collaudato in una sessione DAW con hardware audio. I manifest mantengono
separati questi stati e i test automatici. Non sono certificazioni Dolby Atmos.

L'archivio Linux CI è costruito su Ubuntu 22.04. Il pacchetto cloud locale ha
baseline ABI propria, indicata nel manifest (GLIBC 2.38, GLIBCXX 3.4.32).
I certificati e le credenziali di firma non sono inclusi nei sorgenti né nei
pacchetti. Le [decisioni e correzioni](review-and-build-decisions.md) documentano
le differenze rispetto ai piani iniziali.
