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
#include <minix/callnr.h>
#include <minix/com.h>
#include "proc.h"

#define SAFETY            32	/* safety margin for stack overflow (bytes) */
#define CMASK4          0x9E	/* mask for Planar Control Register */
#define HIGH_INT          17	/* limit of the interrupt vectors */

typedef _PROTOTYPE( void (*vecaddr_t), (void) );

FORWARD _PROTOTYPE( void set_vec, (int vec_nr, vecaddr_t addr,
		phys_clicks base_click) );

#if !INTEL_32BITS
PRIVATE vecaddr_t int_vec[HIGH_INT] = {
  int00, int01, int02, int03, int04, int05, int06, int07,
  int08, int09, int10, int11, int12, int13, int14, int15,
  int16,
};
#endif

/*===========================================================================*
 *                                   main                                    *
 *===========================================================================*/
PUBLIC void main()
{
/* Start the ball rolling. */

  register struct proc *rp;
  register int t;
  int sizeindex;
  phys_clicks base_click;
  phys_clicks text_base;
  vir_clicks text_clicks;
  vir_clicks data_clicks;
  phys_bytes phys_b;
  int stack_size;
  reg_t ktsb;			/* kernel task stack base */

  /* Interrupts are disabled here.
   * They are reenabled when INIT_PSW is loaded by the first restart().
   */

  /* Call the stage 2 assembler hooks to finish machine/mode-specific inits.
   * The 2 stages are needed to handle modes switches, especially 16->32 bits.
   */
  mpx_2hook();
  klib_2hook();

  /* Clear the process table.
   * Set up mappings for proc_addr() and proc_number() macros.
   */
  for (rp = BEG_PROC_ADDR, t = -NR_TASKS; rp < END_PROC_ADDR; ++rp, ++t) {
	rp->p_flags = P_SLOT_FREE;
	rp->p_nr = t;		/* proc number from ptr */
        (pproc_addr + NR_TASKS)[t] = rp;        /* proc ptr from number */
  }

#if (CHIP == INTEL)
  /* Finish off initialization of tables for protected mode. */
  if (protected_mode) ldt_init();
#endif

  /* Set up proc table entries for tasks and servers.  Be very careful about
   * sp, since in real mode the 3 words prior to it will be clobbered when
   * the kernel pushes pc, cs, and psw onto the USER's stack when starting
   * the user the first time.  If an interrupt happens before the user loads
   * a better stack pointer, these 3 words will be used to save the state,
   * and the interrupt handler will use another 3, and a debugger trap will
   * use another 3 or 4, and an "impossible" non-maskable interrupt may use
   * more!  This means that with INIT_SP == 0x1C, user programs must leave
   * the memory between 0x0008 and 0x001B free.  FS currently violates this
   * by using the word at 0x0008.
   */

  base_click = code_base >> CLICK_SHIFT;	/* base for tasks */

  /* Align stack base suitably. */
  ktsb = ((reg_t) t_stack + (ALIGNMENT - 1)) & ~((reg_t) ALIGNMENT - 1);

  for (rp = BEG_PROC_ADDR, t = -NR_TASKS; rp <= BEG_USER_ADDR; ++rp, ++t) {
	if (t < 0) {
		stack_size = tasktab[t + NR_TASKS].stksize;
		rp->p_splimit = ktsb + SAFETY;
		ktsb += stack_size;
		rp->p_reg.sp = ktsb;
		text_base = base_click;	/* tasks are all in the kernel */
		sizeindex = 0;		/* and use the full kernel sizes */
	} else {
		rp->p_splimit = rp->p_reg.sp = INIT_SP;
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

	if (!isidlehardware(t)) lock_ready(rp);	/* IDLE, HARDWARE neveready */
	rp->p_flags = 0;

#if (CHIP == INTEL)
	alloc_segments(rp);
#endif
  }

  /* Interpret memory sizes. */
  mem_init();

  /* Save the old interrupt vectors. */
  phys_b = umap(proc_addr(HARDWARE), D, (vir_bytes) vec_table, VECTOR_BYTES);
  phys_copy(0L, phys_b, (long) VECTOR_BYTES);	/* save all the vectors */

#if !INTEL_32BITS
  /* For protected mode, the interrupt vectors have already been set up by
   * prot_init() (with a different vector table).
   * So this block of code does nothing useful in protected mode.
   * It is not worth skipping, except for 32-bit mode where it would be too
   * much trouble to set up the unused functions in int_vec[].
   * The low-memory vector table was saved mainly for *wini.c to peek at.
   */

  /* Set up the new interrupt vectors. */
  for (t = 0; t < HIGH_INT; t++) set_vec(t, int_vec[t], base_click);
  for (t = HIGH_INT; t < 256; t++) set_vec(t, trp, base_click);

  set_vec(SYS_VECTOR, s_call, base_click);
  set_vec(CLOCK_VECTOR, clock_int, base_click);
  set_vec(KEYBOARD_VECTOR, tty_int, base_click);
  set_vec(SECONDARY_VECTOR, secondary_int, base_click);
  set_vec(RS232_VECTOR, rs232_int, base_click);
  set_vec(FLOPPY_VECTOR, disk_int, base_click);
  set_vec(PRINTER_VECTOR, lpr_int, base_click);

#if NETWORKING_ENABLED
  /* Overwrite RS232 SECONDARY_VECTOR. */
  set_vec(ETHER_VECTOR, eth_int, code_base);
#endif

  if (pc_at) {
	set_vec(AT_WINI_VECTOR, wini_int, base_click);
  } else
	set_vec(XT_WINI_VECTOR, wini_int, base_click);
  if (ps)
	set_vec(PS_KEYB_VECTOR, tty_int, base_click);
#endif /* !INTEL_32BITS */

  /* Put a ptr to proc table in a known place so it can be found in /dev/mem.
   * This is crufty, and fails if the address of the proc table is >= 64K
   * (we should write a phys_bytes number, not a u16_t from half a vector!.
   * However, we need similar hooks for the debugger.  0x600 used to be
   * code_base - the code base is no longer fixed.
   */
  set_vec( (0x600 - 4)/4, (vecaddr_t) proc, (phys_bytes) 0);

  bill_ptr = proc_addr(IDLE);  /* it has to point somewhere */
  lock_pick_proc();

  /* Finish initializing 8259 (needs machine type). */
  init_8259(IRQ0_VECTOR, IRQ8_VECTOR);

  /* Unmask second interrupt controller on AT's.
   * Don't use enable_irq(), because it unlocks interrupts.
   */
  if (pc_at) out_byte(INT_CTLMASK, ~(1 << (CASCADE_IRQ & 0x07)));

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

  soon_reboot();		/* so printf doesn't try to use sys services */
  if (*s != 0) {
	printf("\r\nKernel panic: %s",s);
	if (n != NO_NUM) printf(" %d", n);
	printf("\r\n");
  }
  if (db_exists) {
	db_enabled = TRUE;
	db();
  }
  wreboot();
}

#if (CHIP == INTEL)
/*===========================================================================*
 *                                   set_vec                                 *
 *===========================================================================*/
PRIVATE void set_vec(vec_nr, addr, base_click)
int vec_nr;			/* which vector */
vecaddr_t addr;			/* where to start */
phys_clicks base_click;		/* click where kernel sits in memory */
{
/* Set up an interrupt vector. */

  u16_t vec[2];
  phys_bytes phys_b;

  /* Build the vector in the array 'vec'. */
  vec[0] = (u16_t) addr;
  vec[1] = (u16_t) click_to_hclick(base_click);

  /* Copy the vector into place. */
  phys_b = umap(proc_addr(HARDWARE), D, (vir_bytes) vec, (vir_bytes) 4);
  phys_copy(phys_b, (phys_bytes) (vec_nr * 4), (phys_bytes) 4);
}
#endif
