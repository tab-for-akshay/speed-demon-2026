# Track A: performance log

Every number below is from the live board (i5-13500, 6 P-cores / 12 threads, turbo off), read from board.json. M is the kernel's peak memory in MB (10^6 bytes), S is the code lines scc counts after clang-format (LLVM style); an S written ~N was not recorded at the time and is back-computed from the run's T, M and K. Points = 50 x log10(K). Every row is one board run; "re-run" rows are the same code measured again by a later push (that push changed another track).

Best so far: step 5, K 31.93 (run-4283afd0c7da), from K 9.87 at step 1. Its T moves between 0.038 s and 0.062 s from run to run with the code unchanged; see "Same code, many runs".

| Step | Change | Commit | Run ID | T (s) | M (MB) | S | K | Points |
|---|---|---|---|---|---|---|---|---|
| 0 | The reference, unchanged | | | 32.05 | | | 1 | 0 |
| 1 | C, single thread: read whole file, hand-written parser, integer cents, per-hour arrays, percentiles by selection | 189a99b | run-d5112975cab2 | 0.349 | 43.7 | 415 | 9.87 | 49.7 |
| 2 | Parse in parallel on all hardware threads (chunk split at line ends), per-hour output in parallel | 666e530 | run-16b53dc23586 | 0.118 | 59.4 | 414 | 15.97 | 60.2 |
| 2 (re-run) | Same step-2 code, measured again on a later push | ce2fcae | run-20bc2968e315 | 0.104 | 56.5 | 414 | 17.20 | 61.8 |
| 3 (experiment, lost) | Merge in parallel: per-line global group keys, counts and error counts with atomic adds, then a parallel scatter of latencies (was one serial pass over all 3.1M lines) | e26d36d | run-d3d10deaef60 | 0.118 | 81.2 | 437 | 14.86 | 58.6 |
| 2 (re-run) | Same step-2 code, measured again (the push changed track C) | b10d5a9 | run-4a559bb9eb24 | 0.1119 | 60.5 | 414 | 16.35 | 60.7 |
| 2.1 | All tracks: one worker per CPU the container may use (`sched_getaffinity`: 12 on the board) instead of every CPU online (20) | 515cbb6 | run-713542d9da68 | 0.0857 | 50.7 | ~418 | 19.33 | 64.3 |
| 2.1 (re-run) | Same code | a3a05ad | run-0b7a9ef8a4e1 | 0.0754 | 45.2 | ~418 | 21.08 | 66.2 |
| 2.1 (re-run) | Same code | e08d8d2 | run-c62e95604ebb | 0.0744 | 45.7 | ~418 | 21.18 | 66.3 |
| 4 | Rewrite: mmap; one shared lock-free endpoint table (64-bit hash of the bytes read 8 at a time, slot = endpoint id, CAS to insert); each thread appends (latency, endpoint, 5xx) to its own chain of blocks per hour; output gathers, sorts and formats each hour in parallel. No merge pass at all | b06eda7 | run-9b5849fedd15 | 0.0431 | 56.1 | ~316 | 27.85 | 72.2 |
| 5 | Fix a race in step 4 (below); block pools get `MADV_NOHUGEPAGE` so only touched pages count; finished hours are written in order as they complete instead of all held until the end | 88c8aaa | run-177150f9631b | 0.0419 | 41.1 | 326 | 29.93 | 73.8 |
| 6 (experiment, lost) | Method length from a 64-entry table, endpoint end found 32 bytes at a time with AVX2 | 43de3a8 | run-0b454683cc02 | 0.0476 | 38.4 | 346 | 28.20 | 72.5 |
| 5 (re-run) | Back to step-5 code | 2dce930 | run-c7f9e21896c7 | 0.0380 | 38.2 | 326 | 31.88 | 75.2 |
| 7 (experiment, lost) | Latency parsed 8 bytes at a time (SWAR) | 9f735ce | run-5ea2257b46f3 | 0.0486 | 37.8 | 345 | 27.99 | 72.4 |
| 5 (re-run) | Same step-5 code (the push changed track C or D) | 83341cc | run-8475249fba42 | 0.0509 | 38.5 | 326 | 27.51 | 72.0 |
| 5 (re-run) | Same step-5 code (the push changed track C or D) | efa4906 | run-cd7e8a3fb31a | 0.0381 | 39.6 | 326 | 31.62 | 75.0 |
| 5 (re-run) | Same step-5 code (the push changed track C or D) | 0f08da0 | run-5f83134d4662 | 0.0615 | 40.4 | 326 | 24.77 | 69.7 |
| 5 (re-run) | Same step-5 code (the push changed track C or D) | 7590aff | run-4bdf3ef163c3 | 0.0382 | 38.7 | 326 | 31.70 | 75.0 |
| 5 (re-run) | Same step-5 code (the push changed track C or D) | aa34151 | run-0afda96d93c6 | 0.0386 | 38.0 | 326 | 31.67 | 75.0 |
| 5 (re-run) | Same step-5 code (the push changed track C or D) | cedf7ea | run-1ba182d56b3f | 0.0555 | 45.5 | 326 | 25.48 | 70.3 |
| 5 (re-run) | Same step-5 code (the push changed track C or D) | 14ef17c | run-d5c0e56e140c | 0.0487 | 39.6 | 326 | 27.97 | 72.3 |
| 5 (re-run) | Same step-5 code (the push changed track C or D) | 39e7f02 | run-4283afd0c7da | 0.0376 | 38.9 | 326 | 31.93 | 75.2 |
| 5 (re-run) | Same step-5 code (the push changed track C or D) | 28f932d | run-582d6f1e2cde | 0.0495 | 38.4 | 326 | 27.90 | 72.3 |
| 5 (re-run) | Same step-5 code (the push changed track C or D) | de0fbbd | run-0443dfba3868 | 0.0386 | 38.4 | 326 | 31.59 | 75.0 |
| 7 (re-run) | Step-7 code measured again: with 12 runs of step 5 showing two clusters (below), one run could not decide it | 021d538 | run-537fe27f40d9 | 0.0371 | 38.5 | 345 | 31.92 | 75.2 |


