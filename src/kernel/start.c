/* This file contains the C startup code for Minix on Intel processors.
 * It cooperates with mpx.x to set up a good environment for main().
 *
 * This code runs in real mode for a 16 bit kernel and may have to switch
 * to protected mode for a 286.
 *
 * For a 32 bit kernel this already runs in protected mode, but the selectors
 * are still those given by the BIOS with interrupts disabled, so the
 * descriptors need to be reloaded and interrupt descriptors made.
 */

#include "kernel.h"
#include "protect.h"
#include "string.h"
#include <minix/boot.h>

/* Magic BIOS addresses. */
#if !INTEL_32BITS

/* 16 bit real mode. */
#define BIOS_CSEG		0xF000	/* segment of BIOS code */
#define BIOS_DSEG		0x0040	/* segment of BIOS data */
#	define BIOS_CURSOR	0x0060	/* offset to cursor type word */
#define VEC_TABLE_SEG		0x0000	/* segment of vector table */
#define MACHINE_TYPE_SEG	0xFFFF	/* segment of machine type word */
#	define MACHINE_TYPE_OFF	0x000E	/* offset of machine type word */
#else

/* 32 bit protected mode, the es selector addresses 4Gb with a zero base. */
#define BIOS_CSEG		 0xF000	/* segment of BIOS code (real mode) */
#define BIOS_DSEG	    ES_SELECTOR	/* segment of BIOS data */
#	define BIOS_CURSOR      0x00460	/* offset to cursor type word */
#define VEC_TABLE_SEG	    ES_SELECTOR	/* selector of vector table */
#define MACHINE_TYPE_SEG    ES_SELECTOR	/* selector of machine type word */
#	define MACHINE_TYPE_OFF	0xFFFFE	/* offset of machine type word */
#endif

#	define PC_AT	0xFC	/* code in first byte for PC-AT */
#	define PS	0xFA	/* code in first byte for PS/2 Model 30 */
#	define PS_386	0xF8	/* code in first byte for PS/2 386 70 & 80 */
#	define PS_50	0x04	/* code in second byte for PS/2 Model 50 */
#	define PS_50A	0xBA	/* code in second byte on some Model 50s */
#	define PS_50Z	0xE9	/* code in second byte for Model 50Z */
#	define PS_60	0x05	/* code in second byte for PS/2 Model 60 */

PRIVATE char k_environ[256];	/* environment strings passed by loader */

FORWARD _PROTOTYPE( void db_init, (void) );
FORWARD _PROTOTYPE( int k_atoi, (char *s) );
FORWARD _PROTOTYPE( char *k_getenv, (char *name) );
FORWARD _PROTOTYPE( void rel_vec, (unsigned vec4) );

/*==========================================================================*
 *				cstart					    *
 *==========================================================================*/
PUBLIC void cstart(cs, ds, parmoff, parmseg, parmsize)
u16_t cs, ds;			/* Kernel code and data segment. */
char *parmoff;			/* boot parameters offset */
u16_t parmseg;			/* boot parameters segment */
size_t parmsize;		/* boot parameters length */
{
/* Perform initializations which must be done in real mode. */

  register u16_t *bootp;
  register char *envp;
  unsigned machine_magic;
  unsigned mach_submagic;

  /* Record where the kernel is. */
#if INTEL_32BITS
  register struct segdesc_s *gdtp;

  /* Get the code and data base from the gdt set up by the bootstrap. */
  gdtp = &gdt[cs / sizeof(gdt[0])];
  code_base = gdtp->base_low | ((u32_t) gdtp->base_middle << 16)
				| ((u32_t) gdtp->base_high << 24);
  gdtp = &gdt[ds / sizeof(gdt[0])];
  data_base = gdtp->base_low | ((u32_t) gdtp->base_middle << 16)
				| ((u32_t) gdtp->base_high << 24);
#else
  /* Get the code and data base from the real mode segment registers. */
  code_base = hclick_to_physb(cs);
  data_base = hclick_to_physb(ds);
#endif

  /* Copy the boot parameters to kernel memory. */
  if (parmsize > sizeof k_environ - 2) parmsize = sizeof k_environ - 2;
  parmsize /= 2;	/* word count */
  for (bootp = (u16_t *) &k_environ[0]; parmsize != 0; parmsize--, parmoff += 2)
	*bootp++ = get_word(parmseg, (u16_t *) parmoff);

  /* Just convert environment to boot parameters for now. */
  envp = k_getenv("rootdev");
  if (envp != NIL_PTR) boot_parameters.bp_rootdev = k_atoi(envp);
  envp = k_getenv("ramimagedev");
  if (envp != NIL_PTR) boot_parameters.bp_ramimagedev = k_atoi(envp);
  envp = k_getenv("ramsize");
  if (envp != NIL_PTR) boot_parameters.bp_ramsize = k_atoi(envp);
  envp = k_getenv("scancode");
  if (envp != NIL_PTR) boot_parameters.bp_scancode = k_atoi(envp);
  envp = k_getenv("processor");
  if (envp != NIL_PTR) boot_parameters.bp_processor = k_atoi(envp);

  scan_code = boot_parameters.bp_scancode;

  /* Type of VDU: */
  envp = k_getenv("chrome");
  color = envp != NIL_PTR && strcmp(envp, "color") == 0;
  envp = k_getenv("video");
  if (envp != NIL_PTR) {
	if (strcmp(envp, "ega") == 0) ega = TRUE;
	if (strcmp(envp, "vga") == 0) vga = ega = TRUE;
  }

  /* Memory sizes: */
  envp = k_getenv("memsize");
  low_memsize = envp != NIL_PTR ? k_atoi(envp) : 256;
  envp = k_getenv("emssize");
  ext_memsize = envp != NIL_PTR ? k_atoi(envp) : 0;

  /* Determine machine type. */
  processor = boot_parameters.bp_processor;	/* 86, 186, 286, 386, ... */
  machine_magic = get_word(MACHINE_TYPE_SEG, (u16_t *) MACHINE_TYPE_OFF);
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
#if INTEL_32BITS
  protected_mode = TRUE;
#else
  protected_mode = processor >= 286 && !using_bios;
#endif
  boot_parameters.bp_processor = protected_mode;	/* FS needs to know */

  /* Prepare for relocation of the vector table.  It may contain pointers into
   * itself.  Fix up only the ones used.
   */
  rel_vec(WINI_0_PARM_VEC * 4);
  rel_vec(WINI_1_PARM_VEC * 4);

  /* Initialize debugger (requires 'protected_mode' to be initialized). */
  db_init();

  /* Call stage 1 assembler hooks to begin machine/mode specific inits. */
  mpx_1hook();
  klib_1hook();

  /* Call main() and never return if not protected mode. */
  if (!protected_mode) main();

  /* Initialize protected mode (requires 'break_vector' and other globals). */
  prot_init();

  /* Return to assembler code to switch modes or just reload selectors. */
}


