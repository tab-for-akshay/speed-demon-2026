// Speed Demon 2026 Track D (Traverse) - C solution.
// Built by Tab, an AI assistant, for team lead Akshay Wagh.
#define _GNU_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#define NT_MAX 64
static int ncpu(void) { cpu_set_t s; CPU_ZERO(&s); int n = sched_getaffinity(0, sizeof s, &s) ? 1 : CPU_COUNT(&s); return n < 1 ? 1 : n > NT_MAX ? NT_MAX : n; }  /* the CPUs this container may use (12 on the board), not every CPU online (20) */
static uint32_t N, Q, *off, *qs, *qt; static uint8_t *hasin; static uint32_t *adj; static int64_t *ans;
static const char *G; static size_t GS; static int NT; static uint32_t qnext;
static size_t cb[NT_MAX + 1];
static void run(void *(*f)(void *)) { pthread_t th[NT_MAX]; for (long i = 0; i < NT; i++) pthread_create(&th[i], NULL, f, (void *)i); for (int i = 0; i < NT; i++) pthread_join(th[i], NULL); }
static inline uint32_t num(const char **pp) { const char *p = *pp; uint32_t v = *p++ - '0'; while (*p >= '0') v = v * 10 + (uint32_t)(*p++ - '0'); *pp = p + 1; return v; }
/* an edge is one 32-bit word: head in the low VB bits, weight above; a weight too big for its field is WX there
   and its real value sits in a short sorted list of (edge position, weight): k = 0 forward, 1 reverse, 2 the graph before relabelling */
static uint32_t VB, VM, WX, *cadj, *cradj, *roff, *epos[3], *ewt[3], nesc[3], *tpos[NT_MAX], *twt[NT_MAX], tn[NT_MAX], tcap[NT_MAX];
static inline void esc(long t, uint32_t e, uint32_t w) { if (tn[t] == tcap[t]) { tcap[t] = tcap[t] ? 2 * tcap[t] : 256; tpos[t] = realloc(tpos[t], 4 * (size_t)tcap[t]); twt[t] = realloc(twt[t], 4 * (size_t)tcap[t]); } tpos[t][tn[t]] = e; twt[t][tn[t]++] = w; }
static inline uint32_t pack(long t, uint32_t e, uint32_t v, uint32_t w) { if (w >= WX) { esc(t, e, w); w = WX; } return v | w << VB; }
static inline uint32_t wt(int k, const uint32_t *g, uint32_t e) {
  uint32_t w = g[e] >> VB; if (w != WX) return w;
  uint32_t lo = 0, hi = nesc[k]; while (hi - lo > 1) { uint32_t m = (lo + hi) / 2; if (epos[k][m] <= e) lo = m; else hi = m; } return ewt[k][lo];
}
static int pcmp(const void *a, const void *b) { const uint32_t *x = a, *y = b; return (x[0] > y[0]) - (x[0] < y[0]); }
static void gather(int k) {   /* the threads' escaped weights, sorted by edge position */
  uint32_t n = 0; for (int t = 0; t < NT; t++) n += tn[t];
  uint32_t *pr = malloc(8 * (size_t)n + 8); n = 0;
  for (int t = 0; t < NT; t++) { for (uint32_t i = 0; i < tn[t]; i++) { pr[2 * n] = tpos[t][i]; pr[2 * n++ + 1] = twt[t][i]; } tn[t] = 0; }
  qsort(pr, n, 8, pcmp); epos[k] = malloc(4 * (size_t)n + 4); ewt[k] = malloc(4 * (size_t)n + 4); nesc[k] = n;
  for (uint32_t i = 0; i < n; i++) { epos[k][i] = pr[2 * i]; ewt[k][i] = pr[2 * i + 1]; } free(pr);
}
static void *pass1(void *a) { long i = (long)a; const char *p = G + cb[i], *e = G + cb[i + 1];
  while (p < e) { uint32_t u = num(&p), v = num(&p); while (*p++ != '\n'); __atomic_fetch_add(&off[u + 1], 1, __ATOMIC_RELAXED); hasin[v] = 1; } return NULL; }
static void *pass2(void *a) { long i = (long)a; const char *p = G + cb[i], *e = G + cb[i + 1];
  while (p < e) { uint32_t u = num(&p), v = num(&p), w = num(&p), o = __atomic_fetch_add(&off[u], 1, __ATOMIC_RELAXED); adj[o] = pack(i, o, v, w); } return NULL; }
