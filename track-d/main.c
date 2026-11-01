// Speed Demon 2026 Track D (Traverse) - C solution.
// Built by Tab, an AI assistant, for team lead Akshay Wagh.
// Parallel parse -> forward and reverse CSR -> one bidirectional Dijkstra per query,
// queries spread over all hardware threads; touched lists reset only what a query changed.
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
static uint32_t N, M;
static uint32_t *off;          /* CSR offsets, N+1 */
static uint32_t *adjv, *adjw;  /* heads and weights */
static uint32_t *roff, *radjv, *radjw; /* reverse CSR: tails and weights */
static uint32_t *eu, *ev, *ew; /* edge list */
static const char *G; static size_t GS;
static int NT;

static inline uint32_t num(const char **pp) { const char *p = *pp; uint32_t v = 0; while (*p >= '0') v = v * 10 + (uint32_t)(*p++ - '0'); *pp = p + 1; return v; }

typedef struct { size_t beg, end; uint32_t first, cnt; } Chunk;
static Chunk ch[NT_MAX];

static void *count_lines(void *a) { Chunk *c = a; uint32_t k = 0; const char *p = G + c->beg, *e = G + c->end;
  while (p < e) { const char *q = memchr(p, '\n', (size_t)(e - p)); if (!q) break; k++; p = q + 1; } c->cnt = k; return NULL; }
static void *parse_edges(void *a) { Chunk *c = a; const char *p = G + c->beg; uint32_t i = c->first, end = c->first + c->cnt;
  for (; i < end; i++) { uint32_t u = num(&p), v = num(&p), w = num(&p); eu[i] = u; ev[i] = v; ew[i] = w; __atomic_fetch_add(&off[u + 1], 1, __ATOMIC_RELAXED); __atomic_fetch_add(&roff[v + 1], 1, __ATOMIC_RELAXED); }
  return NULL; }
static uint32_t *pos, *rpos;
static void *fill_csr(void *a) { Chunk *c = a; for (uint32_t i = c->first; i < c->first + c->cnt; i++) {
    uint32_t p = __atomic_fetch_add(&pos[eu[i]], 1, __ATOMIC_RELAXED); adjv[p] = ev[i]; adjw[p] = ew[i];
    uint32_t r = __atomic_fetch_add(&rpos[ev[i]], 1, __ATOMIC_RELAXED); radjv[r] = eu[i]; radjw[r] = ew[i]; } return NULL; }

static uint32_t Q; static uint32_t *qs, *qt; static int64_t *ans;
static volatile uint32_t qnext;

typedef struct { uint64_t d; uint32_t v; } HE;
static inline void hpush(HE **hp, size_t *hn, size_t *hc, uint64_t d, uint32_t v) {
  if (*hn == *hc) { *hc *= 2; *hp = realloc(*hp, sizeof(HE) * *hc); }
  HE *h = *hp; size_t i = (*hn)++; while (i) { size_t p = (i - 1) >> 1; if (h[p].d <= d) break; h[i] = h[p]; i = p; } h[i] = (HE){d, v}; }
static inline HE hpop(HE *h, size_t *hn) {
  HE top = h[0], last = h[--*hn]; size_t n = *hn;
  if (n) { size_t i = 0; for (;;) { size_t l = 2 * i + 1; if (l >= n) break; size_t r = l + 1, m = (r < n && h[r].d < h[l].d) ? r : l; if (h[m].d >= last.d) break; h[i] = h[m]; i = m; } h[i] = last; }
  return top; }
/* Bidirectional Dijkstra: forward search from s on the graph, backward search from t on the reverse graph.
   Per-node stamps (query id) replace clearing; stop when topF + topB >= best. */
