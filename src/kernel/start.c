/* This file contains the C startup code for Minix on Intel processors.
 * It cooperates with mpx.s to set up a good environment for main().
 *
 * This code runs in real mode for a 16 bit kernel and may have to switch
 * to protected mode for a 286.
 *
 * For a 32 bit kernel this already runs in protected mode, but the selectors
 * are still those given by the BIOS with interrupts disabled, so the
 * descriptors need to be reloaded and interrupt descriptors made.
 */

#include "kernel.h"
#include <string.h>
#include <stdlib.h>
#include <minix/boot.h>
#include "protect.h"

/* Magic BIOS addresses. */
#define MACHINE_TYPE 0xFFFFEL	/* address of machine type word */
#	define	PC_AT	0xFC	/* code in first byte for PC-AT */
#	define	PS	0xFA	/* code in first byte for PS/2 Model 30 */
#	define	PS_386	0xF8	/* code in first byte for PS/2 386 70 & 80 */
#	define	PS_50	0x04	/* code in second byte for PS/2 Model 50 */
#	define	PS_50A	0xBA	/* code in second byte on some Model 50s */
#	define	PS_50Z	0xE9	/* code in second byte for Model 50Z */
#	define	PS_60	0x05	/* code in second byte for PS/2 Model 60 */

PRIVATE char k_environ[256];	/* environment strings passed by loader */

FORWARD _PROTOTYPE( int k_atoi, (char *s) );

/*==========================================================================*
 *				cstart					    *
 *==========================================================================*/
PUBLIC void cstart(cs, ds, mcs, mds, parmoff, parmsize)
U16_t cs, ds;			/* Kernel code and data segment */
U16_t mcs, mds;			/* Monitor code and data segment */
U16_t parmoff, parmsize;	/* boot parameters offset and length */
{
/* Perform system initializations prior to calling main(). */

  register char *envp;
  u8_t mach_magic[2];
  phys_bytes mcode_base, mdata_base;
  unsigned mon_start;

  /* Record where the kernel and the monitor are. */
  code_base = seg2phys(cs);
  data_base = seg2phys(ds);
  mcode_base = seg2phys(mcs);
  mdata_base = seg2phys(mds);

  /* Initialize protected mode descriptors. */
  prot_init();

  /* Copy the boot parameters to kernel memory. */
  if (parmsize > sizeof k_environ - 2) parmsize = sizeof k_environ - 2;
  phys_copy(mdata_base + parmoff, vir2phys(k_environ), (phys_bytes) parmsize);

  /* Convert important boot environment variables. */
  envp = k_getenv("rootdev");
  if (envp != NIL_PTR) boot_parameters.bp_rootdev = k_atoi(envp);
  envp = k_getenv("ramimagedev");
  if (envp != NIL_PTR) boot_parameters.bp_ramimagedev = k_atoi(envp);
  envp = k_getenv("ramsize");
  if (envp != NIL_PTR) boot_parameters.bp_ramsize = k_atoi(envp);
  envp = k_getenv("processor");
  if (envp != NIL_PTR) boot_parameters.bp_processor = k_atoi(envp);

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
  phys_copy(MACHINE_TYPE, vir2phys(mach_magic), 2L);

  envp = k_getenv("bus");
  if (envp != NIL_PTR) {
	/* Variable "bus" overrides the machine type. */
	if (strcmp(envp, "at") == 0) pc_at = TRUE;
	if (strcmp(envp, "ps") == 0) ps = TRUE;
	if (strcmp(envp, "mca") == 0) pc_at = ps_mca = TRUE;
  } else
  if (mach_magic[0] == PC_AT) {
	pc_at = TRUE;
	/* could be a PS/2 Model 50 or 60 -- check submodel byte */
	if (mach_magic[2] == PS_50 || mach_magic[2] == PS_60)   ps_mca = TRUE;
	if (mach_magic[2] == PS_50A || mach_magic[2] == PS_50Z) ps_mca = TRUE;
  } else
  if (mach_magic[0] == PS_386) {
	pc_at = ps_mca = TRUE;
  } else
  if (mach_magic[0] == PS) {
	ps = TRUE;
  }

  /* Decide if mode is protected. */
#if _WORD_SIZE == 2
  protected_mode = processor >= 286;
#endif

  /* Is there a monitor to return to?  If so then keep it safe. */
  if (!protected_mode) mon_return = 0;
  mon_start = mcode_base / 1024;
  if (mon_return && low_memsize > mon_start) low_memsize = mon_start;

  /* Return to assembler code to switch to protected mode (if 286), reload
   * selectors and call main().
   */
}


/*==========================================================================*
 *				k_atoi					    *
 *==========================================================================*/
PRIVATE int k_atoi(s)
register char *s;
{
/* Convert string to integer. */

  return strtol(s, (char **) NULL, 10);
}


/*==========================================================================*
 *				k_getenv				    *
 *==========================================================================*/
PUBLIC char *k_getenv(name)
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