/* radix heap of (distance << 32 | node); keys never drop below the last popped one */
typedef struct { uint64_t *a[33]; uint32_t n[33], cap[33]; uint32_t last; } RH;
static inline void rh_push(RH *h, uint64_t x) {
  uint32_t k = (uint32_t)(x >> 32), b = k == h->last ? 0 : 32 - (uint32_t)__builtin_clz(k ^ h->last);
  if (h->n[b] == h->cap[b]) { h->cap[b] = h->cap[b] ? 2 * h->cap[b] : 256; h->a[b] = realloc(h->a[b], 8 * (size_t)h->cap[b]); }
  h->a[b][h->n[b]++] = x;
}
static inline int rh_min(RH *h, uint32_t *k) {   /* the smallest key, or 0 if empty */
  if (!h->n[0]) {
    int b = 1; while (b < 33 && !h->n[b]) b++; if (b == 33) return 0;
    uint64_t m = h->a[b][0]; for (uint32_t i = 1; i < h->n[b]; i++) if (h->a[b][i] < m) m = h->a[b][i];
    h->last = (uint32_t)(m >> 32); uint32_t c = h->n[b]; h->n[b] = 0;
    for (uint32_t i = 0; i < c; i++) rh_push(h, h->a[b][i]);
  }
  *k = h->last; return 1;
}
static void *rcount(void *a) { long t = (long)a; uint32_t lo = (uint32_t)((uint64_t)N * t / NT), hi = (uint32_t)((uint64_t)N * (t + 1) / NT);
  for (uint32_t e = off[lo]; e < off[hi]; e++) __atomic_fetch_add(&roff[(cadj[e] & VM) + 2], 1, __ATOMIC_RELAXED); return NULL; }
static void *rfill(void *a) { long t = (long)a; uint32_t lo = (uint32_t)((uint64_t)N * t / NT), hi = (uint32_t)((uint64_t)N * (t + 1) / NT);
  for (uint32_t u = lo; u < hi; u++) for (uint32_t e = off[u]; e < off[u + 1]; e++) { uint32_t p = __atomic_fetch_add(&roff[(cadj[e] & VM) + 1], 1, __ATOMIC_RELAXED); cradj[p] = pack(t, p, u, wt(0, cadj, e)); } return NULL; }
/* per-search distances live in a small open-addressing hash map (node << 32 | distance, linear probing, at most 3/4 full)
   instead of an N-entry array: a side of a search touches at most ~50K nodes, so the map stays around 1 MB and in cache,
   and the threads no longer carry 7 MB arrays each. tch lists the used slots so a map is cleared in O(used). */
#define EMPTY UINT64_MAX
typedef struct { uint64_t *m; uint32_t *tch, bits, n; RH h; } Side;
static inline uint32_t slot(const Side *X, uint32_t v) {
  uint32_t i = (v * 0x9E3779B1u) >> (32 - X->bits), mask = (1u << X->bits) - 1;
  while (X->m[i] != EMPTY && (uint32_t)(X->m[i] >> 32) != v) i = (i + 1) & mask;
  return i;
}
static inline uint32_t get(const Side *X, uint32_t v) { uint64_t e = X->m[slot(X, v)]; return e == EMPTY ? UINT32_MAX : (uint32_t)e; }
static void grow2(Side *X) {   /* double the map and reinsert what it holds */
  uint64_t *old = X->m; uint32_t n = X->n; X->bits++; X->m = malloc(8ull << X->bits); memset(X->m, 0xff, 8ull << X->bits);
  X->tch = realloc(X->tch, 3ull << X->bits);
  for (uint32_t k = 0; k < n; k++) { uint64_t e = old[X->tch[k]]; uint32_t j = slot(X, (uint32_t)(e >> 32)); X->m[j] = e; X->tch[k] = j; }
  free(old);
}
static inline void put(Side *X, uint32_t j, uint32_t v, uint32_t d) {
  if (X->m[j] == EMPTY) { X->tch[X->n++] = j; X->m[j] = (uint64_t)v << 32 | d; if (4 * X->n > 3u << X->bits) grow2(X); }
  else X->m[j] = (uint64_t)v << 32 | d;
}
/* settle one node of side X; 1 on a 32-bit overflow */
static inline int relax(Side *X, Side *Y, int k, const uint32_t *o, const uint32_t *g, uint64_t *mu) {
  uint64_t top = X->h.a[0][--X->h.n[0]]; uint32_t u = (uint32_t)top, du = (uint32_t)(top >> 32);
  if (du > get(X, u)) return 0;
  for (uint32_t e = o[u]; e < o[u + 1]; e++) {
    uint64_t nd = (uint64_t)du + wt(k, g, e); uint32_t v = g[e] & VM;
    if (nd >= UINT32_MAX) return 1;
    uint32_t j = slot(X, v); if (X->m[j] != EMPTY && nd >= (uint32_t)X->m[j]) continue;
    put(X, j, v, (uint32_t)nd); rh_push(&X->h, nd << 32 | v);
    uint32_t dy = get(Y, v); if (dy != UINT32_MAX && nd + dy < *mu) *mu = nd + dy;
  }
  return 0;
}
static void clear(Side *X) { for (uint32_t k = 0; k < X->n; k++) X->m[X->tch[k]] = EMPTY; X->n = 0; }
static int64_t bisearch(uint32_t s, uint32_t t, Side *F, Side *B) {
  uint64_t mu = UINT64_MAX; int64_t res; uint32_t kf, kb;
  for (int b = 0; b < 33; b++) F->h.n[b] = B->h.n[b] = 0; F->h.last = B->h.last = 0;
  put(F, slot(F, s), s, 0); rh_push(&F->h, s); put(B, slot(B, t), t, 0); rh_push(&B->h, t);
  for (;;) {
    if (!rh_min(&F->h, &kf) || !rh_min(&B->h, &kb) || (uint64_t)kf + kb >= mu) { res = mu == UINT64_MAX ? -1 : (int64_t)mu; break; }
    if (kf <= kb ? relax(F, B, 0, off, cadj, &mu) : relax(B, F, 1, roff, cradj, &mu)) { res = -2; break; }
  }
  clear(F); clear(B); return res;
}
/* relabel nodes so that a search touches nearby memory: each thread grows its own region by BFS from a seed of its own,
   claiming nodes with a CAS; the regions are numbered one after another, each in its BFS order */
