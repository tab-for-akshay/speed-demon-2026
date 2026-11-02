// Speed Demon 2026 Track C (Join) - C solution.
// Built by Tab, an AI assistant, for team lead Akshay Wagh.
#define _GNU_SOURCE
#include <fcntl.h>
#include <immintrin.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#define NT_MAX 64
#define MAXREG 1000000u           /* regions.csv ids are at most 999,999; a larger record region_id is unmatched */
#define TB 17                     /* name table: 2^17 slots for a pool of about 100,500 names */
#define TS (1u << TB)
static int ncpu(void) { cpu_set_t s; CPU_ZERO(&s); int n = sched_getaffinity(0, sizeof s, &s) ? 1 : CPU_COUNT(&s); return n < 1 ? 1 : n > NT_MAX ? NT_MAX : n; }  /* the CPUs this container may use (12 on the board) */
typedef struct { int64_t count, idsum, asum, amin, amax, nbytes, tbytes; } Agg;
static uint16_t *regc, grp[26 * 26 * 26 + 1]; static int NC;   /* regc: region id -> country code index + 1; grp: code index + 1 -> output rank */
static const char *F; static size_t FS; static int NT; static size_t cut[NT_MAX + 1];
static uint64_t *tab, *bits; static Agg agg[NT_MAX][257];
/* a name is identified by a 64-bit hash of its unescaped bytes; the slot is its id in the per-country bitsets */
static inline uint64_t hash(const char *s, uint32_t len) {
  uint64_t h = 0x9E3779B97F4A7C15ull ^ len, x;
  for (; len >= 8; s += 8, len -= 8) { memcpy(&x, s, 8); h = (h ^ x) * 0xFF51AFD7ED558CCDull; h ^= h >> 32; }
  if (len) { memcpy(&x, s, 8); x &= ~0ull >> (64 - 8 * len); h = (h ^ x) * 0xFF51AFD7ED558CCDull; }
  h ^= h >> 29; h *= 0xC4CEB9FE1A85EC53ull; h ^= h >> 32; return h | 1;
}
static inline uint32_t intern(uint64_t h) {
  uint32_t j = (uint32_t)(h >> 40) & (TS - 1);
  for (;;) {
    uint64_t e = __atomic_load_n(&tab[j], __ATOMIC_RELAXED);
    if (e == h || (!e && __atomic_compare_exchange_n(&tab[j], &e, h, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) || e == h) return j;
    j = (j + 1) & (TS - 1);
  }
}
/* decimal digits up to the first non-digit, 8 bytes at a time (SWAR) when 8 bytes are readable, else one by one */
static inline uint64_t num8(const char **pp, const char *lim) {
  const char *p = *pp; uint64_t x, v = 0;
  if (lim - p >= 8) { memcpy(&x, p, 8); uint64_t t = x - 0x3030303030303030ull, m = (t | (t + 0x7676767676767676ull)) & 0x8080808080808080ull;
    int n = m ? __builtin_ctzll(m) >> 3 : 0;
    if (n) { v = x << (64 - 8 * n); v = (v & 0x0F0F0F0F0F0F0F0Full) * 2561 >> 8; v = (v & 0x00FF00FF00FF00FFull) * 6553601 >> 16;
      *pp = p + n; return (v & 0x0000FFFF0000FFFFull) * 42949672960001ull >> 32; } }
  while ((uint8_t)(*p - '0') < 10) v = v * 10 + (uint64_t)(*p++ - '0');
  *pp = p; return v;
}
/* quote characters in [a, b) (offsets from the chunk start), from the ring of per-block quote masks */
#define RING 256
static inline uint32_t qcount(const uint64_t *qm, const char *base, size_t a, size_t b) {
  if (b - a > (RING - 2) * 64) { uint32_t n = 0; for (size_t i = a; i < b; i++) n += base[i] == '"'; return n; }   /* longer than the ring: count bytes */
  uint32_t n = 0;
  for (size_t k = a >> 6; k <= (b - 1) >> 6; k++) {
    uint64_t m = qm[k % RING]; if (k == a >> 6) m &= ~0ull << (a & 63); if (k == (b - 1) >> 6) m &= ~0ull >> (63 - ((b - 1) & 63));
    n += (uint32_t)__builtin_popcountll(m);
  }
  return n;
}
/* field [a, b): unescaped length, number of "" pairs, first content byte */
static inline uint32_t field(const uint64_t *qm, const char *base, size_t a, size_t b, uint32_t *pairs, const char **s) {
  if (base[a] != '"') { *pairs = 0; *s = base + a; return (uint32_t)(b - a); }
  *pairs = (qcount(qm, base, a, b) - 2) / 2; *s = base + a + 1; return (uint32_t)(b - a - 2) - *pairs;
}
static void *work(void *arg) {
  long ti = (long)arg; const char *base = F + cut[ti]; size_t len = cut[ti + 1] - cut[ti];
  Agg *A = agg[ti]; char ub[4096]; uint64_t ph = 0, pb = 0, qb = 0, *pw = NULL, *qw = NULL; int pg = 0;
  for (int g = 0; g <= NC; g++) A[g] = (Agg){0, 0, 0, INT64_MAX, INT64_MIN, 0, 0};
  /* stage 1, per 64-byte block: bit masks of quotes, commas and LFs (AVX2); a prefix XOR of the quote bits marks what lies
     inside quotes (a "" pair toggles twice), so commas and LFs outside quotes are the field and record ends.
     stage 2, per record: the four commas and the LF give every field's bounds; a quoted field's "" pairs are counted
     from the quote bits, so names and notes are never scanned byte by byte */
  uint64_t qm[RING], inq = 0; size_t rs = 0, cpos[4]; int nc = 0;
  const __m256i Q = _mm256_set1_epi8('"'), C = _mm256_set1_epi8(','), L = _mm256_set1_epi8('\n');
  int open_end = len && base[len - 1] != '\n';   /* spec 2.1: the final record may lack a terminator (the generator always writes one) */
  for (size_t b = 0; 64 * b < len + (size_t)open_end; b++) {
    const char *p = base + 64 * b; char tmp[64];
    if (F + FS - p < 64) { memset(tmp, 0, 64); memcpy(tmp, p, (size_t)(F + FS - p)); p = tmp; }   /* never load past the file */
    __m256i x0 = _mm256_loadu_si256((const __m256i *)p), x1 = _mm256_loadu_si256((const __m256i *)(p + 32));
#define MASK(v) ((uint64_t)(uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(x0, v)) | (uint64_t)(uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(x1, v)) << 32)
    uint64_t q = MASK(Q), lf = MASK(L), st = MASK(C) | lf, in = q;
    in ^= in << 1; in ^= in << 2; in ^= in << 4; in ^= in << 8; in ^= in << 16; in ^= in << 32; in ^= inq;
    inq = (uint64_t)((int64_t)in >> 63); qm[b % RING] = q; st &= ~in;
    if (len - 64 * b < 64) { uint64_t keep = (1ull << (len - 64 * b)) - 1; st &= keep; lf &= keep; if (open_end) st |= lf |= 1ull << (len - 64 * b); }   /* a virtual LF at the end */
    while (st) {
      size_t o = 64 * b + (size_t)__builtin_ctzll(st); st &= st - 1;
      if (!(lf >> (o & 63) & 1)) { if (nc < 4) cpos[nc++] = o; continue; }
      /* a record: [rs, cpos[0]) id, name, note, region, amount up to the LF at o */
      const char *p2 = base + rs; if ((uint8_t)*p2 == 0xEF) p2 += 3;
      int64_t id = (int64_t)num8(&p2, F + FS);
      const char *ns, *ts; uint32_t np, tp, nl = field(qm, base, cpos[0] + 1, cpos[1], &np, &ns), tl = field(qm, base, cpos[1] + 1, cpos[2], &tp, &ts);
      p2 = base + cpos[2] + 1; uint32_t reg = (uint32_t)num8(&p2, F + FS);
      p2 = base + cpos[3] + 1; int neg = *p2 == '-'; p2 += neg;
      int64_t a = (int64_t)num8(&p2, F + FS); a = a * 100 + (p2[1] - '0') * 10 + (p2[2] - '0'); if (neg) a = -a;
      rs = o + 1; nc = 0;
      int g = reg < MAXREG && regc[reg] ? grp[regc[reg]] : NC;
      Agg *x = &A[g]; x->count++; x->idsum += id; x->asum += a; if (a < x->amin) x->amin = a; if (a > x->amax) x->amax = a; x->nbytes += nl; x->tbytes += tl;
      const char *k = ns;
      if (np) { uint32_t m = 0; for (uint32_t i = 0; m < nl; i++) { ub[m++] = ns[i]; i += ns[i] == '"'; } k = ub; }
      /* software pipeline: this record's table slot is prefetched, the previous record is looked up and its bit word prefetched,
         and the one before that sets its bit, so both cache misses overlap with parsing */
      uint64_t h = hash(k, nl); __builtin_prefetch(&tab[(uint32_t)(h >> 40) & (TS - 1)]);
      if (ph) { uint32_t slot = intern(ph); pw = &bits[((size_t)pg << (TB - 6)) + (slot >> 6)]; pb = 1ull << (slot & 63); __builtin_prefetch(pw, 1); }
      if (qw && !(*qw & qb)) __atomic_fetch_or(qw, qb, __ATOMIC_RELAXED);
      qw = pw; qb = pb; pw = NULL; ph = h; pg = g;
    }
  }
  if (ph) { uint32_t slot = intern(ph); pw = &bits[((size_t)pg << (TB - 6)) + (slot >> 6)]; pb = 1ull << (slot & 63); }
  if (qw && !(*qw & qb)) __atomic_fetch_or(qw, qb, __ATOMIC_RELAXED);
  if (pw && !(*pw & pb)) __atomic_fetch_or(pw, pb, __ATOMIC_RELAXED);
  return NULL;
}
static char *mapfile(const char *path, size_t *sz) { int fd = open(path, O_RDONLY); if (fd < 0) { perror(path); exit(1); }
  struct stat st; fstat(fd, &st); *sz = (size_t)st.st_size; char *p = mmap(NULL, *sz + 4096, PROT_READ, MAP_PRIVATE, fd, 0); close(fd); return p; }
static char *num(char *w, int64_t v) { char b[24]; int n = 0; do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v); while (n) *w++ = b[--n]; return w; }
static char *money(char *w, int64_t c) { if (c < 0) { *w++ = '-'; c = -c; } w = num(w, c / 100); *w++ = '.'; *w++ = (char)('0' + c % 100 / 10); *w++ = (char)('0' + c % 10); return w; }
int main(int argc, char **argv) {
  const char *data = argc > 1 ? argv[1] : "/data", *out = argc > 2 ? argv[2] : "/out"; char path[4096];
  size_t rs; snprintf(path, sizeof path, "%s/regions.csv", data); const char *R = mapfile(path, &rs);
  regc = calloc(MAXREG, 2);
  for (const char *q = memchr(R, '\n', rs) + 1, *re = R + rs; q < re; q += 5) {   /* "id,ABC\n": a code is a base-26 number, so its order is the codes' byte order */
    uint32_t id = 0; while (*q != ',') id = id * 10 + (uint32_t)(*q++ - '0');
    uint16_t c = (uint16_t)((q[1] - 'A') * 676 + (q[2] - 'A') * 26 + (q[3] - 'A') + 1); regc[id] = c; grp[c] = 1; }
  char codes[257][4];
  for (int c = 1; c <= 26 * 26 * 26; c++) if (grp[c]) { codes[NC][0] = (char)('A' + (c - 1) / 676); codes[NC][1] = (char)('A' + (c - 1) / 26 % 26); codes[NC][2] = (char)('A' + (c - 1) % 26); grp[c] = (uint16_t)NC++; }
  snprintf(path, sizeof path, "%s/records.csv", data); F = mapfile(path, &FS);
  size_t body = (size_t)((const char *)memchr(F, '\n', FS) + 1 - F);
  NT = ncpu();
  /* a record starts right after an LF followed by a digit or a BOM; by spec 2.2 an LF inside a note never is */
  cut[0] = body; cut[NT] = FS;
  for (int i = 1; i < NT; i++) { size_t c = body + (FS - body) * (size_t)i / (size_t)NT; if (c < cut[i - 1]) c = cut[i - 1];
    while (c < FS && !(F[c - 1] == '\n' && ((F[c] >= '0' && F[c] <= '9') || (uint8_t)F[c] == 0xEF))) c++; cut[i] = c; }
  tab = calloc(TS, 8); bits = calloc((size_t)(NC + 1) << (TB - 6), 8);
  pthread_t th[NT_MAX]; for (long i = 0; i < NT; i++) pthread_create(&th[i], NULL, work, (void *)i); for (int i = 0; i < NT; i++) pthread_join(th[i], NULL);
  char buf[257 * 200], *w = buf;
  for (int g = 0; g <= NC; g++) {
    Agg G = {0, 0, 0, INT64_MAX, INT64_MIN, 0, 0};
    for (int i = 0; i < NT; i++) { Agg *a = &agg[i][g]; G.count += a->count; G.idsum += a->idsum; G.asum += a->asum; G.nbytes += a->nbytes; G.tbytes += a->tbytes;
      if (a->amin < G.amin) G.amin = a->amin; if (a->amax > G.amax) G.amax = a->amax; }
    if (!G.count) continue;
    int64_t nd = 0; for (size_t x = 0; x < (TS >> 6); x++) nd += __builtin_popcountll(bits[((size_t)g << (TB - 6)) + x]);
    if (g < NC) { memcpy(w, codes[g], 3); w += 3; } else { memcpy(w, "_UNMATCHED", 10); w += 10; }
    *w++ = ' '; w = num(w, G.count); *w++ = ' '; w = num(w, G.idsum); *w++ = ' '; w = money(w, G.asum); *w++ = ' '; w = money(w, G.amin); *w++ = ' ';
    w = money(w, G.amax); *w++ = ' '; w = num(w, nd); *w++ = ' '; w = num(w, G.nbytes); *w++ = ' '; w = num(w, G.tbytes); *w++ = '\n'; }
  snprintf(path, sizeof path, "%s/aggregates.txt", out); FILE *f = fopen(path, "wb"); if (!f) { perror("fopen"); return 1; }
  fwrite(buf, 1, (size_t)(w - buf), f); fclose(f); return 0;
}
