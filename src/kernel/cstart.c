/* This file contains the C startup code for Minix on Intel processors.
 * It cooperates with start.x to set up a good environment for main().
 *
 * The code must run in 16-bit mode (and be compiled with a 16-bit
 * compiler!) even for a 32-bit kernel.
 * So care must be taken to avoid accessing data structures (such as the
 * process table) which depend on type sizes, and to avoid calling functions
 * except those in the companion files start.x and protect.c, and the stage
 * 1 assembler hooks.
 * Also, variables beyond 64K must not be accessed - this is guaranteed
 * by keeping the kernel small.
 * This is not so easy when the 32-bit compiler does not support separate
 * I&D, since the kernel is already larger than 64K.
 * The order of the objects in the makefile is chosen so all variables
 * accessed from here are early in the list, and the linker is relied on
 * not to alter the order.
 * It might be better to separately-compile start.x, cstart.c and protect.c,
 * and pass the results to main() in a big structure.
 */

#include "kernel.h"
#include <minix/boot.h>

/* Magic BIOS addresses. */
#define BIOS_CSEG		0xF000	/* segment of BIOS code */
#define BIOS_DSEG		0x0040	/* segment of BIOS data */
#	define BIOS_CURSOR	0x0060	/* offset to cursor type word */
#define MACHINE_TYPE_SEG	0xFFFF	/* segment of machine type word */
#	define MACHINE_TYPE_OFF	0x000E	/* offset of machine type word */
#	define PC_AT	0xFC	/* code in first byte for PC-AT */
#	define PS	0xFA	/* code in first byte for PS/2 Model 30 */
#	define PS_386	0xF8	/* code in first byte for PS/2 386 70 & 80 */
#	define PS_50	0x04	/* code in second byte for PS/2 Model 50 */
#	define PS_50A	0xBA	/* code in second byte on some Model 50s */
#	define PS_50Z	0xE9	/* code in second byte for Model 50Z */
#	define PS_60	0x05	/* code in second byte for PS/2 Model 60 */

PRIVATE char k_environ[128];	/* environment strings passed by loader */

FORWARD void db_init();
FORWARD int k_atoi();
FORWARD char *k_getenv();
FORWARD void rel_vec();

/*==========================================================================*
 *				cstart					    *
 *==========================================================================*/
PUBLIC void cstart(ax, bx, cx, dx, si, di, cs, ds)
u16_t ax;			/* boot code (registers from boot loader) */
u16_t bx;			/* scan code */
u16_t cx;			/* amount of boot parameters in bytes */
u16_t dx;			/* not used */
u16_t si;			/* offset of boot parameters in loader */
u16_t di;			/* segment of boot parameters in loader  */
u16_t cs;			/* real mode kernel code segment */
u16_t ds;			/* real mode kernel data segment */
{
/* Perform initializations which must be done in real mode. */

  register u16_t *bootp;
  register char *envp;
  unsigned machine_magic;
  unsigned mach_submagic;

  /* Record where the kernel is. */
  code_base = hclick_to_physb(cs);
  data_base = hclick_to_physb(ds);

  /* Copy the boot parameters, if any, to kernel memory. */
  if (ax == 0) {
	/* New boot loader (ax == bx == scan code for old one). */
	if (cx > sizeof boot_parameters) cx = sizeof boot_parameters;
	cx /= 2;		/* word count */
	for (bootp = (u16_t *) &boot_parameters; cx != 0; cx--, si += 2)
		*bootp++ = get_word(di, si);
	bx = boot_parameters.bp_scancode;
  }
  else if (ax == 0x100) {
	/* Newer boot loader passes environment string. */
	if (cx > sizeof k_environ - 2) cx = sizeof k_environ - 2;
	cx /= 2;		/* word count */
	for (bootp = (u16_t *) &k_environ[0]; cx != 0; cx--, si += 2)
		*bootp++ = get_word(di, si);

	/* Just convert environment to boot parameters for now. */
	envp = k_getenv("rootdev");
	if (envp != NIL_PTR) boot_parameters.bp_rootdev = k_atoi(envp);
	envp = k_getenv("ramimagedev");
	if (envp != NIL_PTR) boot_parameters.bp_ramimagedev = k_atoi(envp);
	envp = k_getenv("ramsize");
	if (envp != NIL_PTR) boot_parameters.bp_ramsize = k_atoi(envp);
	envp = k_getenv("scancode");
	if (envp != NIL_PTR) bx = boot_parameters.bp_scancode = k_atoi(envp);
	envp = k_getenv("processor");
	if (envp != NIL_PTR) boot_parameters.bp_processor = k_atoi(envp);
  }
  scan_code = bx;

  /* Get information from the BIOS. */
  color = get_chrome();
  ega = get_ega();
  ext_memsize = get_ext_memsize();
  low_memsize = get_low_memsize();

  /* Determine machine type. */
  processor = get_processor();	/* 86, 186, 286 or 386 */
  machine_magic = get_word(MACHINE_TYPE_SEG, MACHINE_TYPE_OFF);
  mach_submagic = (machine_magic >> 8) & BYTE;
  machine_magic &= BYTE;
  if (machine_magic == PC_AT) {
	pc_at = TRUE;
	/* could be a PS/2 Model 50 or 60 -- check submodel byte */
	if (mach_submagic == PS_50 || mach_submagic == PS_60)   ps_mca = TRUE;
	if (mach_submagic == PS_50A || mach_submagic == PS_50Z) ps_mca = TRUE;
  } else if (machine_magic == PS_386)
	pc_at = ps_mca = TRUE;
  else if (machine_magic == PS)
	ps = TRUE;

  /* Decide if mode is protected. */
  if (processor >= 286 && boot_parameters.bp_processor >= 286 && !using_bios) {
	protected_mode = TRUE;
  }
  boot_parameters.bp_processor = protected_mode;	/* FS needs to know */

  /* Prepare for relocation of the vector table.  It may contain pointers into
   * itself.  Fix up only the ones used.
   */
  rel_vec(WINI_0_PARM_VEC * 4, ds);
  rel_vec(WINI_1_PARM_VEC * 4, ds);

  /* Initialize debugger (requires 'protected_mode' to be initialized). */
  db_init();

  /* Call stage 1 assembler hooks to begin machine/mode specific inits. */
  mpx_1hook(); 
  klib_1hook(); 

  /* Call main() and never return if not protected mode. */
  if (!protected_mode) main();

  /* Initialize protected mode (requires 'break_vector' and other globals). */
  prot_init();

  /* Return to assembler code to switch modes. */
}


