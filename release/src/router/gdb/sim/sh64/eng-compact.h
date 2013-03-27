/* engine configuration for sh64 */

/* WITH_FAST: non-zero if a fast version of the engine is available
   in addition to the full-featured version.  */
#define WITH_FAST 1

/* WITH_SCACHE_PBB_SH64_COMPACT: non-zero if the pbb engine was selected.  */
#define WITH_SCACHE_PBB_SH64_COMPACT 1

/* HAVE_PARALLEL_INSNS: non-zero if cpu can parallelly execute > 1 insn.  */
#define HAVE_PARALLEL_INSNS 0
#define WITH_PARALLEL_READ 0
#define WITH_PARALLEL_WRITE 0
#define WITH_PARALLEL_GENWRITE 0

/* WITH_SEM_SWITCH_FULL: non-zero if full-featured engine is
   implemented as a switch().  */
#define WITH_SEM_SWITCH_FULL 0

/* WITH_SEM_SWITCH_FAST: non-zero if fast engine is
   implemented as a switch().  */
#define WITH_SEM_SWITCH_FAST 1

/* Functions defined in the generated mainloop.c file
   (which doesn't necessarily have that file name).  */

extern ENGINE_FN sh64_compact_engine_run_full;
extern ENGINE_FN sh64_compact_engine_run_fast;

extern SEM_PC sh64_compact_pbb_begin (SIM_CPU *, int);
extern SEM_PC sh64_compact_pbb_chain (SIM_CPU *, SEM_ARG);
extern SEM_PC sh64_compact_pbb_cti_chain (SIM_CPU *, SEM_ARG, SEM_BRANCH_TYPE, PCADDR);
extern void sh64_compact_pbb_before (SIM_CPU *, SCACHE *);
extern void sh64_compact_pbb_after (SIM_CPU *, SCACHE *);
