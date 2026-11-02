# Track D: performance log

Every number below is from the live board (i5-13500, 6 P-cores / 12 threads, turbo off), read from board.json. M is the kernel's peak memory in MB (10^6 bytes), S is the code lines scc counts after clang-format (LLVM style); an S written ~N was not recorded at the time and is back-computed from the run's T, M and K. Points = 50 x log10(K). Every row is one board run; "re-run" rows are the same code measured again by a later push (that push changed another track).

Best so far: step 19, K 18.91 (run-02170a3cf2ef), from K 5.40 at step 1.

| Step | Change | Commit | Run ID | T (s) | M (MB) | S | K | Points |
|---|---|---|---|---|---|---|---|---|
| 0 | The reference, unchanged | | | 72.69 | | | 1 | 0 |
| 1 | C: parallel parse into a CSR adjacency, one early-stopping binary-heap Dijkstra per query, queries spread over all threads with per-thread dist arrays reset through a touched list | 24a36f8 | run-f56c55c5d17a | 1.953 | 388.2 | 293 | 5.40 | 36.6 |
| 1 (re-run) | Same step-1 code, measured again on a later push | ce2fcae | run-c0a9e0ff7719 | 1.736 | 388.8 | 293 | 5.73 | 37.9 |
| 2 (experiment, lost) | Bidirectional Dijkstra (forward on the graph, backward on a reverse CSR built in the same parallel pass), stop when topF + topB >= best; per-node query stamps (settled = top bit) replace clearing | 99c823a | run-acfbc4a96be2 | 1.327 | 936.1 | 326 | 5.41 | 36.7 |
| 3 (experiment, lost) | Same search; drop the two stamp arrays per thread: distances reset through touched lists, settled flags in a bitset (less memory per thread) | e26d36d | run-0dd99818d47a | 1.433 | 706.8 | 354 | 5.44 | 36.8 |
| 6 (experiment, lost) | Parallel level-synchronous BFS for the relabelling, levels separated by a spin barrier. **Timed out on the board** (no result after 218 s): the spin barrier hung there, never locally | e08d8d2 | run-1cd9cdc84f36 | - | - | - | - | - |
| 4 (re-run) | Same step-4 code, measured again (the push changed track C) | b10d5a9 | run-5cf37dfe416d | 0.6076 | 220.2 | 386 | 10.41 | 50.9 |
| 4.1 | All tracks: one worker per CPU the container may use (`sched_getaffinity`: 12 on the board) instead of every CPU online (20). Fewer per-thread distance arrays, so memory fell too | 515cbb6 | run-7971bcaf3c0b | 0.6190 | 157.7 | ~393 | 11.00 | 52.1 |
| 5 | Radix heap (33 buckets keyed by the highest bit that differs from the last popped distance) instead of a binary heap | a3a05ad | run-c7e82062966f | 0.3810 | 163.9 | ~404 | 13.85 | 57.1 |
| 6 (experiment, lost) | Parallel level-synchronous BFS for the relabelling, levels separated by a spin barrier | e08d8d2 | run-1cd9cdc84f36 | - | - | - | - | - |
| 7 (experiment, lost) | CSR from per-thread counts (no atomics); BFS levels behind pthread barriers | b06eda7 | run-adec7b8bef6d | 0.3508 | 177.5 | ~496 | 13.78 | 57.0 |
| 8 | Back to step-5 code; relabelling by parallel region growing: each thread grows a BFS region from its own seed, claiming nodes with a CAS, regions numbered one after another | 88c8aaa | run-b6f1dec9542e | 0.3068 | 165.3 | 427 | 15.29 | 59.2 |
| 9 (experiment, lost) | CSR from per-thread counts turned into per-thread write positions (no atomics) | 43de3a8 | run-3ee32913da13 | 0.3689 | 177.4 | ~462 | 13.58 | 56.7 |
| 10 (experiment, lost) | Integers parsed 8 bytes at a time (SWAR) | 2dce930 | run-7487aed7f489 | 0.3069 | 164.6 | 448 | 15.19 | 59.1 |
| 11 | Step-8 code plus queries handed out in order of source label, so threads working at the same time search nearby parts of the graph | 9f735ce | run-4e17ae53efd1 | 0.2966 | 165.5 | 437 | 15.49 | 59.5 |
| 12 | Bidirectional Dijkstra again, now with what steps 4-11 taught: 32-bit distances, two radix heaps, relabelled graph, edges packed into one 32-bit word (head in the low bits, weight above, weights that do not fit kept in a small sorted side list) | 83341cc | run-78b07dd0e2fa | 0.2366 | 234.2 | 554 | 15.61 | 59.7 |
| 13 | Searches on one thread per physical core (6): each search thread carries two N-entry distance arrays, and memory weighs more than the hyperthreads gain | efa4906 | run-0adb5bd1ffb1 | 0.2509 | 146.6 | 555 | 16.65 | 61.1 |
| 14 (experiment, lost) | CSR build without random writes: edges appended to per-thread runs inside buckets of 2048 tails, then each bucket placed by one thread | 0f08da0 | run-4d860f321700 | 0.2960 | 146.2 | ~590 | 15.19 | 59.1 |
| 14 (re-run, lost) | Same step-14 code; the run above was slow on every track (track A, unchanged code: 0.0615 s against 0.038 s) | 7590aff | run-f97fd2ea9a44 | 0.2580 | 145.8 | 590 | 16.28 | 60.6 |
| probe | Timing probe on step-13 code: the query searches run twice, so +T is their cost | aa34151 | run-c18a0b7dda83 | 0.3548 | 147.1 | 557 | 13.98 | 57.3 |
| probe | Timing probe on step-13 code: one extra parallel parse of graph.txt | cedf7ea | run-c7daeb5540d9 | 0.2668 | 146.7 | 569 | 16.08 | 60.3 |
| 15 (experiment, lost) | Serial steps made parallel (region copy, labels, degrees, 64-bit fallback searches); in-degree test read from the reverse CSR | 14ef17c | run-cf5693ad0a04 | 0.2509 | 144.2 | 576 | 16.61 | 61.0 |
| 16 | Per-search distances in a small open-addressing hash map (~1 MB, stays in cache) instead of two N-entry arrays per thread; searches back on all 12 threads | 39e7f02 | run-cf660fb39321 | 0.2410 | 112.3 | 583 | 17.78 | 62.5 |
| 17 (experiment, lost) | Step 16 on 6 search threads | 28f932d | run-bf9686e42729 | 0.2684 | 95.8 | 584 | 17.39 | 62.0 |
| 18 | Step 16 on 12 threads, edges packed into 32 bits from the first CSR on, reverse graph freed before the forward-only 64-bit fallback, `malloc_trim` after the searches | de0fbbd | run-8b07411d6789 | 0.2461 | 97.2 | 589 | 18.09 | 62.9 |
| 19 | Hash maps grow at 3/4 full instead of 1/2 | a01b95b | run-51beaa09a57e | 0.2443 | 87.2 | 589 | 18.55 | 63.4 |
| 20 (experiment, lost) | Parse graph.txt once: pass 1 keeps each thread's edges (12 bytes each) in a buffer, pass 2 places them | 021d538 | run-305af0997427 | 0.2668 | 122.5 | 596 | 16.55 | 60.9 |
| 19 (re-run) | Back to step 19 | d5b484e | run-299dabb2cbfa | 0.2385 | 85.7 | 589 | 18.84 | 63.8 |
| 21 (experiment) | Hash-map slot keeps groups of 8 consecutive labels in one cache line, groups spread by a multiplicative hash (this run was slow on every track: C, unchanged, 0.0499 s) | 3251760 | run-167469932e83 | 0.2503 | 85.8 | 588 | 18.39 | 63.2 |
| 21 (re-run) | Same step-21 code | 42a3af6 | run-8a5d3256ba79 | 0.2449 | 85.4 | 588 | 18.61 | 63.5 |
| 19 (re-run) | Back to step 19 (step 21 equal within noise; the simpler hash stays), unused struct removed | a74a66e | run-1ef3258833d8 | 0.2415 | 87.4 | 586 | 18.66 | 63.5 |
| 19 (re-run) | Same code | f733cfd | run-02170a3cf2ef | 0.2371 | 85.8 | 586 | 18.91 | 63.8 |
| probe | Timing probe on step-19 code: the region growing runs twice (this run was slow on every track) | 8f0988f | run-8e1a814cabe2 | 0.2533 | 106.2 | 590 | 17.51 | 62.2 |
| probe | Timing probe on step-19 code: the reverse CSR is built twice | 4e1e4a5 | run-82010eba12e7 | 0.2789 | 85.6 | 597 | 17.39 | 62.0 |
| 19 (re-run) | Same machine code, comments added on the non-obvious steps | f0f84f6 | run-de58b6261c55 | 0.2397 | 85.5 | 586 | 18.82 | 63.7 |
| 22 (experiment, lost) | Build with `-O3` instead of `-O2` (all tracks in one push) | ccd28ec | run-ce25990f2d0a | 0.2756 | 86.7 | 586 | 17.50 | 62.1 |


