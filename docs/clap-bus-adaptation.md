# Adattamento dei bus CLAP

La revisione fissata di clap-juce-extensions espone solamente il layout stereo
iniziale e lascia senza implementazione le configurazioni alternative. La
compilazione genera una copia del wrapper, mantenendo licenza MIT e avvisi
upstream, e sostituisce soltanto la descrizione/selezione dei bus e le estensioni
spaziali. Il checkout verificato non viene modificato. Gli anchor CMake devono
corrispondere alla revisione fissata; un aggiornamento incompatibile fallisce.

Il modulo espone 25 configurazioni con identificatori stabili:

| ID | Layout |
| --- | --- |
| 0 | Mono |
| 1 | Stereo, iniziale |
| 2–17 | Discreto, rispettivamente 1–16 canali |
| 18–21 | 5.1, 7.1, 7.1.2, 7.1.4 |
| 22–24 | AmbiX ACN/SN3D, ordini 1–3 |

Il cambio avviene esclusivamente mentre il plugin è disattivato. Le porte
riportano il layout corrente, distinto da quello iniziale. I layout discreti
restano generici; quelli surround espongono la mappa dei diffusori e quelli
ambisonici dichiarano ACN/SN3D. Nessuna conversione di ordine o normalizzazione.
I canali JUCE Ls/Rs e Lss/Rss sono dichiarati SL/SR; Lsr/Rsr diventano BL/BR.
I diffusori superiori mantengono posizione frontale, laterale o posteriore.
L'ordine delle porte segue quello JUCE ed è descritto alla DAW, senza supporre
che coincida con un ordinamento fisso di un WAV multicanale.

La selezione configura lo stesso AudioProcessor e viene validata dal suo modello
di bus. Il callback audio del wrapper e il DSP non sono sostituiti. Il test di
host reale seleziona tutte le configurazioni, controlla metadati e passaggio dei
singoli canali e rifiuta convenzioni ambisoniche diverse. La disponibilità del
menu di configurazione e dei bus immersivi dipende comunque dall'host CLAP.
