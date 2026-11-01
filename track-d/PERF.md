# Track D: performance log

Every number below is from the live board (i5-13500, 6 P-cores, turbo off), read from board.json. M is the kernel's peak memory in MB (10^6 bytes), S is the code lines scc counts after clang-format (LLVM style).

| Step | Change | Commit | Run ID | T (s) | M (MB) | S | K | Points |
|---|---|---|---|---|---|---|---|---|
| 0 | The reference, unchanged | | | 72.69 | | | 1 | 0 |
| 1 | C: parallel parse into a CSR adjacency, one early-stopping binary-heap Dijkstra per query, queries spread over all threads with per-thread dist arrays reset through a touched list | 24a36f8 | run-f56c55c5d17a | 1.953 | 388.2 | 293 | 5.40 | 36.6 |
| 1 (re-run) | Same step-1 code, measured again on a later push | ce2fcae | run-c0a9e0ff7719 | 1.736 | 388.8 | 293 | 5.73 | 37.9 |
| 2 (experiment, lost) | Bidirectional Dijkstra (forward on the graph, backward on a reverse CSR built in the same parallel pass), stop when topF + topB >= best; per-node query stamps (settled = top bit) replace clearing | 99c823a | run-acfbc4a96be2 | 1.327 | 936.1 | 326 | 5.41 | 36.7 |
| 3 (experiment, lost) | Same search; drop the two stamp arrays per thread: distances reset through touched lists, settled flags in a bitset (less memory per thread) | e26d36d | run-0dd99818d47a | 1.433 | 706.8 | 354 | 5.44 | 36.8 |
| 4 | 32-bit distances (UINT32_MAX = unreached; a query whose distance would pass 2^32 - 1 is redone exactly in 64 bits), no "done" array (stale heap entries skipped), answer `-1` at once when `t` has no in-edges (that query used to flood all 1.74M nodes), parse twice straight into the CSR (count, then scatter) instead of keeping edge arrays, relabel nodes in BFS order so a search touches nearby memory | 26e8cf3 | run-71fbb3125598 | 0.571 | 222.6 | 386 | 10.72 | 51.5 |

## Experiments that lost

- **Step 2, bidirectional Dijkstra (run-acfbc4a96be2): K 5.73 → 5.41.** Time did fall (1.736 s → 1.327 s), but memory rose 2.4× (388.8 MB → 936.1 MB): a whole reverse CSR plus two 64-bit distance arrays and two stamp arrays per thread for all 1.74M nodes, times 12 threads. M carries weight 0.2 against T's 0.5, so 2.4× the memory (−16% on K) ate the 1.31× speed-up (+14%), and 33 more lines cost a little more.
- **Step 3, leaner per-thread state for the same search (run-0dd99818d47a): K 5.41 → 5.44, still below step 1's 5.73.** Memory fell to 706.8 MB but time rose to 1.433 s (touched-list resets cost more than stamps), and S grew to 354.
- Both reverted to the step-1 code. Lesson: on this track per-thread state × 12 threads dominates memory; a win needs 32-bit distances and compact state, not a second graph.