## Same code, many runs: how noisy is T on this track?

Every push re-runs all three tracks, so the step-5 code was measured 12 times while I worked on C and D. Sorted T (s): 0.0376, 0.0380, 0.0381, 0.0382, 0.0386, 0.0386, 0.0419, 0.0487, 0.0495, 0.0509, 0.0555, 0.0615. Two clusters: six runs at 0.038 s, five at 0.049-0.062 s, one between, with M steady at 38-41 MB. Tracks C and D in the same pushes moved by 2-3%, so the slow cluster is specific to this track. My best guess is the 9.7 MB output: A is the only track that writes megabytes, and the runs are short enough that a stall in the page-cache write path shows up. I have not proven it. The practical consequence: on this track one run cannot separate a 10% change from noise, which matters for steps 6 and 7 below.

## How the final code works

1. **Split.** The log is mmapped and cut into one chunk per thread (12 on the board), each cut moved forward to a line end.
2. **Parse.** Each line's hour comes from its timestamp: the date is converted to days only when it differs from the previous line's date (lines arrive roughly in time order), so most lines cost two digit reads. The endpoint is hashed 8 bytes at a time into a 64-bit key and looked up in one shared lock-free table of 4,096 slots (1,842 endpoints); the slot index is the endpoint's id. A new endpoint is inserted with a CAS, and only the thread whose CAS wins records the endpoint's bytes.
3. **Store.** Each thread appends (latency, endpoint id | 5xx bit) to its own chain of 2,048-entry blocks per hour: 6 bytes per line, no sharing, no locks, no merge pass.
4. **Output.** Threads take hours from an atomic counter. For an hour they count lines per endpoint across all threads' blocks, scatter latencies into one array grouped by endpoint, sort each group (insertion sort under 64 values, else 4-pass LSD radix), and format count, errors, p50/p95/p99. Finished hours are written in hour order as soon as every earlier hour is done, so only a few hours of text are held at once.

## A race in step 4

Step 4 passed every gate, but a stress test I ran afterwards (the same input at 1 to 16 threads, outputs compared byte for byte) showed rare differences. In the endpoint table a thread stored the endpoint's bytes pointer *before* its CAS on the slot. When two threads raced for the same empty slot with *different* endpoints, the loser's pointer could overwrite the winner's, and that slot then printed under the wrong name. It needs two new endpoints hashing to the same slot at the same moment, so it was rare. Step 5 lets only the CAS winner record the bytes, and they are read only after the threads join. The stress test has been run on every A change since.

## Experiments that lost

- **Step 3, parallel merge (run-d3d10deaef60): K 17.20 → 14.86.** It was slower (0.104 s → 0.118 s), not faster: the atomic adds on shared per-group counters contend between threads (the hot endpoints are a heavy Zipf head, so many threads hit the same counters), and the extra per-line global key array raised peak memory from 56.5 MB to 81.2 MB. It also added 23 lines. Reverted to the step-2 code in the next commit. Lesson: on this data the serial merge was not the bottleneck worth paying memory for; the next attempt has to cut memory and passes, not add threads to a cheap pass.
- **Step 6, method-length table and AVX2 endpoint scan (run-0b454683cc02): K 29.93 → 28.20 on its one run.** T 0.0476 s, which falls in the slow cluster above, so I cannot claim it was slower; it was not visibly faster, and it added 20 lines (S 326 → 346, −0.9% on K). Reverted.
- **Step 7, SWAR latency parsing (run-5ea2257b46f3): K 31.88 → 27.99 on its one run.** Same story: T 0.0486 s (slow cluster), +19 lines. The same SWAR parse won on track C, where it replaced three integer loops per record; here it replaces one, and the line is dominated by the endpoint lookup. Reverted.
- **Not pushed: quickselect for the three percentiles instead of sorting each group.** Locally the output phase went from 0.063 s to 0.077 s (single thread): three branchy partitions per group lose to a branch-free radix sort.
