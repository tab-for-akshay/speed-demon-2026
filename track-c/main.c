// Speed Demon 2026 Track C (Join) - C solution.
// Built by Tab, an AI assistant, for team lead Akshay Wagh.
#define _GNU_SOURCE
#include <fcntl.h>
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
static int ncpu(void) { cpu_set_t s; CPU_ZERO(&s); int n = sched_getaffinity(0, sizeof s, &s) ? 1 : CPU_COUNT(&s); return n < 1 ? 1 : n > NT_MAX ? NT_MAX : n; }  /* the CPUs this container may use (12 on the board), not every CPU online (20) */
#define MAXREG 10000000u
#define TB 18                     /* name table: 2^18 slots for a pool of 100,000 names */
#define TS (1u << TB)
typedef struct { int64_t count, idsum, asum, amin, amax, nbytes, tbytes; } Agg;
static uint8_t *regc; static int NC; static char codes[256][4];
static const char *F; static size_t FS; static int NT; static size_t cut[NT_MAX + 1];
static uint64_t *tab;             /* slot = hash:24 | len:8 | arena offset:32 */
static char *arena; static size_t aused; static uint64_t *bits; static Agg agg[NT_MAX][257];
/* intern a name (unescaped bytes s[0..len)) and return its slot; names are copied to one dense arena so lookups stay in cache */
static uint32_t intern(const char *s, uint32_t len) {
  uint64_t h = 0x9E3779B97F4A7C15ull; for (uint32_t i = 0; i < len; i++) h = (h ^ (uint8_t)s[i]) * 0x100000001B3ull;
  h ^= h >> 29; uint32_t j = (uint32_t)h & (TS - 1); uint64_t tag = (h >> 40) << 40 | (uint64_t)len << 32;
  for (;;) {
    uint64_t e = __atomic_load_n(&tab[j], __ATOMIC_ACQUIRE);
    if (!e) {
      size_t a = __atomic_fetch_add(&aused, len, __ATOMIC_RELAXED); memcpy(arena + a, s, len);
      if (__atomic_compare_exchange_n(&tab[j], &e, tag | a, 0, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE)) return j;
    }
    if ((e & ~0xFFFFFFFFull) == tag && memcmp(arena + (uint32_t)e, s, len) == 0) return j;
    j = (j + 1) & (TS - 1);
  }
}
static inline const char *quoted(const char *p, uint32_t *ulen, uint32_t *pairs) {  /* p on the opening quote */
  const char *q = p + 1; uint32_t n = 0;
  for (;;) { q = memchr(q, '"', (size_t)(F + FS - q)); if (q[1] != '"') break; n++; q += 2; }
  *ulen = (uint32_t)(q - p - 1) - n; *pairs = n; return q + 1;
}
static void *work(void *arg) {
  long ti = (long)arg; const char *p = F + cut[ti], *e = F + cut[ti + 1]; Agg *A = agg[ti]; char ub[4096];
  for (int g = 0; g <= NC; g++) A[g] = (Agg){0, 0, 0, INT64_MAX, INT64_MIN, 0, 0};
  while (p < e) {
    if ((uint8_t)*p == 0xEF) p += 3;
    int64_t id = 0; while (*p != ',') id = id * 10 + (*p++ - '0'); p++;
    const char *ns = p; uint32_t nl = 0, np = 0;
    if (*p == '"') { p = quoted(p, &nl, &np); ns++; } else { while (*p != ',') p++; nl = (uint32_t)(p - ns); }
    p++;
    uint32_t tl, tp; if (*p == '"') p = quoted(p, &tl, &tp); else { const char *s0 = p; while (*p != ',') p++; tl = (uint32_t)(p - s0); }
    p++;
    uint32_t reg = 0; while (*p != ',') reg = reg * 10 + (uint32_t)(*p++ - '0'); p++;
    int neg = *p == '-'; p += neg;
    int64_t a = 0; while (*p != '.') a = a * 10 + (*p++ - '0'); a = a * 100 + (p[1] - '0') * 10 + (p[2] - '0'); p += 3; if (neg) a = -a;
    p += (*p == '\r') + 1;
    int g = (reg < MAXREG && regc[reg]) ? regc[reg] - 1 : NC;
    Agg *x = &A[g]; x->count++; x->idsum += id; x->asum += a; if (a < x->amin) x->amin = a; if (a > x->amax) x->amax = a; x->nbytes += nl; x->tbytes += tl;
    const char *k = ns;
    if (np) { uint32_t m = 0; for (uint32_t i = 0; m < nl; i++) { ub[m++] = ns[i]; i += ns[i] == '"'; } k = ub; }
    uint32_t slot = intern(k, nl);
    uint64_t *w = &bits[((size_t)g << (TB - 6)) + (slot >> 6)], b = 1ull << (slot & 63);
    if (!(*w & b)) __atomic_fetch_or(w, b, __ATOMIC_RELAXED);
  }
  return NULL;
}
static char *mapfile(const char *path, size_t *sz) { int fd = open(path, O_RDONLY); if (fd < 0) { perror(path); exit(1); }
  struct stat st; fstat(fd, &st); *sz = (size_t)st.st_size; char *p = mmap(NULL, *sz + 4096, PROT_READ, MAP_PRIVATE, fd, 0); close(fd); return p; }
