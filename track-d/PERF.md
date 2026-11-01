# Track D: performance log

| Step | Change | Run ID | T (s) | M (MB) | S | K | Points |
|---|---|---|---|---|---|---|---|
| 0 | The reference, unchanged | | 72.69 | | | 1 | 0 |
| 1 | C: parallel parse into a CSR adjacency, one early-stopping binary-heap Dijkstra per query, queries spread over all threads with per-thread dist arrays reset through a touched list | run-f56c55c5d17a | 1.953 | 388.2 | 293 | 5.40 | 36.6 |
