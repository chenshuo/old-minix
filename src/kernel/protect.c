/* This file contains code for initialization of protected mode.
 *
 * The code must run in 16-bit mode (and be compiled with a 16-bit
 * compiler!) even for a 32-bit kernel.
 */

#include "kernel.h"
#include "protect.h"

#if INTEL_32BITS
#include "protect1.c"		/* local 16-bit versions of some functions */
#define INT_GATE_TYPE (INT_286_GATE | DESC_386_BIT)
#define TSS_TYPE      (AVL_286_TSS  | DESC_386_BIT)
#else
#define INT_GATE_TYPE INT_286_GATE
#define TSS_TYPE      AVL_286_TSS
#endif

struct desctableptr_s {
  u16_t limit;
  u32_t base;			/* really u24_t + pad for 286 */
};

struct gatedesc_s {
  u16_t offset_low;
  u16_t selector;
  u8_t pad;			/* |000|XXXXX| ig & trpg, |XXXXXXXX| task g */
  u8_t p_dpl_type;		/* |P|DL|0|TYPE| */
#if INTEL_32BITS
  u16_t offset_high;
#else
  u16_t reserved;
#endif
};

struct tss_s {
  reg_t backlink;
  reg_t sp0;
  reg_t ss0;
  reg_t sp1;
  reg_t ss1;
  reg_t sp2;
  reg_t ss2;
#if INTEL_32BITS
  reg_t cr3;
#endif
  reg_t ip;
  reg_t flags;
  reg_t ax;
  reg_t cx;
  reg_t dx;
  reg_t bx;
  reg_t sp;
  reg_t bp;
  reg_t si;
  reg_t di;
  reg_t es;
  reg_t cs;
  reg_t ss;
  reg_t ds;
#if INTEL_32BITS
  reg_t fs;
  reg_t gs;
#endif
  reg_t ldt;
#if INTEL_32BITS
  u16_t trap;
  u16_t iobase;
/* u8_t iomap[0]; */
#endif
};

PUBLIC struct segdesc_s gdt[GDT_SIZE];
PRIVATE struct gatedesc_s idt[IDT_SIZE];	/* zero-init so none present */
PUBLIC struct tss_s tss;	/* zero init */

FORWARD void int_gate();

/*=========================================================================*
 *				int_gate				   *
 *=========================================================================*/
PRIVATE void int_gate(vec_nr, base, dpl_type)
unsigned vec_nr;
phys_bytes base;
unsigned dpl_type;
{
/* Build descriptor for an interrupt gate. */

  register struct gatedesc_s *idp;

  idp = &idt[vec_nr];
  idp->offset_low = base;
  idp->selector = CS_SELECTOR;
  idp->p_dpl_type = dpl_type;
#if INTEL_32BITS
  idp->offset_high = base >> OFFSET_HIGH_SHIFT;
#endif
}

/*=========================================================================*
 *				prot_init				   *
 *=========================================================================*/