static uint32_t *order, *nid, *noff, *reg[NT_MAX], rn[NT_MAX];
static void *grow(void *a) {
  long t = (long)a; uint32_t *q = reg[t] = malloc(4 * (size_t)N), n = 0, x = UINT32_MAX, s = (uint32_t)((uint64_t)N * t / NT);
  while (s < N && off[s] == off[s + 1]) s++;
  if (s < N && __atomic_compare_exchange_n(&nid[s], &x, 0, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) q[n++] = s;
  for (uint32_t h = 0; h < n; h++) { uint32_t u = q[h]; if (h + 4 < n) __builtin_prefetch(&off[q[h + 4]]);
    for (uint32_t e = off[u]; e < off[u + 1]; e++) { uint32_t v = adj[e] & VM; x = UINT32_MAX;
      if (nid[v] == UINT32_MAX && __atomic_compare_exchange_n(&nid[v], &x, 0, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) q[n++] = v; } }
  rn[t] = n; return NULL;
}
static void *pass3(void *a) { long t = (long)a; uint32_t lo = (uint32_t)((uint64_t)N * t / NT), hi = (uint32_t)((uint64_t)N * (t + 1) / NT);
  for (uint32_t i = lo; i < hi; i++) { uint32_t u = order[i], o = noff[i]; for (uint32_t e = off[u]; e < off[u + 1]; e++, o++) cadj[o] = pack(t, o, nid[adj[e] & VM], wt(2, adj, e)); } return NULL; }
static void relabel(void) {
  order = malloc(4 * (size_t)N); nid = malloc(4 * (size_t)N); memset(nid, 0xff, 4 * (size_t)N);
  run(grow);
  uint32_t tail = 0;
  for (int t = 0; t < NT; t++) { for (uint32_t i = 0; i < rn[t]; i++) order[tail + i] = reg[t][i]; tail += rn[t]; free(reg[t]); }
  for (uint32_t i = 0; i < tail; i++) nid[order[i]] = i;
  for (uint32_t next = 0, head = tail; next < N; next++) {   /* what no region reached: small pieces, each in BFS order from its first node */
    if (nid[next] != UINT32_MAX) continue;
    nid[next] = tail; order[tail++] = next;
    for (; head < tail; head++) { uint32_t u = order[head];
      for (uint32_t e = off[u]; e < off[u + 1]; e++) { uint32_t v = adj[e] & VM; if (nid[v] == UINT32_MAX) { nid[v] = tail; order[tail++] = v; } } }
  }
  noff = malloc(4 * ((size_t)N + 1)); noff[0] = 0;
  for (uint32_t i = 0; i < N; i++) noff[i + 1] = noff[i] + off[order[i] + 1] - off[order[i]];
  cadj = malloc(4 * ((size_t)off[N] + 1));
  run(pass3); gather(0);
  free(adj); free(off); free(order); off = noff;
}
static uint32_t *qord;
static int qcmp(const void *a, const void *b) { uint32_t x = qs[*(const uint32_t *)a], y = qs[*(const uint32_t *)b]; return (x > y) - (x < y); }
static void *solve(void *a) {
  (void)a; Side F = {0}, B = {0}; F.bits = B.bits = 12;
  F.m = malloc(8 << 12); B.m = malloc(8 << 12); memset(F.m, 0xff, 8 << 12); memset(B.m, 0xff, 8 << 12); F.tch = malloc(3 << 12); B.tch = malloc(3 << 12);
  for (;;) { uint32_t i = __atomic_fetch_add(&qnext, 1, __ATOMIC_RELAXED); if (i >= Q) break; i = qord[i];
    uint32_t s = qs[i], t = qt[i];
    ans[i] = s == t ? 0 : (off[s] == off[s + 1] || !hasin[t]) ? -1 : bisearch(s, t, &F, &B); }
  free(F.m); free(B.m); free(F.tch); free(B.tch); for (int b = 0; b < 33; b++) { free(F.h.a[b]); free(B.h.a[b]); } return NULL;
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
      uint64_t nd = top.d + wt(0, cadj, e); uint32_t v = cadj[e] & VM; if (nd >= dist[v]) continue;
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
  run(pass1);
  for (uint32_t i = 0; i < N; i++) off[i + 1] += off[i];
  VB = 1; while ((1u << VB) < N) VB++; VM = (1u << VB) - 1; WX = (uint32_t)((1ull << (32 - VB)) - 1);   /* edges packed from the start */
  adj = malloc(4 * ((size_t)off[N] + 1));
  run(pass2); gather(2);   /* leaves off[u] = end of u = start of u+1: shift back */
  memmove(off + 1, off, 4 * (size_t)N); off[0] = 0;
  size_t qsz; snprintf(path, sizeof path, "%s/queries.txt", data); const char *qf = mapfile(path, &qsz);
  Q = 0; for (size_t i = 0; i < qsz; i++) Q += qf[i] == '\n';
  qs = malloc(4 * (size_t)Q + 4); qt = malloc(4 * (size_t)Q + 4); ans = malloc(8 * (size_t)Q + 8);
  p = qf; for (uint32_t i = 0; i < Q; i++) { qs[i] = num(&p); qt[i] = num(&p); }
  relabel();
  roff = calloc((size_t)N + 2, 4); run(rcount);   /* reverse CSR for the backward search */
  for (uint32_t i = 0; i < N; i++) roff[i + 2] += roff[i + 1];
  cradj = malloc(4 * ((size_t)off[N] + 1)); run(rfill); gather(1);
  { uint8_t *h2 = calloc(N, 1); for (uint32_t i = 0; i < N; i++) h2[nid[i]] = hasin[i]; free(hasin); hasin = h2; }
  for (uint32_t i = 0; i < Q; i++) { qs[i] = nid[qs[i]]; qt[i] = nid[qt[i]]; }
  free(nid);
  qord = malloc(4 * (size_t)Q + 4); for (uint32_t i = 0; i < Q; i++) qord[i] = i;
  qsort(qord, Q, 4, qcmp);   /* queries in source order: threads working at the same time search nearby parts of the graph */
  run(solve);
  free(cradj); free(roff); malloc_trim(0);   /* the 64-bit fallback searches forward only: drop the reverse graph and what the threads freed */
  uint64_t *d64 = NULL;
  for (uint32_t i = 0; i < Q; i++) if (ans[i] == -2) { if (!d64) { d64 = malloc(8 * (size_t)N); memset(d64, 0xff, 8 * (size_t)N); } ans[i] = search64(qs[i], qt[i], d64); }
  snprintf(path, sizeof path, "%s/distances.txt", out);
  FILE *f = fopen(path, "wb"); if (!f) { perror("fopen"); return 1; }
  char *buf = malloc((size_t)Q * 24 + 1), *w = buf;
  for (uint32_t i = 0; i < Q; i++) { if (ans[i] < 0) { *w++ = '-'; *w++ = '1'; } else { char b[24]; int n = 0; uint64_t v = (uint64_t)ans[i]; do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v); while (n) *w++ = b[--n]; } *w++ = '\n'; }
  fwrite(buf, 1, (size_t)(w - buf), f); fclose(f);
  return 0;
}
