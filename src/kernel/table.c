/* The object file of "table.c" contains all the data.  In the *.h files, 
 * declared variables appear with EXTERN in front of them, as in
 *
 *    EXTERN int x;
 *
 * Normally EXTERN is defined as extern, so when they are included in another
 * file, no storage is allocated.  If the EXTERN were not present, but just
 * say,
 *
 *    int x;
 *
 * then including this file in several source files would cause 'x' to be
 * declared several times.  While some linkers accept this, others do not,
 * so they are declared extern when included normally.  However, it must
 * be declared for real somewhere.  That is done here, by redefining
 * EXTERN as the null string, so the inclusion of all the *.h files in
 * table.c actually generates storage for them.  All the initialized
 * variables are also declared here, since
 *
 * extern int x = 4;
 *
 * is not allowed.  If such variables are shared, they must also be declared
 * in one of the *.h files without the initialization.
 */

#define _TABLE

#include "kernel.h"
#include <minix/com.h>
#include "proc.h"
#include "tty.h"

FORWARD _PROTOTYPE( void ___dummy, (void) );

/* The startup routine of each task is given below, from -NR_TASKS upwards.
 * The order of the names here MUST agree with the numerical values assigned to
 * the tasks in <minix/com.h>.
 */
#define SMALL_STACK	(128 * sizeof (char *))

#define	TTY_STACK	(3 * SMALL_STACK)
#define SYN_ALRM_STACK	SMALL_STACK

#if NETWORKING_ENABLED
#define EHW_STACK	SMALL_STACK
#else
#define EHW_STACK	0
#endif

#if (CHIP == INTEL)
#define	IDLE_STACK	(3 * 2 + 3 * 2 + 4 * 2)	/* 3 intr, 3 temps, 4 db */
#else
#define IDLE_STACK	SMALL_STACK
#endif

#define	PRINTER_STACK	SMALL_STACK

#if (CHIP == INTEL)
#define	WINCH_STACK	SMALL_STACK
#else
#define	WINCH_STACK	(2 * SMALL_STACK)
#endif
#if (MACHINE == ATARI)
#define	SCSI_STACK	(2 * SMALL_STACK)
#else
#define	SCSI_STACK	0
#endif

#define	FLOP_STACK	(3 * SMALL_STACK)
#define	MEM_STACK	SMALL_STACK
#define	CLOCK_STACK	SMALL_STACK
#define	SYS_STACK	SMALL_STACK
#define	HARDWARE_STACK	0		/* dummy task, uses kernel stack */



#define	TOT_STACK_SPACE		(TTY_STACK + EHW_STACK + \
	SYN_ALRM_STACK + IDLE_STACK + HARDWARE_STACK + PRINTER_STACK + \
	WINCH_STACK + FLOP_STACK + MEM_STACK + CLOCK_STACK + SYS_STACK + \
	SCSI_STACK)

/*
 * Some notes about the following table:
 *  1) The tty_task should always be first so that other tasks can use printf
 *     if their initialisation has problems.
 *  2) If you add a new kernel task, add it before the printer task.
 *  3) The task name is used for process status (F1 key) and must be six (6)
 *     characters in length.  Pad it with blanks if it is too short.
 */

PUBLIC struct tasktab tasktab[] = {
	tty_task,		TTY_STACK,	"TTY   ",
#if (MACHINE == ATARI)
	scsi_task,		SCSI_STACK,	"SCSI  ",
#endif
#if NETWORKING_ENABLED
	ehw_task,		EHW_STACK,	"DL_ETH",
#endif
	syn_alrm_task,		SYN_ALRM_STACK, "SYN_AL",
	idle_task,		IDLE_STACK,	"IDLE  ",
	printer_task,		PRINTER_STACK,	"PRINTR",
	winchester_task,	WINCH_STACK,	"WINCHE",
	floppy_task,		FLOP_STACK,	"FLOPPY",
	mem_task,		MEM_STACK,	"RAMDSK",
	clock_task,		CLOCK_STACK,	"CLOCK ",
	sys_task,		SYS_STACK,	"SYS   ",
	0,			HARDWARE_STACK,	"HARDWA",
	0,			0,		"MM    ",
	0,			0,		"FS    ",
#if NETWORKING_ENABLED
	0,			0,		"NW_TSK",
#endif
	0,			0,		"INIT  "
};

PUBLIC char t_stack[TOT_STACK_SPACE + ALIGNMENT - 1];	/* to be aligned */

/*
 * The number of kernel tasks must be the same as NR_TASKS.
 * If NR_TASKS is not correct then you will get the compile error:
 *   multiple case entry for value 0
 * The function ___dummy is never called.
 */

#define NKT (sizeof tasktab / sizeof (struct tasktab) - (INIT_PROC_NR + 1))
PRIVATE void ___dummy()
{
	switch(0)
	{
	case 0:
	case (NR_TASKS == NKT):
		;
	}
}
