// Speed Demon 2026 Track D (Traverse) - C solution.
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
typedef struct { uint32_t v, w; } E;
static uint32_t N, Q, *off, *qs, *qt; static uint8_t *hasin; static E *adj; static int64_t *ans;
static const char *G; static size_t GS; static int NT; static uint32_t qnext;
static size_t cb[NT_MAX + 1];
static void run(void *(*f)(void *)) { pthread_t th[NT_MAX]; for (long i = 0; i < NT; i++) pthread_create(&th[i], NULL, f, (void *)i); for (int i = 0; i < NT; i++) pthread_join(th[i], NULL); }
static inline uint32_t num(const char **pp) { const char *p = *pp; uint32_t v = *p++ - '0'; while (*p >= '0') v = v * 10 + (uint32_t)(*p++ - '0'); *pp = p + 1; return v; }
/* CSR build without atomics: each thread counts its own edges per tail, the counts become per-thread write positions */
static uint32_t *cnt[NT_MAX], rsum[NT_MAX];
static void *pass1(void *a) { long i = (long)a; const char *p = G + cb[i], *e = G + cb[i + 1]; uint32_t *c = cnt[i] = calloc((size_t)N + 1, 4);
  while (p < e) { uint32_t u = num(&p), v = num(&p); while (*p++ != '\n'); c[u]++; hasin[v] = 1; } return NULL; }
static void *scan(void *a) { long t = (long)a; uint32_t lo = (uint32_t)((uint64_t)N * t / NT), hi = (uint32_t)((uint64_t)N * (t + 1) / NT), s = 0;
  if (!off[N + 1]) { for (uint32_t u = lo; u < hi; u++) for (int k = 0; k < NT; k++) s += cnt[k][u]; rsum[t] = s; return NULL; }
  s = rsum[t]; for (uint32_t u = lo; u < hi; u++) { off[u] = s; for (int k = 0; k < NT; k++) { uint32_t x = cnt[k][u]; cnt[k][u] = s; s += x; } } return NULL; }
static void *pass2(void *a) { long i = (long)a; const char *p = G + cb[i], *e = G + cb[i + 1]; uint32_t *c = cnt[i];
  while (p < e) { uint32_t u = num(&p), v = num(&p), w = num(&p); adj[c[u]++] = (E){v, w}; } return NULL; }
/* radix heap of (distance << 32 | node); keys never drop below the last popped one */
typedef struct { uint64_t *a[33]; uint32_t n[33], cap[33]; uint32_t last; } RH;
static inline void rh_push(RH *h, uint64_t x) {
  uint32_t k = (uint32_t)(x >> 32), b = k == h->last ? 0 : 32 - (uint32_t)__builtin_clz(k ^ h->last);
  if (h->n[b] == h->cap[b]) { h->cap[b] = h->cap[b] ? 2 * h->cap[b] : 256; h->a[b] = realloc(h->a[b], 8 * (size_t)h->cap[b]); }
  h->a[b][h->n[b]++] = x;
}
static inline int rh_pop(RH *h, uint64_t *x) {
  if (!h->n[0]) {
    int b = 1; while (b < 33 && !h->n[b]) b++; if (b == 33) return 0;
    uint64_t m = h->a[b][0]; for (uint32_t i = 1; i < h->n[b]; i++) if (h->a[b][i] < m) m = h->a[b][i];
    h->last = (uint32_t)(m >> 32); uint32_t c = h->n[b]; h->n[b] = 0;
    for (uint32_t i = 0; i < c; i++) rh_push(h, h->a[b][i]);
  }
  *x = h->a[0][--h->n[0]]; return 1;
}
/* Dijkstra with 32-bit distances; returns -2 if a distance would not fit (redone in 64 bits) */
static int64_t search(uint32_t s, uint32_t t, uint32_t *dist, uint32_t **tp, size_t *tc, RH *h) {
  uint32_t *tch = *tp; size_t nt = 0; int64_t res = -1; uint64_t top;
  for (int b = 0; b < 33; b++) h->n[b] = 0; h->last = 0;
  dist[s] = 0; tch[nt++] = s; rh_push(h, s);
  while (rh_pop(h, &top)) {
    uint32_t u = (uint32_t)top, du = (uint32_t)(top >> 32);
    if (du > dist[u]) continue;
    if (u == t) { res = du; break; }
    for (uint32_t e = off[u]; e < off[u + 1]; e++) {
      uint64_t nd = (uint64_t)du + adj[e].w; uint32_t v = adj[e].v;
      if (nd >= UINT32_MAX) { res = -2; goto out; }
      if (nd >= dist[v]) continue;
      if (dist[v] == UINT32_MAX) { if (nt == *tc) { *tc *= 2; *tp = tch = realloc(tch, *tc * 4); } tch[nt++] = v; }
      dist[v] = (uint32_t)nd; rh_push(h, nd << 32 | v);
    }
  }
out:
  for (size_t i = 0; i < nt; i++) dist[tch[i]] = UINT32_MAX;
  return res;
}

