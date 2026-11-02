// Speed Demon 2026 Track A (Ingest) - C solution.
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
#define NH 256          /* hour window: 64 hours of slack before 2026-10-30T18 and well past the 72-hour span */
#define TB 12           /* endpoint table: 4096 slots for 1,842 endpoints; the slot is the endpoint's id */
#define TS (1u << TB)
#define BS 2048         /* entries per block; each (thread, hour) fills its own chain of blocks */
static int ncpu(void) { cpu_set_t s; CPU_ZERO(&s); int n = sched_getaffinity(0, sizeof s, &s) ? 1 : CPU_COUNT(&s); return n < 1 ? 1 : n > NT_MAX ? NT_MAX : n; }  /* the CPUs this container may use (12 on the board) */
typedef struct Blk { struct Blk *next; uint32_t n, lat[BS]; uint16_t key[BS]; } Blk;   /* key = endpoint slot | 5xx flag << 15 */
static const char *F; static size_t FS; static int NT; static size_t cut[NT_MAX + 1]; static long BASE_H;
static uint64_t tab[TS]; static const char *eps[TS]; static uint32_t epl[TS];
static Blk *head[NT_MAX][NH]; static uint16_t order[TS]; static int NE; static char *hout[NH]; static size_t hlen[NH]; static int nexth, wnext, ready[NH]; static FILE *outf; static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static long days_from_civil(int y, int m, int d) {
  y -= m <= 2; long era = (y >= 0 ? y : y - 399) / 400; unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}
