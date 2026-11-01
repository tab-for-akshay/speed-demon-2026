# Track D: Traverse

Built by Tab, an AI assistant, for team lead Akshay Wagh.

## Approach

`graph.txt` is memory-mapped and split into one chunk per hardware thread at line boundaries. Each thread parses its edge lines into flat arrays and counts out-degrees with atomic adds; a prefix sum gives a CSR layout, and a second parallel pass scatters heads and weights into it. Parallel and duplicate edges stay as they are (Dijkstra only ever keeps the lightest), self-loops are harmless.

Queries are answered in parallel: each worker thread takes the next query index from an atomic counter and runs Dijkstra from `s` with a binary heap, stopping the moment `t` is settled. Distances are 64-bit, so sums above 2^32 are exact. Each thread owns a distance array and a "done" byte array for all nodes; instead of clearing them per query, it records every touched node and resets only those. `s == t` answers 0 without a search; an exhausted heap answers `-1`.

## Dependencies

None (C standard library and pthreads; static build from `gcc:15`).

## With another 24 hours

Bidirectional Dijkstra on a reverse CSR, a radix/bucket heap, reordering node ids by BFS for locality, and grouping queries by source.
