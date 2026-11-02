# Track D: Traverse

Built by Tab, an AI assistant, for team lead Akshay Wagh.

## Approach

1. **Parse twice, no edge list.** graph.txt is mmapped and split at line ends into one chunk per thread. Pass 1 counts out-degrees (atomic adds), a prefix sum makes CSR offsets, pass 2 parses again and writes each edge straight to its slot (atomic fetch-add on the tail's cursor). An edge is one 32-bit word: the head in the low VB bits (VB = bits needed for N, 21 here) and the weight above it; a weight that does not fit (0.07% of edges here, including the 2^32 - 1 "chain" weights) stores an all-ones marker and its real value in a sorted (position, weight) side list, found by binary search. Parallel and duplicate edges stay as they are (Dijkstra keeps the lightest), and self-loops are harmless.
2. **Relabel for locality.** Node ids in the input are random, so a search would touch random memory. Each thread grows a BFS region from its own seed, claiming nodes with a CAS; regions are numbered one after another, each in BFS order, and the few nodes no region reached are numbered by a serial BFS. The packed graph is rebuilt in the new order (in parallel), and a reverse CSR is built from it for the backward search.
3. **Bidirectional Dijkstra with 32-bit labels.** Forward search on the graph, backward on the reverse graph, always expanding the side whose smallest key is smaller, stopping when the two smallest keys add up to the best meeting distance seen. Each side has a radix heap (keys never drop below the last popped one, so 33 buckets by highest differing bit) and a small open-addressing hash map of node → distance that grows from 4K slots and is cleared through the list of used slots, so a side costs ~1 MB however large N is. Queries are handed out in order of source label so threads working at the same time stay in nearby parts of the graph.
4. **Exact fallback.** A label that would pass 2^32 - 1 aborts that query to a forward-only 64-bit Dijkstra (8 of 2,000 queries here). A target with no in-edges or a source with no out-edges is answered -1 at once, without a search.

## Dependencies

None (C standard library and pthreads; static build from `gcc:15`).

## With another 24 hours

About 0.15 s of the 0.24 s now goes to building: two parses, the CSR scatter, the relabelling and the reverse CSR. A relabelling that needs fewer passes over the graph (or one computed from a sample of the edges), and offsets stored as 16-bit deltas from a block base, would cut both that time and the 61 MB the two graphs take while the searches run.

The full history, with a board run ID for every step and every experiment that lost, is in [PERF.md](PERF.md).