/* an endpoint is identified by a 64-bit hash of its bytes, read 8 at a time */
static inline uint32_t intern(const char *s, uint32_t len) {
  uint64_t h = 0x9E3779B97F4A7C15ull ^ len, x; const char *p = s; uint32_t n = len;
  for (; n >= 8; p += 8, n -= 8) { memcpy(&x, p, 8); h = (h ^ x) * 0xFF51AFD7ED558CCDull; h ^= h >> 32; }
  if (n) { memcpy(&x, p, 8); x &= ~0ull >> (64 - 8 * n); h = (h ^ x) * 0xFF51AFD7ED558CCDull; }
  h ^= h >> 29; h *= 0xC4CEB9FE1A85EC53ull; h ^= h >> 32; h |= 1;
  for (uint32_t j = (uint32_t)(h >> 40) & (TS - 1);; j = (j + 1) & (TS - 1)) {
    uint64_t e = __atomic_load_n(&tab[j], __ATOMIC_ACQUIRE);
    if (e == h) return j;
    if (!e) { if (__atomic_compare_exchange_n(&tab[j], &e, h, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) { eps[j] = s; epl[j] = len; return j; } if (e == h) return j; }   /* the winner records the bytes; they are read only after the threads join */
  }
}
static void *parse(void *arg) {
  long t = (long)arg; const char *p = F + cut[t], *end = F + cut[t + 1];
  size_t nb = (size_t)(end - p) / 40 / BS + NH + 1; Blk *pool = mmap(NULL, nb * sizeof(Blk), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0), *cur[NH] = {0};
  madvise(pool, nb * sizeof(Blk), MADV_NOHUGEPAGE);   /* only the pages touched count toward memory */
  char lastday[10] = {0}; long lastdh = 0;
  while (p < end) {
    int hh = (p[11] - '0') * 10 + (p[12] - '0');
    if (memcmp(p, lastday, 10)) { memcpy(lastday, p, 10); lastdh = days_from_civil((p[0] - '0') * 1000 + (p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0'), (p[5] - '0') * 10 + (p[6] - '0'), (p[8] - '0') * 10 + (p[9] - '0')) * 24 - BASE_H; }
    long h = lastdh + hh;
    const char *q = p + 28; while (*q != ' ') q++;   /* the timestamp is 27 bytes and a space; skip the method */
    const char *es = ++q; q = memchr(q, ' ', 256);
    uint32_t id = intern(es, (uint32_t)(q - es));
    uint16_t key = (uint16_t)(id | (q[1] == '5') << 15); q += 5;   /* status is 3 digits: a leading 5 flags an error */
    uint32_t v = 0; while (*q != '\n') v = v * 10 + (uint32_t)(*q++ - '0');
    p = q + 1;
    Blk *b = cur[h]; if (!b || b->n == BS) {   /* this thread's chain for hour h: new blocks come from its own pool */ Blk *nbk = pool++; nbk->next = b; nbk->n = 0; cur[h] = head[t][h] = b = nbk; }
    b->lat[b->n] = v; b->key[b->n++] = key;
  }
  return NULL;
}
static void sort_u32(uint32_t *a, size_t n, uint32_t *tmp) {
  if (n < 64) { for (size_t i = 1; i < n; i++) { uint32_t v = a[i]; size_t j = i; while (j && a[j - 1] > v) { a[j] = a[j - 1]; j--; } a[j] = v; } return; }
  uint32_t *src = a, *dst = tmp;   /* LSD radix, 4 x 8 bits */
  for (int sh = 0; sh < 32; sh += 8) {
    size_t c[257] = {0};
    for (size_t i = 0; i < n; i++) c[((src[i] >> sh) & 255) + 1]++;
    if (c[1 + ((src[0] >> sh) & 255)] == n) continue;
    for (int i = 0; i < 256; i++) c[i + 1] += c[i];
    for (size_t i = 0; i < n; i++) dst[c[(src[i] >> sh) & 255]++] = src[i];
    uint32_t *x = src; src = dst; dst = x;
  }
  if (src != a) memcpy(a, src, n * 4);
}
static char *u2s(char *o, uint32_t v) { char b[12]; int n = 0; do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v); while (n) *o++ = b[--n]; return o; }
static int epcmp(const void *a, const void *b) {
  uint16_t x = *(const uint16_t *)a, y = *(const uint16_t *)b; uint32_t n = epl[x] < epl[y] ? epl[x] : epl[y];
  int c = memcmp(eps[x], eps[y], n); return c ? c : (int)epl[x] - (int)epl[y];
}
/* one hour at a time: gather its latencies by endpoint from every thread's blocks, sort each group, format its lines */
static void *output(void *arg) {
  (void)arg; uint32_t cnt[TS + 1], err[TS], pos[TS]; size_t cap = 0; uint32_t *v = NULL, *tmp = NULL;
  for (;;) {
    int h = __atomic_fetch_add(&nexth, 1, __ATOMIC_RELAXED); if (h >= NH) break;
    memset(cnt, 0, sizeof cnt); memset(err, 0, sizeof err); size_t n = 0;
    /* counting sort by endpoint: count every thread's lines of this hour, then scatter latencies into one grouped array */
    for (int t = 0; t < NT; t++) for (Blk *b = head[t][h]; b; b = b->next) { n += b->n; for (uint32_t i = 0; i < b->n; i++) { cnt[b->key[i] & (TS - 1)]++; err[b->key[i] & (TS - 1)] += b->key[i] >> 15; } }
    char *o = NULL, *w = NULL;
    if (n) {
    if (n > cap) { cap = n; v = realloc(v, 4 * n); tmp = realloc(tmp, 4 * n); }
    uint32_t s = 0; for (uint32_t k = 0; k < TS; k++) { pos[k] = s; s += cnt[k]; }
    for (int t = 0; t < NT; t++) for (Blk *b = head[t][h]; b; b = b->next) for (uint32_t i = 0; i < b->n; i++) v[pos[b->key[i] & (TS - 1)]++] = b->lat[i];
    long ah = BASE_H + h, z = ah / 24 + 719468, era = (z >= 0 ? z : z - 146096) / 146097; unsigned doe = (unsigned)(z - era * 146097);   /* civil_from_days */
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365, doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    unsigned d = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9; long y = (long)yoe + era * 400 + (m <= 2);
    char hs[32]; snprintf(hs, sizeof hs, "%04ld-%02u-%02uT%02d:00:00Z ", y, m, d, (int)(ah % 24));
    w = o = malloc((size_t)NE * 180);
    for (int k = 0; k < NE; k++) {
      uint32_t e = order[k], c = cnt[e]; if (!c) continue;
      uint32_t *g = v + pos[e] - c; sort_u32(g, c, tmp);   /* percentiles are nearest-rank: the ceil(p * c / 100)-th smallest */
      memcpy(w, hs, 21); w += 21; memcpy(w, eps[e], epl[e]); w += epl[e]; *w++ = ' ';
      w = u2s(w, c); *w++ = ' '; w = u2s(w, g[(50ull * c + 99) / 100 - 1]); *w++ = ' '; w = u2s(w, g[(95ull * c + 99) / 100 - 1]); *w++ = ' ';
      w = u2s(w, g[(99ull * c + 99) / 100 - 1]); *w++ = ' '; w = u2s(w, err[e]); *w++ = ' '; w = u2s(w, (uint32_t)((uint64_t)err[e] * 1000000ull / c)); *w++ = '\n';
    }
    }
    /* hand the hour over, then write every finished hour in order, so few hours' text is held at once */
    pthread_mutex_lock(&mu); hout[h] = o; hlen[h] = (size_t)(w - o); ready[h] = 1;
    for (; wnext < NH && ready[wnext]; wnext++) { fwrite(hout[wnext], 1, hlen[wnext], outf); free(hout[wnext]); }
    pthread_mutex_unlock(&mu);
  }
  free(v); free(tmp); return NULL;
}
static void run(void *(*f)(void *)) { pthread_t th[NT_MAX]; for (long i = 0; i < NT; i++) pthread_create(&th[i], NULL, f, (void *)i); for (int i = 0; i < NT; i++) pthread_join(th[i], NULL); }
int main(int argc, char **argv) {
  const char *data = argc > 1 ? argv[1] : "/data", *out = argc > 2 ? argv[2] : "/out"; char path[4096];
  snprintf(path, sizeof path, "%s/requests.log", data); int fd = open(path, O_RDONLY); if (fd < 0) { perror("open"); return 1; }
  struct stat st; fstat(fd, &st); FS = (size_t)st.st_size; F = FS ? mmap(NULL, FS, PROT_READ, MAP_PRIVATE, fd, 0) : ""; close(fd);
  BASE_H = days_from_civil(2026, 10, 30) * 24 + 18 - 64;
  NT = FS < (1 << 20) ? 1 : ncpu(); cut[0] = 0; cut[NT] = FS;   /* split at line ends */
  for (int i = 1; i < NT; i++) { size_t c = FS * (size_t)i / (size_t)NT; if (c < cut[i - 1]) c = cut[i - 1]; while (c < FS && F[c - 1] != '\n') c++; cut[i] = c; }
  run(parse);
  for (uint32_t j = 0; j < TS; j++) if (tab[j]) order[NE++] = (uint16_t)j;   /* output order: endpoints by their bytes */
  qsort(order, (size_t)NE, 2, epcmp);
  snprintf(path, sizeof path, "%s/summary.txt", out); outf = fopen(path, "wb"); if (!outf) { perror("fopen"); return 1; }
  run(output);
  fclose(outf); return 0;
}
