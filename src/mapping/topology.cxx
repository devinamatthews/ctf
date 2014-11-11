/*Copyright (c) 2011, Edgar Solomonik, all rights reserved.*/

#include "topology.h"
#include "../shared/util.h"

#ifdef BGQ
#include "mpix.h"
#endif

namespace CTF_int {

  topology::topology(){
    order        = 0;
    lens         = NULL;
    lda          = NULL;
    is_activated = false;
    dim_comm     = NULL;
  }
  
  topology::~topology(){
    CTF_free(lens);
    CTF_free(lda);
    CTF_free(dim_comm);
  }

  topology::topology(topology const & other){
    order = other.order;

    lens     = (int*)CTF_alloc(order*sizeof(int));
    memcpy(lens, other.lens, order*sizeof(int));

    lda      = (int*)CTF_alloc(order*sizeof(int));
    memcpy(lda, other.lda, order*sizeof(int));

    dim_comm = (CommData*)CTF_alloc(order*sizeof(CommData));
    memcpy(dim_comm, other.dim_comm, order*sizeof(CommData));

    is_activated = other.is_activated;
  }

  topology::topology(int         order_,
                     int const * lens_,
                     CommData    cdt,
                     bool        activate){
    glb_comm     = cdt;
    order        = order_;
    lens         = (int*)CTF_alloc(order_*sizeof(int));
    lda          = (int*)CTF_alloc(order_*sizeof(int));
    dim_comm     = (CommData*)CTF_alloc(order_*sizeof(CommData));
    is_activated = false;
    
    int stride = 1, cut = 0;
    int rank = glb_comm.rank;
    for (int i=0; i<order; i++){
      lda[i] = stride;
      dim_comm[i] = CommData(((rank/stride)%lens[order-i-1]),
                             (((rank/(stride*lens[order-i-1]))*stride)+cut),
                             lens[order-i-1]);
//      SETUP_SUB_COMM_SHELL(cdt, dim_comm[i],
      stride*=lens[order-i-1];
      cut = (rank - (rank/stride)*stride);
    }
    this->activate();
  }

  void topology::activate(){
    if (!is_activated){
      for (int i=0; i<order; i++){
        dim_comm[i].activate(glb_comm.cm);
      }
    } 
    is_activated = true;
  }

