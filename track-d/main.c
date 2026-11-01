// Speed Demon 2026 Track D (Traverse) - C solution.
// Built by Tab, an AI assistant, for team lead Akshay Wagh.
// Parallel parse -> CSR adjacency -> one early-stopping Dijkstra per query,
// queries spread over all hardware threads, each with its own dist array
// that is reset through a touched-list (no O(n) clears).
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
static uint32_t *eu, *ev, *ew; /* edge list */
static const char *G; static size_t GS;
static int NT;

static inline uint32_t num(const char **pp) { const char *p = *pp; uint32_t v = 0; while (*p >= '0') v = v * 10 + (uint32_t)(*p++ - '0'); *pp = p + 1; return v; }

typedef struct { size_t beg, end; uint32_t first, cnt; } Chunk;
static Chunk ch[NT_MAX];

static void *count_lines(void *a) { Chunk *c = a; uint32_t k = 0; const char *p = G + c->beg, *e = G + c->end;
  while (p < e) { const char *q = memchr(p, '\n', (size_t)(e - p)); if (!q) break; k++; p = q + 1; } c->cnt = k; return NULL; }
static void *parse_edges(void *a) { Chunk *c = a; const char *p = G + c->beg; uint32_t i = c->first, end = c->first + c->cnt;
  for (; i < end; i++) { uint32_t u = num(&p), v = num(&p), w = num(&p); eu[i] = u; ev[i] = v; ew[i] = w; __atomic_fetch_add(&off[u + 1], 1, __ATOMIC_RELAXED); }
  return NULL; }
static uint32_t *pos;
static void *fill_csr(void *a) { Chunk *c = a; for (uint32_t i = c->first; i < c->first + c->cnt; i++) {
    uint32_t p = __atomic_fetch_add(&pos[eu[i]], 1, __ATOMIC_RELAXED); adjv[p] = ev[i]; adjw[p] = ew[i]; } return NULL; }

static uint32_t Q; static uint32_t *qs, *qt; static int64_t *ans;
static volatile uint32_t qnext;

typedef struct { uint64_t d; uint32_t v; } HE;
static void *solve(void *arg) {
  (void)arg;
  uint64_t *dist = malloc(sizeof(uint64_t) * N); memset(dist, 0xff, sizeof(uint64_t) * N);
  uint8_t *done = calloc(N, 1);
  uint32_t *touched = malloc(sizeof(uint32_t) * N); size_t nt = 0;
  size_t hcap = 1 << 16; HE *h = malloc(sizeof(HE) * hcap);
  for (;;) {
    uint32_t qi = __atomic_fetch_add(&qnext, 1, __ATOMIC_RELAXED); if (qi >= Q) break;
    uint32_t s = qs[qi], t = qt[qi];
    if (s == t) { ans[qi] = 0; continue; }
    int64_t res = -1; size_t hn = 0;
    dist[s] = 0; touched[nt++] = s; h[hn++] = (HE){0, s};
    while (hn) {
      HE top = h[0]; HE last = h[--hn];
      if (hn) { size_t i = 0; for (;;) { size_t l = 2 * i + 1; if (l >= hn) break; size_t r = l + 1, m = (r < hn && h[r].d < h[l].d) ? r : l; if (h[m].d >= last.d) break; h[i] = h[m]; i = m; } h[i] = last; }
      uint32_t u = top.v; if (done[u]) continue; if (top.d > dist[u]) continue;
      done[u] = 1;
      if (u == t) { res = (int64_t)top.d; break; }
      uint64_t du = top.d;
      for (uint32_t e = off[u]; e < off[u + 1]; e++) {
        uint32_t v = adjv[e]; uint64_t nd = du + adjw[e];
        if (nd < dist[v]) {
          if (dist[v] == UINT64_MAX) touched[nt++] = v;
          dist[v] = nd;
          if (hn == hcap) { hcap *= 2; h = realloc(h, sizeof(HE) * hcap); }
          size_t i = hn++; while (i) { size_t p = (i - 1) / 2; if (h[p].d <= nd) break; h[i] = h[p]; i = p; } h[i] = (HE){nd, v};
        }
      }
    }
    ans[qi] = res;
    for (size_t i = 0; i < nt; i++) { dist[touched[i]] = UINT64_MAX; done[touched[i]] = 0; } nt = 0;
  }
  free(dist); free(done); free(touched); free(h); return NULL;
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
  off = calloc((size_t)N + 2, 4); eu = malloc(4 * (size_t)M + 4); ev = malloc(4 * (size_t)M + 4); ew = malloc(4 * (size_t)M + 4);
  for (int i = 0; i < NT; i++) pthread_create(&th[i], NULL, parse_edges, &ch[i]);
  for (int i = 0; i < NT; i++) pthread_join(th[i], NULL);
  for (uint32_t i = 0; i < N; i++) off[i + 1] += off[i];
  pos = malloc(4 * ((size_t)N + 1)); memcpy(pos, off, 4 * ((size_t)N + 1));
  adjv = malloc(4 * (size_t)M + 4); adjw = malloc(4 * (size_t)M + 4);
  for (int i = 0; i < NT; i++) pthread_create(&th[i], NULL, fill_csr, &ch[i]);
  for (int i = 0; i < NT; i++) pthread_join(th[i], NULL);
  free(eu); free(ev); free(ew); free(pos); munmap((void *)G, GS);
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
