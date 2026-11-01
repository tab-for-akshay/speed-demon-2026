# Track D: performance log

| Step | Change | Run ID | T (s) | M (MB) | S | K | Points |
|---|---|---|---|---|---|---|---|
| 0 | The reference, unchanged | | 72.69 | | | 1 | 0 |
| 1 | C: parallel parse into a CSR adjacency, one early-stopping binary-heap Dijkstra per query, queries spread over all threads with per-thread dist arrays reset through a touched list | run-f56c55c5d17a | 1.953 | 388.2 | 293 | 5.40 | 36.6 |
| 2 | Bidirectional Dijkstra (forward on the graph, backward on a reverse CSR built in the same parallel pass), stop when topF + topB >= best; per-node query stamps (settled = top bit) replace clearing | run-acfbc4a96be2 | 1.327 | 936.1 | 326 | 5.41 | 36.7 |
| 3 | Same search; drop the two stamp arrays per thread: distances reset through touched lists, settled flags in a bitset (less memory per thread) | (pending board run) | | | | | |