  topology * get_phys_topo(CommData glb_comm,
                           TOPOLOGY mach){
    int np = glb_comm.np;
    int * dl;
    int * dim_len;
    topology * topo;
    if (mach == NO_TOPOLOGY){
      dl = (int*)CTF_alloc(sizeof(int));
      dl[0] = np;
      topo = new topology(1, dl, glb_comm, 1);
      CTF_free(dl);
      return topo;
    }
    if (mach == TOPOLOGY_GENERIC){
      int order;
      factorize(np, &order, &dim_len);
      topo = new topology(order, dim_len, glb_comm, 1);
      CTF_free(dim_len);
      return topo;
    } else if (mach == TOPOLOGY_BGQ) {
      dl = (int*)CTF_alloc((7)*sizeof(int));
      dim_len = dl;
      #ifdef BGQ
      if (np >= 512){
        int i, dim;
        MPIX_Hardware_t hw;
        MPIX_Hardware(&hw);

        int * topo_dims = (int*)CTF_alloc(7*sizeof(int));
        topo_dims[0] = hw.Size[0];
        topo_dims[1] = hw.Size[1];
        topo_dims[2] = hw.Size[2];
        topo_dims[3] = hw.Size[3];
        topo_dims[4] = hw.Size[4];
        topo_dims[5] = MIN(4, np/(topo_dims[0]*topo_dims[1]*
                                  topo_dims[2]*topo_dims[3]*
                                  topo_dims[4]));
        topo_dims[6] = (np/ (topo_dims[0]*topo_dims[1]*
                            topo_dims[2]*topo_dims[3]*
                            topo_dims[4])) / 4;
        dim = 0;
        for (i=0; i<7; i++){
          if (topo_dims[i] > 1){
            dl[dim] = topo_dims[i];
            dim++;
          }
        }
        topo = new topology(dim, topo_dims, glb_comm, 1);
        CTF_free(topo_dims);
        return topo;
      } else 
      #else
      {
        int order;
        factorize(np, &order, &dim_len);
        topo = new topology(order, dim_len, glb_comm, 1);
        CTF_free(dim_len);
        return topo;
      }
      #endif
    } else if (mach == TOPOLOGY_BGP) {
      int order;
      if (1<<(int)log2(np) != np){
        factorize(np, &order, &dim_len);
        topo = new topology(order, dim_len, glb_comm, 1);
        CTF_free(dim_len);
        return topo;
      }
      if ((int)log2(np) == 0) order = 0;
      else if ((int)log2(np) <= 2) order = 1;
      else if ((int)log2(np) <= 4) order = 2;
      else order = 3;
      dim_len = (int*)CTF_alloc((order)*sizeof(int));
      switch ((int)log2(np)){
        case 0:
          break;
        case 1:
          dim_len[0] = 2;
          break;
        case 2:
          dim_len[0] = 4;
          break;
        case 3:
          dim_len[0] = 4;
          dim_len[1] = 2;
          break;
        case 4:
          dim_len[0] = 4;
          dim_len[1] = 4;
          break;
        case 5:
          dim_len[0] = 4;
          dim_len[1] = 4;
          dim_len[2] = 2;
          break;
        case 6:
          dim_len[0] = 4;
          dim_len[1] = 4;
          dim_len[2] = 4;
          break;
        case 7:
          dim_len[0] = 8;
          dim_len[1] = 4;
          dim_len[2] = 4;
          break;
        case 8:
          dim_len[0] = 8;
          dim_len[1] = 8;
          dim_len[2] = 4;
          break;
        case 9:
          dim_len[0] = 8;
          dim_len[1] = 8;
          dim_len[2] = 8;
          break;
        case 10:
          dim_len[0] = 16;
          dim_len[1] = 8;
          dim_len[2] = 8;
          break;
        case 11:
          dim_len[0] = 32;
          dim_len[1] = 8;
          dim_len[2] = 8;
          break;
        case 12:
          dim_len[0] = 32;
          dim_len[1] = 16;
          dim_len[2] = 8;
          break;
        case 13:
          dim_len[0] = 32;
          dim_len[1] = 32;
          dim_len[2] = 8;
          break;
        case 14:
          dim_len[0] = 32;
          dim_len[1] = 32;
          dim_len[2] = 16;
          break;
        case 15:
          dim_len[0] = 32;
          dim_len[1] = 32;
          dim_len[2] = 32;
          break;
        default:
          factorize(np, &order, &dim_len);
          break;
      }
      topo = new topology(order, dim_len, glb_comm, 1);
      CTF_free(dim_len);
      return topo;
    } else if (mach == TOPOLOGY_8D) {
      int order;
      int * dim_len;
      if (1<<(int)log2(np) != np){
        factorize(np, &order, &dim_len);
        topo = new topology(order, dim_len, glb_comm, 1);
        CTF_free(dim_len);
        return topo;
      }
      order = MIN((int)log2(np),8);
      if (order > 0)
        dim_len = (int*)CTF_alloc((order)*sizeof(int));
      else dim_len = NULL;
      switch ((int)log2(np)){
        case 0:
          break;
        case 1:
          dim_len[0] = 2;
          break;
        case 2:
          dim_len[0] = 2;
          dim_len[1] = 2;
          break;
        case 3:
          dim_len[0] = 2;
          dim_len[1] = 2;
          dim_len[2] = 2;
          break;
        case 4:
          dim_len[0] = 2;
          dim_len[1] = 2;
          dim_len[2] = 2;
          dim_len[3] = 2;
          break;
        case 5:
          dim_len[0] = 2;
          dim_len[1] = 2;
          dim_len[2] = 2;
          dim_len[3] = 2;
          dim_len[4] = 2;
          break;
        case 6:
          dim_len[0] = 2;
          dim_len[1] = 2;
          dim_len[2] = 2;
          dim_len[3] = 2;
          dim_len[4] = 2;
          dim_len[5] = 2;
          break;
        case 7:
          dim_len[0] = 2;
          dim_len[1] = 2;
          dim_len[2] = 2;
          dim_len[3] = 2;
          dim_len[4] = 2;
          dim_len[5] = 2;
          dim_len[6] = 2;
          break;
        case 8:
          dim_len[0] = 2;
          dim_len[1] = 2;
          dim_len[2] = 2;
          dim_len[3] = 2;
          dim_len[4] = 2;
          dim_len[5] = 2;
          dim_len[6] = 2;
          dim_len[7] = 2;
          break;
        case 9:
          dim_len[0] = 4;
          dim_len[1] = 2;
          dim_len[2] = 2;
          dim_len[3] = 2;
          dim_len[4] = 2;
          dim_len[5] = 2;
          dim_len[6] = 2;
          dim_len[7] = 2;
          break;
        case 10:
          dim_len[0] = 4;
          dim_len[1] = 4;
          dim_len[2] = 2;
          dim_len[3] = 2;
          dim_len[4] = 2;
          dim_len[5] = 2;
          dim_len[6] = 2;
          dim_len[7] = 2;
          break;
        case 11:
          dim_len[0] = 4;
          dim_len[1] = 4;
          dim_len[2] = 4;
          dim_len[3] = 2;
          dim_len[4] = 2;
          dim_len[5] = 2;
          dim_len[6] = 2;
          dim_len[7] = 2;
          break;
        case 12:
          dim_len[0] = 4;
          dim_len[1] = 4;
          dim_len[2] = 4;
          dim_len[3] = 4;
          dim_len[4] = 2;
          dim_len[5] = 2;
          dim_len[6] = 2;
          dim_len[7] = 2;
          break;
        case 13:
          dim_len[0] = 4;
          dim_len[1] = 4;
          dim_len[2] = 4;
          dim_len[3] = 4;
          dim_len[4] = 4;
          dim_len[5] = 2;
          dim_len[6] = 2;
          dim_len[7] = 2;
          break;
        case 14:
          dim_len[0] = 4;
          dim_len[1] = 4;
          dim_len[2] = 4;
          dim_len[3] = 4;
          dim_len[4] = 4;
          dim_len[5] = 4;
          dim_len[6] = 2;
          dim_len[7] = 2;
          break;
        case 15:
          dim_len[0] = 4;
          dim_len[1] = 4;
          dim_len[2] = 4;
          dim_len[3] = 4;
          dim_len[4] = 4;
          dim_len[5] = 4;
          dim_len[6] = 4;
          dim_len[7] = 2;
          break;
        default:
          factorize(np, &order, &dim_len);
          break;

      }
      topo = new topology(order, dim_len, glb_comm, 1);
      CTF_free(dim_len);
      return topo;
    } else {
      int order;
      dim_len = (int*)CTF_alloc((log2(np)+1)*sizeof(int));
      factorize(np, &order, &dim_len);
      topo = new topology(order, dim_len, glb_comm, 1);
      CTF_free(dim_len);
      return topo;
    }
  }

