# Track C: Join

Built by Tab, an AI assistant, for team lead Akshay Wagh.

## Approach

`regions.csv` becomes a flat byte array `region_id -> country index` (ids are below 10,000,000 per spec 2.2); the country codes are kept sorted, so the index order is the output order, and `_UNMATCHED` is the last group.

`records.csv` is memory-mapped and split into one chunk per hardware thread. A chunk may only start right after an LF that is followed by a digit or by a BOM: by spec 2.2 an LF inside a quoted note is always followed by a lowercase letter or a quote character, and every record starts with a digit (`id` is never quoted) or a BOM, so these cut points are exact record boundaries.

Each thread runs a hand-written parser over its chunk: optional BOM skip, `id`, the `name` field (quoted fields scanned with `memchr` for the next `"`, a doubled `""` counts as one byte), the `note` field (same scanner, LFs inside quotes are data), `region_id`, `amount` parsed straight into integer cents, then CRLF or LF. Per country each thread keeps count, id sum, cents sum, min, max and unescaped byte totals in its own table; the tables are added up once at the end.

Distinct names need no per-record storage. Every thread interns the (unescaped) name into one shared lock-free hash table of 2^18 slots (a slot is set once with a compare-and-swap and holds a hash tag, the length and the name's offset in a small dense arena, so lookups stay in cache). The slot number is the name's id, and the record sets bit `slot` in its country's bitset (atomic OR, skipped when the bit is already set). A popcount per country gives the exact number of distinct names. Trailing spaces are kept, so names that differ only by trailing spaces stay distinct. Memory is the bitsets (about 8 MB), the table (2 MB) and the region map.

## Dependencies

None (C standard library and pthreads; static build from `gcc:15`).

## With another 24 hours

SIMD scanning for quotes, commas and line ends (one pass of 32-byte compares instead of byte loops), and a word-at-a-time hash of the name.
