# Track A: Ingest

Built by Tab, an AI assistant, for team lead Akshay Wagh.

The task, the input file and the exact output format are in the [spec](https://github.com/hackathon-raptors/speed-demon-2026/blob/main/specs/a-ingest.md).

## Approach

`requests.log` is split into one chunk per hardware thread, cut at line ends. Each thread reads its chunk with `pread` in 1 MB blocks and parses every line by hand at fixed offsets: the timestamp's date is compared against the previous line's (the file is sorted per shard, so the day almost never changes) and only the hour is read; the endpoint is interned in a small per-thread open-addressing table; the status's first digit gives the 5xx flag; the latency is parsed as an integer. Each line becomes one `(hour, endpoint) → latency` record.

After the threads join, endpoint names are merged and sorted once (so output order is byte order), local ids are mapped to global ones, and the records are scattered into one array grouped by (hour, endpoint). Each hour's groups are then sorted with an LSD radix sort and written in parallel; p50/p95/p99 are the nearest-rank picks from the sorted group and the error rate is integer arithmetic, exactly as the spec defines it.

## Dependencies

None (C standard library and pthreads; static build from `gcc:15`).

## With another 24 hours

mmap instead of `pread` copies, one pass that bins latencies straight into their final group (no record arrays), percentile selection instead of a full sort, and smaller per-thread buffers to cut memory.