static void *solve(void *arg) {
  (void)arg;
  uint64_t *df = malloc(8 * (size_t)N), *db = malloc(8 * (size_t)N);
  uint64_t *xf = calloc((N + 63) / 64, 8), *xb = calloc((N + 63) / 64, 8);   /* settled bitsets */
  size_t tcf = 1 << 14, tcb = 1 << 14, ntf = 0, ntb = 0; uint32_t *tf = malloc(4 * tcf), *tb = malloc(4 * tcb);  /* touched lists */
  memset(df, 0xff, 8 * (size_t)N); memset(db, 0xff, 8 * (size_t)N);
  size_t cf = 1 << 14, cb = 1 << 14; HE *hf = malloc(sizeof(HE) * cf), *hb = malloc(sizeof(HE) * cb);
  for (;;) {
    uint32_t qi = __atomic_fetch_add(&qnext, 1, __ATOMIC_RELAXED); if (qi >= Q) break;
    uint32_t s = qs[qi], t = qt[qi];
    if (s == t) { ans[qi] = 0; continue; }
    if (off[s] == off[s + 1] || roff[t] == roff[t + 1]) { ans[qi] = -1; continue; }
    uint64_t best = UINT64_MAX; size_t nf = 0, nb = 0;
    df[s] = 0; tf[ntf++] = s; hpush(&hf, &nf, &cf, 0, s);
    db[t] = 0; tb[ntb++] = t; hpush(&hb, &nb, &cb, 0, t);
    while (nf && nb) {
      if (hf[0].d + hb[0].d >= best) break;
      if (hf[0].d <= hb[0].d) {
        HE e = hpop(hf, &nf); uint32_t u = e.v; if ((xf[u >> 6] >> (u & 63)) & 1 || e.d > df[u]) continue; xf[u >> 6] |= 1ull << (u & 63);
        for (uint32_t k = off[u], ke = off[u + 1]; k < ke; k++) { uint32_t v = adjv[k]; uint64_t nd = e.d + adjw[k];
          if (nd < df[v]) { if (df[v] == UINT64_MAX) { if (ntf == tcf) { tcf *= 2; tf = realloc(tf, 4 * tcf); } tf[ntf++] = v; } df[v] = nd; hpush(&hf, &nf, &cf, nd, v);
            if (db[v] != UINT64_MAX && nd + db[v] < best) best = nd + db[v]; } }
      } else {
        HE e = hpop(hb, &nb); uint32_t u = e.v; if ((xb[u >> 6] >> (u & 63)) & 1 || e.d > db[u]) continue; xb[u >> 6] |= 1ull << (u & 63);
        for (uint32_t k = roff[u], ke = roff[u + 1]; k < ke; k++) { uint32_t v = radjv[k]; uint64_t nd = e.d + radjw[k];
          if (nd < db[v]) { if (db[v] == UINT64_MAX) { if (ntb == tcb) { tcb *= 2; tb = realloc(tb, 4 * tcb); } tb[ntb++] = v; } db[v] = nd; hpush(&hb, &nb, &cb, nd, v);
            if (df[v] != UINT64_MAX && nd + df[v] < best) best = nd + df[v]; } }
      }
    }
    ans[qi] = best == UINT64_MAX ? -1 : (int64_t)best;
    for (size_t k = 0; k < ntf; k++) { uint32_t v = tf[k]; df[v] = UINT64_MAX; xf[v >> 6] = 0; } ntf = 0;
    for (size_t k = 0; k < ntb; k++) { uint32_t v = tb[k]; db[v] = UINT64_MAX; xb[v >> 6] = 0; } ntb = 0;
  }
  return NULL;
}

static char *mapfile(const char *path, size_t *sz) { int fd = open(path, O_RDONLY); if (fd < 0) { perror(path); exit(1); }
  struct stat st; fstat(fd, &st); *sz = (size_t)st.st_size; if (!*sz) return NULL;
  char *p = mmap(NULL, *sz, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0); close(fd); return p; }

