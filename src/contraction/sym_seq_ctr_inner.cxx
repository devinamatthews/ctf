/*Copyright (c) 2011, Edgar Solomonik, all rights reserved.*/

#include "../shared/iter_tsr.h"
#include <limits.h>
#include "sym_seq_ctr.h"
#include "../shared/offload.h"

int sym_seq_ctr_inr(char const *       alpha,
                    char const *       A,
                    semiring           sr_A,
                    int                order_A,
                    int const *        edge_len_A,
                    int const *        _lda_A,
                    int const *        sym_A,
                    int const *        idx_map_A,
                    char const *       B,
                    semiring           sr_B,
                    int                order_B,
                    int const *        edge_len_B,
                    int const *        _lda_B,
                    int const *        sym_B,
                    int const *        idx_map_B,
                    char const *       beta,
                    char *             C,
                    semiring           sr_C,
                    int                order_C,
                    int const *        edge_len_C,
                    int const *        _lda_C,
                    int const *        sym_C,
                    int const *        idx_map_C,
                    iparam const *     prm){
  TAU_FSTART(sym_seq_ctr_inner);
  int idx, i, idx_max, imin, imax, idx_A, idx_B, idx_C, iA, iB, iC, j, k;
  int off_idx, off_lda, sym_pass, stride_A, stride_B, stride_C;
  int * idx_glb, * rev_idx_map;
  int * dlen_A, * dlen_B, * dlen_C;

  stride_A = prm->m*prm->k;
  stride_B = prm->k*prm->n;
  stride_C = prm->m*prm->n;

  inv_idx(order_A,       idx_map_A,
          order_B,       idx_map_B,
          order_C,       idx_map_C,
          &idx_max,     &rev_idx_map);

  dlen_A = (int*)CTF_alloc(sizeof(int)*order_A);
  dlen_B = (int*)CTF_alloc(sizeof(int)*order_B);
  dlen_C = (int*)CTF_alloc(sizeof(int)*order_C);
  memcpy(dlen_A, edge_len_A, sizeof(int)*order_A);
  memcpy(dlen_B, edge_len_B, sizeof(int)*order_B);
  memcpy(dlen_C, edge_len_C, sizeof(int)*order_C);

  idx_glb = (int*)CTF_alloc(sizeof(int)*idx_max);
  memset(idx_glb, 0, sizeof(int)*idx_max);


  /* Scale C immediately. FIXME: wrong for iterators over subset of C */
#ifndef OFFLOAD
  if (sr_C.isequal(beta, sr_C.mulid)){
    CTF_FLOPS_ADD(prm->sz_C);
/*    for (i=0; i<prm->sz_C; i++){
      C[i] = C[i]*beta;
    }*/
    sr_C.scal(prm->sz_C, beta, C, 1);
  }
#endif
  idx_A = 0, idx_B = 0, idx_C = 0;
  sym_pass = 1;

  for (;;){
    //printf("[%d] <- [%d]*[%d]\n",idx_C, idx_A, idx_B);
    if (sym_pass){
//      C[idx_C] += alpha*A[idx_A]*B[idx_B];
      TAU_FSTART(gemm);
#ifdef OFFLOAD
//      if (prm->m*prm->n*prm->k > 1000){
      offload_gemm<dtype>(prm->tA, prm->tB, prm->m, prm->n, prm->k, alpha, 
                          A+idx_A*stride_A*sr_A.el_size, prm->k,
                          B+idx_B*stride_B*sr_B.el_size, prm->k, sr_C.mulid,
                          C+idx_C*stride_C*sr_C.el_size, prm->m);
#else
      sr_C.gemm(prm->tA, prm->tB, prm->m, prm->n, prm->k, alpha, 
                A+idx_A*stride_A*sr_A.el_size, 
                B+idx_B*stride_B*sr_B.el_size, sr_C.mulid,
                C+idx_C*stride_C*sr_C.el_size);
#endif
      TAU_FSTOP(gemm);
      // count n^2 FLOPS too
      CTF_FLOPS_ADD((2 * (int64_t)prm->n * (int64_t)prm->m * (int64_t)(prm->k+1)));
    }
    //printf("[%lf] <- [%lf]*[%lf]\n",C[idx_C],A[idx_A],B[idx_B]);

    for (idx=0; idx<idx_max; idx++){
      imin = 0, imax = INT_MAX;

      GET_MIN_MAX(A,0,3);
      GET_MIN_MAX(B,1,3);
      GET_MIN_MAX(C,2,3);

      ASSERT(idx_glb[idx] >= imin && idx_glb[idx] < imax);

      idx_glb[idx]++;

      if (idx_glb[idx] >= imax){
        idx_glb[idx] = imin;
      }
      if (idx_glb[idx] != imin) {
        break;
      }
    }
    if (idx == idx_max) break;

    CHECK_SYM(A);
    if (!sym_pass) continue;
    CHECK_SYM(B);
    if (!sym_pass) continue;
    CHECK_SYM(C);
    if (!sym_pass) continue;
    

    if (order_A > 0)
      RESET_IDX(A);
    if (order_B > 0)
      RESET_IDX(B);
    if (order_C > 0)
      RESET_IDX(C);
  }
  CTF_free(dlen_A);
  CTF_free(dlen_B);
  CTF_free(dlen_C);
  CTF_free(idx_glb);
  CTF_free(rev_idx_map);
  TAU_FSTOP(sym_seq_ctr_inner);
  return 0;
}
