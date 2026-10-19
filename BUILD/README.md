# Artefatti compilati

Questa è la cartella di consegna richiesta per CONVOLUTION HOST. `BUILD/`
conterrà esclusivamente le consegne e la relativa documentazione. Il piano
prevede di spostare gli intermedi CMake dall'attuale `build/` a `.build/`:
su Windows e sui Mac con filesystem non sensibile alle maiuscole, `build` e
`BUILD` possono indicare la stessa directory.

## Struttura prevista

| Directory | Artefatti |
| --- | --- |
| `Linux/x86_64/` | VST3, CLAP, eseguibile standalone Linux |
| `Windows/x86_64/` | VST3, CLAP, AAX, standalone `.exe` |
| `macOS/universal/` | VST3, CLAP, AU `.component`, AAX, standalone `.app` per arm64 e x86_64 |

Ogni piattaforma avrà `manifest.json`, `SHA256SUMS` e `LICENSES/`. Bundle e
permessi eseguibili saranno preservati in archivi `.tar.gz` per Linux/macOS e
`.zip` per Windows. I nomi e la struttura originali dei bundle saranno mantenuti.
AU è previsto solo su macOS; AAX solo su Windows/macOS.

**Stato attuale: contratto di consegna; nessun binario finale pubblicato qui.**
La versione esistente mono/stereo compila VST3 e Standalone Linux in
`build/ConvolutionHost_artefacts/Release/`. Non è la consegna finale multicanale.
Gli artefatti finali verranno raccolti dopo l'implementazione e i controlli
descritti nei piani [multicanale](../docs/superpowers/plans/2026-10-19-multichannel-plugin.md)
e [formati e packaging](../docs/superpowers/plans/2026-10-19-platform-builds.md).

Il manifest dovrà distinguere compilazione, test automatizzati, validazione in
host e firma. Un file AAX compilato ma non firmato non implica utilizzabilità
nella normale versione di Pro Tools. Firma Avid/PACE, firma Windows, Developer
ID e notarizzazione Apple non sono disponibili in questo ambiente e non
verranno dichiarate eseguite.

I binari di grandi dimensioni saranno distribuiti tramite artefatti CI o GitHub
Release; l'archivio scaricato avrà `BUILD/` come radice e potrà essere estratto
nel checkout. Non verranno creati file segnaposto con estensioni di plugin.
