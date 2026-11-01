// Speed Demon 2026 Track C (Join) - C solution.
// Built by Tab, an AI assistant, for team lead Akshay Wagh.
#define _GNU_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define NT_MAX 64
#define MAXREG 10000000u
static uint8_t *regc;            /* region_id -> country index + 1 (0 = unknown) */
static int NC;                   /* number of countries; group NC = _UNMATCHED */
static char codes[256][4];
static const char *F; static size_t FS;

typedef struct { int64_t count, idsum, asum, amin, amax, nbytes, tbytes; } Agg;
typedef struct { const char *s; uint32_t len, hash; } Nm;
typedef struct {
  size_t beg, end;
  Agg agg[257];
  Nm *nm; int32_t *tab; uint32_t nnm, tcap;  /* name interning (unescaped) */
  char *arena; size_t aused, acap;
  uint32_t *pair; size_t np, pcap;           /* (local name id << 8) | group */
} Th;

static uint32_t intern(Th *t, const char *s, uint32_t len) {
  uint32_t h = 2166136261u; for (uint32_t i = 0; i < len; i++) h = (h ^ (uint8_t)s[i]) * 16777619u;
  if ((t->nnm + 1) * 2 > t->tcap) {   /* grow */
    uint32_t nc = t->tcap ? t->tcap * 2 : 1 << 16; int32_t *nt = malloc(4 * (size_t)nc); memset(nt, -1, 4 * (size_t)nc);
    for (uint32_t i = 0; i < t->nnm; i++) { uint32_t j = t->nm[i].hash & (nc - 1); while (nt[j] >= 0) j = (j + 1) & (nc - 1); nt[j] = (int32_t)i; }
    free(t->tab); t->tab = nt; t->tcap = nc; t->nm = realloc(t->nm, sizeof(Nm) * (nc / 2 + 1));
  }
  uint32_t j = h & (t->tcap - 1);
  for (;;) { int32_t id = t->tab[j]; if (id < 0) break; Nm *n = &t->nm[id]; if (n->hash == h && n->len == len && memcmp(n->s, s, len) == 0) return (uint32_t)id; j = (j + 1) & (t->tcap - 1); }
  if (t->aused + len > t->acap) { t->acap = t->acap ? t->acap * 2 : 1 << 20; if (t->acap < len) t->acap = len * 2; t->arena = malloc(t->acap); t->aused = 0; }
  char *c = t->arena + t->aused; memcpy(c, s, len); t->aused += len;
  uint32_t id = t->nnm++; t->nm[id] = (Nm){c, len, h}; t->tab[j] = (int32_t)id; return id;
}

/* parse a quoted field starting at p (on the opening quote). Writes unescaped bytes to out if out != NULL.
   returns pointer after closing quote; *ulen = unescaped length; *esc = whether "" occurred */
static inline const char *quoted(const char *p, uint32_t *ulen, int *esc) {
  const char *s = p + 1; uint32_t pairs = 0; const char *q = s;
  for (;;) { q = memchr(q, '"', (size_t)(F + FS - q)); if (q[1] == '"') { pairs++; q += 2; continue; } break; }
  *ulen = (uint32_t)(q - s) - pairs; *esc = pairs > 0; return q + 1;
}

