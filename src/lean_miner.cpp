// Cuckoo Cycle, a memory-hard proof-of-work
// Copyright (c) 2013-2016 John Tromp

#include "lean_miner.hpp"
#include <unistd.h>

#define MAXSOLS 8
// arbitrary length of header hashed into siphash key
#define HEADERLEN 80


void lean_miner(
  unsigned nthreads, unsigned ntrims,
  unsigned nonce, unsigned range,
  char *header_in, size_t header_len,
  unsigned *proofs_out, unsigned *nonces_out, unsigned *nsols_out,
  bool debug) {
  char header[HEADERLEN];
  memset(header, 0, sizeof(header));
  assert(header_len <= sizeof(header));
  memcpy(header, header_in, header_len);

  if (debug) {
    printf("Looking for %d-cycle on cuckoo%d(\"%s\",%d", PROOFSIZE, EDGEBITS+1, header, nonce);
    if (range > 1)
      printf("-%d", nonce+range-1);
    printf(") with 50%% edges, %d trims, %d threads\n", ntrims, nthreads);
  }

  u64 edgeBytes = NEDGES/8, nodeBytes = TWICE_ATOMS*sizeof(atwice);
  int edgeUnit, nodeUnit;
  for (edgeUnit=0; edgeBytes >= 1024; edgeBytes>>=10,edgeUnit++) ;
  for (nodeUnit=0; nodeBytes >= 1024; nodeBytes>>=10,nodeUnit++) ;
  if (debug) {
    printf("Using %d%cB edge and %d%cB node memory, %d-way siphash, and %d-byte counters\n",
       (int)edgeBytes, " KMGT"[edgeUnit], (int)nodeBytes, " KMGT"[nodeUnit], NSIPHASH, SIZEOF_TWICE_ATOM);
  }

  thread_ctx *threads = (thread_ctx *)calloc(nthreads, sizeof(thread_ctx));
  assert(threads);
  cuckoo_ctx ctx(nthreads, ntrims, MAXSOLS, debug);

  u32 sumnsols = 0;
  bool done = false;
  for (unsigned r = 0; r < range && !done; r++) {
    ctx.setheadernonce(header, sizeof(header), nonce + r);
    for (unsigned t = 0; t < nthreads; t++) {
      threads[t].id = t;
      threads[t].ctx = &ctx;
      int err = pthread_create(&threads[t].thread, NULL, worker, (void *)&threads[t]);
      assert(err == 0);
    }
    for (unsigned t = 0; t < nthreads; t++) {
      int err = pthread_join(threads[t].thread, NULL);
      assert(err == 0);
    }
    for (unsigned s = 0; s < ctx.nsols; s++) {
      if (proofs_out && nonces_out) {
        for (unsigned i = 0; i < PROOFSIZE; i++) {
          proofs_out[(s * PROOFSIZE) + i] = ctx.sols[s][i];
        }
        nonces_out[s] = ctx.nonce;
        done = true;
      }
      if (debug) {
        printf("Solution");
        for (unsigned i = 0; i < PROOFSIZE; i++)
          printf(" %jx", (uintmax_t)ctx.sols[s][i]);
        printf("\n");
      }
    }
    sumnsols += ctx.nsols;
  }
  free(threads);
  if (nsols_out) {
    *nsols_out = sumnsols;
  }
  if (debug) {
    printf("%d total solutions\n", sumnsols);
  }
}

int main(int argc, char **argv) {
  unsigned nthreads = 1;
  unsigned ntrims   = 1 + (PART_BITS+3)*(PART_BITS+4)/2;
  unsigned nonce = 0;
  unsigned range = 1;
  char header[HEADERLEN];
  unsigned len = 0;
  int c;

  memset(header, 0, sizeof(header));
  while ((c = getopt (argc, argv, "h:m:n:r:t:")) != -1) {
    switch (c) {
      case 'h':
        len = strlen(optarg);
        assert(len <= sizeof(header));
        memcpy(header, optarg, len);
        break;
      case 'n':
        nonce = atoi(optarg);
        break;
      case 'r':
        range = atoi(optarg);
        break;
      case 'm':
        ntrims = atoi(optarg);
        break;
      case 't':
        nthreads = atoi(optarg);
        break;
    }
  }
  lean_miner(
    nthreads, ntrims,
    nonce, range,
    header, len,
    NULL, NULL, NULL,
    true);

  return 0;
}