static int codecmp(const void *a, const void *b) { return memcmp(a, b, 3); }
static char *num(char *w, int64_t v) { char b[24]; int n = 0; do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v); while (n) *w++ = b[--n]; return w; }
static char *money(char *w, int64_t c) { if (c < 0) { *w++ = '-'; c = -c; } w = num(w, c / 100); *w++ = '.'; *w++ = (char)('0' + c % 100 / 10); *w++ = (char)('0' + c % 10); return w; }
int main(int argc, char **argv) {
  const char *data = argc > 1 ? argv[1] : "/data", *out = argc > 2 ? argv[2] : "/out"; char path[4096];
  size_t rs; snprintf(path, sizeof path, "%s/regions.csv", data); const char *R = mapfile(path, &rs);
  const char *p = memchr(R, '\n', rs) + 1, *re = R + rs;
  for (const char *q = p + 1; q < re; q = memchr(q, '\n', (size_t)(re - q)) + 1) { const char *c = memchr(q, ',', 16) + 1;   /* collect codes */
    if (!bsearch(c, codes, (size_t)NC, 4, codecmp)) { memcpy(codes[NC++], c, 3); qsort(codes, (size_t)NC, 4, codecmp); } }
  regc = calloc(MAXREG, 1);
  for (const char *q = p; q < re; q += 4) { uint32_t id = 0; while (*q != ',') id = id * 10 + (uint32_t)(*q++ - '0'); q++;
    regc[id] = (uint8_t)((char (*)[4])bsearch(q, codes, (size_t)NC, 4, codecmp) - codes + 1); }
  snprintf(path, sizeof path, "%s/records.csv", data); F = mapfile(path, &FS);
  p = memchr(F, '\n', FS) + 1; size_t body = (size_t)(p - F);
  NT = ncpu();
  /* a record starts right after an LF followed by a digit or a BOM; by spec 2.2 an LF inside a note never is */
  cut[0] = body; cut[NT] = FS;
  for (int i = 1; i < NT; i++) { size_t c = body + (FS - body) * (size_t)i / (size_t)NT; if (c < cut[i - 1]) c = cut[i - 1];
    while (c < FS && !(F[c - 1] == '\n' && ((F[c] >= '0' && F[c] <= '9') || (uint8_t)F[c] == 0xEF))) c++; cut[i] = c; }
  tab = calloc(TS, 8); arena = malloc(1 << 24); bits = calloc((size_t)(NC + 1) << (TB - 6), 8);
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
