# Track C: performance log

Every number below is from the live board (i5-13500, 6 P-cores / 12 threads, turbo off), read from board.json. M is the kernel's peak memory in MB (10^6 bytes), S is the code lines scc counts after clang-format (LLVM style); an S written ~N was not recorded at the time and is back-computed from the run's T, M and K. Points = 50 x log10(K). Every row is one board run; "re-run" rows are the same code measured again by a later push (that push changed another track).

Best so far: step 7, K 32.83 (run-ce057353460e), from K 2.99 at step 1.

| Step | Change | Commit | Run ID | T (s) | M (MB) | S | K | Points |
|---|---|---|---|---|---|---|---|---|
| 0 | The reference, unchanged | | | 14.44 | | | 1 | 0 |
| 1 | C: mmap, parallel chunked parse (safe record boundaries from spec 2.2), per-thread name interning, sort-merge of names, bitset distinct-name count | 62fbaf7 | run-091a150f2d29 | 1.515 | 158.7 | 413 | 2.99 | 23.8 |
| 2 | Merge names through one global hash (hashes reused from the threads) instead of sort + binary search; mark distinct-name bits in parallel with atomic OR | ce2fcae | run-a86c7c1528ee | 0.215 | 139.4 | 430 | 8.08 | 45.4 |
| 2 (re-run) | Same step-2 code, measured again on a later push | e26d36d | run-a355c43ed6c7 | 0.205 | 139.9 | 430 | 8.27 | 45.9 |
| 3 | One shared lock-free name table (slot = name id) and per-country bitsets set while parsing; no per-record pairs, no merge pass | b10d5a9 | run-710c09cbf8cd | 0.1221 | 33.5 | 291 | 15.13 | 59.0 |
| 3.1 | All tracks: one worker per CPU the container may use (`sched_getaffinity`: 12 on the board) instead of every CPU online (20) | 515cbb6 | run-b41713b80f98 | 0.1103 | 30.9 | ~296 | 16.13 | 60.4 |
| 3.1 (re-run) | Same code | a3a05ad | run-dc9a3852b0c3 | 0.1115 | 22.1 | ~295 | 17.15 | 61.7 |
| 4 | Names keyed by a 64-bit hash of the unescaped bytes read 8 at a time, in a 2^17-slot table (no byte compares); bitsets sized to the table; country codes parsed once into base-26 indexes | e08d8d2 | run-dec43ae9ad75 | 0.0527 | 10.5 | ~296 | 28.95 | 73.1 |
| 5 (experiment, lost) | Bits set per country slice from per-round pair buffers, no atomics (rounds separated by barriers) | b06eda7 | run-d37dde1ff289 | 0.0842 | 25.9 | ~323 | 18.87 | 63.8 |
| 6 | Back to step 4, software-pipelined: prefetch this record's table slot, look up the previous record and prefetch its bit word, set the bit of the one before, so both cache misses overlap with parsing | 88c8aaa | run-7502f82377f9 | 0.0447 | 10.3 | 320 | 31.21 | 74.7 |
| 6 (re-run) | Same code | 43de3a8 | run-2d260eaa713d | 0.0451 | 9.5 | 320 | 31.58 | 75.0 |
| 7 | Integers (id, region, amount) parsed 8 bytes at a time with SWAR: find the first non-digit with one subtract/or/and, convert with three multiplies | 2dce930 | run-96f27a93dbe7 | 0.0413 | 9.6 | 335 | 32.70 | 75.7 |
| 7 (re-run) | Same step-7 code (the push changed track D) | 9f735ce | run-3e7aa1b6a461 | 0.0410 | 9.7 | 335 | 32.73 | 75.7 |
| 7 (re-run) | Same step-7 code (the push changed track D) | 83341cc | run-11f6df640c90 | 0.0417 | 10.4 | 335 | 32.05 | 75.3 |
| 7 (re-run) | Same step-7 code (the push changed track D) | efa4906 | run-ce057353460e | 0.0411 | 9.5 | 335 | 32.83 | 75.8 |
| 7 (re-run) | Same step-7 code (the push changed track D) | 0f08da0 | run-879f31fbc8f0 | 0.0445 | 10.1 | 335 | 31.17 | 74.7 |
| 8 (experiment, lost) | Quoted fields scanned 32 bytes at a time with AVX2 instead of one `memchr` call per quote | 7590aff | run-cf5f74814d67 | 0.0409 | 10.1 | 357 | 32.24 | 75.4 |
| 7 (re-run) | Same step-7 code (the push changed track D) | aa34151 | run-dcb8c1bd2b75 | 0.0422 | 10.4 | 335 | 31.87 | 75.2 |
| 7 (re-run) | Same step-7 code (the push changed track D) | cedf7ea | run-72d87b788946 | 0.0414 | 9.5 | 335 | 32.71 | 75.7 |
| 7 (re-run) | Same step-7 code (the push changed track D) | 14ef17c | run-b2680b18d54e | 0.0423 | 10.0 | 335 | 32.05 | 75.3 |
| 7 (re-run) | Same step-7 code (the push changed track D) | 39e7f02 | run-18967dc055b8 | 0.0418 | 10.0 | 335 | 32.22 | 75.4 |
| 7 (re-run) | Same step-7 code (the push changed track D) | 28f932d | run-28baf08bddda | 0.0419 | 9.9 | 335 | 32.25 | 75.4 |
| 7 (re-run) | Same step-7 code (the push changed track D) | de0fbbd | run-ea36ee3696a6 | 0.0409 | 10.2 | 335 | 32.43 | 75.5 |
| 7 (re-run) | Same step-7 code (the push changed track A or D) | a01b95b | run-0eeb77125c33 | 0.0422 | 10.2 | 335 | 31.95 | 75.2 |
| 7 (re-run) | Same step-7 code (the push changed track A or D) | 021d538 | run-de33faf7ca1a | 0.0469 | 10.0 | 335 | 30.43 | 74.2 |
| 7 (re-run) | Same step-7 code (the push changed track A or D) | d5b484e | run-126d9417681d | 0.0412 | 10.4 | 335 | 32.24 | 75.4 |
| 7 (re-run) | Same step-7 code (the push changed track A or D) | 3251760 | run-490c0ff1f5b7 | 0.0499 | 10.1 | 335 | 29.45 | 73.5 |
| 9 (experiment, lost) | Lookup pipeline spaced two records per stage (slot prefetched, looked up two records later, bit set two after that) | 42a3af6 | run-e59cac1be816 | 0.0438 | 10.5 | 338 | 31.20 | 74.7 |
| 7 (re-run) | Back to step 7 | a74a66e | run-5eaab58cb008 | 0.0411 | 10.0 | 335 | 32.53 | 75.6 |
| 10 (experiment, lost) | Structural index: per 64-byte block, AVX2 masks of quotes, commas and LFs, a prefix XOR of the quote bits for inside-quote bytes; fields from comma/LF positions, quoted lengths from a popcount of quote bits | f733cfd | run-58441018d3f7 | 0.0464 | 9.7 | 383 | 30.20 | 74.0 |
| 7 (re-run) | Same step-7 code (the push changed track A or D) | 8f0988f | run-8804c52fb35b | 0.0507 | 10.5 | 335 | 29.00 | 73.1 |
| 7 (re-run) | Same step-7 code (the push changed track A or D) | 4e1e4a5 | run-dbb63956f952 | 0.0414 | 10.0 | 335 | 32.43 | 75.5 |
| 7 (re-run) | Same step-7 code (the push changed track A or D) | f0f84f6 | run-c23ba920dfd4 | 0.0407 | 10.0 | 335 | 32.69 | 75.7 |
| 11 (experiment, lost) | Build with `-O3` instead of `-O2` (all tracks in one push) | ccd28ec | run-6b8d679a8817 | 0.0421 | 10.3 | 335 | 31.94 | 75.2 |


