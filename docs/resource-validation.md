# Misure delle risorse — 20 ottobre 2026

Misura locale Linux x86_64, kernel 6.18.55, sui binari Release del commit
`d49bc85e3e2f90dbacfd1411df43fbf85b9332fc`, senza ricompilazione. Le due suite
sono state eseguite in sequenza con il toolchain cloud attivato e `DISPLAY=:99`.

| Processo | Esito | Tempo totale | Picco RSS |
| --- | --- | --- | --- |
| `ch_tests` | 45/45, exit 0 | 2,731 s | 621656 KiB = 607,1 MiB |
| `ch_plugin_tests` | 22/22, exit 0 | 2,187 s | 44824 KiB = 43,8 MiB |

Il picco è quello riportato da Linux `wait4().ru_maxrss` per ciascuna esecuzione;
non è una somma dei consumi simultanei. Comprende il processo dei test, fixture,
preparazione e caricamento degli IR. Non permette di attribuire il picco a un
singolo caso, né rappresenta la memoria di una sessione DAW o il limite di
memoria del solo motore. In particolare, la stima conservativa di 355 MiB
stampata dal caso stereo non costituisce un limite RSS del processo.

I timer già presenti nei test hanno riportato:

| Caso | Lavoro | Tempo di elaborazione | Durata audio |
| --- | --- | --- | --- |
| 16 canali, routing diagonale, IR 2 s, 48 kHz | 101 blocchi da 256 campioni | 459,348 ms | 538,667 ms |
| Stereo, IR 30 s, 96 kHz | 101 blocchi da 256 campioni | 118,380 ms | 269,333 ms |

Il primo timer include il controllo delle allocazioni del test. Entrambi sono
misure aggregate di una sola esecuzione: non misurano il peggior tempo del
callback, le scadenze di ogni blocco o la latenza sotto carico della DAW. Il
margine del caso a 16 canali è limitato su questa macchina condivisa; non
estenderlo a matrici dense da 256 percorsi o ad altri sistemi. Nessuna garanzia
realtime deriva da questi valori.

## Riproduzione

Dalla radice del repository, con i test già compilati e Xvfb disponibile su
`:99`, il seguente comando misura ciascun processo separatamente. Le unità RSS
sono KiB su Linux. Conserva l'errore se una suite fallisce.

```bash
source /workspace/toolchain/activate.sh
DISPLAY=:99 python3 - <<'PY'
import os
import subprocess
import time

failed = False
for executable in ("./.build/ch_tests", "./.build/ch_plugin_tests"):
    start = time.monotonic()
    process = subprocess.Popen([executable])
    _, status, usage = os.wait4(process.pid, 0)
    process.returncode = os.waitstatus_to_exitcode(status)
    print(executable, "exit", process.returncode,
          "seconds", round(time.monotonic() - start, 3),
          "peak_rss_kib", usage.ru_maxrss, flush=True)
    failed |= process.returncode != 0
raise SystemExit(failed)
PY
```

I log dell'esecuzione originale e il JSON con tempi CPU, kernel e SHA-256 dei
due eseguibili sono conservati localmente in `.build/resource-validation/`.
Questa verifica aggiuntiva non modifica i pacchetti nativi già pubblicati.
