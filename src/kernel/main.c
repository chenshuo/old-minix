/* This file contains the main program of MINIX.  The routine main()
 * initializes the system and starts the ball rolling by setting up the proc
 * table, interrupt vectors, and scheduling each task to run to initialize
 * itself.
 *
 * The entries into this file are:
 *   main:		MINIX main program
 *   panic:		abort MINIX due to a fatal error
 */

#include "kernel.h"
#include <signal.h>
#include <unistd.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include "proc.h"

#define CMASK4          0x9E	/* mask for Planar Control Register */


/*===========================================================================*
 *                                   main                                    *
 *===========================================================================*/
PUBLIC void main()
{
/* Start the ball rolling. */

  register struct proc *rp;
  register int t;
  int sizeindex;
  phys_clicks text_base;
  vir_clicks text_clicks;
  vir_clicks data_clicks;
  phys_bytes phys_b;
  int stack_size;
  reg_t ktsb;			/* kernel task stack base */
  struct memory *memp;

  /* Finish initializing 8259 (needs machine type). */
  init_8259(IRQ0_VECTOR, IRQ8_VECTOR);

  /* Interrupts are still disabled here.
   * They are reenabled when INIT_PSW is loaded by the first restart().
   */

  /* Clear the process table.
   * Set up mappings for proc_addr() and proc_number() macros.
   */
  for (rp = BEG_PROC_ADDR, t = -NR_TASKS; rp < END_PROC_ADDR; ++rp, ++t) {
	rp->p_flags = P_SLOT_FREE;
	rp->p_nr = t;		/* proc number from ptr */
        (pproc_addr + NR_TASKS)[t] = rp;        /* proc ptr from number */
  }

  /* Finish off initialization of tables for protected mode. */
  if (protected_mode) ldt_init();

  /* Interpret memory sizes. */
  mem_init();

  /* Set up proc table entries for tasks and servers.  The stacks of the
   * kernel tasks are initialized to an array in data space.  The stacks
   * of the servers have been added to the data segment by the monitor, so
   * the stack pointer is set to the end of the data segment.  All the
   * processes are in low memory on the 8086.  On the 386 only the kernel
   * is in low memory, the rest if loaded in extended memory.
   */

  /* Task stacks. */
  ktsb = (reg_t) t_stack;

  for (rp = BEG_PROC_ADDR, t = -NR_TASKS; rp <= BEG_USER_ADDR; ++rp, ++t) {
	if (t < 0) {
		stack_size = tasktab[t + NR_TASKS].stksize;
		if (stack_size > 0) {
			rp->p_stguard = (reg_t *) ktsb;
			*rp->p_stguard = STACK_GUARD;
		}
		ktsb += stack_size;
		rp->p_reg.sp = ktsb;
		text_base = code_base >> CLICK_SHIFT;
					/* tasks are all in the kernel */
		sizeindex = 0;		/* and use the full kernel sizes */
		memp = &mem[0];		/* remove from this memory chunk */
	} else {
		sizeindex = 2 * t + 2;	/* MM, FS, INIT have their own sizes */
	}
	rp->p_reg.pc = (reg_t) tasktab[t + NR_TASKS].initial_pc;
	rp->p_reg.psw = istaskp(rp) ? INIT_TASK_PSW : INIT_PSW;

	text_clicks = sizes[sizeindex];
	data_clicks = sizes[sizeindex + 1];
	rp->p_map[T].mem_phys = text_base;
	rp->p_map[T].mem_len  = text_clicks;
	rp->p_map[D].mem_phys = text_base + text_clicks;
	rp->p_map[D].mem_len  = data_clicks;
	rp->p_map[S].mem_phys = text_base + text_clicks + data_clicks;
	rp->p_map[S].mem_vir  = data_clicks;	/* empty - stack is in data */
	text_base += text_clicks + data_clicks;	/* ready for next, if server */
	memp->size -= (text_base - memp->base);
	memp->base = text_base;			/* memory no longer free */

	if (t >= 0) {
		/* Initialize the server stack pointer.  Take it down one word
		 * to give crtso.s something to use as "argc".
		 */
		rp->p_reg.sp = (rp->p_map[S].mem_vir +
				rp->p_map[S].mem_len) << CLICK_SHIFT;
		rp->p_reg.sp -= sizeof(reg_t);
	}

#if _WORD_SIZE == 4
	/* Servers are loaded in extended memory if in 386 mode. */
	if (t < 0) {
		memp = &mem[1];
		text_base = 0x100000 >> CLICK_SHIFT;
	}
#endif
	if (!isidlehardware(t)) lock_ready(rp);	/* IDLE, HARDWARE neveready */
	rp->p_flags = 0;

	alloc_segments(rp);
  }

  bill_ptr = proc_addr(IDLE);  /* it has to point somewhere */
  lock_pick_proc();

  /* Set planar control registers on PS's.  Fix this.  CMASK4 is magic and
   * probably ought to be set by the individual drivers.
   */
  if (ps) out_byte(PCR, CMASK4);

  /* Now go to the assembly code to start running the current process. */
  restart();
}


/*===========================================================================*
 *                                   panic                                   *
 *===========================================================================*/
PUBLIC void panic(s,n)
_CONST char *s;
int n;
{
/* The system has run aground of a fatal error.  Terminate execution.
 * If the panic originated in MM or FS, the string will be empty and the
 * file system already syncked.  If the panic originates in the kernel, we are
 * kind of stuck.
 */

  if (*s != 0) {
	printf("\nKernel panic: %s",s);
	if (n != NO_NUM) printf(" %d", n);
	printf("\n");
  }
  wreboot(RBT_PANIC);
}