  std::vector<topology> peel_torus(topology const & topo,
                                   CommData         glb_comm){
    std::vector<topology> topos;
    topos.push_back(topo);
    
    if (topo.order <= 1) return topos;
    
    int * new_lens = (int*)malloc(sizeof(int)*topo.order-1);

    for (int i=0; i<topo.order-1; i++){
      for (int j=0; j<i; j++){
        new_lens[j] = topo.lens[j];
      }
      new_lens[i] = topo.lens[i]+topo.lens[i+1];
      for (int j=i+2; j<topo.order; j++){
        new_lens[j-1] = topo.lens[j];
      }
    }
    topology new_topo = topology(topo.order-1, new_lens, glb_comm);
    topos.push_back(new_topo);
    for (int i=0; i<(int)topos.size(); i++){
      std::vector<topology> more_topos = peel_torus(topos[i], glb_comm);
      for (int j=0; j<(int)more_topos.size(); j++){
        if (find_topology(more_topos[j], topos) == -1)
          topos.push_back(more_topos[j]);
      }
    }
    return topos;
  }
    
  int find_topology(topology const &              topo,
                    std::vector<topology> &       topovec){
    int i, j, found;
    std::vector<topology>::iterator iter;
    
    found = -1;
    for (j=0, iter=topovec.begin(); iter<topovec.end(); iter++, j++){
      if (iter->order == topo.order){
        found = j;
        for (i=0; i<iter->order; i++) {
          if (iter->dim_comm[i].np != topo.dim_comm[i].np){
            found = -1;
          }
        }
      }
      if (found != -1) return found;
    }
    return -1;  
  }

  int get_best_topo(int64_t  nvirt,
                    int      topo,
                    CommData global_comm,
                    int64_t  bcomm_vol,
                    int64_t  bmemuse){
      int64_t gnvirt, nv, gcomm_vol, gmemuse, bv;
      int btopo, gtopo;
      nv = nvirt;
      MPI_Allreduce(&nv, &gnvirt, 1, MPI_INT64_T, MPI_MIN, global_comm.cm);
      ASSERT(gnvirt <= nvirt);

      nv = bcomm_vol;
      bv = bmemuse;
      if (nvirt == gnvirt){
        btopo = topo;
      } else {
        btopo = INT_MAX;
        nv    = INT64_MAX;
        bv    = INT64_MAX;
      }
      MPI_Allreduce(&nv, &gcomm_vol, 1, MPI_INT64_T, MPI_MIN, global_comm.cm);
      if (bcomm_vol != gcomm_vol){
        btopo = INT_MAX;
        bv    = INT64_MAX;
      }
      MPI_Allreduce(&bv, &gmemuse, 1, MPI_INT64_T, MPI_MIN, global_comm.cm);
      if (bmemuse != gmemuse){
        btopo = INT_MAX;
      }
      MPI_Allreduce(&btopo, &gtopo, 1, MPI_INT, MPI_MIN, global_comm.cm);
      /*printf("nvirt = " PRIu64 " bcomm_vol = " PRIu64 " bmemuse = " PRIu64 " topo = %d\n",
        nvirt, bcomm_vol, bmemuse, topo);
      printf("gnvirt = " PRIu64 " gcomm_vol = " PRIu64 " gmemuse = " PRIu64 " bv = " PRIu64 " nv = " PRIu64 " gtopo = %d\n",
        gnvirt, gcomm_vol, gmemuse, bv, nv, gtopo);*/

      return gtopo;
  }
}