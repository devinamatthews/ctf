BDIR=$(realpath  $(CTF_BUILD_DIR))
ifeq (,${BDIR})
  BDIR=$(shell pwd)
endif
export BDIR
export ODIR=$(BDIR)/obj
include $(BDIR)/config.mk
export FCXX
export OFFLOAD_CXX
export LIBS

all: $(BDIR)/lib/libctf.a


EXAMPLES = algebraic_multigrid apsp bitonic_sort btwn_central ccsd checkpoint dft_3D fft force_integration force_integration_sparse jacobi matmul neural_network particle_interaction qinformatics recursive_matmul scan sparse_mp3 sparse_permuted_slice spectral_element spmv sssp strassen trace 
TESTS = bivar_function bivar_transform ccsdt_map_test ccsdt_t3_to_t2 dft diag_ctr diag_sym endomorphism_cust endomorphism_cust_sp endomorphism gemm_4D multi_tsr_sym permute_multiworld readall_test readwrite_test repack scalar speye sptensor_sum subworld_gemm sy_times_ns test_suite univar_function weigh_4D 

BENCHMARKS = bench_contraction bench_nosym_transp bench_redistribution model_trainer

SCALAPACK_TESTS = nonsq_pgemm_test nonsq_pgemm_bench 

STUDIES = fast_diagram fast_3mm fast_sym fast_sym_4D \
          fast_tensor_ctr fast_sy_as_as_tensor_ctr fast_as_as_sy_tensor_ctr

EXECUTABLES = $(EXAMPLES) $(TESTS) $(BENCHMARKS) $(SCALAPACK_TESTS) $(STUDIES)

export EXAMPLES
export TESTS
export BENCHMARKS
export SCALAPACK_TESTS
export STUDIES




.PHONY: executables
executables: $(EXECUTABLES)
$(EXECUTABLES): $(BDIR)/lib/libctf.a


.PHONY: examples
examples: $(EXAMPLES)
$(EXAMPLES):
	$(MAKE) $@ -C examples

.PHONY: tests
tests: $(TESTS)
$(TESTS):
	$(MAKE) $@ -C test

.PHONY: scalapack_tests
scalapack_tests: $(SCALAPACK_TESTS)
$(SCALAPACK_TESTS):
	$(MAKE) $@ -C scalapack_tests



.PHONY: bench
bench: $(BENCHMARKS)
$(BENCHMARKS):
	$(MAKE) $@ -C bench

.PHONY: studies
studies: $(STUDIES)
$(STUDIES):
	$(MAKE) $@ -C studies

.PHONY: ctf
ctf:
	$(MAKE) ctf -C src; 

.PHONY: ctflib
ctflib: ctf 
	$(AR) -crs $(BDIR)/lib/libctf.a $(ODIR)/*.o; 

$(BDIR)/lib/libctf.a: src/*/*.cu src/*/*.cxx src/*/*.h Makefile src/Makefile src/*/Makefile $(BDIR)/config.mk
	$(MAKE) ctflib
	
clean: clean_bin clean_lib clean_obj
#	$(MAKE) $@ -C src


test: test_suite
	$(BDIR)/bin/test_suite

test2: test_suite
	mpirun -np 2 $(BDIR)/bin/test_suite

test3: test_suite
	mpirun -np 3 $(BDIR)/bin/test_suite

test4: test_suite
	mpirun -np 4 $(BDIR)/bin/test_suite

test6: test_suite
	mpirun -np 6 $(BDIR)/bin/test_suite

test7: test_suite
	mpirun -np 7 $(BDIR)/bin/test_suite

test8: test_suite
	mpirun -np 8 $(BDIR)/bin/test_suite

clean_bin:
	for comp in $(EXECUTABLES) ; do \
		rm -f $(BDIR)/bin/$$comp ; \
	done 

clean_lib:
	rm -f $(BDIR)/lib/libctf.a

clean_obj:
	rm -f src/*/*.o #DEPRECATED: objs no longer created here, but keep it clean in case of git pull
	rm -f $(ODIR)/*.o 
