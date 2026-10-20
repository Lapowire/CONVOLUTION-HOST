# Licenze delle build di sviluppo

I sorgenti originali del repository mantengono la licenza GPL-3.0 in `LICENSE`.
Le build qui prodotte usano l'opzione open source AGPLv3 di JUCE 8.0.6: non si
presume una licenza commerciale JUCE. Il testo AGPLv3 è incluso in `licenses/`
e nelle consegne. Per l'opera combinata si applicano anche le condizioni AGPLv3;
la GPLv3, sezione 13, consente questa combinazione senza cambiare la licenza dei
singoli sorgenti originali.

FFTW usa GPL v2 o successiva; l'adattamento al planner è documentato in
`docs/fftw-realtime.md` ed è riproducibile dal modulo CMake incluso nei sorgenti.
CLAP, clap-helpers e clap-juce-extensions mantengono i rispettivi avvisi MIT.
Gli SDK VST3 e AAX inclusi nella revisione JUCE fissata offrono l'alternativa
GPLv3 indicata nei loro file LICENSE; non si presume alcun contratto commerciale
Steinberg/Avid. AudioUnitSDK è Apache 2.0.

Gli avvisi JUCE identificano inoltre le dipendenze incorporate. Le consegne
includono le licenze delle dipendenze effettive e un collegamento al commit dei
sorgenti, che conserva script di build e revisioni/checksum delle dipendenze.
Firma, notarizzazione e autorizzazione Avid/PACE non sono implicate da queste
licenze né dal successo della compilazione.