static void *work(void *arg) {
  Th *t = arg; const char *p = F + t->beg, *e = F + t->end; char ubuf[65536];
  for (int g = 0; g <= NC; g++) { t->agg[g].amin = INT64_MAX; t->agg[g].amax = INT64_MIN; }
  while (p < e) {
    if ((uint8_t)p[0] == 0xEF && (uint8_t)p[1] == 0xBB && (uint8_t)p[2] == 0xBF) p += 3;
    int64_t id = 0; while (*p != ',') id = id * 10 + (*p++ - '0'); p++;
    /* name */
    const char *ns; uint32_t nlen; int esc = 0;
    if (*p == '"') { ns = p + 1; p = quoted(p, &nlen, &esc); } else { ns = p; while (*p != ',') p++; nlen = (uint32_t)(p - ns); }
    const char *nraw = ns; uint32_t nrawlen = esc ? (uint32_t)(p - 1 - ns) : nlen;
    p++;
    /* note */
    uint32_t tlen; int tesc;
    if (*p == '"') p = quoted(p, &tlen, &tesc); else { const char *s0 = p; while (*p != ',') p++; tlen = (uint32_t)(p - s0); }
    p++;
    uint32_t reg = 0; while (*p != ',') reg = reg * 10 + (uint32_t)(*p++ - '0'); p++;
    int neg = 0; if (*p == '-') { neg = 1; p++; }
    int64_t a = 0; while (*p != '.') a = a * 10 + (*p++ - '0'); a = a * 100 + (p[1] - '0') * 10 + (p[2] - '0'); p += 3; if (neg) a = -a;
    if (*p == '\r') p++; p++;
    int g = (reg < MAXREG && regc[reg]) ? regc[reg] - 1 : NC;
    Agg *A = &t->agg[g]; A->count++; A->idsum += id; A->asum += a; if (a < A->amin) A->amin = a; if (a > A->amax) A->amax = a; A->nbytes += nlen; A->tbytes += tlen;
    const char *key = nraw; uint32_t klen = nlen;
    if (esc) { uint32_t k = 0; for (uint32_t i = 0; i < nrawlen; i++) { ubuf[k++] = nraw[i]; if (nraw[i] == '"') i++; } key = ubuf; klen = k; }
    uint32_t lid = intern(t, key, klen);
    if (t->np == t->pcap) { t->pcap = t->pcap ? t->pcap * 2 : 1 << 18; t->pair = realloc(t->pair, 4 * t->pcap); }
    t->pair[t->np++] = (lid << 8) | (uint32_t)g;
  }
  return NULL;
}

static char *mapfile(const char *path, size_t *sz) { int fd = open(path, O_RDONLY); if (fd < 0) { perror(path); exit(1); }
  struct stat st; fstat(fd, &st); *sz = (size_t)st.st_size; if (!*sz) return NULL;
  char *p = mmap(NULL, *sz + 4096, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0); close(fd); return p; }

typedef struct { const char *s; uint32_t len; } GN;
static int gncmp(const void *a, const void *b) { const GN *x = a, *y = b; uint32_t n = x->len < y->len ? x->len : y->len; int c = memcmp(x->s, y->s, n); return c ? c : (int)x->len - (int)y->len; }
static int codecmp(const void *a, const void *b) { return memcmp(a, b, 3); }

static char *money(char *w, int64_t c) { if (c < 0) { *w++ = '-'; c = -c; } int64_t ip = c / 100; int f = (int)(c % 100);
  char b[24]; int n = 0; do { b[n++] = (char)('0' + ip % 10); ip /= 10; } while (ip); while (n) *w++ = b[--n]; *w++ = '.'; *w++ = (char)('0' + f / 10); *w++ = (char)('0' + f % 10); return w; }
static char *num(char *w, int64_t v) { char b[24]; int n = 0; do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v); while (n) *w++ = b[--n]; return w; }

