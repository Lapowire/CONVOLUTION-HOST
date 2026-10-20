# Decisioni della fase multicanale e build native

I due piani approvati condividono prodotto, processor e wrapper; sono stati
sottoposti a una sola revisione finale congiunta. La revisione ha riprodotto
quattro problemi e identificato un difetto CMake nativo. Le correzioni hanno
regressioni che fallivano prima dell'intervento: interpretazione salvata
incompatibile, gain della matrice stereo, identità dei binari consegnati,
flag MSVC FFTW e aggiornamento del bus nell'editor. Quest'ultimo era classificato
minore dal revisore e viene trattato come importante perché impediva di
selezionare nuovi canali. Nessun problema minore della revisione resta rinviato.

Decisioni adottate durante l'esecuzione:

- Usato il checkout cloud esistente, come richiesto da AGENTS.md, senza un altro
  worktree. Gli helper degli skill non esposti come file eseguibili sono stati
  sostituiti da registri e verifiche manuali; una risorsa collegata sui test non
  era leggibile. Il costo è una registrazione manuale delle evidenze.
- Il processor invia al worker un'istantanea semantica del bus; il worker risolve
  la diagonale dopo la decodifica, quando conosce il numero di canali dell'IR.
  Le richieste del processor richiedono un layout valido; le API core mantengono
  compatibilità con le richieste precedenti.
- Usato l'archivio ufficiale FFTW 3.3.10 dal mirror Debian con hash fissato:
  il tag Git non contiene i codelet generati e il server upstream restituiva 403.
- FFTW statico bundled obbligatorio su Windows/Linux, con copia generata del
  planner che forza NO_BUFFERING e simboli nascosti. I solver buffered
  allocavano durante l'audio; il backend di sistema non permette di imporre il
  flag attraverso JUCE. Il costo è manutenzione dell'adattamento e possibile
  differenza di prestazioni. macOS usa vDSP.
- Provenienza distinta per configurazione: `build-provenance-Release.json` evita
  conflitti nei generatori Windows con più configurazioni.
- Wrapper, CI e staging integrati nello stesso commit perché la workflow usa
  direttamente gli script di raccolta; separarli avrebbe creato un commit
  intermedio con pipeline incompleta.
- Il wrapper CLAP fissato offriva soltanto stereo: una copia generata espone 25 configurazioni, mappe surround e ACN/SN3D, senza cambiare il checkout upstream. Il costo è manutenzione dei punti di adattamento, verificati da un test sul modulo reale e da controlli CMake. Vedere [adattamento CLAP](clap-bus-adaptation.md).
- La pubblicazione confronta gli hash con una nuova esecuzione CTest e controlla
  gli entrypoint nativi. Il commit compilato viene controllato dai test e dallo
  stato dei moduli VST3/CLAP caricati. Un XML importato non costituisce prova
  di esecuzione corrente. Non è un sistema di attestazione crittografica contro
  chi modifica deliberatamente script e metadati.

AU, AAX e applicazioni Windows/macOS richiedono evidenze dei runner nativi.
La CI è stata avviata, ma l'API Actions restituisce Forbidden in questa
sessione. La pagina pubblica ha confermato il primo pacchetto Linux e gli arresti nei test Windows e nella validazione AU macOS. Le annotazioni diagnostiche CI espongono solo estratti delle uscite dei test; non contengono variabili o credenziali. Firma Avid/PACE, Developer ID, notarizzazione e prove
nelle DAW non sono state eseguite. Vedere lo [stato](development-status.md).

Esito finale: il run [39384746931](https://github.com/Lapowire/CONVOLUTION-HOST/actions/runs/39384746931#artifacts), commit `d49bc85`, è verde su tutte le piattaforme e ha pubblicato il pacchetto aggregato. I limiti residui sono firma/notarizzazione e prove nelle DAW, non build mancanti. Il download API dal cloud resta Forbidden; i pacchetti sono disponibili via GitHub. [Evidenze finali](native-validation.md).
