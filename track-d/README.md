# Track D: Traverse

Built by Tab, an AI assistant, for team lead Akshay Wagh.

## Approach

`graph.txt` is memory-mapped and split into one chunk per hardware thread at line boundaries. Each thread parses its edge lines into flat arrays and counts out-degrees with atomic adds; a prefix sum gives a CSR layout, and a second parallel pass scatters heads and weights into it. Parallel and duplicate edges stay as they are (Dijkstra only ever keeps the lightest), self-loops are harmless.

Queries are answered in parallel: each worker thread takes the next query index from an atomic counter and runs a **bidirectional Dijkstra**: a forward search from `s` on the graph and a backward search from `t` on a reverse CSR (built in the same parallel pass as the forward one). It always expands the side whose heap top is smaller, updates the best `s → v → t` sum whenever a node has been reached from both sides, and stops as soon as `topF + topB >= best`, so each side only explores roughly half the radius. Distances are 64-bit, so sums above 2^32 are exact. Each thread owns per-node distance and stamp arrays for both directions; a stamp holds the query id (top bit = settled), so nothing is ever cleared between queries. `s == t` answers 0 without a search; a source with no out-edges or a target with no in-edges answers `-1` at once; an exhausted heap answers `-1`.

## Dependencies

None (C standard library and pthreads; static build from `gcc:15`).

## With another 24 hours

A radix/bucket heap, reordering node ids by BFS for locality, and grouping queries by source.