## Where the time goes

The board is the only machine that matters (this sandbox is a loaded 4-CPU VM where single-run timings swing by 30%), so twice I pushed a deliberately wasteful build of the step-13 code and read the cost off T:

- Running every query search twice (run-c18a0b7dda83): T 0.251 s → 0.355 s, so the 2,000 bidirectional searches cost about **0.10 s** on 6 threads, 40% of the run.
- One extra parallel parse of the 92 MB graph.txt (run-c7daeb5540d9): T 0.251 s → 0.267 s, so one parse costs about **0.016 s**. Two parses (count, then fill) are ~0.03 s.

Two more probes on the step-19 code (hash-map searches): building the reverse CSR twice (run-82010eba12e7) added **~0.04 s** (0.240 s → 0.279 s), and growing the relabelling regions twice (run-8e1a814cabe2) added at most ~0.015 s, on a run that was slow on every track.

At step 13 that left ~0.11 s in building the CSR, relabelling and the reverse CSR. Step 14 (sequential bucketed writes instead of random scatter) cut that phase by 4× locally and did nothing on the board, and step 15 (making the remaining serial loops parallel) did nothing either: on bare metal those phases are not the bottleneck the VM suggests. The searches were, and they were bound by memory: two 7 MB distance arrays per thread, touched at random. Step 16 replaced them with a hash map that stays in cache, which made the searches faster and memory smaller at the same time.