int main(int argc, char **argv) {
  const char *data = argc > 1 ? argv[1] : "/data", *out = argc > 2 ? argv[2] : "/out"; char path[4096];
  /* regions */
  size_t rs; snprintf(path, sizeof path, "%s/regions.csv", data); const char *R = mapfile(path, &rs);
  const char *p = memchr(R, '\n', rs) + 1, *re = R + rs;
  char (*tmp)[4] = malloc(4 * 300000); int nr = 0;
  regc = calloc(MAXREG, 1);
  /* pass 1: collect codes */
  for (const char *q = p; q < re;) { while (*q != ',') q++; q++; memcpy(tmp[nr], q, 3); tmp[nr][3] = 0; nr++; q += 4; }
  qsort(tmp, (size_t)nr, 4, codecmp);
  NC = 0; for (int i = 0; i < nr; i++) if (!NC || memcmp(codes[NC - 1], tmp[i], 3)) { memcpy(codes[NC], tmp[i], 4); NC++; }
  for (const char *q = p; q < re;) { uint32_t id = 0; while (*q != ',') id = id * 10 + (uint32_t)(*q++ - '0'); q++;
    char (*f)[4] = bsearch(q, codes, (size_t)NC, 4, codecmp); regc[id] = (uint8_t)((f - codes) + 1); q += 4; }
  /* records */
  snprintf(path, sizeof path, "%s/records.csv", data); F = mapfile(path, &FS);
  p = F; if ((uint8_t)p[0] == 0xEF) p += 3; p = memchr(p, '\n', FS) + 1; size_t body = (size_t)(p - F);
  long nt = sysconf(_SC_NPROCESSORS_ONLN); if (nt < 1) nt = 1; if (nt > NT_MAX) nt = NT_MAX;
  Th *th = calloc((size_t)nt, sizeof(Th)); pthread_t pt[NT_MAX];
  /* split: a record starts right after an LF that is followed by a digit or a BOM (2.2: notes are lowercase words,
     an LF inside a note is always followed by a letter or a quote, never a digit or 0xEF) */
  size_t prev = body;
  for (long i = 0; i < nt; i++) {
    size_t c = (i == nt - 1) ? FS : body + (FS - body) * (size_t)(i + 1) / (size_t)nt;
    if (c < prev) c = prev;
    while (c < FS) { if (F[c - 1] == '\n' && ((F[c] >= '0' && F[c] <= '9') || (uint8_t)F[c] == 0xEF)) break; c++; }
    th[i].beg = prev; th[i].end = c; prev = c;
  }
  for (long i = 0; i < nt; i++) pthread_create(&pt[i], NULL, work, &th[i]);
  for (long i = 0; i < nt; i++) pthread_join(pt[i], NULL);
  /* merge names */
  size_t tot = 0; for (long i = 0; i < nt; i++) tot += th[i].nnm;
  GN *all = malloc(sizeof(GN) * (tot + 1)); size_t k = 0;
  for (long i = 0; i < nt; i++) for (uint32_t j = 0; j < th[i].nnm; j++) all[k++] = (GN){th[i].nm[j].s, th[i].nm[j].len};
  qsort(all, k, sizeof(GN), gncmp);
  GN *uq = malloc(sizeof(GN) * (k + 1)); size_t NU = 0; for (size_t i = 0; i < k; i++) if (!NU || gncmp(&uq[NU - 1], &all[i])) uq[NU++] = all[i];
  size_t words = (NU + 63) / 64; uint64_t *bits = calloc(words * (size_t)(NC + 1), 8);
  Agg G[257]; for (int g = 0; g <= NC; g++) { G[g] = (Agg){0, 0, 0, INT64_MAX, INT64_MIN, 0, 0}; }
  for (long i = 0; i < nt; i++) { Th *t = &th[i];
    uint32_t *map = malloc(4 * (t->nnm + 1));
    for (uint32_t j = 0; j < t->nnm; j++) { GN x = {t->nm[j].s, t->nm[j].len}; GN *f = bsearch(&x, uq, NU, sizeof(GN), gncmp); map[j] = (uint32_t)(f - uq); }
    for (size_t j = 0; j < t->np; j++) { uint32_t g = t->pair[j] & 255, gid = map[t->pair[j] >> 8]; bits[(size_t)g * words + gid / 64] |= 1ull << (gid % 64); }
    for (int g = 0; g <= NC; g++) { Agg *a = &t->agg[g]; if (!a->count) continue; G[g].count += a->count; G[g].idsum += a->idsum; G[g].asum += a->asum;
      if (a->amin < G[g].amin) G[g].amin = a->amin; if (a->amax > G[g].amax) G[g].amax = a->amax; G[g].nbytes += a->nbytes; G[g].tbytes += a->tbytes; }
    free(map);
  }
  char *buf = malloc(256 * 300), *w = buf;
  for (int g = 0; g <= NC; g++) { if (!G[g].count) continue;
    int64_t nd = 0; for (size_t x = 0; x < words; x++) nd += __builtin_popcountll(bits[(size_t)g * words + x]);
    if (g < NC) { memcpy(w, codes[g], 3); w += 3; } else { memcpy(w, "_UNMATCHED", 10); w += 10; }
    *w++ = ' '; w = num(w, G[g].count); *w++ = ' '; w = num(w, G[g].idsum); *w++ = ' '; w = money(w, G[g].asum); *w++ = ' ';
    w = money(w, G[g].amin); *w++ = ' '; w = money(w, G[g].amax); *w++ = ' '; w = num(w, nd); *w++ = ' '; w = num(w, G[g].nbytes); *w++ = ' '; w = num(w, G[g].tbytes); *w++ = '\n'; }
  snprintf(path, sizeof path, "%s/aggregates.txt", out); FILE *f = fopen(path, "wb"); if (!f) { perror("fopen"); return 1; }
  fwrite(buf, 1, (size_t)(w - buf), f); fclose(f); return 0;
}
