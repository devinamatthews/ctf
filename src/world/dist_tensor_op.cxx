/*Copyright (c) 2011, Edgar Solomonik, all rights reserved.*/

#include "sym_indices.hxx"

/**
 * \brief Scale each tensor element by alpha
 * \param[in] alpha scaling factor
 * \param[in] tid handle to tensor
 */
template<typename dtype>
int dist_tensor<dtype>::scale_tsr(dtype const alpha, int const tid){
  if (global_comm.rank == 0)
    printf("FAILURE: scale_tsr currently only supported for tensors of type double\n");
  return CTF_ERROR;
}

template<> 
int dist_tensor<double>::scale_tsr(double const alpha, int const tid){
  int i;
  tensor<double> * tsr;

  tsr = tensors[tid];
  if (tsr->has_zero_edge_len){
    return CTF_SUCCESS;
  }

  if (tsr->is_mapped){
    cdscal(tsr->size, alpha, tsr->data, 1);
  } else {
    for (i=0; i<tsr->size; i++){
      tsr->pairs[i].d = tsr->pairs[i].d*alpha;
    }
  }

  return CTF_SUCCESS;
}

/**
 * \brief  Compute dot product of two tensors. The tensors
    must have the same mapping.
 * \param[in] tid_A handle to tensor A
 * \param[in] tid_B handle to tensor B
 * \param[out] product dot product A dot B
 */
template<typename dtype>
int dist_tensor<dtype>::dot_loc_tsr(int const tid_A, int const tid_B, dtype *product){
  if (global_comm.rank == 0)
    printf("FAILURE: dot_loc_tsr currently only supported for tensors of type double\n");
  return CTF_ERROR;
}

template<> 
int dist_tensor<double>::dot_loc_tsr(int const tid_A, int const tid_B, double *product){
  double dprod;
  tensor<double> * tsr_A, * tsr_B;

  tsr_A = tensors[tid_A];
  tsr_B = tensors[tid_B];
  if (tsr_A->has_zero_edge_len || tsr_B->has_zero_edge_len){
    *product = 0.0;
    return CTF_SUCCESS;
  }

  ASSERT(tsr_A->is_mapped && tsr_B->is_mapped);
  ASSERT(tsr_A->size == tsr_B->size);

  dprod = cddot(tsr_A->size, tsr_A->data, 1, tsr_B->data, 1);

  /* FIXME: Wont work for single precision */
  ALLREDUCE(&dprod, product, 1, COMM_DOUBLE_T, COMM_OP_SUM, global_comm);

  return CTF_SUCCESS;
}

/* Perform an elementwise reduction on a tensor. All processors
   end up with the final answer. */
template<typename dtype>
int dist_tensor<dtype>::red_tsr(int const tid, CTF_OP op, dtype * result){
  if (global_comm.rank == 0)
    printf("FAILURE: reductions currently only supported for tensors of type double\n");
  return CTF_ERROR;
}

void sum_abs(double const alpha, double const a, double & b){
  b += alpha*fabs(a);
}

/* Perform an elementwise reduction on a tensor. All processors
   end up with the final answer. */
template<> 
int dist_tensor<double>::red_tsr(int const tid, CTF_OP op, double * result){
  int64_t i;
  double acc;
  tensor<double> * tsr;
  mapping * map;
  int is_AS;
  int idx_lyr = 0;
  int tid_scal, is_asym;
  int * idx_map;
  fseq_tsr_sum<double> fs;
  fseq_elm_sum<double> felm;
  fseq_tsr_ctr<double> fcs;
  fseq_elm_ctr<double> fcelm;


  tsr = tensors[tid];
  if (tsr->has_zero_edge_len){
    *result = 0.0;
    return CTF_SUCCESS;
  }
  unmap_inner(tsr);
  set_padding(tsr);

  if (tsr->is_mapped){
    idx_lyr = global_comm.rank;
    for (i=0; i<tsr->order; i++){
      map = &tsr->edge_map[i];
      if (map->type == PHYSICAL_MAP){
        idx_lyr -= topovec[tsr->itopo].dim_comm[map->cdt].rank
        *topovec[tsr->itopo].lda[map->cdt];
      }
      while (map->has_child){
        map = map->child;
        if (map->type == PHYSICAL_MAP){
          idx_lyr -= topovec[tsr->itopo].dim_comm[map->cdt].rank
               *topovec[tsr->itopo].lda[map->cdt];
        }
      }
    }
  }
  is_asym = 0;
  for (i=0; i<tsr->order; i++){
    if (tsr->sym[i] == AS)
      is_asym = 1;
  }

  switch (op){
    case CTF_OP_SUM:
      if (is_asym) {
        *result = 0.0;
      } else {
        CTF_alloc_ptr(sizeof(int)*tsr->order, (void**)&idx_map);
        for (i=0; i<tsr->order; i++){
          idx_map[i] = i;
        }
        define_tensor(0, NULL, NULL, &tid_scal, 1);
        fs.func_ptr=sym_seq_sum_ref<double>;
        felm.func_ptr = NULL;
        home_sum_tsr(1.0, 0.0, tid, tid_scal, idx_map, NULL, fs, felm);
        if (global_comm.rank == 0)
          *result = tensors[tid_scal]->data[0];
        else
          *result = 0.0;
        POST_BCAST(result, sizeof(double), COMM_CHAR_T, 0, global_comm, 0);
        CTF_free(idx_map);
      }

/*      acc = 0.0;
      if (tsr->is_mapped){
        if (idx_lyr == 0){
          for (i=0; i<tsr->size; i++){
            acc += tsr->data[i];
          }
        }
      } else {
        for (i=0; i<tsr->size; i++){
          acc += tsr->pairs[i].d;
        }
      }
      ALLREDUCE(&acc, result, 1, MPI_DOUBLE, MPI_SUM, global_comm);*/
      break;

    case CTF_OP_NORM1:
    case CTF_OP_SUMABS:
      CTF_alloc_ptr(sizeof(int)*tsr->order, (void**)&idx_map);
      is_AS = 0;
      for (i=0; i<tsr->order; i++){
        idx_map[i] = i;
        if (tsr->sym[i] == AS) is_AS = 1;
      }
      define_tensor(0, NULL, NULL, &tid_scal, 1);
      fs.func_ptr=NULL;
      felm.func_ptr = sum_abs;
      if (is_AS){
        int * sh_sym, * save_sym;
        CTF_alloc_ptr(sizeof(int)*tsr->order, (void**)&sh_sym);
        for (i=0; i<tsr->order; i++){
          if (tsr->sym[i] == AS) sh_sym[i] = SH;
          else sh_sym[i] = tsr->sym[i];
        }
        /** FIXME: This ruins tensor meta data immutability */
        save_sym = tsr->sym;
        tsr->sym = sh_sym;
        home_sum_tsr(1.0, 0.0, tid, tid_scal, idx_map, NULL, fs, felm);
        tsr->sym = save_sym;
        CTF_free(sh_sym);
      } else {
        home_sum_tsr(1.0, 0.0, tid, tid_scal, idx_map, NULL, fs, felm);
      }
      if (global_comm.rank == 0)
        *result = tensors[tid_scal]->data[0];
      else
        *result = 0.0;

      POST_BCAST(result, sizeof(double), COMM_CHAR_T, 0, global_comm, 0);
      CTF_free(idx_map);

/*      acc = 0.0;
      if (tsr->is_mapped){
        if (idx_lyr == 0){
          for (i=0; i<tsr->size; i++){
            acc += fabs(tsr->data[i]);
          }
        }
      } else {
        for (i=0; i<tsr->size; i++){
          acc += fabs(tsr->pairs[i].d);
        }
      }
      ALLREDUCE(&acc, result, 1, MPI_DOUBLE, MPI_SUM, global_comm);*/
      break;

    case CTF_OP_NORM2:
      CTF_alloc_ptr(sizeof(int)*tsr->order, (void**)&idx_map);
      for (i=0; i<tsr->order; i++){
        idx_map[i] = i;
      }
      define_tensor(0, NULL, NULL, &tid_scal, 1);
      CTF_ctr_type_t ctype;
      ctype.tid_A = tid; 
      ctype.tid_B = tid; 
      ctype.tid_C = tid_scal; 
      ctype.idx_map_A = idx_map; 
      ctype.idx_map_B = idx_map; 
      ctype.idx_map_C = NULL; 
      fcs.func_ptr=sym_seq_ctr_ref<double>;
#ifdef OFFLOAD
      fcs.is_offloadable = 0;
#endif
      fcelm.func_ptr = NULL;
      home_contract(&ctype, fcs, fcelm, 1.0, 0.0);
      if (global_comm.rank == 0)
        *result = sqrt(tensors[tid_scal]->data[0]);
      else
        *result = 0.0;

      POST_BCAST(result, sizeof(double), COMM_CHAR_T, 0, global_comm, 0);
      CTF_free(idx_map);

      /*if (tsr->is_mapped){
        if (idx_lyr == 0){
          for (i=0; i<tsr->size; i++){
            acc += tsr->data[i]*tsr->data[i];
          }
        }
      } else {
        for (i=0; i<tsr->size; i++){
          acc += tsr->pairs[i].d*tsr->pairs[i].d;
        }
      }
      ALLREDUCE(&acc, result, 1, MPI_DOUBLE, MPI_SUM, global_comm);*/
      break;

    case CTF_OP_MAX:
      if (is_asym) {
        red_tsr(tid, CTF_OP_MAXABS, result);
      } else {
        acc = -DBL_MAX;
        if (tsr->is_mapped){
          if (idx_lyr == 0){
            acc = tsr->data[0];
            for (i=1; i<tsr->size; i++){
              acc = MAX(acc, tsr->data[i]);
            }
          }
        } else {
          acc = tsr->pairs[0].d;
          for (i=1; i<tsr->size; i++){
            acc = MAX(acc, tsr->pairs[i].d);
          }
        }
        ALLREDUCE(&acc, result, 1, MPI_DOUBLE, MPI_MAX, global_comm);
      }
      break;

    /* FIXME: incorrect when there is padding and the actual MIN is > 0 */
    case CTF_OP_MIN:
      if (is_asym) {
        red_tsr(tid, CTF_OP_MAXABS, result);
        *result = -1.0 * (*result);
      } else {
        acc = DBL_MAX;
        if (tsr->is_mapped){
          if (idx_lyr == 0){
            acc = tsr->data[0];
            for (i=1; i<tsr->size; i++){
              acc = MIN(acc, tsr->data[i]);
            }
          }
        } else {
          acc = tsr->pairs[0].d;
          for (i=1; i<tsr->size; i++){
            acc = MIN(acc, tsr->pairs[i].d);
          }
        }
        ALLREDUCE(&acc, result, 1, MPI_DOUBLE, MPI_MIN, global_comm);
      }
      break;

    case CTF_OP_MAXABS:
    case CTF_OP_NORM_INFTY:
      acc = 0.0;
      if (tsr->is_mapped){
        if (idx_lyr == 0){
          acc = fabs(tsr->data[0]);
          for (i=1; i<tsr->size; i++){
            acc = MAX(fabs(acc), fabs(tsr->data[i]));
          }
        }
      } else {
        acc = fabs(tsr->pairs[0].d);
        for (i=1; i<tsr->size; i++){
          acc = MAX(fabs(acc), fabs(tsr->pairs[i].d));
        }
      }
      ALLREDUCE(&acc, result, 1, MPI_DOUBLE, MPI_MAX, global_comm);
      break;

    case CTF_OP_MINABS:
      acc = DBL_MAX;
      if (tsr->is_mapped){
        if (idx_lyr == 0){
          acc = fabs(tsr->data[0]);
          for (i=1; i<tsr->size; i++){
            acc = MIN(fabs(acc), fabs(tsr->data[i]));
          }
        }
      } else {
        acc = fabs(tsr->pairs[0].d);
        for (i=1; i<tsr->size; i++){
          acc = MIN(fabs(acc), fabs(tsr->pairs[i].d));
        }
      }
      ALLREDUCE(&acc, result, 1, MPI_DOUBLE, MPI_MIN, global_comm);
      break;

    default:
      return CTF_ERROR;
      break;
  }
  return CTF_SUCCESS;
}