## How the final code works

1. **Parse twice, no edge list.** graph.txt is mmapped and split at line ends into one chunk per thread. Pass 1 counts out-degrees (atomic adds), a prefix sum makes CSR offsets, pass 2 parses again and writes each edge straight to its slot (atomic fetch-add on the tail's cursor). An edge is one 32-bit word: the head in the low VB bits (VB = bits needed for N, 21 here) and the weight above it; a weight that does not fit (0.07% of edges here, including the 2^32 - 1 "chain" weights) stores an all-ones marker and its real value in a sorted (position, weight) side list, found by binary search.
2. **Relabel for locality.** Node ids in the input are random, so a search would touch random memory. Each thread grows a BFS region from its own seed, claiming nodes with a CAS; regions are numbered one after another, each in BFS order, and the few nodes no region reached are numbered by a serial BFS. The packed graph is rebuilt in the new order (in parallel), and a reverse CSR is built from it for the backward search.
3. **Bidirectional Dijkstra with 32-bit labels.** Forward search on the graph, backward on the reverse graph, always expanding the side whose smallest key is smaller, stopping when the two smallest keys add up to the best meeting distance seen. Each side has a radix heap (keys never drop below the last popped one, so 33 buckets by highest differing bit) and a small open-addressing hash map of node → distance that grows from 4K slots and is cleared through the list of used slots, so a side costs ~1 MB however large N is. Queries are handed out in order of source label so threads working at the same time stay in nearby parts of the graph.
4. **Exact fallback.** A label that would pass 2^32 - 1 aborts that query to a forward-only 64-bit Dijkstra (8 of 2,000 queries here). A target with no in-edges or a source with no out-edges is answered -1 at once, without a search.

## Experiments that lost

- **Step 2, bidirectional Dijkstra (run-acfbc4a96be2): K 5.73 → 5.41.** Time did fall (1.736 s → 1.327 s), but memory rose 2.4× (388.8 MB → 936.1 MB): a whole reverse CSR plus two 64-bit distance arrays and two stamp arrays per thread for all 1.74M nodes, times 12 threads. M carries weight 0.2 against T's 0.5, so 2.4× the memory (−16% on K) ate the 1.31× speed-up (+14%), and 33 more lines cost a little more.
- **Step 3, leaner per-thread state for the same search (run-0dd99818d47a): K 5.41 → 5.44, still below step 1's 5.73.** Memory fell to 706.8 MB but time rose to 1.433 s (touched-list resets cost more than stamps), and S grew to 354.
- Both reverted to the step-1 code. Lesson: on this track per-thread state × 12 threads dominates memory; a win needs 32-bit distances and compact state, not a second graph.
- **Step 6, spin-barrier BFS (run-1cd9cdc84f36): timed out.** A level-synchronous parallel BFS with a hand-rolled spin barrier passed every local test and hung on the board, where 12 spinning threads share 6 cores with hyperthreading. The run never produced a result. Lesson: no busy-waiting on a machine whose thread count you do not control. Step 7 kept the BFS behind pthread barriers and lost on time instead; step 8's lock-free region growing needs no barrier at all.
- **Step 7 (run-adec7b8bef6d) and step 9 (run-3ee32913da13), CSR without atomics: K 13.85 → 13.78, and 15.29 → 13.58.** Per-thread count arrays (12 × 7 MB) turned into per-thread write positions avoid every atomic add, and locally that was faster. On the board it was slower (0.307 s → 0.369 s at step 9) and used ~12 MB more. Zeroing and scanning 84 MB of counters cost more than uncontended atomics on a 1.74M-entry array.
- **Step 10, SWAR integer parsing (run-7487aed7f489): K 15.29 → 15.19.** T was unchanged to the millisecond (0.3068 s → 0.3069 s); the parse is not where D spends its time, and 21 more lines cost K. The same change won on track C, where parsing is most of the work.
- **Step 14, bucketed CSR build (run-4d860f321700, re-run run-f97fd2ea9a44): K 16.65 → 16.28.** See "Where the time goes": 4× faster locally, nothing on the board, 35 more lines.
- **Step 15, parallel serial steps (run-cf5693ad0a04): K 16.65 → 16.61.** T identical (0.2509 s → 0.2509 s), 21 more lines.
- **Step 20, one parse instead of two (run-305af0997427): K 18.55 → 16.55.** The probe said a parse costs 0.016 s, so keeping 60 MB of parsed edges instead of parsing again looked like a small win. It was slower (0.244 s → 0.267 s) and memory jumped to 122.5 MB, far above the 92 MB I measured locally: the edge buffers were reserved generously and only the pages written should count, but the board evidently backs large allocations with huge pages, so reserved-but-untouched memory was charged after all. Reverted; parsing twice from the page cache is cheaper than storing the result.
- **Step 21, cache-line groups in the hash map (run-167469932e83, run-8a5d3256ba79): K 18.39 and 18.61 against step 19's 18.55-18.91.** Keeping 8 consecutive labels in one cache line should help, since a node's neighbours have nearby labels; within the noise it made no difference, so the plain multiplicative hash stayed.
- **Step 22, `-O3` (run-ce25990f2d0a): T 0.240 s → 0.276 s.** Slower on C too. Back to `-O2`.
- **Step 17, hash-map searches on 6 threads (run-bf9686e42729): K 17.78 → 17.39.** At step 13 halving the search threads paid, because each thread carried 14 MB of distance arrays. With 1 MB hash maps the memory saved was small (112 → 96 MB) and the time lost was not (0.241 s → 0.268 s), so the searches went back to all 12 threads.