/*==========================================================================*
 *				db_init					    *
 *==========================================================================*/
PRIVATE void db_init()
{
/* Initialize vectors for external debugger. */

  break_vector.offset = get_word(VEC_TABLE_SEG, BREAKPOINT_VECTOR * 4);
  break_vector.selector = get_word(VEC_TABLE_SEG, BREAKPOINT_VECTOR * 4 + 2);
  sstep_vector.offset = get_word(VEC_TABLE_SEG, DEBUG_VECTOR * 4);
  sstep_vector.selector = get_word(VEC_TABLE_SEG, DEBUG_VECTOR * 4 + 2);

  /* No debugger if the breakpoint vector points into the BIOS. */
  if ((u16_t) break_vector.selector >= BIOS_CSEG) return;

  /* Debugger vectors for protected mode are by convention stored as 16-bit
   * offsets in the debugger code segment, just before the corresponding real
   * mode entry points.
   */
  if (protected_mode) {
	break_vector.offset = get_word((u16_t) break_vector.selector,
				       (u16_t) break_vector.offset - 2);
	sstep_vector.offset = get_word((u16_t) sstep_vector.selector,
				       (u16_t) sstep_vector.offset - 2);

	/* Different code segments are not supported. */
	if ((u16_t) break_vector.selector != (u16_t) sstep_vector.selector)
		return;
  }

  /* Enable debugger. */
  db_exists = TRUE;
  db_enabled = TRUE;

  /* Tell debugger about Minix's normal cursor shape via the BIOS.  It would
   * be nice to tell it the variable video parameters, but too hard.  At least
   * the others get refreshed by the console driver.  Blame the 6845's
   * read-only registers.
   */
  put_word(BIOS_DSEG, BIOS_CURSOR, CURSOR_SHAPE);
}


/*==========================================================================*
 *				k_atoi					    *
 *==========================================================================*/
PRIVATE int k_atoi(s)
register char *s;
{
/* Convert string to integer - kernel version of atoi to make sure it is in
 * 16-bit mode and to avoid bloat from isspace().
 */

  register int total = 0;
  register unsigned digit;
  register minus = 0;

  while (*s == ' ' || *s == '\t') s++;
  if (*s == '-') {
	s++;
	minus = 1;
  }
  while ((digit = *s++ - '0') < 10) {
	total *= 10;
	total += digit;
  }
  return(minus ? -total : total);
}


/*==========================================================================*
 *				k_getenv				    *
 *==========================================================================*/
PRIVATE char *k_getenv(name)
char *name;
{
/* Get environment value - kernel version of getenv to avoid setting up the
 * usual environment array.
 */

  register char *namep;
  register char *envp;

  for (envp = k_environ; *envp != 0;) {
	for (namep = name; *namep != 0 && *namep++ == *envp++;)
		;
	if (*namep == '\0' && *envp == '=') return(envp + 1);
	while (*envp++ != 0)
		;
  }
  return(NIL_PTR);
}


/*==========================================================================*
 *				rel_vec					    *
 *==========================================================================*/
PRIVATE void rel_vec(vec4, ds)
unsigned vec4;			/* vector to be relocated (times 4) */
u16_t ds;			/* real mode kernel data segment */
{
/* If the vector 'vec4' points into the vector table, relocate it to the copy
 * of the vector table.
 */

  phys_bytes address;
  unsigned off;
  unsigned seg;

  off = get_word(VEC_TABLE_SEG, vec4);
  seg = get_word(VEC_TABLE_SEG, vec4 + 2);
  address = hclick_to_physb(seg) + off;
  if (address != 0 && address < VECTOR_BYTES) {
	put_word(VEC_TABLE_SEG, vec4, off + (unsigned) vec_table);
	put_word(VEC_TABLE_SEG, vec4 + 2, seg + ds);
  }
}
