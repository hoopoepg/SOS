/* -*- mode: C; tab-width: 2; indent-tabs-mode: nil; -*- */

/*
 * This code has been contributed by the DARPA HPCS program.  Contact
 * David Koester <dkoester@mitre.org> or Bob Lucas <rflucas@isi.edu>
 * if you have questions.
 *
 *
 * GUPS (Giga UPdates per Second) is a measurement that profiles the memory
 * architecture of a system and is a measure of performance similar to MFLOPS.
 * The HPCS HPCchallenge RandomAccess benchmark is intended to exercise the
 * GUPS capability of a system, much like the LINPACK benchmark is intended to
 * exercise the MFLOPS capability of a computer.  In each case, we would
 * expect these benchmarks to achieve close to the "peak" capability of the
 * memory system. The extent of the similarities between RandomAccess and
 * LINPACK are limited to both benchmarks attempting to calculate a peak system
 * capability.
 *
 * GUPS is calculated by identifying the number of memory locations that can be
 * randomly updated in one second, divided by 1 billion (1e9). The term "randomly"
 * means that there is little relationship between one address to be updated and
 * the next, except that they occur in the space of one half the total system
 * memory.  An update is a read-modify-write operation on a table of 64-bit words.
 * An address is generated, the value at that address read from memory, modified
 * by an integer operation (add, and, or, xor) with a literal value, and that
 * new value is written back to memory.
 *
 * We are interested in knowing the GUPS performance of both entire systems and
 * system subcomponents --- e.g., the GUPS rating of a distributed memory
 * multiprocessor the GUPS rating of an SMP node, and the GUPS rating of a
 * single processor.  While there is typically a scaling of FLOPS with processor
 * count, a similar phenomenon may not always occur for GUPS.
 *
 * Select the memory size to be the power of two such that 2^n <= 1/2 of the
 * total memory.  Each CPU operates on its own address stream, and the single
 * table may be distributed among nodes. The distribution of memory to nodes
 * is left to the implementer.  A uniform data distribution may help balance
 * the workload, while non-uniform data distributions may simplify the
 * calculations that identify processor location by eliminating the requirement
 * for integer divides. A small (less than 1%) percentage of missed updates
 * are permitted.
 *
 * When implementing a benchmark that measures GUPS on a distributed memory
 * multiprocessor system, it may be required to define constraints as to how
 * far in the random address stream each node is permitted to "look ahead".
 * Likewise, it may be required to define a constraint as to the number of
 * update messages that can be stored before processing to permit multi-level
 * parallelism for those systems that support such a paradigm.  The limits on
 * "look ahead" and "stored updates" are being implemented to assure that the
 * benchmark meets the intent to profile memory architecture and not induce
 * significant artificial data locality. For the purpose of measuring GUPS,
 * we will stipulate that each thread is permitted to look ahead no more than
 * 1024 random address stream samples with the same number of update messages
 * stored before processing.
 *
 * The supplied MPI-1 code generates the input stream {A} on all processors
 * and the global table has been distributed as uniformly as possible to
 * balance the workload and minimize any Amdahl fraction.  This code does not
 * exploit "look-ahead".  Addresses are sent to the appropriate processor
 * where the table entry resides as soon as each address is calculated.
 * Updates are performed as addresses are received.  Each message is limited
 * to a single 64 bit long integer containing element ai from {A}.
 * Local offsets for T[ ] are extracted by the destination processor.
 *
 * If the number of processors is equal to a power of two, then the global
 * table can be distributed equally over the processors.  In addition, the
 * processor number can be determined from that portion of the input stream
 * that identifies the address into the global table by masking off log2(p)
 * bits in the address.
 *
 * If the number of processors is not equal to a power of two, then the global
 * table cannot be equally distributed between processors.  In the MPI-1
 * implementation provided, there has been an attempt to minimize the differences
 * in workloads and the largest difference in elements of T[ ] is one.  The
 * number of values in the input stream generated by each processor will be
 * related to the number of global table entries on each processor.
 *
 * The MPI-1 version of RandomAccess treats the potential instance where the
 * number of processors is a power of two as a special case, because of the
 * significant simplifications possible because processor location and local
 * offset can be determined by applying masks to the input stream values.
 * The non power of two case uses an integer division to determine the processor
 * location.  The integer division will be more costly in terms of machine
 * cycles to perform than the bit masking operations
 *
 * For additional information on the GUPS metric, the HPCchallenge RandomAccess
 * Benchmark,and the rules to run RandomAccess or modify it to optimize
 * performance -- see http://icl.cs.utk.edu/hpcc/
 *
 */

