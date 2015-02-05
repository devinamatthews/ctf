#ifndef __INT_SUMMATION_H__
#define __INT_SUMMATION_H__

#include "assert.h"
#include "sum_tsr.h"

namespace CTF_int {
  class tensor; 
  class topology; 

  /**
   * \brief class for execution distributed summation of tensors
   */
  class summation {
     public:
      /** \brief left operand */
      tensor * A;
      /** \brief output */
      tensor * B;

      /** \brief scaling of A */
      char const * alpha;
      /** \brief scaling of existing B */
      char const * beta;
    
      /** \brief indices of left operand */
      int * idx_A;
      /** \brief indices of output */
      int * idx_B;
      /** \brief whether there is a elementwise custom function */
      bool is_custom;
      /** \brief function to execute on elements */
      univar_function func;

      /** \brief lazy constructor */
      summation(){ idx_A = NULL; idx_B = NULL; };
      
      /** \brief destructor */
      ~summation();

      /** \brief copy constructor \param[in] other object to copy */
      summation(summation const & other);

      /**
       * \brief constructor definining summation with C's mul and add ops
       * \param[in] A left operand tensor
       * \param[in] idx_A indices of left operand
       * \param[in] alpha scaling factor alpha * A[idx_A]; (can be NULL)
       * \param[in] B ouput operand tensor
       * \param[in] idx_B indices of right operand
       * \param[in] beta scaling factor of ouput (can be NULL)
                      C[idx_B] = beta*B[idx_B] + alpha * A[idx_A]
       */
      summation(tensor *     A,
                int const *  idx_A,
                char const * alpha,
                tensor *     B,
                int const *  idx_B,
                char const * beta);
     
      /**
       * \brief constructor definining summation with custom function
       * \param[in] A left operand tensor
       * \param[in] idx_A indices of left operand
       * \param[in] alpha scaling factor alpha * A[idx_A]; (can be NULL)
       * \param[in] B ouput operand tensor
       * \param[in] idx_B indices of right operand
       * \param[in] func custom elementwise function 
                      func(A[idx_A],&B[idx_B])
       * \param[in] beta scaling factor of ouput (can be NULL)
                      C[idx_B] = beta*B[idx_B] + alpha * A[idx_A]
       */
      summation(tensor *        A,
                int const *     idx_A,
                char const *    alpha,
                tensor *        B,
                int const *     idx_B,
                char const *    beta,
                univar_function func);

      /** \brief run summation  */
      void execute();
      
      /** \brief predicts execution time in seconds using performance models */
      double estimate_time();
   
      /**
       * \brief returns 1 if summations have same tensors and index map
       * \param[in] os summation object to compare this with
       */
      int is_equal(summation const & os);
    private:
      /**
       * \brief finds and return all summation indices which can be folded into
       *    dgemm,for which they must (1) not break symmetry (2) belong to 
       *    exactly two of (A,B).
       * \param[in] type contraction specification
       * \param[out] num_fold number of indices that can be folded
       * \param[out] fold_idx indices that can be folded
       */
      void get_fold_indices(int *  num_fold,
                            int ** fold_idx);
    
      /**
       * \brief determines whether this summation can be folded
       * \return whether we can fold this summation
       */
      int can_fold();
 
      /**
       * \brief fold tensors into matrices for summation
       * \return inner stride (daxpy size)
       */
      int map_fold();

      /**
       * \brief find ordering of indices of tensor to reduce to DAXPY
       *
       * \param[out] new_ordering_A the new ordering for indices of A
       * \param[out] new_ordering_B the new ordering for indices of B
       */
      void get_len_ordering(int ** new_ordering_A,
                            int ** new_ordering_B);
  

      /**
       * \brief constructs function pointer to sum tensors A and B,B = B*beta+alpha*A
       * \param[in] inner_stride local daxpy stride
       * \return tsum summation class pointer to run
      */
      tsum * construct_sum(int inner_stride=-1);

      /**
       * \brief a*idx_map_A(A) + b*idx_map_B(B) -> idx_map_B(B).
       *        performs all necessary symmetric permutations removes/returns A/B to home buffer
       * \param[in] run_diag if 1 run diagonal sum
       */
      int home_sum_tsr(bool run_diag);

      /**
       * \brief a*idx_map_A(A) + b*idx_map_B(B) -> idx_map_B(B).
       *        performs all necessary symmetric permutations
       * \param[in] run_diag if 1 run diagonal sum
       */
      int sym_sum_tsr(bool run_diag);

      /**
       * \brief PDAXPY: a*idx_map_A(A) + b*idx_map_B(B) -> idx_map_B(B).
       * \param[in] run_diag if 1 run diagonal sum
       */
      int sum_tensors(bool run_diag);

      /**
       * \brief unfolds a broken symmetry in a summation by defining new tensors
       * \param[out] new_sum new summations specification (new tsrss)
       * \return 3*idx+tsr_type if finds broken sym,-1 otherwise
       */
      int unfold_broken_sym(summation ** new_sum);

      /**
       * \brief checks the edge lengths specfied for this sum match
       *          throws error if not
       */
      void check_consistency();


      /**
       * \brief checks whether mapping of tensors to topology is valid for this summation 
       * \return 1 if valid 0 if not
      */
      int check_mapping();

      /**
       * \brief map the indices which are common in a sum
       *
       * \param topo topology to map to
       * \return status corresponding to mapping success or failure
       */
      int map_sum_indices(topology const * topo);

      /**
       * \brief find best possible mapping for summation and redistribute tensors to this mapping
       * \return SUCCESS if valid mapping found, ERROR if not enough memory or another issue
       */
      int map();
  };
}

#endif