/**
 * \brief apply a function to each element to transform tensor
 * \param[in] tid handle to tensor
 * \param[in] map_func map function to apply to each element
 */
template<typename dtype>
int dist_tensor<dtype>::map_tsr(int const tid,
                                dtype (*map_func)(int const order,
                                                  int const * indices,
                                                  dtype const elem)){
  int64_t i, j, np, stat;
  int * idx;
  tensor<dtype> * tsr;
  key k;
  tkv_pair<dtype> * prs;

  tsr = tensors[tid];
  if (tsr->has_zero_edge_len){
    return CTF_SUCCESS;
  }
  unmap_inner(tsr);
  set_padding(tsr);

  CTF_alloc_ptr(tsr->order*sizeof(int), (void**)&idx);

  /* Extract key-value pair representation */
  if (tsr->is_mapped){
    stat = read_local_pairs(tid, &np, &prs);
    if (stat != CTF_SUCCESS) return stat;
  } else {
    np = tsr->size;
    prs = tsr->pairs;
  }
  /* Extract location from key and map */
  for (i=0; i<np; i++){
    k = prs[i].k;
    for (j=0; j<tsr->order; j++){
      idx[j] = k%tsr->edge_len[j];
      k = k/tsr->edge_len[j];
    }
    prs[i].d = map_func(tsr->order, idx, prs[i].d);
  }
  /* Rewrite pairs to packed layout */
  if (tsr->is_mapped){
    stat = write_pairs(tid, np, 1.0, 0.0, prs,'w');
    CTF_free(prs);
    if (stat != CTF_SUCCESS) return stat;
  }
  CTF_free(idx);
  return CTF_SUCCESS;
}

/**
 * \brief daxpy tensors A and B, B = B+alpha*A
 * \param[in] alpha scaling factor
 * \param[in] tid_A handle to tensor A
 * \param[in] tid_B handle to tensor B
 */
template<typename dtype>
int dist_tensor<dtype>::
    daxpy_local_tensor_pair(dtype alpha, const int tid_A, const int tid_B){
  if (global_comm.rank == 0)
    printf("FAILURE: daxpy currently only supported for tensors of type double\n");
  return CTF_ERROR;
}

template<> 
int dist_tensor<double>::
    daxpy_local_tensor_pair(double alpha, const int tid_A, const int tid_B){
  tensor<double> * tsr_A, * tsr_B;
  tsr_A = tensors[tid_A];
  tsr_B = tensors[tid_B];
  if (tsr_A->has_zero_edge_len || tsr_B->has_zero_edge_len){
    return CTF_SUCCESS;
  }
  ASSERT(tsr_A->size == tsr_B->size);
  cdaxpy(tsr_A->size, alpha, tsr_A->data, 1, tsr_B->data, 1);
  return CTF_SUCCESS;
}



/**
 * \brief contracts tensors alpha*A*B+beta*C -> C.
 *  seq_func needed to perform sequential op
 * \param[in] type the contraction type (defines contraction actors)
 * \param[in] ftsr pointer to sequential block contract function
 * \param[in] felm pointer to sequential element-wise contract function
 * \param[in] alpha scaling factor for A*B
 * \param[in] beta scaling factor for C
 * \param[in] is_inner whether the tensors have two levels of blocking
 *                     0->no blocking 1->inner_blocking 2->folding
 * \param[in] inner_params parameters for inner contraction
 * \param[out] nvirt_all total virtualization factor
 * \param[in] is_used whether this ctr pointer will actually be run
 * \return ctr contraction class to run
 */
