# Track A: performance log

Every number below is from the live board (i5-13500, 6 P-cores, turbo off), read from board.json. M is the kernel's peak memory in MB (10^6 bytes), S is the code lines scc counts after clang-format (LLVM style).

| Step | Change | Commit | Run ID | T (s) | M (MB) | S | K | Points |
|---|---|---|---|---|---|---|---|---|
| 0 | The reference, unchanged | | | 32.05 | | | 1 | 0 |
| 1 | C, single thread: read whole file, hand-written parser, integer cents, per-hour arrays, percentiles by selection | 189a99b | run-d5112975cab2 | 0.349 | 43.7 | 415 | 9.87 | 49.7 |
| 2 | Parse in parallel on all hardware threads (chunk split at line ends), per-hour output in parallel | 666e530 | run-16b53dc23586 | 0.118 | 59.4 | 414 | 15.97 | 60.2 |
| 2 (re-run) | Same step-2 code, measured again on a later push | ce2fcae | run-20bc2968e315 | 0.104 | 56.5 | 414 | 17.20 | 61.8 |
| 3 (experiment, lost) | Merge in parallel: per-line global group keys, counts and error counts with atomic adds, then a parallel scatter of latencies (was one serial pass over all 3.1M lines) | e26d36d | run-d3d10deaef60 | 0.118 | 81.2 | 437 | 14.86 | 58.6 |

## Experiments that lost

- **Step 3, parallel merge (run-d3d10deaef60): K 17.20 → 14.86.** It was slower (0.104 s → 0.118 s), not faster: the atomic adds on shared per-group counters contend between threads (the hot endpoints are a heavy Zipf head, so many threads hit the same counters), and the extra per-line global key array raised peak memory from 56.5 MB to 81.2 MB. It also added 23 lines. Reverted to the step-2 code in the next commit. Lesson: on this data the serial merge was not the bottleneck worth paying memory for; the next attempt has to cut memory and passes, not add threads to a cheap pass.
