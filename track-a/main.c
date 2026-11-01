// Speed Demon 2026 Track A (Ingest) - C solution.
// Built by Tab, an AI assistant, for team lead Akshay Wagh.
#define _GNU_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define NT_MAX 64
#define HSIZE 8192          /* per-thread endpoint table, power of two */
#define MAXE 4096           /* max distinct endpoints per thread */
#define BUFSZ (1 << 20)

typedef struct { const char *s; uint32_t len, hash; } Ep;
typedef struct {
  int fd; off_t beg, end;
  Ep ep[MAXE]; int nep; int16_t tab[HSIZE];
  char *arena; size_t arena_used;
  uint32_t *key, *lat; size_t n, cap;   /* key = hour<<12 | local endpoint id */
  uint8_t *err;                         /* 1 if 5xx */
} Th;

static long days_from_civil(int y, int m, int d) {
  y -= m <= 2; long era = (y >= 0 ? y : y - 399) / 400; unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}
static long BASE_H;   /* absolute hour of 2026-10-30T18 minus slack */
#define NH 256

static inline uint32_t intern(Th *t, const char *s, uint32_t len) {
  uint32_t h = 2166136261u;
  for (uint32_t i = 0; i < len; i++) h = (h ^ (uint8_t)s[i]) * 16777619u;
  uint32_t i = h & (HSIZE - 1);
  for (;;) {
    int id = t->tab[i];
    if (id < 0) break;
    Ep *e = &t->ep[id];
    if (e->hash == h && e->len == len && memcmp(e->s, s, len) == 0) return (uint32_t)id;
    i = (i + 1) & (HSIZE - 1);
  }
  int id = t->nep++;
  char *c = t->arena + t->arena_used; memcpy(c, s, len); t->arena_used += len;
  t->ep[id].s = c; t->ep[id].len = len; t->ep[id].hash = h; t->tab[i] = (int16_t)id;
  return (uint32_t)id;
}

static inline void push(Th *t, uint32_t k, uint32_t l, uint8_t e) {
  if (t->n == t->cap) {
    t->cap = t->cap ? t->cap * 2 : (1 << 16);
    t->key = realloc(t->key, t->cap * 4); t->lat = realloc(t->lat, t->cap * 4); t->err = realloc(t->err, t->cap);
  }
  t->key[t->n] = k; t->lat[t->n] = l; t->err[t->n] = e; t->n++;
}

/* cache: day string -> absolute day hour base, to avoid recomputing */
static inline long hour_of(const char *p) {
  int y = (p[0]-'0')*1000 + (p[1]-'0')*100 + (p[2]-'0')*10 + (p[3]-'0');
  int m = (p[5]-'0')*10 + (p[6]-'0'); int d = (p[8]-'0')*10 + (p[9]-'0');
  int hh = (p[11]-'0')*10 + (p[12]-'0');
  return days_from_civil(y, m, d) * 24 + hh;
}

static void parse(Th *t, const char *p, const char *end) {
  char lastday[10] = {0}; long lastdh = 0;
  while (p < end) {
    long ah;
    if (memcmp(p, lastday, 10) == 0) ah = lastdh + (p[11]-'0')*10 + (p[12]-'0');
    else { memcpy(lastday, p, 10); ah = hour_of(p); lastdh = ah - ((p[11]-'0')*10 + (p[12]-'0')); }
    const char *q = p + 28;                 /* after "ts " */
    while (*q != ' ') q++;                   /* method */
    const char *es = ++q;
    q = memchr(q, ' ', 200);
    uint32_t elen = (uint32_t)(q - es);
    q++;
    uint8_t e = (q[0] == '5');
    q += 4;
    uint32_t v = 0; while (*q != '\n') { v = v * 10 + (uint32_t)(*q - '0'); q++; }
    p = q + 1;
    uint32_t id = intern(t, es, elen);
    push(t, ((uint32_t)(ah - BASE_H) << 12) | id, v, e);
  }
}

static void *worker(void *arg) {
  Th *t = arg;
  memset(t->tab, -1, sizeof t->tab);
  t->arena = malloc(MAXE * 128);
  char *buf = malloc(BUFSZ + 1);
  off_t pos = t->beg;
  size_t carry = 0;
  while (pos < t->end) {
    size_t want = BUFSZ - carry; if ((off_t)want > t->end - pos) want = (size_t)(t->end - pos);
    ssize_t r = pread(t->fd, buf + carry, want, pos);
    if (r <= 0) break;
    pos += r; size_t have = carry + (size_t)r;
    char *last = memrchr(buf, '\n', have);
    size_t full = last ? (size_t)(last - buf) + 1 : 0;
    parse(t, buf, buf + full);
    carry = have - full; memmove(buf, buf + full, carry);
  }
  free(buf);
  return NULL;
}

