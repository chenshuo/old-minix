/* This file contains routines to initialize code and data segment
 * descriptors, and to initialize global descriptors for local descriptors
 * in the process table.
 * It is separate from protect.c for the 386 32-bit kernel.
 * There it must be compiled into 32-bit code (for use by the kernel) and
 * (apart from the LDT initialization) into static 16-bit code (for use by
 * the routines in protect.c), while protect.c must be compiled into 16-bit
 * code to initialize 32-bit mode.
 */

#ifdef _SYSTEM			/* this is being included by protect.c */
#define MAYBE_PUBLIC PRIVATE
#define NEED_LDT_INIT 0
#define init_codeseg linit_codeseg
#define init_dataseg linit_dataseg
#define sdesc lsdesc
#else				/* native version */
#include "kernel.h"
#include "proc.h"
#include "protect.h"
#define MAYBE_PUBLIC PUBLIC
#define NEED_LDT_INIT 1
#endif

FORWARD void sdesc();

/*=========================================================================*
 *				init_codeseg				   *
 *=========================================================================*/
MAYBE_PUBLIC void init_codeseg(segdp, base, size, privilege)
register struct segdesc_s *segdp;
phys_bytes base;
phys_bytes size;
int privilege;
{
/* Build descriptor for a code segment. */

  sdesc(segdp, base, size);
  segdp->access = (privilege << DPL_SHIFT)
	        | (PRESENT | SEGMENT | EXECUTABLE | READABLE);
		/* CONFORMING = 0, ACCESSED = 0 */
}

/*=========================================================================*
 *				init_dataseg				   *
 *=========================================================================*/
MAYBE_PUBLIC void init_dataseg(segdp, base, size, privilege)
register struct segdesc_s *segdp;
phys_bytes base;
phys_bytes size;
int privilege;
{
/* Build descriptor for a data segment. */

  sdesc(segdp, base, size);
  segdp->access = (privilege << DPL_SHIFT) | (PRESENT | SEGMENT | WRITEABLE);
		/* EXECUTABLE = 0, EXPAND_DOWN = 0, ACCESSED = 0 */
}

#if NEED_LDT_INIT
/*=========================================================================*
 *				ldt_init				   *
 *=========================================================================*/
PUBLIC void ldt_init()
{
/* Build local descriptors in GDT for LDT's in process table.
 * The LDT's are allocated at compile time in the process table, and
 * initialized whenever a process' map is initialized or changed.
 */

  unsigned ldt_selector;
  register struct proc *rp;

  for (rp = BEG_PROC_ADDR, ldt_selector = FIRST_LDT_INDEX * DESC_SIZE;
       rp < END_PROC_ADDR; ++rp, ldt_selector += DESC_SIZE) {
	init_dataseg(&gdt[ldt_selector / DESC_SIZE],
		     data_base + (phys_bytes) (vir_bytes) rp->p_ldt,
		     (phys_bytes) sizeof rp->p_ldt, INTR_PRIVILEGE);
	gdt[ldt_selector / DESC_SIZE].access = PRESENT | LDT;
	rp->p_ldt_sel = ldt_selector;
  }
}
#endif				/* NEED_LDT_INIT */

/*=========================================================================*
 *				sdesc					   *
 *=========================================================================*/
PRIVATE void sdesc(segdp, base, size)
register struct segdesc_s *segdp;
phys_bytes base;
phys_bytes size;
{
/* Fill in the size fields (base, limit and granularity) of a descriptor. */

  segdp->base_low = base;
  segdp->base_middle = base >> BASE_MIDDLE_SHIFT;
#if INTEL_32BITS
  segdp->base_high = base >> BASE_HIGH_SHIFT;
  --size;			/* convert to a limit, 0 size means 4G */
  if (size > BYTE_GRAN_MAX) {
	segdp->limit_low = size >> PAGE_GRAN_SHIFT;
	segdp->granularity = GRANULAR | (size >>
				     (PAGE_GRAN_SHIFT + GRANULARITY_SHIFT));
  } else {
	segdp->limit_low = size;
	segdp->granularity = size >> GRANULARITY_SHIFT;
  }
  segdp->granularity |= DEFAULT;	/* means BIG for data seg */
#else
  segdp->limit_low = size - 1;
#endif
}
