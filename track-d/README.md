# Track D: Traverse

Built by Tab, an AI assistant, for team lead Akshay Wagh.

## Approach

`graph.txt` is memory-mapped and split into one chunk per hardware thread at line boundaries. The edge lines are parsed twice, straight into a CSR layout and with no edge arrays in between: the first pass counts out-degrees with atomic adds and marks every node that has an in-edge, a prefix sum gives the offsets, and the second pass parses again and scatters `(head, weight)` pairs into place. Parallel and duplicate edges stay as they are (Dijkstra keeps the lightest), self-loops are harmless.

The generator relabels node ids at random, so neighbours sit far apart in memory. The program relabels them back: a BFS over the CSR numbers the nodes in visiting order (nodes it cannot reach go last) and the CSR is rebuilt in that order in parallel, so a search now walks mostly nearby cache lines.

Queries run in parallel: each thread takes the next query from an atomic counter and runs Dijkstra from `s` with a binary heap of packed `(distance << 32 | node)` keys, stopping when `t` is popped. Distances are 32-bit (`UINT32_MAX` = unreached) and reset through a touched list; stale heap entries are skipped instead of keeping a "done" array. A query whose distance would reach 2^32 - 1 (the islands' heavy chain, the heavy out-edges of source-only nodes) is flagged and redone exactly with 64-bit distances after the parallel phase. `s == t` answers 0; a source with no out-edges or a target with no in-edges answers `-1` without a search (the second case used to flood all 1.74M nodes).

## Dependencies

None (C standard library and pthreads; static build from `gcc:15`).

## With another 24 hours

A radix/bucket heap, bidirectional search on a compact reverse graph, and less per-thread memory (distances in a small hash table instead of one full array per thread).
