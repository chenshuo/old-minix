/* Global variables used in the kernel. */

/* EXTERN is defined as extern except in table.c. */
#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

/* Kernel memory. */
EXTERN phys_bytes code_base;	/* base of kernel code */
EXTERN phys_bytes data_base;	/* base of kernel data */

/* Low level interrupt communications. */
EXTERN struct proc *held_head;	/* head of queue of held-up interrupts */
EXTERN struct proc *held_tail;	/* tail of queue of held-up interrupts */
EXTERN unsigned char k_reenter;	/* kernel reentry count (entry count less 1)*/

/* Process table.  Here to stop too many things having to include proc.h. */
EXTERN struct proc *proc_ptr;	/* pointer to currently running process */

/* Signals. */
EXTERN int sig_procs;		/* number of procs with p_pending != 0 */

/* Memory sizes. */
EXTERN struct memory mem[NR_MEMS];	/* base and size of chunks of memory */
EXTERN phys_clicks tot_mem_size;	/* total system memory size */

/* Miscellaneous. */
EXTERN int rebooting;		/* nonzero while rebooting */
extern u16_t sizes[8];		/* table filled in by build */
extern struct tasktab tasktab[];/* initialized in table.c, so extern here */
extern char t_stack[];		/* initialized in table.c, so extern here */

#if (CHIP == INTEL)

/* Machine type. */
EXTERN int pc_at;		/* PC-AT compatible hardware interface */
EXTERN int ps;			/* PS/2 */
EXTERN int ps_mca;		/* PS/2 with Micro Channel */
EXTERN unsigned int processor;	/* 86, 186, 286, 386, ... */
EXTERN int protected_mode;	/* nonzero if running in Intel protected mode*/

/* Debugger control. */
EXTERN struct farptr_s break_vector;	/* debugger breakpoint hook */
EXTERN int db_enabled;		/* nonzero if external debugger is enabled */
EXTERN int db_exists;		/* nonzero if external debugger exists */
EXTERN struct farptr_s sstep_vector;	/* debugger single-step hook */

/* Video cards and keyboard types. */
EXTERN int color;		/* nonzero if console is color, 0 if mono */
EXTERN int ega;			/* nonzero if console is supports EGA */
EXTERN int vga;			/* nonzero if console is supports VGA */
EXTERN int snow;		/* nonzero if screen needs snow removal */

/* Memory sizes. */
EXTERN unsigned ext_memsize;	/* initialized by assembler startup code */
EXTERN unsigned low_memsize;

/* Miscellaneous. */
EXTERN u16_t Ax, Bx, Cx, Dx, Es;	/* to hold registers for BIOS calls */
EXTERN u16_t vec_table[VECTOR_BYTES / sizeof(u16_t)]; /* copy of BIOS vectors*/
EXTERN irq_handler_t irq_table[NR_IRQ_VECTORS];

/* Variables that are initialized elsewhere are just extern here. */
extern struct segdesc_s gdt[];	/* global descriptor table for protected mode*/

EXTERN _PROTOTYPE( void (*level0_func), (void) );
#endif /* (CHIP == INTEL) */

#if (CHIP == M68000)
/* Variables that are initialized elsewhere are just extern here. */
extern int keypad;		/* Flag for keypad mode */
extern int app_mode;		/* Flag for arrow key application mode */
extern int STdebKey;		/* nonzero if ctl-alt-Fx detected */
extern struct tty_struct *cur_cons; /* virtual cons currently displayed */
extern unsigned char font8[];	/* 8 pixel wide font table (initialized) */
extern unsigned char font12[];	/* 12 pixel wide font table (initialized) */
extern unsigned char font16[];	/* 16 pixel wide font table (initialized) */
extern unsigned short resolution; /* screen res; ST_RES_LOW..TT_RES_HIGH */
#endif
