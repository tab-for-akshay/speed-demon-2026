# Track C: Join

Built by Tab, an AI assistant, for team lead Akshay Wagh.

## Approach

1. **Regions first.** regions.csv (tiny) maps each region id to its country code, turned into a base-26 index once; records.csv is mmapped and cut into one chunk per thread at safe record boundaries (spec 2.2: a cut moves forward to a newline that cannot be inside a quoted field).
2. **One pass per record, no buffers.** Each thread parses id, name, note, region and amount with integer arithmetic (amounts in cents; SWAR for the three integers; `memchr` for the closing quote of a quoted field, with `""` pairs counted so the unescaped length is exact). Count, id sum, amount sum/min/max and the two byte totals go into the thread's per-country accumulator.
3. **Distinct names without storing names.** A name is identified by a 64-bit hash of its unescaped bytes, read 8 at a time. One lock-free table of 2^17 slots (about 100,500 names) maps the hash to a slot with a CAS on first sight; the slot is the name's id. Each country has a 2^17-bit bitset; a record sets bit (country, id) with an atomic OR, skipped when already set. The distinct count per country is a popcount at the end. With ~10^5 names the chance that two names share a 64-bit hash is about 3 × 10^-10.
4. **Overlapping the misses.** The table slot and the bit word are both random accesses into a few MB. The loop is software-pipelined over three records: record i's table slot is prefetched, record i-1 is looked up and its bit word prefetched, record i-2's bit is set. Both misses are in flight while the next record is parsed.
5. **Merge.** The per-thread accumulators (12 threads × ~240 countries) are summed and printed in country order.

## Dependencies

None (C standard library and pthreads; static build from `gcc:15`).

## With another 24 hours

Parsing is now most of the time. The next step is a SIMD structural scan: find quotes, commas and line ends in 64-byte blocks, mask out what lies inside quotes with a prefix-XOR of the quote bits, and read fields from the positions instead of walking bytes.

The full history, with a board run ID for every step and every experiment that lost, is in [PERF.md](PERF.md).