/* Jan 2005
 *
 * This code has been modified to allow local bucket sorting of updates.
 * The total maximum number of updates in the local buckets of a process
 * is currently defined in "RandomAccess.h" as MAX_TOTAL_PENDING_UPDATES.
 * When the total maximum number of updates is reached, the process selects
 * the bucket (or destination process) with the largest number of
 * updates and sends out all the updates in that bucket. See buckets.c
 * for details about the buckets' implementation.
 *
 * This code also supports posting multiple MPI receive descriptors (based
 * on a contribution by David Addison).
 *
 * In addition, this implementation provides an option for limiting
 * the execution time of the benchmark to a specified time bound
 * (see time_bound.c). The time bound is currently defined in
 * time_bound.h, but it should be a benchmark parameter. By default
 * the benchmark will execute the recommended number of updates,
 * that is, four times the global table size.
 */

/*
 * OpenSHMEM version:
 *
 * Copyright (c) 2011 - 2015
 *   University of Houston System and UT-Battelle, LLC.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * o Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * o Neither the name of the University of Houston System,
 *   UT-Battelle, LLC. nor the names of its contributors may be used to
 *   endorse or promote products derived from this software without specific
 *   prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Copyright (c) 2016 Los Alamos National Security, LLC. All rights reserved.
 *
 *  This file is part of the Sandia OpenSHMEM software package. For license
 * information, see the LICENSE file in the top level directory of the
 * distribution.
 *
 */

#include <stdio.h>
#include <shmem.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#if 0
#include "config.h"
#endif

/* Random number generator */
#define POLY 0x0000000000000007UL
#define PERIOD 1317624576693539401L

/* Define 64-bit constants */
#define ZERO64B 0LL

uint64_t TotalMemOpt = 8192;
int NumUpdatesOpt = 0;
double SHMEMGUPs;
double SHMEMRandomAccess_ErrorsFraction;
double SHMEMRandomAccess_time;
double SHMEMRandomAccess_CheckTime;
int Failure;

/* Allocate main table (in global memory) */
uint64_t *HPCC_Table;
long *HPCC_PELock;

static uint64_t GlobalStartMyProc;

int SHMEMRandomAccess(void);

static double RTSEC(void)
{
  struct timeval tp;
  gettimeofday (&tp, NULL);
  return tp.tv_sec + tp.tv_usec/(double)1.0e6;
}

static void print_usage(void)
{
  fprintf(stderr, "\nOptions:\n");
  fprintf(stderr, " %-20s %s\n", "-h", "display this help message");
  fprintf(stderr, " %-20s %s\n", "-m", "memory in bytes per PE");
  fprintf(stderr, " %-20s %s\n", "-n", "number of updates per PE");

  return;
}

static int64_t starts(uint64_t n)
{
  /* int64_t i, j; */
  int i, j;
  uint64_t m2[64];
  uint64_t temp, ran;

  /*
   * this loop doesn't make sense
   * so commenting out.
   */
#if 0
  while (n < 0)
    n += PERIOD;
#endif
  while (n > PERIOD)
    n -= PERIOD;
  if (n == 0)
    return 0x1;

  temp = 0x1;
  for (i=0; i<64; i++) {
    m2[i] = temp;
    temp = (temp << 1) ^ ((int64_t) temp < 0 ? POLY : 0);
    temp = (temp << 1) ^ ((int64_t) temp < 0 ? POLY : 0);
  }

  for (i=62; i>=0; i--)
    if ((n >> i) & 1) break;

  ran = 0x2;

  while (i > 0) {
    temp = 0;
    for (j=0; j<64; j++)
      if ((ran >> j) & 1) temp ^= m2[j];
    ran = temp;
    i -= 1;
    if ((n >> i) & 1)
      ran = (ran << 1) ^ ((int64_t) ran < 0 ? POLY : 0);
  }

  return ran;
}

static void
UpdateTable(uint64_t *Table,
            uint64_t TableSize,
            uint64_t MinLocalTableSize,
            uint64_t Top,
            int Remainder,
            uint64_t niterate,
            int use_lock)
{
  uint64_t iterate;
  int index;
  uint64_t ran, global_offset;
  int remote_pe;
  int global_start_at_pe;
#ifdef USE_GET_PUT
  uint64_t remote_val;
#endif

  shmem_barrier_all();

  /* setup: should not really be part of this timed routine */
  ran = starts(4*GlobalStartMyProc);

  for (iterate = 0; iterate < niterate; iterate++) {
      ran = (ran << 1) ^ ((int64_t) ran < ZERO64B ? POLY : ZERO64B);
      global_offset = ran & (TableSize-1);
      if (global_offset < Top) {
          remote_pe = global_offset / (MinLocalTableSize + 1);
          global_start_at_pe = (MinLocalTableSize + 1) * remote_pe;
      } else {
          remote_pe = (global_offset - Remainder) / MinLocalTableSize;
          global_start_at_pe = MinLocalTableSize * remote_pe + Remainder;
      }
      index = global_offset - global_start_at_pe;

      if (use_lock) shmem_set_lock(&HPCC_PELock[remote_pe]);
#ifdef USE_GET_PUT
      remote_val = (uint64_t) shmem_long_g((long *)&Table[index], remote_pe);
      remote_val ^= ran;
      shmem_long_p((long *)&Table[index], remote_val, remote_pe);
#else
      shmem_uint64_atomic_xor(&Table[index], ran, remote_pe);
#endif
      if (use_lock) shmem_clear_lock(&HPCC_PELock[remote_pe]);
  }

  shmem_barrier_all();

}