PUBLIC void prot_init()
{
/* Set up tables for protected mode.
 * All GDT slots are allocated at compile time.
 */

  phys_bytes code_bytes;
  phys_bytes data_bytes;
  struct gate_table_s *gtp;

  static struct gate_table_s {
	void (*gate)();
	unsigned char vec_nr;
	unsigned char privilege;
  }
  gate_table[] = {
	divide_error, DIVIDE_VECTOR, INTR_PRIVILEGE,
	single_step_exception, DEBUG_VECTOR, INTR_PRIVILEGE,
	nmi, NMI_VECTOR, INTR_PRIVILEGE,
	breakpoint_exception, BREAKPOINT_VECTOR, USER_PRIVILEGE,
	overflow, OVERFLOW_VECTOR, USER_PRIVILEGE,
	bounds_check, BOUNDS_VECTOR, INTR_PRIVILEGE,
	inval_opcode, INVAL_OP_VECTOR, INTR_PRIVILEGE,
	copr_not_available, COPROC_NOT_VECTOR, INTR_PRIVILEGE,
	double_fault, DOUBLE_FAULT_VECTOR, INTR_PRIVILEGE,
	copr_seg_overrun, COPROC_SEG_VECTOR, INTR_PRIVILEGE,
	inval_tss, INVAL_TSS_VECTOR, INTR_PRIVILEGE,
	segment_not_present, SEG_NOT_VECTOR, INTR_PRIVILEGE,
	stack_exception, STACK_FAULT_VECTOR, INTR_PRIVILEGE,
	general_protection, PROTECTION_VECTOR, INTR_PRIVILEGE,
#if INTEL_32BITS
	page_fault, PAGE_FAULT_VECTOR, INTR_PRIVILEGE,
	copr_error, COPROC_ERR_VECTOR, INTR_PRIVILEGE,
#endif
	clock_int, CLOCK_VECTOR, INTR_PRIVILEGE,
	tty_int, KEYBOARD_VECTOR, INTR_PRIVILEGE,
	psecondary_int, SECONDARY_VECTOR, INTR_PRIVILEGE,
#if AM_KERNEL
#if !NONET 
	eth_int, ETHER_VECTOR, INTR_PRIVILEGE,
#endif
#endif
	prs232_int, RS232_VECTOR, INTR_PRIVILEGE,
	disk_int, FLOPPY_VECTOR, INTR_PRIVILEGE,
	lpr_int, PRINTER_VECTOR, INTR_PRIVILEGE,
	wini_int, AT_WINI_VECTOR, INTR_PRIVILEGE,
  };

  /* This is called early and can't use tables set up by main(). */
  data_bytes = (phys_bytes) sizes[1] << CLICK_SHIFT;
  if (sizes[0] == 0)
	code_bytes = data_bytes;	/* common I&D */
  else
	code_bytes = (phys_bytes) sizes[0] << CLICK_SHIFT;

  /* Build temporary gdt and idt pointers in GDT where BIOS needs them. */
  ((struct desctableptr_s *) &gdt[BIOS_GDT_INDEX])->limit = sizeof gdt - 1;
  ((struct desctableptr_s *) &gdt[BIOS_GDT_INDEX])->base =
	data_base + (phys_bytes) (vir_bytes) gdt;
  ((struct desctableptr_s *) &gdt[BIOS_IDT_INDEX])->limit = sizeof idt - 1;
  ((struct desctableptr_s *) &gdt[BIOS_IDT_INDEX])->base =
	data_base + (phys_bytes) (vir_bytes) idt;

  /* Build segment descriptors for tasks and interrupt handlers. */
  init_dataseg(&gdt[DS_INDEX], data_base, data_bytes, INTR_PRIVILEGE);
  init_dataseg(&gdt[ES_INDEX], data_base, data_bytes, INTR_PRIVILEGE);
  init_dataseg(&gdt[SS_INDEX], data_base, data_bytes, INTR_PRIVILEGE);
  init_codeseg(&gdt[CS_INDEX], code_base, code_bytes, INTR_PRIVILEGE);

  /* Build segments for debugger. */
  init_codeseg(&gdt[DB_CS_INDEX], hclick_to_physb(break_vector.selector),
	       (phys_bytes) MAX_286_SEG_SIZE, INTR_PRIVILEGE);
  init_dataseg(&gdt[DB_DS_INDEX], hclick_to_physb(break_vector.selector),
	       (phys_bytes) MAX_286_SEG_SIZE, INTR_PRIVILEGE);
  init_dataseg(&gdt[GDT_INDEX], data_base + (phys_bytes) (vir_bytes) gdt,
	       (phys_bytes) sizeof gdt, INTR_PRIVILEGE);

  /* Build scratch descriptors for functions in klib286. */
  init_dataseg(&gdt[DS_286_INDEX], (phys_bytes) 0,
	       (phys_bytes) MAX_286_SEG_SIZE, TASK_PRIVILEGE);
  init_dataseg(&gdt[ES_286_INDEX], (phys_bytes) 0,
	       (phys_bytes) MAX_286_SEG_SIZE, TASK_PRIVILEGE);

  /* Build descriptors for video segments. */
  init_dataseg(&gdt[COLOR_INDEX], (phys_bytes) COLOR_BASE,
	       (phys_bytes) COLOR_SIZE, TASK_PRIVILEGE);
  init_dataseg(&gdt[MONO_INDEX], (phys_bytes) MONO_BASE,
	       (phys_bytes) MONO_SIZE, TASK_PRIVILEGE);

#if AM_KERNEL
#if !NONET
/* Build descriptor for Western Digital Etherplus card buffer. */
  init_dataseg(&gdt[EPLUS_INDEX], (phys_bytes) EPLUS_BASE,
		(phys_bytes) EPLUS_SIZE, TASK_PRIVILEGE);
#endif /* !NONET */
#endif /* AM_KERNEL */

  /* Build main TSS.
   * This is used only to record the stack pointer to be used after an
   * interrupt.
   * The pointer is set up so that an interrupt automatically saves the
   * current process's registers ip:cs:f:sp:ss in the correct slots in the
   * process table.
   */
  tss.ss0 = DS_SELECTOR;
  init_dataseg(&gdt[TSS_INDEX], data_base + (phys_bytes) (vir_bytes) &tss,
	       (phys_bytes) sizeof tss, INTR_PRIVILEGE);
  gdt[TSS_INDEX].access = PRESENT | (INTR_PRIVILEGE << DPL_SHIFT) | TSS_TYPE;

  /* Build descriptors for interrupt gates in IDT. */
  for (gtp = &gate_table[0];
       gtp < &gate_table[sizeof gate_table / sizeof gate_table[0]]; ++gtp) {
	int_gate(gtp->vec_nr, (phys_bytes) (vir_bytes) gtp->gate,
		 PRESENT | INT_GATE_TYPE | (gtp->privilege << DPL_SHIFT));
  }
  int_gate(SYS_VECTOR, (phys_bytes) (vir_bytes) p_s_call,
	   PRESENT | (USER_PRIVILEGE << DPL_SHIFT) | INT_GATE_TYPE);

#if INTEL_32BITS
  /* Build 16-bit code segment for debugger.  Debugger runs mainly in 16-bit
   * mode but it is inconvenient to switch directly.
   */
  init_codeseg(&gdt[DB_CS16_INDEX], hclick_to_physb(break_vector.selector),
	       (phys_bytes) MAX_286_SEG_SIZE, INTR_PRIVILEGE);
  gdt[DB_CS16_INDEX].granularity &= ~DEFAULT;

  /* Complete building of main TSS. */
  tss.iobase = sizeof tss;	/* empty i/o permissions map */

  /* Build flat data segment for functions in klib386. */
  init_dataseg(&gdt[FLAT_DS_INDEX], (phys_bytes) 0, (phys_bytes) 0,
	       TASK_PRIVILEGE);

  /* Complete building of interrupt gates. */
  int_gate(SYS386_VECTOR, (phys_bytes) (vir_bytes) s_call,
	   PRESENT | (USER_PRIVILEGE << DPL_SHIFT) | INT_GATE_TYPE);
#endif

  /* Fix up debugger selectors, now the real-mode values are finished with. */
  break_vector.selector = DB_CS_SELECTOR;
  sstep_vector.selector = DB_CS_SELECTOR;
}