/*==========================================================================*
 *				db_init					    *
 *==========================================================================*/
PRIVATE void db_init()
{
/* Initialize vectors for external debugger. */

  break_vector.offset = get_word(VEC_TABLE_SEG,
  					(u16_t *) (BREAKPOINT_VECTOR * 4));
  break_vector.selector = get_word(VEC_TABLE_SEG,
  					(u16_t *) (BREAKPOINT_VECTOR * 4) + 1);
  sstep_vector.offset = get_word(VEC_TABLE_SEG,
  					(u16_t *) (DEBUG_VECTOR * 4));
  sstep_vector.selector = get_word(VEC_TABLE_SEG,
  					(u16_t *) (DEBUG_VECTOR * 4) + 1);

  /* No debugger if the breakpoint vector points into the BIOS. */
  if (break_vector.selector >= BIOS_CSEG) return;

  /* Debugger vectors for protected mode are by convention stored as 16-bit
   * offsets in the debugger code segment, just before the corresponding real
   * mode entry points.
   */
  if (protected_mode) {
#if !INTEL_32BITS
	break_vector.offset = get_word(break_vector.selector,
					(u16_t *) break_vector.offset - 1);
	sstep_vector.offset = get_word(sstep_vector.selector,
					(u16_t *) sstep_vector.offset - 1);
#else
	/* Can only poke around in 386 memory using the es selector. */
	break_vector.offset = get_word(ES_SELECTOR, (u16_t *)
	    (hclick_to_physb(break_vector.selector) + break_vector.offset) - 1);
	sstep_vector.offset = get_word(ES_SELECTOR, (u16_t *)
	    (hclick_to_physb(sstep_vector.selector) + sstep_vector.offset) - 1);
#endif

	/* Different code segments are not supported. */
	if (break_vector.selector != sstep_vector.selector)
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
  put_word(BIOS_DSEG, (u16_t *) BIOS_CURSOR, CURSOR_SHAPE);
}


/*==========================================================================*
 *				k_atoi					    *
 *==========================================================================*/
PRIVATE int k_atoi(s)
register char *s;
{
/* Convert string to integer - kernel version to avoid bloat from isspace().
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
	for (namep = name; *namep != 0 && *namep == *envp; namep++, envp++)
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
PRIVATE void rel_vec(vec4)
unsigned vec4;			/* vector to be relocated (times 4) */
{
/* If the vector 'vec4' points into the vector table, relocate it to the copy
 * of the vector table.
 */

  phys_bytes address;
  u16_t off;
  u16_t seg;

  off = get_word(VEC_TABLE_SEG, (u16_t *) vec4);
  seg = get_word(VEC_TABLE_SEG, (u16_t *) vec4 + 1);
  address = hclick_to_physb(seg) + off;
  if (address != 0 && address < VECTOR_BYTES) {
	address += data_base + (vir_bytes) vec_table;
	seg = physb_to_hclick(address);
	off = address - hclick_to_physb(seg);
	put_word(VEC_TABLE_SEG, (u16_t *) vec4, off);
	put_word(VEC_TABLE_SEG, (u16_t *) vec4 + 1, seg);
  }
}
