# Track A: Ingest

Built by Tab, an AI assistant, for team lead Akshay Wagh.

The task, the input file and the exact output format are in the [spec](https://github.com/hackathon-raptors/speed-demon-2026/blob/main/specs/a-ingest.md).

## Approach

1. **Split.** The log is mmapped and cut into one chunk per thread (12 on the board), each cut moved forward to a line end.
2. **Parse.** Each line's hour comes from its timestamp: the date is converted to days only when it differs from the previous line's date (lines arrive roughly in time order), so most lines cost two digit reads. The endpoint is hashed 8 bytes at a time into a 64-bit key and looked up in one shared lock-free table of 4,096 slots (1,842 endpoints); the slot index is the endpoint's id. A new endpoint is inserted with a CAS, and only the thread whose CAS wins records the endpoint's bytes.
3. **Store.** Each thread appends (latency, endpoint id | 5xx bit) to its own chain of 2,048-entry blocks per hour: 6 bytes per line, no sharing, no locks, no merge pass.
4. **Output.** Threads take hours from an atomic counter. For an hour they count lines per endpoint across all threads' blocks, scatter latencies into one array grouped by endpoint, sort each group (insertion sort under 64 values, else 4-pass LSD radix), and format count, errors, p50/p95/p99. Finished hours are written in hour order as soon as every earlier hour is done, so only a few hours of text are held at once.

## Dependencies

None (C standard library and pthreads; static build from `gcc:15`).

## With another 24 hours

Find out what puts some board runs at 0.05-0.06 s instead of 0.038 s with the same code (my suspect is the 9.7 MB write into the page cache), and cut the 18.6 MB of per-line blocks: a counting pass that sizes every (hour, endpoint) group first would let the second pass write each latency straight to its final place, 4 bytes per line instead of 6, with no gather.

The full history, with a board run ID for every step and every experiment that lost, is in [PERF.md](PERF.md).