template<typename dtype>
ctr<dtype> * dist_tensor<dtype>::
    construct_contraction(CTF_ctr_type_t const *      type,
                          fseq_tsr_ctr<dtype> const   ftsr,
                          fseq_elm_ctr<dtype> const   felm,
                          dtype const                 alpha,
                          dtype const                 beta,
                          int const                   is_inner,
                          iparam const *              inner_params,
                          int *                       nvirt_all,
                          int                         is_used){
  int num_tot, i, i_A, i_B, i_C, is_top, j, nphys_dim,  k;
  int64_t nvirt;
  int64_t blk_sz_A, blk_sz_B, blk_sz_C;
  int64_t vrt_sz_A, vrt_sz_B, vrt_sz_C;
  int sA, sB, sC, need_rep;
  int * blk_len_A, * virt_blk_len_A, * blk_len_B;
  int * virt_blk_len_B, * blk_len_C, * virt_blk_len_C;
  int * idx_arr, * virt_dim, * phys_mapped;
  tensor<dtype> * tsr_A, * tsr_B, * tsr_C;
  strp_tsr<dtype> * str_A, * str_B, * str_C;
  mapping * map;
  ctr<dtype> * hctr = NULL;
  ctr<dtype> ** rec_ctr = NULL;

  TAU_FSTART(construct_contraction);

  tsr_A = tensors[type->tid_A];
  tsr_B = tensors[type->tid_B];
  tsr_C = tensors[type->tid_C];

  inv_idx(tsr_A->order, type->idx_map_A, tsr_A->edge_map,
          tsr_B->order, type->idx_map_B, tsr_B->edge_map,
          tsr_C->order, type->idx_map_C, tsr_C->edge_map,
          &num_tot, &idx_arr);

  nphys_dim = topovec[tsr_A->itopo].order;

  CTF_alloc_ptr(sizeof(int)*tsr_A->order, (void**)&virt_blk_len_A);
  CTF_alloc_ptr(sizeof(int)*tsr_B->order, (void**)&virt_blk_len_B);
  CTF_alloc_ptr(sizeof(int)*tsr_C->order, (void**)&virt_blk_len_C);

  CTF_alloc_ptr(sizeof(int)*tsr_A->order, (void**)&blk_len_A);
  CTF_alloc_ptr(sizeof(int)*tsr_B->order, (void**)&blk_len_B);
  CTF_alloc_ptr(sizeof(int)*tsr_C->order, (void**)&blk_len_C);
  CTF_alloc_ptr(sizeof(int)*num_tot, (void**)&virt_dim);
  CTF_alloc_ptr(sizeof(int)*nphys_dim*3, (void**)&phys_mapped);
  memset(phys_mapped, 0, sizeof(int)*nphys_dim*3);


  /* Determine the block dimensions of each local subtensor */
  blk_sz_A = tsr_A->size;
  blk_sz_B = tsr_B->size;
  blk_sz_C = tsr_C->size;
  calc_dim(tsr_A->order, blk_sz_A, tsr_A->edge_len, tsr_A->edge_map,
           &vrt_sz_A, virt_blk_len_A, blk_len_A);
  calc_dim(tsr_B->order, blk_sz_B, tsr_B->edge_len, tsr_B->edge_map,
           &vrt_sz_B, virt_blk_len_B, blk_len_B);
  calc_dim(tsr_C->order, blk_sz_C, tsr_C->edge_len, tsr_C->edge_map,
           &vrt_sz_C, virt_blk_len_C, blk_len_C);

  /* Strip out the relevant part of the tensor if we are contracting over diagonal */
  sA = strip_diag<dtype>( tsr_A->order, num_tot, type->idx_map_A, vrt_sz_A,
                          tsr_A->edge_map, &topovec[tsr_A->itopo],
                          blk_len_A, &blk_sz_A, &str_A);
  sB = strip_diag<dtype>( tsr_B->order, num_tot, type->idx_map_B, vrt_sz_B,
                          tsr_B->edge_map, &topovec[tsr_B->itopo],
                          blk_len_B, &blk_sz_B, &str_B);
  sC = strip_diag<dtype>( tsr_C->order, num_tot, type->idx_map_C, vrt_sz_C,
                          tsr_C->edge_map, &topovec[tsr_C->itopo],
                          blk_len_C, &blk_sz_C, &str_C);

  is_top = 1;
  if (sA || sB || sC){
    if (global_comm.rank == 0)
      DPRINTF(1,"Stripping tensor\n");
    strp_ctr<dtype> * sctr = new strp_ctr<dtype>;
    hctr = sctr;
    hctr->num_lyr = 1;
    hctr->idx_lyr = 0;
    is_top = 0;
    rec_ctr = &sctr->rec_ctr;

    sctr->rec_strp_A = str_A;
    sctr->rec_strp_B = str_B;
    sctr->rec_strp_C = str_C;
    sctr->strip_A = sA;
    sctr->strip_B = sB;
    sctr->strip_C = sC;
  }

  for (i=0; i<tsr_A->order; i++){
    map = &tsr_A->edge_map[i];
    if (map->type == PHYSICAL_MAP){
      phys_mapped[3*map->cdt+0] = 1;
    }
    while (map->has_child) {
      map = map->child;
      if (map->type == PHYSICAL_MAP){
        phys_mapped[3*map->cdt+0] = 1;
      }
    }
  }
  for (i=0; i<tsr_B->order; i++){
    map = &tsr_B->edge_map[i];
    if (map->type == PHYSICAL_MAP){
      phys_mapped[3*map->cdt+1] = 1;
    }
    while (map->has_child) {
      map = map->child;
      if (map->type == PHYSICAL_MAP){
        phys_mapped[3*map->cdt+1] = 1;
      }
    }
  }
  for (i=0; i<tsr_C->order; i++){
    map = &tsr_C->edge_map[i];
    if (map->type == PHYSICAL_MAP){
      phys_mapped[3*map->cdt+2] = 1;
    }
    while (map->has_child) {
      map = map->child;
      if (map->type == PHYSICAL_MAP){
        phys_mapped[3*map->cdt+2] = 1;
      }
    }
  }
  need_rep = 0;
  for (i=0; i<nphys_dim; i++){
    if (phys_mapped[3*i+0] == 0 ||
      phys_mapped[3*i+1] == 0 ||
      phys_mapped[3*i+2] == 0){
      /*ASSERT((phys_mapped[3*i+0] == 0 && phys_mapped[3*i+1] == 0) ||
      (phys_mapped[3*i+0] == 0 && phys_mapped[3*i+2] == 0) ||
      (phys_mapped[3*i+1] == 0 && phys_mapped[3*i+2] == 0));*/
      need_rep = 1;
      break;
    }
  }
  if (need_rep){
    if (global_comm.rank == 0)
      DPRINTF(1,"Replicating tensor\n");

    ctr_replicate<dtype> * rctr = new ctr_replicate<dtype>;
    if (is_top){
      hctr = rctr;
      is_top = 0;
    } else {
      *rec_ctr = rctr;
    }
    rec_ctr = &rctr->rec_ctr;
    hctr->idx_lyr = 0;
    hctr->num_lyr = 1;
    rctr->idx_lyr = 0;
    rctr->num_lyr = 1;
    rctr->ncdt_A = 0;
    rctr->ncdt_B = 0;
    rctr->ncdt_C = 0;
    rctr->size_A = blk_sz_A;
    rctr->size_B = blk_sz_B;
    rctr->size_C = blk_sz_C;
    rctr->cdt_A = NULL;
    rctr->cdt_B = NULL;
    rctr->cdt_C = NULL;
    for (i=0; i<nphys_dim; i++){
      if (phys_mapped[3*i+0] == 0 &&
          phys_mapped[3*i+1] == 0 &&
          phys_mapped[3*i+2] == 0){
/*        printf("ERROR: ALL-TENSOR REPLICATION NO LONGER DONE\n");
        ABORT;
        ASSERT(rctr->num_lyr == 1);
        hctr->idx_lyr = topovec[tsr_A->itopo].dim_comm[i].rank;
        hctr->num_lyr = topovec[tsr_A->itopo].dim_comm[i]->np;
        rctr->idx_lyr = topovec[tsr_A->itopo].dim_comm[i].rank;
        rctr->num_lyr = topovec[tsr_A->itopo].dim_comm[i]->np;*/
      } else {
        if (phys_mapped[3*i+0] == 0){
          rctr->ncdt_A++;
        }
        if (phys_mapped[3*i+1] == 0){
          rctr->ncdt_B++;
        }
        if (phys_mapped[3*i+2] == 0){
          rctr->ncdt_C++;
        }
      }
    }
    if (rctr->ncdt_A > 0)
      CTF_alloc_ptr(sizeof(CommData)*rctr->ncdt_A, (void**)&rctr->cdt_A);
    if (rctr->ncdt_B > 0)
      CTF_alloc_ptr(sizeof(CommData)*rctr->ncdt_B, (void**)&rctr->cdt_B);
    if (rctr->ncdt_C > 0)
      CTF_alloc_ptr(sizeof(CommData)*rctr->ncdt_C, (void**)&rctr->cdt_C);
    rctr->ncdt_A = 0;
    rctr->ncdt_B = 0;
    rctr->ncdt_C = 0;
    for (i=0; i<nphys_dim; i++){
      if (!(phys_mapped[3*i+0] == 0 &&
            phys_mapped[3*i+1] == 0 &&
            phys_mapped[3*i+2] == 0)){
        if (phys_mapped[3*i+0] == 0){
          rctr->cdt_A[rctr->ncdt_A] = topovec[tsr_A->itopo].dim_comm[i];
          if (is_used && rctr->cdt_A[rctr->ncdt_A].alive == 0)
            SHELL_SPLIT(global_comm, rctr->cdt_A[rctr->ncdt_A]);
          rctr->ncdt_A++;
        }
        if (phys_mapped[3*i+1] == 0){
          rctr->cdt_B[rctr->ncdt_B] = topovec[tsr_B->itopo].dim_comm[i];
          if (is_used && rctr->cdt_B[rctr->ncdt_B].alive == 0)
            SHELL_SPLIT(global_comm, rctr->cdt_B[rctr->ncdt_B]);
          rctr->ncdt_B++;
        }
        if (phys_mapped[3*i+2] == 0){
          rctr->cdt_C[rctr->ncdt_C] = topovec[tsr_C->itopo].dim_comm[i];
          if (is_used && rctr->cdt_C[rctr->ncdt_C].alive == 0)
            SHELL_SPLIT(global_comm, rctr->cdt_C[rctr->ncdt_C]);
          rctr->ncdt_C++;
        }
      }
    }
  }

//#ifdef OFFLOAD
  int total_iter = 1;
  int upload_phase_A = 1;
  int upload_phase_B = 1;
  int download_phase_C = 1;
//#endif
  nvirt = 1;

  ctr_2d_general<dtype> * bottom_ctr_gen = NULL;
/*  if (nvirt_all != NULL)
    *nvirt_all = 1;*/
  for (i=0; i<num_tot; i++){
    virt_dim[i] = 1;
    i_A = idx_arr[3*i+0];
    i_B = idx_arr[3*i+1];
    i_C = idx_arr[3*i+2];
    /* If this index belongs to exactly two tensors */
    if ((i_A != -1 && i_B != -1 && i_C == -1) ||
        (i_A != -1 && i_B == -1 && i_C != -1) ||
        (i_A == -1 && i_B != -1 && i_C != -1)) {
      ctr_2d_general<dtype> * ctr_gen = new ctr_2d_general<dtype>;
      ctr_gen->buffer = NULL; //fix learn to use buffer space
#ifdef OFFLOAD
      ctr_gen->alloc_host_buf = false;
#endif
      int is_built = 0;
      if (i_A == -1){
        is_built = ctr_2d_gen_build(is_used,
                                    global_comm,
                                    i,
                                    virt_dim,
                                    ctr_gen->edge_len,
                                    total_iter,
                                    topovec,
                                    tsr_A,
                                    i_A,
                                    ctr_gen->cdt_A,
                                    ctr_gen->ctr_lda_A,
                                    ctr_gen->ctr_sub_lda_A,
                                    ctr_gen->move_A,
                                    blk_len_A,
                                    blk_sz_A,
                                    virt_blk_len_A,
                                    upload_phase_A,
                                    tsr_B,
                                    i_B,
                                    ctr_gen->cdt_B,
                                    ctr_gen->ctr_lda_B,
                                    ctr_gen->ctr_sub_lda_B,
                                    ctr_gen->move_B,
                                    blk_len_B,
                                    blk_sz_B,
                                    virt_blk_len_B,
                                    upload_phase_B,
                                    tsr_C,
                                    i_C,
                                    ctr_gen->cdt_C,
                                    ctr_gen->ctr_lda_C,
                                    ctr_gen->ctr_sub_lda_C,
                                    ctr_gen->move_C,
                                    blk_len_C,
                                    blk_sz_C,
                                    virt_blk_len_C,
                                    download_phase_C);
      }
      if (i_B == -1){
        is_built = ctr_2d_gen_build(is_used,
                                    global_comm,
                                    i,
                                    virt_dim,
                                    ctr_gen->edge_len,
                                    total_iter,
                                    topovec,
                                    tsr_B,
                                    i_B,
                                    ctr_gen->cdt_B,
                                    ctr_gen->ctr_lda_B,
                                    ctr_gen->ctr_sub_lda_B,
                                    ctr_gen->move_B,
                                    blk_len_B,
                                    blk_sz_B,
                                    virt_blk_len_B,
                                    upload_phase_B,
                                    tsr_C,
                                    i_C,
                                    ctr_gen->cdt_C,
                                    ctr_gen->ctr_lda_C,
                                    ctr_gen->ctr_sub_lda_C,
                                    ctr_gen->move_C,
                                    blk_len_C,
                                    blk_sz_C,
                                    virt_blk_len_C,
                                    download_phase_C,
                                    tsr_A,
                                    i_A,
                                    ctr_gen->cdt_A,
                                    ctr_gen->ctr_lda_A,
                                    ctr_gen->ctr_sub_lda_A,
                                    ctr_gen->move_A,
                                    blk_len_A,
                                    blk_sz_A,
                                    virt_blk_len_A,
                                    upload_phase_A);
      }
      if (i_C == -1){
        is_built = ctr_2d_gen_build(is_used,
                                    global_comm,
                                    i,
                                    virt_dim,
                                    ctr_gen->edge_len,
                                    total_iter,
                                    topovec,
                                    tsr_C,
                                    i_C,
                                    ctr_gen->cdt_C,
                                    ctr_gen->ctr_lda_C,
                                    ctr_gen->ctr_sub_lda_C,
                                    ctr_gen->move_C,
                                    blk_len_C,
                                    blk_sz_C,
                                    virt_blk_len_C,
                                    download_phase_C,
                                    tsr_A,
                                    i_A,
                                    ctr_gen->cdt_A,
                                    ctr_gen->ctr_lda_A,
                                    ctr_gen->ctr_sub_lda_A,
                                    ctr_gen->move_A,
                                    blk_len_A,
                                    blk_sz_A,
                                    virt_blk_len_A,
                                    upload_phase_A,
                                    tsr_B,
                                    i_B,
                                    ctr_gen->cdt_B,
                                    ctr_gen->ctr_lda_B,
                                    ctr_gen->ctr_sub_lda_B,
                                    ctr_gen->move_B,
                                    blk_len_B,
                                    blk_sz_B,
                                    virt_blk_len_B,
                                    upload_phase_B);
      }
      if (is_built){
        if (is_top){
          hctr = ctr_gen;
          hctr->idx_lyr = 0;
          hctr->num_lyr = 1;
          is_top = 0;
        } else {
          *rec_ctr = ctr_gen;
        }
        if (bottom_ctr_gen == NULL)
          bottom_ctr_gen = ctr_gen;
        rec_ctr = &ctr_gen->rec_ctr;
      } else {
        ctr_gen->rec_ctr = NULL;
        delete ctr_gen;
      }
    } else {
      if (i_A != -1){
        map = &tsr_A->edge_map[i_A];
        while (map->has_child) map = map->child;
        if (map->type == VIRTUAL_MAP)
          virt_dim[i] = map->np;
      } else if (i_B != -1){
        map = &tsr_B->edge_map[i_B];
        while (map->has_child) map = map->child;
        if (map->type == VIRTUAL_MAP)
          virt_dim[i] = map->np;
      } else if (i_C != -1){
        map = &tsr_C->edge_map[i_C];
        while (map->has_child) map = map->child;
        if (map->type == VIRTUAL_MAP)
          virt_dim[i] = map->np;
      }
    }
    if (sA && i_A != -1){
      nvirt = virt_dim[i]/str_A->strip_dim[i_A];
    } else if (sB && i_B != -1){
      nvirt = virt_dim[i]/str_B->strip_dim[i_B];
    } else if (sC && i_C != -1){
      nvirt = virt_dim[i]/str_C->strip_dim[i_C];
    }
    
    nvirt = nvirt * virt_dim[i];
  }
  if (nvirt_all != NULL)
    *nvirt_all = nvirt;

  ASSERT(blk_sz_A >= vrt_sz_A);
  ASSERT(blk_sz_B >= vrt_sz_B);
  ASSERT(blk_sz_C >= vrt_sz_C);
    
  int * new_sym_A, * new_sym_B, * new_sym_C;
  CTF_alloc_ptr(sizeof(int)*tsr_A->order, (void**)&new_sym_A);
  memcpy(new_sym_A, tsr_A->sym, sizeof(int)*tsr_A->order);
  CTF_alloc_ptr(sizeof(int)*tsr_B->order, (void**)&new_sym_B);
  memcpy(new_sym_B, tsr_B->sym, sizeof(int)*tsr_B->order);
  CTF_alloc_ptr(sizeof(int)*tsr_C->order, (void**)&new_sym_C);
  memcpy(new_sym_C, tsr_C->sym, sizeof(int)*tsr_C->order);

#ifdef OFFLOAD
  if (ftsr.is_offloadable || is_inner > 0){
    if (bottom_ctr_gen != NULL)
      bottom_ctr_gen->alloc_host_buf = true;
    ctr_offload<dtype> * ctroff = new ctr_offload<dtype>;
    if (is_top){
      hctr = ctroff;
      hctr->idx_lyr = 0;
      hctr->num_lyr = 0;
      is_top = 0;
    } else {
      *rec_ctr = ctroff;
    }
    rec_ctr = &ctroff->rec_ctr;

    ctroff->size_A = blk_sz_A;
    ctroff->size_B = blk_sz_B;
    ctroff->size_C = blk_sz_C;
    ctroff->total_iter = total_iter;
    ctroff->upload_phase_A = upload_phase_A;
    ctroff->upload_phase_B = upload_phase_B;
    ctroff->download_phase_C = download_phase_C;
  }
#endif

  /* Multiply over virtual sub-blocks */
  if (nvirt > 1){
#ifdef USE_VIRT_25D
    ctr_virt_25d<dtype> * ctrv = new ctr_virt_25d<dtype>;
#else
    ctr_virt<dtype> * ctrv = new ctr_virt<dtype>;
#endif
    if (is_top) {
      hctr = ctrv;
      hctr->idx_lyr = 0;
      hctr->num_lyr = 1;
      is_top = 0;
    } else {
      *rec_ctr = ctrv;
    }
    rec_ctr = &ctrv->rec_ctr;

    ctrv->num_dim   = num_tot;
    ctrv->virt_dim  = virt_dim;
    ctrv->order_A  = tsr_A->order;
    ctrv->blk_sz_A  = vrt_sz_A;
    ctrv->idx_map_A = type->idx_map_A;
    ctrv->order_B  = tsr_B->order;
    ctrv->blk_sz_B  = vrt_sz_B;
    ctrv->idx_map_B = type->idx_map_B;
    ctrv->order_C  = tsr_C->order;
    ctrv->blk_sz_C  = vrt_sz_C;
    ctrv->idx_map_C = type->idx_map_C;
    ctrv->buffer  = NULL;
  } else
    CTF_free(virt_dim);

  seq_tsr_ctr<dtype> * ctrseq = new seq_tsr_ctr<dtype>;
  if (is_top) {
    hctr = ctrseq;
    hctr->idx_lyr = 0;
    hctr->num_lyr = 1;
    is_top = 0;
  } else {
    *rec_ctr = ctrseq;
  }
  if (!is_inner){
    ctrseq->is_inner  = 0;
    ctrseq->func_ptr  = ftsr;
  } else if (is_inner == 1) {
    ctrseq->is_inner    = 1;
    ctrseq->inner_params  = *inner_params;
    ctrseq->inner_params.sz_C = vrt_sz_C;
    tensor<dtype> * itsr;
    int * iphase;
    itsr = tensors[tsr_A->rec_tid];
    iphase = calc_phase<dtype>(itsr);
    for (i=0; i<tsr_A->order; i++){
      if (virt_blk_len_A[i]%iphase[i] > 0)
        virt_blk_len_A[i] = virt_blk_len_A[i]/iphase[i]+1;
      else
        virt_blk_len_A[i] = virt_blk_len_A[i]/iphase[i];

    }
    CTF_free(iphase);
    itsr = tensors[tsr_B->rec_tid];
    iphase = calc_phase<dtype>(itsr);
    for (i=0; i<tsr_B->order; i++){
      if (virt_blk_len_B[i]%iphase[i] > 0)
        virt_blk_len_B[i] = virt_blk_len_B[i]/iphase[i]+1;
      else
        virt_blk_len_B[i] = virt_blk_len_B[i]/iphase[i];
    }
    CTF_free(iphase);
    itsr = tensors[tsr_C->rec_tid];
    iphase = calc_phase<dtype>(itsr);
    for (i=0; i<tsr_C->order; i++){
      if (virt_blk_len_C[i]%iphase[i] > 0)
        virt_blk_len_C[i] = virt_blk_len_C[i]/iphase[i]+1;
      else
        virt_blk_len_C[i] = virt_blk_len_C[i]/iphase[i];
    }
    CTF_free(iphase);
  } else if (is_inner == 2) {
    if (global_comm.rank == 0){
      DPRINTF(1,"Folded tensor n=%d m=%d k=%d\n", inner_params->n,
        inner_params->m, inner_params->k);
    }

    ctrseq->is_inner    = 1;
    ctrseq->inner_params  = *inner_params;
    ctrseq->inner_params.sz_C = vrt_sz_C;
    tensor<dtype> * itsr;
    itsr = tensors[tsr_A->rec_tid];
    for (i=0; i<itsr->order; i++){
      j = tsr_A->inner_ordering[i];
      for (k=0; k<tsr_A->order; k++){
        if (tsr_A->sym[k] == NS) j--;
        if (j<0) break;
      }
      j = k;
      while (k>0 && tsr_A->sym[k-1] != NS){
        k--;
      }
      for (; k<=j; k++){
/*        printf("inner_ordering[%d]=%d setting dim %d of A, to len %d from len %d\n",
                i, tsr_A->inner_ordering[i], k, 1, virt_blk_len_A[k]);*/
        virt_blk_len_A[k] = 1;
        new_sym_A[k] = NS;
      }
    }
    itsr = tensors[tsr_B->rec_tid];
    for (i=0; i<itsr->order; i++){
      j = tsr_B->inner_ordering[i];
      for (k=0; k<tsr_B->order; k++){
        if (tsr_B->sym[k] == NS) j--;
        if (j<0) break;
      }
      j = k;
      while (k>0 && tsr_B->sym[k-1] != NS){
        k--;
      }
      for (; k<=j; k++){
      /*  printf("inner_ordering[%d]=%d setting dim %d of B, to len %d from len %d\n",
                i, tsr_B->inner_ordering[i], k, 1, virt_blk_len_B[k]);*/
        virt_blk_len_B[k] = 1;
        new_sym_B[k] = NS;
      }
    }
    itsr = tensors[tsr_C->rec_tid];
    for (i=0; i<itsr->order; i++){
      j = tsr_C->inner_ordering[i];
      for (k=0; k<tsr_C->order; k++){
        if (tsr_C->sym[k] == NS) j--;
        if (j<0) break;
      }
      j = k;
      while (k>0 && tsr_C->sym[k-1] != NS){
        k--;
      }
      for (; k<=j; k++){
      /*  printf("inner_ordering[%d]=%d setting dim %d of C, to len %d from len %d\n",
                i, tsr_C->inner_ordering[i], k, 1, virt_blk_len_C[k]);*/
        virt_blk_len_C[k] = 1;
        new_sym_C[k] = NS;
      }
    }
  }
  ctrseq->alpha         = alpha;
  ctrseq->order_A        = tsr_A->order;
  ctrseq->idx_map_A     = type->idx_map_A;
  ctrseq->edge_len_A    = virt_blk_len_A;
  ctrseq->sym_A         = new_sym_A;
  ctrseq->order_B        = tsr_B->order;
  ctrseq->idx_map_B     = type->idx_map_B;
  ctrseq->edge_len_B    = virt_blk_len_B;
  ctrseq->sym_B         = new_sym_B;
  ctrseq->order_C        = tsr_C->order;
  ctrseq->idx_map_C     = type->idx_map_C;
  ctrseq->edge_len_C    = virt_blk_len_C;
  ctrseq->sym_C         = new_sym_C;
  ctrseq->custom_params = felm;
  ctrseq->is_custom     = (felm.func_ptr != NULL);

  hctr->A   = tsr_A->data;
  hctr->B   = tsr_B->data;
  hctr->C   = tsr_C->data;
  hctr->beta  = beta;
/*  if (global_comm.rank == 0){
    int64_t n,m,k;
    dtype old_flops;
    dtype new_flops;
    ggg_sym_nmk(tsr_A->order, tsr_A->edge_len, type->idx_map_A, tsr_A->sym,
    tsr_B->order, tsr_B->edge_len, type->idx_map_B, tsr_B->sym,
    tsr_C->order, &n, &m, &k);
    old_flops = 2.0*(dtype)n*(dtype)m*(dtype)k;
    new_flops = calc_nvirt(tsr_A);
    new_flops *= calc_nvirt(tsr_B);
    new_flops *= calc_nvirt(tsr_C);
    new_flops *= global_comm.np;
    new_flops = sqrt(new_flops);
    new_flops *= global_comm.np;
    ggg_sym_nmk(tsr_A->order, virt_blk_len_A, type->idx_map_A, tsr_A->sym,
    tsr_B->order, virt_blk_len_B, type->idx_map_B, tsr_B->sym,
    tsr_C->order, &n, &m, &k);
    printf("Each subcontraction is a " PRId64 " by " PRId64 " by " PRId64 " DGEMM performing %E flops\n",n,m,k,
      2.0*(dtype)n*(dtype)m*(dtype)k);
    new_flops *= 2.0*(dtype)n*(dtype)m*(dtype)k;
    printf("Contraction performing %E flops rather than %E, a factor of %lf more flops due to padding\n",
      new_flops, old_flops, new_flops/old_flops);

  }*/

  CTF_free(idx_arr);
  CTF_free(blk_len_A);
  CTF_free(blk_len_B);
  CTF_free(blk_len_C);
  CTF_free(phys_mapped);
  TAU_FSTOP(construct_contraction);
  return hctr;
}

