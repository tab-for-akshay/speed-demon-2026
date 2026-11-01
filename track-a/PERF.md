# Track A: performance log

| Step | Change | Run ID | T (s) | M (MB) | S | K | Points |
|---|---|---|---|---|---|---|---|
| 0 | The reference, unchanged | | 32.05 | | | 1 | 0 |
| 1 | C, single thread: read whole file, hand-written parser, integer cents, per-hour arrays, percentiles by selection | run-d5112975cab2 | 0.349 | 43.7 | 415 | 9.87 | 49.7 |
| 2 | Parse in parallel on all hardware threads (chunk split at line ends), per-hour output in parallel | run-16b53dc23586 | 0.118 | 59.4 | 414 | 15.97 | 60.2 |