## How the final code works

1. **Regions first.** regions.csv (tiny) maps each region id to its country code, turned into a base-26 index once; records.csv is mmapped and cut into one chunk per thread at safe record boundaries (spec 2.2: a cut moves forward to a newline that cannot be inside a quoted field).
2. **One pass per record, no buffers.** Each thread parses id, name, note, region and amount with integer arithmetic (amounts in cents; SWAR for the three integers; `memchr` for the closing quote of a quoted field, with `""` pairs counted so the unescaped length is exact). Count, id sum, amount sum/min/max and the two byte totals go into the thread's per-country accumulator.
3. **Distinct names without storing names.** A name is identified by a 64-bit hash of its unescaped bytes, read 8 at a time. One lock-free table of 2^17 slots (about 100,500 names) maps the hash to a slot with a CAS on first sight; the slot is the name's id. Each country has a 2^17-bit bitset; a record sets bit (country, id) with an atomic OR, skipped when already set. The distinct count per country is a popcount at the end. With ~10^5 names the chance that two names share a 64-bit hash is about 3 × 10^-10.
4. **Overlapping the misses.** The table slot and the bit word are both random accesses into a few MB. The loop is software-pipelined over three records: record i's table slot is prefetched, record i-1 is looked up and its bit word prefetched, record i-2's bit is set. Both misses are in flight while the next record is parsed.
5. **Merge.** The per-thread accumulators (12 threads × ~240 countries) are summed and printed in country order.