/**
 * \brief contracts tensors alpha*A*B+beta*C -> C.
        Accepts custom-sized buffer-space (set to NULL for dynamic allocs).
 *      seq_func used to perform sequential op
 * \param[in] type the contraction type (defines contraction actors)
 * \param[in] ftsr pointer to sequential block contract function
 * \param[in] felm pointer to sequential element-wise contract function
 * \param[in] alpha scaling factor for A*B
 * \param[in] beta scaling factor for C
 */
template<typename dtype>
int dist_tensor<dtype>::
     home_contract(CTF_ctr_type_t const *    stype,
                   fseq_tsr_ctr<dtype> const ftsr,
                   fseq_elm_ctr<dtype> const felm,
                   dtype const               alpha,
                   dtype const               beta){
#ifndef HOME_CONTRACT
  return sym_contract(stype, ftsr, felm, alpha, beta);
#else
  int ret;
  int was_home_A, was_home_B, was_home_C;
  int was_cyclic_C;
  int64_t old_size_C;
  int * old_phase_C, * old_rank_C, * old_virt_dim_C, * old_pe_lda_C;
  int * old_padding_C, * old_edge_len_C;
  tensor<dtype> * tsr_A, * tsr_B, * tsr_C;
  tensor<dtype> * ntsr_A, * ntsr_B, * ntsr_C;
  tsr_A = tensors[stype->tid_A];
  tsr_B = tensors[stype->tid_B];
  tsr_C = tensors[stype->tid_C];
  unmap_inner(tsr_A);
  unmap_inner(tsr_B);
  unmap_inner(tsr_C);
  
  if (tsr_A->has_zero_edge_len || 
      tsr_B->has_zero_edge_len || 
      tsr_C->has_zero_edge_len){
    if (beta != 1.0 && !tsr_C->has_zero_edge_len){ 
      int * new_idx_map_C; 
      int num_diag = 0;
      new_idx_map_C = (int*)CTF_alloc(sizeof(int)*tsr_C->order);
      for (int i=0; i<tsr_C->order; i++){
        new_idx_map_C[i]=i-num_diag;
        for (int j=0; j<i; j++){
          if (stype->idx_map_C[i] == stype->idx_map_C[j]){
            new_idx_map_C[i]=new_idx_map_C[j];
            num_diag++;
            break;
          }
        }
      }
      fseq_tsr_scl<dtype> fs;
      fs.func_ptr=sym_seq_scl_ref<dtype>;
      fseq_elm_scl<dtype> felm;
      felm.func_ptr = NULL;
      scale_tsr(beta, stype->tid_C, new_idx_map_C, fs, felm); 
      CTF_free(new_idx_map_C);
    }
    return CTF_SUCCESS;
  }

  contract_mst();

  //if (stype->tid_A == stype->tid_B || stype->tid_A == stype->tid_C){
  /*if (stype->tid_A == stype->tid_C){
    clone_tensor(stype->tid_A, 1, &new_tid);
    CTF_ctr_type_t new_type = *stype;
    new_type.tid_A = new_tid;
    ret = home_contract(&new_type, ftsr, felm, alpha, beta);
    del_tsr(new_tid);
    return ret;
  } else if (stype->tid_B == stype->tid_C){
    clone_tensor(stype->tid_B, 1, &new_tid);
    CTF_ctr_type_t new_type = *stype;
    new_type.tid_B = new_tid;
    ret = home_contract(&new_type, ftsr, felm, alpha, beta);
    del_tsr(new_tid);
    return ret;
  }*/ 

  CTF_ctr_type_t ntype = *stype;

  was_home_A = tsr_A->is_home;
  was_home_B = tsr_B->is_home;
  was_home_C = tsr_C->is_home;

  if (was_home_A){
    clone_tensor(stype->tid_A, 0, &ntype.tid_A, 0);
    ntsr_A = tensors[ntype.tid_A];
    ntsr_A->data = tsr_A->data;
    ntsr_A->home_buffer = tsr_A->home_buffer;
    ntsr_A->is_home = 1;
    ntsr_A->is_mapped = 1;
    ntsr_A->itopo = tsr_A->itopo;
    copy_mapping(tsr_A->order, tsr_A->edge_map, ntsr_A->edge_map);
    set_padding(ntsr_A);
  }     
  if (was_home_B){
    if (stype->tid_A == stype->tid_B){
      ntype.tid_B = ntype.tid_A;
      ntsr_B = tensors[ntype.tid_B];
    } else {
      clone_tensor(stype->tid_B, 0, &ntype.tid_B, 0);
      ntsr_B = tensors[ntype.tid_B];
      ntsr_B->data = tsr_B->data;
      ntsr_B->home_buffer = tsr_B->home_buffer;
      ntsr_B->is_home = 1;
      ntsr_B->is_mapped = 1;
      ntsr_B->itopo = tsr_B->itopo;
      copy_mapping(tsr_B->order, tsr_B->edge_map, ntsr_B->edge_map);
      set_padding(ntsr_B);
    }
  }
  if (was_home_C){
    if (stype->tid_C == stype->tid_A){
      ntype.tid_C = ntype.tid_A;
      ntsr_C = tensors[ntype.tid_C];
    } else if (stype->tid_C == stype->tid_B){
      ntype.tid_C = ntype.tid_B;
      ntsr_C = tensors[ntype.tid_C];
    } else {
      clone_tensor(stype->tid_C, 0, &ntype.tid_C, 0);
      ntsr_C = tensors[ntype.tid_C];
      ntsr_C->data = tsr_C->data;
      ntsr_C->home_buffer = tsr_C->home_buffer;
      ntsr_C->is_home = 1;
      ntsr_C->is_mapped = 1;
      ntsr_C->itopo = tsr_C->itopo;
      copy_mapping(tsr_C->order, tsr_C->edge_map, ntsr_C->edge_map);
      set_padding(ntsr_C);
    }
  }

  ret = sym_contract(&ntype, ftsr, felm, alpha, beta);
  if (ret!= CTF_SUCCESS) return ret;
  if (was_home_A) unmap_inner(ntsr_A);
  if (was_home_B && stype->tid_A != stype->tid_B) unmap_inner(ntsr_B);
  if (was_home_C) unmap_inner(ntsr_C);

  if (was_home_C && !ntsr_C->is_home){
    if (global_comm.rank == 0)
      DPRINTF(2,"Migrating tensor %d back to home\n", stype->tid_C);
    save_mapping(ntsr_C,
                 &old_phase_C, &old_rank_C, 
                 &old_virt_dim_C, &old_pe_lda_C, 
                 &old_size_C,  
                 &was_cyclic_C, &old_padding_C, 
                 &old_edge_len_C, &topovec[ntsr_C->itopo]);
    tsr_C->data = ntsr_C->data;
    tsr_C->is_home = 0;
    TAU_FSTART(redistribute_for_ctr_home);
    remap_tensor(stype->tid_C, tsr_C, &topovec[tsr_C->itopo], old_size_C, 
                 old_phase_C, old_rank_C, old_virt_dim_C, 
                 old_pe_lda_C, was_cyclic_C, 
                 old_padding_C, old_edge_len_C, global_comm);
    TAU_FSTOP(redistribute_for_ctr_home);
    memcpy(tsr_C->home_buffer, tsr_C->data, tsr_C->size*sizeof(dtype));
    CTF_free(tsr_C->data);
    tsr_C->data = tsr_C->home_buffer;
    tsr_C->is_home = 1;
    ntsr_C->is_data_aliased = 1;
    del_tsr(ntype.tid_C);
    CTF_free(old_phase_C);
    CTF_free(old_rank_C);
    CTF_free(old_virt_dim_C);
    CTF_free(old_pe_lda_C);
    CTF_free(old_padding_C);
    CTF_free(old_edge_len_C);
  } else if (was_home_C) {
/*    tsr_C->itopo = ntsr_C->itopo;
    copy_mapping(tsr_C->order, ntsr_C->edge_map, tsr_C->edge_map);
    set_padding(tsr_C);*/
    ASSERT(ntsr_C->data == tsr_C->data);
    ntsr_C->is_data_aliased = 1;
    del_tsr(ntype.tid_C);
  }
  if (ntype.tid_A != ntype.tid_C){
    if (was_home_A && !ntsr_A->is_home){
      ntsr_A->has_home = 0;
      del_tsr(ntype.tid_A);
    } else if (was_home_A) {
      ntsr_A->is_data_aliased = 1;
      del_tsr(ntype.tid_A);
    }
  }
  if (ntype.tid_B != ntype.tid_A &&
      ntype.tid_B != ntype.tid_C){
    if (was_home_B && stype->tid_A != stype->tid_B && !ntsr_B->is_home){
      ntsr_B->has_home = 0;
      del_tsr(ntype.tid_B);
    } else if (was_home_B && stype->tid_A != stype->tid_B) {
      ntsr_B->is_data_aliased = 1;
      del_tsr(ntype.tid_B);
    }
  }
  return CTF_SUCCESS;
#endif
}