/* relabel nodes in BFS order so that a search touches nearby memory: a level-synchronous parallel BFS,
   each thread appending what it discovers to its own buffer, the buffers concatenated in thread order per level */
static uint32_t *order, *nid, *noff, *lbuf[NT_MAX], lcnt[NT_MAX], qlo, qhi, qtail; static E *nadj;
static pthread_barrier_t bar;
static void *bfs(void *a) {
  long t = (long)a; uint32_t *buf = lbuf[t];
  for (;;) {
    uint32_t lo = qlo, hi = qhi, n = hi - lo, b = lo + (uint32_t)((uint64_t)n * t / NT), e = lo + (uint32_t)((uint64_t)n * (t + 1) / NT), c = 0;
    if (n == 0) break;
    for (uint32_t i = b; i < e; i++) { uint32_t u = order[i]; if (i + 4 < e) __builtin_prefetch(&off[order[i + 4]]);
      for (uint32_t k = off[u]; k < off[u + 1]; k++) { uint32_t v = adj[k].v, x = UINT32_MAX;
        if (nid[v] == UINT32_MAX && __atomic_compare_exchange_n(&nid[v], &x, UINT32_MAX - 1, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) buf[c++] = v; } }
    lcnt[t] = c; pthread_barrier_wait(&bar);
    uint32_t o = hi; for (long s = 0; s < t; s++) o += lcnt[s];
    for (uint32_t i = 0; i < c; i++) { order[o + i] = buf[i]; nid[buf[i]] = o + i; }
    pthread_barrier_wait(&bar);
    if (t == 0) { uint32_t tot = 0; for (long s = 0; s < NT; s++) tot += lcnt[s]; qlo = hi; qhi = hi + tot; }
    pthread_barrier_wait(&bar);
  }
  return NULL;
}
static void *pass3(void *a) { long t = (long)a; uint32_t lo = (uint32_t)((uint64_t)N * t / NT), hi = (uint32_t)((uint64_t)N * (t + 1) / NT);
  for (uint32_t i = lo; i < hi; i++) { uint32_t u = order[i], o = noff[i]; for (uint32_t e = off[u]; e < off[u + 1]; e++) nadj[o++] = (E){nid[adj[e].v], adj[e].w}; } return NULL; }
static void relabel(uint32_t root) {
  order = malloc(4 * (size_t)N); nid = malloc(4 * (size_t)N); memset(nid, 0xff, 4 * (size_t)N);
  for (int t = 0; t < NT; t++) lbuf[t] = malloc(4 * (size_t)N);
  pthread_barrier_init(&bar, NULL, (unsigned)NT);
  uint32_t tail = 0, next = 0;
  for (;;) {   /* the main body first, in parallel; then each small leftover piece from its first unvisited node */
    if (tail == 0) next = root; else { while (next < N && nid[next] != UINT32_MAX) next++; if (next == N) break; }
    nid[next] = tail; order[tail] = next; qlo = tail; qhi = tail + 1;
    if (tail == 0) { run(bfs); tail = qhi; next = 0; continue; }
    tail++;
    for (uint32_t head = qlo; head < tail; head++) { uint32_t u = order[head];
      for (uint32_t e = off[u]; e < off[u + 1]; e++) { uint32_t v = adj[e].v; if (nid[v] == UINT32_MAX) { nid[v] = tail; order[tail++] = v; } } }
  }
  for (int t = 0; t < NT; t++) free(lbuf[t]);
  noff = malloc(4 * ((size_t)N + 1)); noff[0] = 0;
  for (uint32_t i = 0; i < N; i++) noff[i + 1] = noff[i] + off[order[i] + 1] - off[order[i]];
  nadj = malloc(sizeof(E) * ((size_t)off[N] + 1));
  run(pass3);
  free(adj); free(off); free(order); adj = nadj; off = noff;
}
static void *solve(void *a) {
  (void)a; uint32_t *dist = malloc(4 * (size_t)N); memset(dist, 0xff, 4 * (size_t)N);
  size_t tc = 1 << 16; uint32_t *tch = malloc(tc * 4); RH h = {0};
  for (;;) { uint32_t i = __atomic_fetch_add(&qnext, 1, __ATOMIC_RELAXED); if (i >= Q) break;
    uint32_t s = qs[i], t = qt[i];
    ans[i] = s == t ? 0 : (off[s] == off[s + 1] || !hasin[t]) ? -1 : search(s, t, dist, &tch, &tc, &h); }
  free(dist); free(tch); for (int b = 0; b < 33; b++) free(h.a[b]); return NULL;
}
/* exact 64-bit Dijkstra for the few queries whose distances pass 2^32 - 1 */
static int64_t search64(uint32_t s, uint32_t t, uint64_t *dist) {
  typedef struct { uint64_t d; uint32_t v; } H; size_t hc = 1 << 16, hn = 0; H *h = malloc(hc * sizeof(H)); int64_t res = -1;
  uint32_t *tch = malloc(4 * (size_t)N); size_t nt = 0;
  dist[s] = 0; tch[nt++] = s; h[hn++] = (H){0, s};
  while (hn) {
    H top = h[0], last = h[--hn]; size_t i = 0;
    for (;;) { size_t l = 2 * i + 1; if (l >= hn) break; size_t m = (l + 1 < hn && h[l + 1].d < h[l].d) ? l + 1 : l; if (h[m].d >= last.d) break; h[i] = h[m]; i = m; }
    h[i] = last;
    if (top.d > dist[top.v]) continue;
    if (top.v == t) { res = (int64_t)top.d; break; }
    for (uint32_t e = off[top.v]; e < off[top.v + 1]; e++) {
      uint64_t nd = top.d + adj[e].w; uint32_t v = adj[e].v; if (nd >= dist[v]) continue;
      if (dist[v] == UINT64_MAX) tch[nt++] = v;
      dist[v] = nd; if (hn == hc) { hc *= 2; h = realloc(h, hc * sizeof(H)); }
      size_t j = hn++; while (j) { size_t p = (j - 1) / 2; if (h[p].d <= nd) break; h[j] = h[p]; j = p; } h[j] = (H){nd, v};
    }
  }
  for (size_t i = 0; i < nt; i++) dist[tch[i]] = UINT64_MAX;
  free(h); free(tch); return res;
}
static char *mapfile(const char *path, size_t *sz) { int fd = open(path, O_RDONLY); if (fd < 0) { perror(path); exit(1); }
  struct stat st; fstat(fd, &st); *sz = (size_t)st.st_size; char *p = *sz ? mmap(NULL, *sz, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0) : NULL; close(fd); return p; }
int main(int argc, char **argv) {
  const char *data = argc > 1 ? argv[1] : "/data", *out = argc > 2 ? argv[2] : "/out"; char path[4096];
  NT = ncpu();
  snprintf(path, sizeof path, "%s/graph.txt", data); G = mapfile(path, &GS);
  const char *p = G; N = num(&p); num(&p); size_t body = (size_t)(p - G);
  cb[0] = body; cb[NT] = GS;
  for (int i = 1; i < NT; i++) { size_t e = body + (GS - body) * (size_t)i / (size_t)NT; if (e < cb[i - 1]) e = cb[i - 1]; while (e < GS && G[e - 1] != '\n') e++; cb[i] = e; }
  off = calloc((size_t)N + 2, 4); hasin = calloc(N, 1);
  run(pass1); run(scan);
  { uint32_t s = 0; for (int t = 0; t < NT; t++) { uint32_t x = rsum[t]; rsum[t] = s; s += x; } off[N] = s; off[N + 1] = 1; }
  run(scan);
  adj = malloc(sizeof(E) * ((size_t)off[N] + 1));
  run(pass2);
  for (int t = 0; t < NT; t++) free(cnt[t]);
  size_t qsz; snprintf(path, sizeof path, "%s/queries.txt", data); const char *qf = mapfile(path, &qsz);
  Q = 0; for (size_t i = 0; i < qsz; i++) Q += qf[i] == '\n';
  qs = malloc(4 * (size_t)Q + 4); qt = malloc(4 * (size_t)Q + 4); ans = malloc(8 * (size_t)Q + 8);
  p = qf; for (uint32_t i = 0; i < Q; i++) { qs[i] = num(&p); qt[i] = num(&p); }
  { uint32_t r = 0; while (off[r] == off[r + 1]) r++; relabel(r); }
  { uint8_t *h2 = calloc(N, 1); for (uint32_t i = 0; i < N; i++) h2[nid[i]] = hasin[i]; free(hasin); hasin = h2; }
  for (uint32_t i = 0; i < Q; i++) { qs[i] = nid[qs[i]]; qt[i] = nid[qt[i]]; }
  free(nid);
  run(solve);
  uint64_t *d64 = NULL;
  for (uint32_t i = 0; i < Q; i++) if (ans[i] == -2) { if (!d64) { d64 = malloc(8 * (size_t)N); memset(d64, 0xff, 8 * (size_t)N); } ans[i] = search64(qs[i], qt[i], d64); }
  snprintf(path, sizeof path, "%s/distances.txt", out);
  FILE *f = fopen(path, "wb"); if (!f) { perror("fopen"); return 1; }
  char *buf = malloc((size_t)Q * 24 + 1), *w = buf;
  for (uint32_t i = 0; i < Q; i++) { if (ans[i] < 0) { *w++ = '-'; *w++ = '1'; } else { char b[24]; int n = 0; uint64_t v = (uint64_t)ans[i]; do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v); while (n) *w++ = b[--n]; } *w++ = '\n'; }
  fwrite(buf, 1, (size_t)(w - buf), f); fclose(f);
  return 0;
}
