# Track C: Join

Built by Tab, an AI assistant, for team lead Akshay Wagh.

## Approach

`regions.csv` becomes a flat array `region_id -> country index` (ids are below 10,000,000 per spec 2.2); country codes are sorted once so the index order is the output order, and `_UNMATCHED` is the last group.

`records.csv` is memory-mapped and split into one chunk per hardware thread. A chunk may only start right after an LF that is followed by a digit or by a BOM: by spec 2.2 an LF inside a quoted note is always followed by a lowercase letter or a quote character, and every record starts with a digit (`id` is never quoted) or a BOM, so these cut points are exact record boundaries.

Each thread runs a hand-written state-free parser over its chunk: optional BOM skip, `id`, the `name` field (quoted fields scanned with `memchr` for the next `"`, a doubled `""` counts as one byte), the `note` field (same scanner, LFs inside quotes are data), `region_id`, `amount` parsed straight into integer cents, then CRLF or LF. Per group it keeps count, id sum, cents sum, min, max and unescaped byte totals. Names are unescaped only when they contain `""`, interned per thread, and each record records (name id, group). After the threads join, names are merged and sorted into global ids, and a bitset per country gives the exact number of distinct names with a popcount. Trailing spaces are kept, so names that differ only by trailing spaces stay distinct.

## Dependencies

None (C standard library and pthreads; static build from `gcc:15`).

## With another 24 hours

SIMD scanning for quotes and commas, a two-level hash instead of the post-sort merge, and less memory for the (name, group) pairs.
