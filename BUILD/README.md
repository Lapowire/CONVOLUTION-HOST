# Plugin compilati da copiare nella DAW

`BUILD/` è la cartella di consegna. Gli intermedi CMake usano `.build/`, evitando
collisioni fra `build` e `BUILD` su Windows e sui filesystem Mac non sensibili
alle maiuscole.

| Directory | File o bundle completi |
| --- | --- |
| `Linux/x86_64/VST3/` | `CONVOLUTION HOST.vst3` |
| `Linux/x86_64/CLAP/` | `CONVOLUTION HOST.clap` |
| `Linux/x86_64/Standalone/` | `CONVOLUTION HOST` eseguibile Linux |
| `Windows/x86_64/` | directory VST3, CLAP, AAX e Standalone `.exe`, prodotte dal runner Windows |
| `macOS/universal/` | directory VST3, CLAP, AU `.component`, AAX e Standalone `.app`, prodotte dal runner macOS per arm64 e x86_64 |

Copiare **il file o bundle completo**, senza aprirlo per estrarre il binario
interno. I percorsi abituali sono:

| Formato | Linux | Windows | macOS |
| --- | --- | --- | --- |
| VST3 | `~/.vst3/` | `C:\Program Files\Common Files\VST3\` | `~/Library/Audio/Plug-Ins/VST3/` |
| CLAP | `~/.clap/` | `C:\Program Files\Common Files\CLAP\` | `~/Library/Audio/Plug-Ins/CLAP/` |
| AU | — | — | `~/Library/Audio/Plug-Ins/Components/` |
| AAX | — | `C:\Program Files\Common Files\Avid\Audio\Plug-Ins\` | `/Library/Application Support/Avid/Audio/Plug-Ins/` |

Le cartelle effettivamente scansionate dipendono dalla DAW. Gli eseguibili
Standalone sono applicazioni separate; su macOS si copia `.app` in Applications.

## Disponibilità e download

La build Linux viene generata e verificata nel workspace cloud. Le build
Windows/macOS richiedono l'esecuzione della workflow
[Native plugin builds](https://github.com/Lapowire/CONVOLUTION-HOST/actions/workflows/build.yml).
La prima esecuzione ha prodotto l’artefatto Linux; Windows si è fermato nei test e macOS in auval. Le build Windows/macOS non sono quindi consegne verificate. L’API restituisce `Forbidden`; le annotazioni pubbliche e le diagnostiche CI permettono di seguire i tentativi. Vedere lo [stato](../docs/development-status.md).

Gli output generati sono esclusi dal controllo versione: aprire il workspace
`BUILD` oppure scaricare gli artefatti della workflow. Gli archivi piattaforma
sono in `BUILD/archives/`, con radice interna `BUILD/<piattaforma>/<architettura>`;
il job di assemblaggio produce `convolution-host-all-platforms.tar.gz` solo dopo
il successo di tutte e tre le build native.

Ogni consegna contiene `manifest.json`, `SHA256SUMS`, `INSTALL.txt` e `LICENSES/`. La CI estrae l’archivio e verifica inventario, checksum, architetture native e permessi degli eseguibili tramite `scripts/verify-delivery.py` (Python 3.12+).
Il manifest riporta commit, architetture compilate/testate, esito dei test e stato
della firma. Su Linux sono riportate le versioni minime ABI rilevate: la build
cloud corrente richiede GLIBC 2.38 e GLIBCXX 3.4.32. FFTW è incluso staticamente;
restano i normali runtime desktop indicati da `ldd`.

I file AAX compilati senza firma Avid/PACE sono build di sviluppo e non sono
pronti per la normale versione di Pro Tools. Firma Windows, Developer ID e
notarizzazione Apple non sono state eseguite. I test automatici non sostituiscono
una verifica nella DAW. Vedere [stato e limiti](../docs/development-status.md).
