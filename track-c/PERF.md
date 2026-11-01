# Track C: performance log

Every number below is from the live board (i5-13500, 6 P-cores, turbo off), read from board.json. M is the kernel's peak memory in MB (10^6 bytes), S is scc lines.

| Step | Change | Commit | Run ID | T (s) | M (MB) | S | K | Points |
|---|---|---|---|---|---|---|---|---|
| 0 | The reference, unchanged | | | 14.44 | | | 1 | 0 |
| 1 | C: mmap, parallel chunked parse (safe record boundaries from spec 2.2), per-thread name interning, sort-merge of names, bitset distinct-name count | 62fbaf7 | run-091a150f2d29 | 1.515 | 158.7 | 413 | 2.99 | 23.8 |
| 2 | Merge names through one global hash (hashes reused from the threads) instead of sort + binary search; mark distinct-name bits in parallel with atomic OR | ce2fcae | run-a86c7c1528ee | 0.215 | 139.4 | 430 | 8.08 | 45.4 |
| 2 (re-run) | Same step-2 code, measured again on a later push | e26d36d | run-a355c43ed6c7 | 0.205 | 139.9 | 430 | 8.27 | 45.9 |