/**
 * \brief contracts tensors alpha*A*B+beta*C -> C.
        Accepts custom-sized buffer-space (set to NULL for dynamic allocs).
 *      seq_func used to perform sequential op
 * \param[in] type the contraction type (defines contraction actors)
 * \param[in] ftsr pointer to sequential block contract function
 * \param[in] felm pointer to sequential element-wise contract function
 * \param[in] alpha scaling factor for A*B
 * \param[in] beta scaling factor for C
 */
template<typename dtype>
int dist_tensor<dtype>::
     sym_contract(CTF_ctr_type_t const *    stype,
                  fseq_tsr_ctr<dtype> const ftsr,
                  fseq_elm_ctr<dtype> const felm,
                  dtype const               alpha,
                  dtype const               beta){
  int i;
  //int ** scl_idx_maps_C;
  //dtype * scl_alpha_C;
  int stat, new_tid;
  int * new_idx_map;
  int * map_A, * map_B, * map_C, * dstack_tid_C;
  int ** dstack_map_C;
  int ntid_A, ntid_B, ntid_C, nst_C;
  CTF_ctr_type_t unfold_type, ntype = *stype;
  CTF_ctr_type_t * type = &ntype;
  std::vector<CTF_ctr_type_t> perm_types;
  std::vector<dtype> signs;
  dtype dbeta;
  ctr<dtype> * ctrf;
  check_contraction(stype);
  unmap_inner(tensors[stype->tid_A]);
  unmap_inner(tensors[stype->tid_B]);
  unmap_inner(tensors[stype->tid_C]);
  if (tensors[stype->tid_A]->has_zero_edge_len || tensors[stype->tid_B]->has_zero_edge_len
      || tensors[stype->tid_C]->has_zero_edge_len){
    tensor<dtype>* tsr_C = tensors[stype->tid_C];
    if (beta != 1.0 && !tsr_C->has_zero_edge_len){ 
      int * new_idx_map_C; 
      int num_diag = 0;
      new_idx_map_C = (int*)CTF_alloc(sizeof(int)*tsr_C->order);
      for (int i=0; i<tsr_C->order; i++){
        new_idx_map_C[i]=i-num_diag;
        for (int j=0; j<i; j++){
          if (stype->idx_map_C[i] == stype->idx_map_C[j]){
            new_idx_map_C[i]=j-num_diag;
            num_diag++;
            break;
          }
        }
      }
      fseq_tsr_scl<dtype> fs;
      fs.func_ptr=sym_seq_scl_ref<dtype>;
      fseq_elm_scl<dtype> felm;
      felm.func_ptr = NULL;
      scale_tsr(beta, stype->tid_C, new_idx_map_C, fs, felm); 
      CTF_free(new_idx_map_C);
    }
    return CTF_SUCCESS;
  }
  ntid_A = type->tid_A;
  ntid_B = type->tid_B;
  ntid_C = type->tid_C;
  CTF_alloc_ptr(sizeof(int)*tensors[ntid_A]->order,   (void**)&map_A);
  CTF_alloc_ptr(sizeof(int)*tensors[ntid_B]->order,   (void**)&map_B);
  CTF_alloc_ptr(sizeof(int)*tensors[ntid_C]->order,   (void**)&map_C);
  CTF_alloc_ptr(sizeof(int*)*tensors[ntid_C]->order,   (void**)&dstack_map_C);
  CTF_alloc_ptr(sizeof(int)*tensors[ntid_C]->order,   (void**)&dstack_tid_C);
  memcpy(map_A, type->idx_map_A, tensors[ntid_A]->order*sizeof(int));
  memcpy(map_B, type->idx_map_B, tensors[ntid_B]->order*sizeof(int));
  memcpy(map_C, type->idx_map_C, tensors[ntid_C]->order*sizeof(int));
  while (extract_diag(ntid_A, map_A, 1, &new_tid, &new_idx_map) == CTF_SUCCESS){
    if (ntid_A != type->tid_A) del_tsr(ntid_A);
    CTF_free(map_A);
    ntid_A = new_tid;
    map_A = new_idx_map;
  }
  while (extract_diag(ntid_B, map_B, 1, &new_tid, &new_idx_map) == CTF_SUCCESS){
    if (ntid_B != type->tid_B) del_tsr(ntid_B);
    CTF_free(map_B);
    ntid_B = new_tid;
    map_B = new_idx_map;
  }
  nst_C = 0;
  while (extract_diag(ntid_C, map_C, 1, &new_tid, &new_idx_map) == CTF_SUCCESS){
    dstack_map_C[nst_C] = map_C;
    dstack_tid_C[nst_C] = ntid_C;
    nst_C++;
    ntid_C = new_tid;
    map_C = new_idx_map;
  }
  type->tid_A = ntid_A;
  type->tid_B = ntid_B;
  type->tid_C = ntid_C;
  type->idx_map_A = map_A;
  type->idx_map_B = map_B;
  type->idx_map_C = map_C;

  unmap_inner(tensors[ntid_A]);
  unmap_inner(tensors[ntid_B]);
  unmap_inner(tensors[ntid_C]);
  /*if (ntid_A == ntid_B || ntid_A == ntid_C){*/
  if (ntid_A == ntid_C){
    clone_tensor(ntid_A, 1, &new_tid);
    CTF_ctr_type_t new_type = *type;
    new_type.tid_A = new_tid;
    stat = sym_contract(&new_type, ftsr, felm, alpha, beta);
    del_tsr(new_tid);
    ASSERT(stat == CTF_SUCCESS);
  } else if (ntid_B == ntid_C){
    clone_tensor(ntid_B, 1, &new_tid);
    CTF_ctr_type_t new_type = *type;
    new_type.tid_B = new_tid;
    stat = sym_contract(&new_type, ftsr, felm, alpha, beta);
    del_tsr(new_tid);
    ASSERT(stat == CTF_SUCCESS);
  } else {

    double alignfact = align_symmetric_indices(tensors[ntid_A]->order,
                                              map_A,
                                              tensors[ntid_A]->sym,
                                              tensors[ntid_B]->order,
                                              map_B,
                                              tensors[ntid_B]->sym,
                                              tensors[ntid_C]->order,
                                              map_C,
                                              tensors[ntid_C]->sym);

    /*
     * Apply a factor of n! for each set of n symmetric indices which are contracted over
     */
    double ocfact = overcounting_factor(tensors[ntid_A]->order,
                                       map_A,
                                       tensors[ntid_A]->sym,
                                       tensors[ntid_B]->order,
                                       map_B,
                                       tensors[ntid_B]->sym,
                                       tensors[ntid_C]->order,
                                       map_C,
                                       tensors[ntid_C]->sym);

    //std::cout << alpha << ' ' << alignfact << ' ' << ocfact << std::endl;

    if (unfold_broken_sym(type, NULL) != -1){
      if (global_comm.rank == 0)
        DPRINTF(1,"Contraction index is broken\n");

      unfold_broken_sym(type, &unfold_type);
#if PERFORM_DESYM
      if (map_tensors(&unfold_type, 
                      ftsr, felm, alpha, beta, &ctrf, 0) == CTF_SUCCESS){
#else
      int * sym, dim, sy;
      sy = 0;
      sym = get_sym(ntid_A);
      dim = get_dim(ntid_A);
      for (i=0; i<dim; i++){
        if (sym[i] == SY) sy = 1;
      }
      CTF_free(sym);
      sym = get_sym(ntid_B);
      dim = get_dim(ntid_B);
      for (i=0; i<dim; i++){
        if (sym[i] == SY) sy = 1;
      }
      CTF_free(sym);
      sym = get_sym(ntid_C);
      dim = get_dim(ntid_C);
      for (i=0; i<dim; i++){
        if (sym[i] == SY) sy = 1;
      }
      CTF_free(sym);
      if (sy && map_tensors(&unfold_type,
                            ftsr, felm, alpha, beta, &ctrf, 0) == CTF_SUCCESS){
#endif
        if (ntid_A == ntid_B){
          clone_tensor(ntid_A, 1, &ntid_A);
        }
        desymmetrize(ntid_A, unfold_type.tid_A, 0);
        desymmetrize(ntid_B, unfold_type.tid_B, 0);
        desymmetrize(ntid_C, unfold_type.tid_C, 1);
        if (global_comm.rank == 0)
          DPRINTF(1,"Performing index desymmetrization\n");
        sym_contract(&unfold_type, ftsr, felm,
                     alpha*alignfact, beta);
        symmetrize(ntid_C, unfold_type.tid_C);
        if (ntid_A != unfold_type.tid_A){
          unmap_inner(tensors[unfold_type.tid_A]);
          dealias(ntid_A, unfold_type.tid_A);
          del_tsr(unfold_type.tid_A);
          CTF_free(unfold_type.idx_map_A);
        }
        if (ntid_B != unfold_type.tid_B){
          unmap_inner(tensors[unfold_type.tid_B]);
          dealias(ntid_B, unfold_type.tid_B);
          del_tsr(unfold_type.tid_B);
          CTF_free(unfold_type.idx_map_B);
        }
        if (ntid_C != unfold_type.tid_C){
          unmap_inner(tensors[unfold_type.tid_C]);
          dealias(ntid_C, unfold_type.tid_C);
          del_tsr(unfold_type.tid_C);
          CTF_free(unfold_type.idx_map_C);
        }
      } else {
        get_sym_perms(type, alpha*alignfact*ocfact, 
                      perm_types, signs);
                      //&nscl_C, &scl_maps_C, &scl_alpha_C);
        dbeta = beta;
        for (i=0; i<(int)perm_types.size(); i++){
          contract(&perm_types[i], ftsr, felm,
                    signs[i], dbeta);
          free_type(&perm_types[i]);
          dbeta = 1.0;
      }
      perm_types.clear();
      signs.clear();
      }
    } else {
      contract(type, ftsr, felm, alpha*alignfact*ocfact, beta);
    }
    if (ntid_A != type->tid_A) del_tsr(ntid_A);
    if (ntid_B != type->tid_B) del_tsr(ntid_B);
    for (i=nst_C-1; i>=0; i--){
      extract_diag(dstack_tid_C[i], dstack_map_C[i], 0, &ntid_C, &new_idx_map);
      del_tsr(ntid_C);
      ntid_C = dstack_tid_C[i];
    }
    ASSERT(ntid_C == type->tid_C);
  }

  CTF_free(map_A);
  CTF_free(map_B);
  CTF_free(map_C);
  CTF_free(dstack_map_C);
  CTF_free(dstack_tid_C);

  return CTF_SUCCESS;
}

/**
 * \brief contracts tensors alpha*A*B+beta*C -> C.
        Accepts custom-sized buffer-space (set to NULL for dynamic allocs).
 *      seq_func used to perform sequential op
 * \param[in] type the contraction type (defines contraction actors)
 * \param[in] ftsr pointer to sequential block contract function
 * \param[in] felm pointer to sequential element-wise contract function
 * \param[in] alpha scaling factor for A*B
 * \param[in] beta scaling factor for C
 */
template<typename dtype>
int dist_tensor<dtype>::
     contract(CTF_ctr_type_t const *      type,
              fseq_tsr_ctr<dtype> const   ftsr,
              fseq_elm_ctr<dtype> const   felm,
              dtype const                 alpha,
              dtype const                 beta){
  int stat, new_tid;
  ctr<dtype> * ctrf;

  if (tensors[type->tid_A]->has_zero_edge_len || tensors[type->tid_B]->has_zero_edge_len
      || tensors[type->tid_C]->has_zero_edge_len){
    tensor<dtype> * tsr_C = tensors[type->tid_C];
    if (beta != 1.0 && !tsr_C->has_zero_edge_len){ 
      int * new_idx_map_C; 
      int num_diag = 0;
      new_idx_map_C = (int*)CTF_alloc(sizeof(int)*tsr_C->order);
      for (int i=0; i<tsr_C->order; i++){
        new_idx_map_C[i]=i-num_diag;
        for (int j=0; j<i; j++){
          if (type->idx_map_C[i] == type->idx_map_C[j]){
            new_idx_map_C[i]=j-num_diag;
            num_diag++;
            break;
          }
        }
      }
      fseq_tsr_scl<dtype> fs;
      fs.func_ptr=sym_seq_scl_ref<dtype>;
      fseq_elm_scl<dtype> felm;
      felm.func_ptr = NULL;
      scale_tsr(beta, type->tid_C, new_idx_map_C, fs, felm); 
      CTF_free(new_idx_map_C);
    }
    return CTF_SUCCESS;
  }
  if (type->tid_A == type->tid_B || type->tid_A == type->tid_C){
    clone_tensor(type->tid_A, 1, &new_tid);
    CTF_ctr_type_t new_type = *type;
    new_type.tid_A = new_tid;
    stat = contract(&new_type, ftsr, felm, alpha, beta);
    del_tsr(new_tid);
    return stat;
  }
  if (type->tid_B == type->tid_C){
    clone_tensor(type->tid_B, 1, &new_tid);
    CTF_ctr_type_t new_type = *type;
    new_type.tid_B = new_tid;
    stat = contract(&new_type, ftsr, felm, alpha, beta);
    del_tsr(new_tid);
    return stat;
  }
#if DEBUG >= 1 //|| VERBOSE >= 1)
  if (get_global_comm().rank == 0)
    printf("Contraction permutation:\n");
  print_ctr(type, alpha, beta);
#endif

  TAU_FSTART(contract);
#if VERIFY
  int64_t nsA, nsB;
  int64_t nA, nB, nC, up_nC;
  dtype * sA, * sB, * ans_C;
  dtype * uA, * uB, * uC;
  dtype * up_C, * up_ans_C, * pup_C;
  int order_A, order_B, order_C, i, pass;
  int * edge_len_A, * edge_len_B, * edge_len_C;
  int * sym_A, * sym_B, * sym_C;
  int * sym_tmp;
  stat = allread_tsr(type->tid_A, &nsA, &sA);
  assert(stat == CTF_SUCCESS);

  stat = allread_tsr(type->tid_B, &nsB, &sB);
  assert(stat == CTF_SUCCESS);

  stat = allread_tsr(type->tid_C, &nC, &ans_C);
  assert(stat == CTF_SUCCESS);
#endif
  /* Check if the current tensor mappings can be contracted on */
  fseq_tsr_ctr<dtype> fftsr=ftsr;
  if (ftsr.func_ptr == NULL){
    fftsr.func_ptr = &sym_seq_ctr_ref<dtype>;
#ifdef OFFLOAD
    fftsr.is_offloadable = 0;
#endif
  }
#if REDIST
  stat = map_tensors(type, fftsr, felm, alpha, beta, &ctrf);
  if (stat == CTF_ERROR) {
    printf("Failed to map tensors to physical grid\n");
    return CTF_ERROR;
  }
#else
  if (check_contraction_mapping(type) == 0) {
    /* remap if necessary */
    stat = map_tensors(type, fftsr, felm, alpha, beta, &ctrf);
    if (stat == CTF_ERROR) {
      printf("Failed to map tensors to physical grid\n");
      return CTF_ERROR;
    }
  } else {
    /* Construct the tensor algorithm we would like to use */
#if DEBUG >= 2
    if (get_global_comm().rank == 0)
      printf("Keeping mappings:\n");
    print_map(stdout, type->tid_A);
    print_map(stdout, type->tid_B);
    print_map(stdout, type->tid_C);
#endif
    ctrf = construct_contraction(type, fftsr, felm, alpha, beta);
#ifdef VERBOSE
    if (global_comm.rank == 0){
      uint64_t memuse = ctrf->mem_rec();
      DPRINTF(1,"Contraction does not require redistribution, will use %E bytes per processor out of %E available memory and take an estimated of %lf sec\n",
              (double)memuse,(double)proc_bytes_available(),ctrf->est_time_rec(1));
    }
#endif
  }
#endif
  ASSERT(check_contraction_mapping(type));
#if FOLD_TSR
  if (felm.func_ptr == NULL && 
      ftsr.func_ptr == NULL && //sym_seq_ctr_ref<dtype> && 
      can_fold(type)){
    iparam prm;
    TAU_FSTART(map_fold);
    stat = map_fold(type, &prm);
    TAU_FSTOP(map_fold);
    if (stat == CTF_ERROR){
      return CTF_ERROR;
    }
    if (stat == CTF_SUCCESS){
      delete ctrf;
      ctrf = construct_contraction(type, fftsr, felm, alpha, beta, 2, &prm);
    }
  } 
#endif
#if DEBUG >=2
  if (get_global_comm().rank == 0)
    ctrf->print();
#endif
#if DEBUG >=1
  double dtt = MPI_Wtime();
  if (get_global_comm().rank == 0){
    DPRINTF(1,"[%d] performing contraction\n",
        get_global_comm().rank);
    DPRINTF(1,"%E bytes of buffer space will be needed for this contraction\n",
      (double)ctrf->mem_rec());
    DPRINTF(1,"System memory = %E bytes total, %E bytes used, %E bytes available.\n",
      (double)proc_bytes_total(),
      (double)proc_bytes_used(),
      (double)proc_bytes_available());
  }
#endif
/*  print_map(stdout, type->tid_A);
  print_map(stdout, type->tid_B);
  print_map(stdout, type->tid_C);*/
//  stat = zero_out_padding(type->tid_A);
//  stat = zero_out_padding(type->tid_B);
  TAU_FSTART(ctr_func);
  /* Invoke the contraction algorithm */
  ctrf->run();

  TAU_FSTOP(ctr_func);
#ifndef SEQ
  if (tensors[type->tid_C]->is_cyclic)
    stat = zero_out_padding(type->tid_C);
#endif
  if (get_global_comm().rank == 0){
    DPRINTF(1, "Contraction permutation completed in %lf sec.\n",MPI_Wtime()-dtt);
  }


#if VERIFY
  stat = allread_tsr(type->tid_A, &nA, &uA);
  assert(stat == CTF_SUCCESS);
  stat = get_tsr_info(type->tid_A, &order_A, &edge_len_A, &sym_A);
  assert(stat == CTF_SUCCESS);

  stat = allread_tsr(type->tid_B, &nB, &uB);
  assert(stat == CTF_SUCCESS);
  stat = get_tsr_info(type->tid_B, &order_B, &edge_len_B, &sym_B);
  assert(stat == CTF_SUCCESS);

  if (nsA != nA) { printf("nsA = " PRId64 ", nA = " PRId64 "\n",nsA,nA); ABORT; }
  if (nsB != nB) { printf("nsB = " PRId64 ", nB = " PRId64 "\n",nsB,nB); ABORT; }
  for (i=0; (uint64_t)i<nA; i++){
    if (fabs(uA[i] - sA[i]) > 1.E-6){
      printf("A[i] = %lf, sA[i] = %lf\n", uA[i], sA[i]);
    }
  }
  for (i=0; (uint64_t)i<nB; i++){
    if (fabs(uB[i] - sB[i]) > 1.E-6){
      printf("B[%d] = %lf, sB[%d] = %lf\n", i, uB[i], i, sB[i]);
    }
  }

  stat = allread_tsr(type->tid_C, &nC, &uC);
  assert(stat == CTF_SUCCESS);
  stat = get_tsr_info(type->tid_C, &order_C, &edge_len_C, &sym_C);
  assert(stat == CTF_SUCCESS);
  DEBUG_PRINTF("packed size of C is " PRId64 " (should be " PRId64 ")\n", nC,
    sy_packed_size(order_C, edge_len_C, sym_C));

  pup_C = (dtype*)CTF_alloc(nC*sizeof(dtype));

  cpy_sym_ctr(alpha,
        uA, order_A, edge_len_A, edge_len_A, sym_A, type->idx_map_A,
        uB, order_B, edge_len_B, edge_len_B, sym_B, type->idx_map_B,
        beta,
    ans_C, order_C, edge_len_C, edge_len_C, sym_C, type->idx_map_C);
  assert(stat == CTF_SUCCESS);

#if ( DEBUG>=5)
  for (i=0; i<nC; i++){
//    if (fabs(C[i]-ans_C[i]) > 1.E-6){
      printf("PACKED: C[%d] = %lf, ans_C[%d] = %lf\n",
       i, C[i], i, ans_C[i]);
//     }
  }
#endif

  punpack_tsr(uC, order_C, edge_len_C,
        sym_C, 1, &sym_tmp, &up_C);
  punpack_tsr(ans_C, order_C, edge_len_C,
        sym_C, 1, &sym_tmp, &up_ans_C);
  punpack_tsr(up_ans_C, order_C, edge_len_C,
        sym_C, 0, &sym_tmp, &pup_C);
  for (i=0; (uint64_t)i<nC; i++){
    assert(fabs(pup_C[i] - ans_C[i]) < 1.E-6);
  }
  pass = 1;
  up_nC = 1;
  for (i=0; i<order_C; i++){ up_nC *= edge_len_C[i]; };

  for (i=0; i<(int)up_nC; i++){
    if (fabs((up_C[i]-up_ans_C[i])/up_ans_C[i]) > 1.E-6 &&
  fabs((up_C[i]-up_ans_C[i])) > 1.E-6){
      printf("C[%d] = %lf, ans_C[%d] = %lf\n",
       i, up_C[i], i, up_ans_C[i]);
      pass = 0;
    }
  }
  if (!pass) ABORT;

#endif

  delete ctrf;

  TAU_FSTOP(contract);
  return CTF_SUCCESS;


}