/* global endpoint table */
typedef struct { const char *s; uint32_t len; } GE;
static int ge_cmp(const void *a, const void *b) {
  const GE *x = a, *y = b; uint32_t n = x->len < y->len ? x->len : y->len;
  int c = memcmp(x->s, y->s, n); if (c) return c; return (int)x->len - (int)y->len;
}

static void sort_u32(uint32_t *a, size_t n, uint32_t *tmp) {
  if (n < 64) { for (size_t i = 1; i < n; i++) { uint32_t v = a[i]; size_t j = i; while (j && a[j-1] > v) { a[j] = a[j-1]; j--; } a[j] = v; } return; }
  /* LSD radix, 4 x 8 bits */
  uint32_t *src = a, *dst = tmp;
  for (int sh = 0; sh < 32; sh += 8) {
    size_t c[257] = {0};
    for (size_t i = 0; i < n; i++) c[((src[i] >> sh) & 255) + 1]++;
    if (c[1+((src[0]>>sh)&255)] == n) continue;
    for (int i = 0; i < 256; i++) c[i+1] += c[i];
    for (size_t i = 0; i < n; i++) dst[c[(src[i] >> sh) & 255]++] = src[i];
    uint32_t *x = src; src = dst; dst = x;
  }
  if (src != a) memcpy(a, src, n * 4);
}

static uint32_t NE; static GE *ges; static uint32_t *gcount, *goff, *gerr, *all;
static char **hourout; static size_t *houroutlen; static char hourstr[NH][24];
static int nthreads_g; static volatile int nexthour;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;

static char *u2s(char *o, uint32_t v) { char b[12]; int n = 0; do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v); while (n) *o++ = b[--n]; return o; }

static void *outworker(void *arg) {
  (void)arg; uint32_t *tmp = malloc(4 << 20); size_t tmpcap = 1 << 20;
  for (;;) {
    pthread_mutex_lock(&mu); int h = nexthour++; pthread_mutex_unlock(&mu);
    if (h >= NH) break;
    size_t cap = 0, len = 0; char *o = NULL;
    for (uint32_t e = 0; e < NE; e++) {
      size_t g = (size_t)h * NE + e; uint32_t n = gcount[g]; if (!n) continue;
      uint32_t *v = all + goff[g];
      if (n > tmpcap) { tmpcap = n; tmp = realloc(tmp, tmpcap * 4); }
      sort_u32(v, n, tmp);
      if (len + 256 > cap) { cap = cap ? cap * 2 : 65536; o = realloc(o, cap); }
      char *w = o + len;
      memcpy(w, hourstr[h], 20); w += 20; *w++ = ' ';
      memcpy(w, ges[e].s, ges[e].len); w += ges[e].len; *w++ = ' ';
      w = u2s(w, n); *w++ = ' ';
      w = u2s(w, v[(50ull * n + 99) / 100 - 1]); *w++ = ' ';
      w = u2s(w, v[(95ull * n + 99) / 100 - 1]); *w++ = ' ';
      w = u2s(w, v[(99ull * n + 99) / 100 - 1]); *w++ = ' ';
      w = u2s(w, gerr[g]); *w++ = ' ';
      w = u2s(w, (uint32_t)((uint64_t)gerr[g] * 1000000ull / n)); *w++ = '\n';
      len = (size_t)(w - o);
    }
    hourout[h] = o; houroutlen[h] = len;
  }
  free(tmp); return NULL;
}