## Experiments that lost

- **Step 5, bits set without atomics (run-d37dde1ff289): K 28.95 → 18.87.** Each thread buffered (country, name) pairs per round, then threads set bits for their own slice of countries between barriers. No atomics, but the pair buffers raised memory from 10.5 MB to 25.9 MB, and the rounds and barriers made it slower (0.053 s → 0.084 s): atomic ORs on mostly uncontended words are cheap, waiting for the slowest thread every round is not.
- **Step 8, AVX2 quote scanning (run-cf5f74814d67): K 32.83 → 32.24.** T moved within noise (0.0411 s → 0.0409 s) and the code grew 22 lines (S 335 → 357). Names average 10 bytes and notes 22, so glibc's `memchr` (itself AVX2) is already in the right ballpark; an inline 32-byte scan saved a call per field and nothing measurable. Reverted.
- **Step 9, a deeper lookup pipeline (run-e59cac1be816): K 32.69 → 31.20.** Spacing the stages two records apart gives each cache miss twice as long to land; T went the wrong way (0.041 s → 0.044 s). One record of parsing already covers most of the miss, and the deeper ring costs registers in the hottest loop.
- **Step 10, SIMD structural index (run-58441018d3f7): K 32.69 → 30.20.** The textbook fast-CSV design: classify 64 bytes at a time with AVX2, find inside-quote bytes with a prefix XOR of the quote mask, then read every field from comma and LF positions, with a quoted field's unescaped length taken from a popcount of its quote bits, so names and notes are never walked. It was correct first time on every seed and edge case (including a final record without terminator), and single-threaded it beat step 7 locally. On the board, with 12 threads on 6 cores, it was slower (0.041 s → 0.046 s) and 48 lines longer. My reading: two hyperthreads share one core's vector units, and step 7's scalar loop with its two overlapped cache misses already kept each core busy; classifying every byte adds work that the scalar parser skips (it jumps over names and notes with `memchr`). Reverted.
- **Step 11, `-O3` (run-6b8d679a8817): T 0.0407 s → 0.0421 s,** and slower on D as well. Back to `-O2`.
- **The same SWAR integer parse lost on tracks A and D** (see their logs). It won here because a C record has three integers and little else to do.
