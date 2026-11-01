# Track C: performance log

| Step | Change | Run ID | T (s) | M (MB) | S | K | Points |
|---|---|---|---|---|---|---|---|
| 0 | The reference, unchanged | | 14.44 | | | 1 | 0 |
| 1 | C: mmap, parallel chunked parse (safe record boundaries from spec 2.2), per-thread name interning, sort-merge of names, bitset distinct-name count | run-091a150f2d29 | 1.515 | 158.7 | 413 | 2.99 | 23.8 |
| 2 | Merge names through one global hash (hashes reused from the threads) instead of sort + binary search; mark distinct-name bits in parallel with atomic OR | (pending board run) | | | | | |