int
SHMEMRandomAccess(void)
{
  int64_t i;
  uint64_t i_u;
  static int64_t NumErrors, GlbNumErrors;

  int NumProcs, MyProc;
  int Remainder;            /* Number of processors with (LocalTableSize + 1) entries */
  uint64_t Top;               /* Number of table entries in top of Table */
  uint64_t LocalTableSize;    /* Local table width */
  uint64_t MinLocalTableSize; /* Integer ratio TableSize/NumProcs */
  uint64_t logTableSize, TableSize;

  double RealTime;              /* Real time to update table */

  double TotalMem;
  static int sAbort, rAbort;

  uint64_t NumUpdates; /* total number of updates to table */
  uint64_t ProcNumUpdates; /* number of updates per processor */

  static long pSync_bcast[SHMEM_BCAST_SYNC_SIZE];
  static long long int llpWrk[SHMEM_REDUCE_MIN_WRKDATA_SIZE];

  static long pSync_reduce[SHMEM_REDUCE_SYNC_SIZE];
  static int ipWrk[SHMEM_REDUCE_MIN_WRKDATA_SIZE];

  FILE *outFile = NULL;
  double *GUPs;
  double *temp_GUPs;


  for (i = 0; i < SHMEM_BCAST_SYNC_SIZE; i += 1){
        pSync_bcast[i] = SHMEM_SYNC_VALUE;
  }

  for (i = 0; i < SHMEM_REDUCE_SYNC_SIZE; i += 1){
        pSync_reduce[i] = SHMEM_SYNC_VALUE;
  }

  SHMEMGUPs = -1;
  GUPs = &SHMEMGUPs;

  NumProcs = shmem_n_pes();
  MyProc = shmem_my_pe();

  if (0 == MyProc) {
    outFile = stdout;
    setbuf(outFile, NULL);
  }

  /*
   * TODO: replace this
   */

  TotalMem = TotalMemOpt; /* max single node memory */
  TotalMem *= NumProcs;   /* max memory in NumProcs nodes */

  TotalMem /= sizeof(uint64_t);

  /* calculate TableSize --- the size of update array (must be a power of 2) */
  for (TotalMem *= 0.5, logTableSize = 0, TableSize = 1;
       TotalMem >= 1.0;
       TotalMem *= 0.5, logTableSize++, TableSize <<= 1)
    ; /* EMPTY */

  /*
   * Calculate local table size, etc.
   */

  MinLocalTableSize = TableSize / NumProcs;

  /* Number of processors with (LocalTableSize + 1) entries */

  Remainder = TableSize  - (MinLocalTableSize * NumProcs);

  /* Number of table entries in top of Table */
  Top = (MinLocalTableSize + 1) * Remainder;
  /* Local table size */
  if (MyProc < Remainder) {
    LocalTableSize = MinLocalTableSize + 1;
    GlobalStartMyProc = ( (MinLocalTableSize + 1) * MyProc);
  } else {
    LocalTableSize = MinLocalTableSize;
    GlobalStartMyProc = ( (MinLocalTableSize * MyProc) + Remainder );
  }


  sAbort = 0;
  /* Ensure the allocation size is symmetric */
  HPCC_Table = shmem_malloc((Remainder > 0 ? (MinLocalTableSize + 1) : LocalTableSize)
                            * sizeof(uint64_t));
  if (! HPCC_Table) sAbort = 1;

  HPCC_PELock = (long *) shmem_malloc(sizeof(long) * NumProcs);
  if (! HPCC_PELock) sAbort = 1;

  shmem_barrier_all();
  shmem_int_sum_to_all(&rAbort, &sAbort, 1, 0, 0, NumProcs, ipWrk, pSync_reduce);
  shmem_barrier_all();

  if (rAbort > 0) {
    if (MyProc == 0) fprintf(outFile, "Failed to allocate memory\n");
    /* check all allocations in case there are new added and their order changes */
    if (HPCC_Table) shmem_free( HPCC_Table );
    if (HPCC_PELock) shmem_free( HPCC_PELock );
    goto failed_table;
  }

  for (i = 0; i < NumProcs; i++)
      HPCC_PELock[i] = 0;

  /* Default number of global updates to table: 4x number of table entries */
  if (NumUpdatesOpt == 0) {
     ProcNumUpdates = 4 * LocalTableSize;
     NumUpdates = 4 * TableSize;
  } else {
     ProcNumUpdates = NumUpdatesOpt;
     NumUpdates = NumUpdatesOpt * NumProcs;
  }

  if (MyProc == 0) {
    fprintf( outFile, "Running on %d processors\n", NumProcs);
    fprintf( outFile, "Total Main table size = 2^%" PRIu64 " = %" PRIu64 " words\n",
             logTableSize, TableSize );
    fprintf( outFile, "PE Main table size = (2^%" PRIu64 ")/%d  = %" PRIu64 " words/PE MAX\n",
             logTableSize, NumProcs, LocalTableSize);

    fprintf( outFile, "Total number of updates = %" PRIu64 "\n", NumUpdates);
  }

  /* Initialize main table */
  for (i_u=0; i_u<LocalTableSize; i_u++)
    HPCC_Table[i_u] = i_u + GlobalStartMyProc;

  shmem_barrier_all();

  RealTime = -RTSEC();

  UpdateTable(HPCC_Table,
              TableSize,
              MinLocalTableSize,
              Top,
              Remainder,
              ProcNumUpdates,
              0);

  shmem_barrier_all();

  /* End timed section */

  RealTime += RTSEC();

  /* Print timing results */
  if (MyProc == 0){
    SHMEMRandomAccess_time = RealTime;
    *GUPs = 1e-9*NumUpdates / RealTime;
    fprintf( outFile, "Real time used = %.6f seconds\n", RealTime );
    fprintf( outFile, "%.9f Billion(10^9) Updates    per second [GUP/s]\n",
             *GUPs );
    fprintf( outFile, "%.9f Billion(10^9) Updates/PE per second [GUP/s]\n",
             *GUPs / NumProcs );
  }
  /* distribute result to all nodes */
  temp_GUPs = GUPs;
  shmem_barrier_all();
  shmem_broadcast64(GUPs,temp_GUPs,1,0,0,0,NumProcs,pSync_bcast);
  shmem_barrier_all();

  /* Verification phase */

  /* Begin timing here */

  RealTime = -RTSEC();

  UpdateTable(HPCC_Table,
              TableSize,
              MinLocalTableSize,
              Top,
              Remainder,
              ProcNumUpdates,
              1);

  NumErrors = 0;
  for (i_u=0; i_u<LocalTableSize; i_u++){
    if (HPCC_Table[i_u] != i_u + GlobalStartMyProc)
      NumErrors++;
  }

  shmem_barrier_all();
  shmem_longlong_sum_to_all( (long long *)&GlbNumErrors,  (long long *)&NumErrors, 1, 0,0, NumProcs,llpWrk, pSync_reduce);
  shmem_barrier_all();

  /* End timed section */

  RealTime += RTSEC();

  if(MyProc == 0){
    fprintf( outFile, "Verification:  Real time used = %.6f seconds\n", RealTime);
    fprintf( outFile, "Found %" PRIu64 " errors in %" PRIu64 " locations (%s).\n",
             GlbNumErrors, TableSize, (GlbNumErrors <= 0.01*TableSize) ?
             "passed" : "failed");
    if (GlbNumErrors > 0.01*TableSize) Failure = 1;
  }
  /* End verification phase */

  shmem_free( HPCC_Table );
  shmem_free( HPCC_PELock );
  failed_table:

  if (0 == MyProc) if (outFile != stderr) fclose( outFile );

  shmem_barrier_all();

  return 0;
}

int main(int argc, char **argv)
{
  int op;

  while ((op = getopt(argc, argv, "hm:n:")) != -1) {
    switch (op) {
      /*
       * memory per PE (used for determining table size)
       */
      case 'm':
        TotalMemOpt = atoll(optarg);
        if (TotalMemOpt <= 0) {
          print_usage();
          return -1;
        }
        break;

        /*
         * num updates/PE
         */
      case 'n':
        NumUpdatesOpt = atoi(optarg);
        if (NumUpdatesOpt <= 0) {
          print_usage();
          return -1;
        }
        break;

      case '?':
      case 'h':
        print_usage();
        return -1;
    }
  }

  shmem_init();
  SHMEMRandomAccess();
  shmem_finalize();

  return 0;
}