int main(int argc, char **argv) {
  const char *data = argc > 1 ? argv[1] : "/data", *out = argc > 2 ? argv[2] : "/out";
  char path[4096]; snprintf(path, sizeof path, "%s/requests.log", data);
  int fd = open(path, O_RDONLY); if (fd < 0) { perror("open"); return 1; }
  struct stat st; fstat(fd, &st); off_t size = st.st_size;
  BASE_H = days_from_civil(2026, 10, 30) * 24 + 18 - 64;   /* slack before the span */
  long nt = sysconf(_SC_NPROCESSORS_ONLN); if (nt < 1) nt = 1; if (nt > NT_MAX) nt = NT_MAX;
  if (size < (1 << 20)) nt = 1;
  Th *th = calloc((size_t)nt, sizeof(Th)); pthread_t pt[NT_MAX];
  /* split at newlines */
  off_t *cut = malloc(sizeof(off_t) * (size_t)(nt + 1)); cut[0] = 0; cut[nt] = size;
  for (long i = 1; i < nt; i++) {
    off_t p = size * i / nt; char b[512];
    if (p <= cut[i-1]) { cut[i] = cut[i-1]; continue; }
    for (;;) { ssize_t r = pread(fd, b, sizeof b, p); if (r <= 0) { p = size; break; }
      char *nl = memchr(b, '\n', (size_t)r); if (nl) { p += (nl - b) + 1; break; } p += r; }
    cut[i] = p;
  }
  for (long i = 0; i < nt; i++) { th[i].fd = fd; th[i].beg = cut[i]; th[i].end = cut[i+1]; pthread_create(&pt[i], NULL, worker, &th[i]); }
  for (long i = 0; i < nt; i++) pthread_join(pt[i], NULL);

  /* merge endpoints */
  size_t tot = 0; for (long i = 0; i < nt; i++) tot += (size_t)th[i].nep;
  GE *tmpe = malloc(sizeof(GE) * (tot + 1)); size_t k = 0;
  for (long i = 0; i < nt; i++) for (int j = 0; j < th[i].nep; j++) { tmpe[k].s = th[i].ep[j].s; tmpe[k].len = th[i].ep[j].len; k++; }
  qsort(tmpe, k, sizeof(GE), ge_cmp);
  ges = malloc(sizeof(GE) * (k + 1)); NE = 0;
  for (size_t i = 0; i < k; i++) if (!NE || ge_cmp(&ges[NE-1], &tmpe[i])) ges[NE++] = tmpe[i];
  /* local -> global maps */
  uint32_t **map = malloc(sizeof(uint32_t*) * (size_t)nt);
  for (long i = 0; i < nt; i++) {
    map[i] = malloc(4 * (size_t)(th[i].nep + 1));
    for (int j = 0; j < th[i].nep; j++) { GE x = { th[i].ep[j].s, th[i].ep[j].len };
      GE *f = bsearch(&x, ges, NE, sizeof(GE), ge_cmp); map[i][j] = (uint32_t)(f - ges); }
  }
  size_t NG = (size_t)NH * NE;
  gcount = calloc(NG + 1, 4); goff = calloc(NG + 1, 4); gerr = calloc(NG + 1, 4);
  size_t N = 0;
  for (long i = 0; i < nt; i++) { Th *t = &th[i]; N += t->n;
    for (size_t j = 0; j < t->n; j++) { uint32_t kk = t->key[j]; size_t g = (size_t)(kk >> 12) * NE + map[i][kk & 4095];
      t->key[j] = (uint32_t)g; gcount[g]++; gerr[g] += t->err[j]; } }
  uint32_t acc = 0; for (size_t g = 0; g < NG; g++) { goff[g] = acc; acc += gcount[g]; }
  all = malloc(4 * (N + 1));
  uint32_t *fill = malloc(4 * (NG + 1)); memcpy(fill, goff, 4 * NG);
  for (long i = 0; i < nt; i++) { Th *t = &th[i]; free(t->err);
    for (size_t j = 0; j < t->n; j++) all[fill[t->key[j]]++] = t->lat[j];
    free(t->key); free(t->lat); }
  free(fill);
  for (int h = 0; h < NH; h++) {
    long ah = BASE_H + h; long days = ah / 24; int hh = (int)(ah % 24);
    /* civil_from_days */
    long z = days + 719468; long era = (z >= 0 ? z : z - 146096) / 146097; unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365; long y = (long)yoe + era * 400; unsigned doy = doe - (365*yoe + yoe/4 - yoe/100);
    unsigned mp = (5*doy + 2)/153; unsigned d = doy - (153*mp+2)/5 + 1; unsigned m = mp < 10 ? mp+3 : mp-9; y += (m <= 2);
    snprintf(hourstr[h], 24, "%04ld-%02u-%02uT%02d:00:00Z", y, m, d, hh);
  }
  hourout = calloc(NH, sizeof(char*)); houroutlen = calloc(NH, sizeof(size_t));
  nthreads_g = (int)nt;
  for (long i = 0; i < nt; i++) pthread_create(&pt[i], NULL, outworker, NULL);
  for (long i = 0; i < nt; i++) pthread_join(pt[i], NULL);
  snprintf(path, sizeof path, "%s/summary.txt", out);
  FILE *f = fopen(path, "wb"); if (!f) { perror("fopen"); return 1; }
  for (int h = 0; h < NH; h++) if (houroutlen[h]) fwrite(hourout[h], 1, houroutlen[h], f);
  fclose(f);
  return 0;
}