int main(int argc, char **argv) {
  const char *data = argc > 1 ? argv[1] : "/data", *out = argc > 2 ? argv[2] : "/out";
  char path[4096];
  long nt = sysconf(_SC_NPROCESSORS_ONLN); if (nt < 1) nt = 1; if (nt > NT_MAX) nt = NT_MAX; NT = (int)nt;
  snprintf(path, sizeof path, "%s/graph.txt", data); G = mapfile(path, &GS);
  const char *p = G; N = num(&p); M = num(&p); size_t body = (size_t)(p - G);
  /* chunk at newline boundaries */
  size_t span = GS - body; size_t prev = body;
  for (int i = 0; i < NT; i++) { size_t e = (i == NT - 1) ? GS : body + span * (size_t)(i + 1) / (size_t)NT;
    if (e < prev) e = prev; while (e < GS && e > prev && G[e - 1] != '\n') e++; ch[i].beg = prev; ch[i].end = e; prev = e; }
  pthread_t th[NT_MAX];
  for (int i = 0; i < NT; i++) pthread_create(&th[i], NULL, count_lines, &ch[i]);
  for (int i = 0; i < NT; i++) pthread_join(th[i], NULL);
  uint32_t acc = 0; for (int i = 0; i < NT; i++) { ch[i].first = acc; acc += ch[i].cnt; }
  M = acc;
  off = calloc((size_t)N + 2, 4); roff = calloc((size_t)N + 2, 4); eu = malloc(4 * (size_t)M + 4); ev = malloc(4 * (size_t)M + 4); ew = malloc(4 * (size_t)M + 4);
  for (int i = 0; i < NT; i++) pthread_create(&th[i], NULL, parse_edges, &ch[i]);
  for (int i = 0; i < NT; i++) pthread_join(th[i], NULL);
  for (uint32_t i = 0; i < N; i++) { off[i + 1] += off[i]; roff[i + 1] += roff[i]; }
  pos = malloc(4 * ((size_t)N + 1)); memcpy(pos, off, 4 * ((size_t)N + 1)); rpos = malloc(4 * ((size_t)N + 1)); memcpy(rpos, roff, 4 * ((size_t)N + 1));
  adjv = malloc(4 * (size_t)M + 4); adjw = malloc(4 * (size_t)M + 4); radjv = malloc(4 * (size_t)M + 4); radjw = malloc(4 * (size_t)M + 4);
  for (int i = 0; i < NT; i++) pthread_create(&th[i], NULL, fill_csr, &ch[i]);
  for (int i = 0; i < NT; i++) pthread_join(th[i], NULL);
  free(eu); free(ev); free(ew); free(pos); free(rpos); munmap((void *)G, GS);
  size_t qsz; snprintf(path, sizeof path, "%s/queries.txt", data); const char *qf = mapfile(path, &qsz);
  Q = 0; for (size_t i = 0; i < qsz; i++) Q += (qf[i] == '\n');
  qs = malloc(4 * (size_t)Q + 4); qt = malloc(4 * (size_t)Q + 4); ans = malloc(8 * (size_t)Q + 8);
  p = qf; for (uint32_t i = 0; i < Q; i++) { qs[i] = num(&p); qt[i] = num(&p); }
  for (int i = 0; i < NT; i++) pthread_create(&th[i], NULL, solve, NULL);
  for (int i = 0; i < NT; i++) pthread_join(th[i], NULL);
  snprintf(path, sizeof path, "%s/distances.txt", out);
  FILE *f = fopen(path, "wb"); if (!f) { perror("fopen"); return 1; }
  char *buf = malloc((size_t)Q * 24 + 1), *w = buf;
  for (uint32_t i = 0; i < Q; i++) { if (ans[i] < 0) { *w++ = '-'; *w++ = '1'; } else { char b[24]; int n = 0; uint64_t v = (uint64_t)ans[i]; do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v); while (n) *w++ = b[--n]; } *w++ = '\n'; }
  fwrite(buf, 1, (size_t)(w - buf), f); fclose(f);
  return 0;
}
