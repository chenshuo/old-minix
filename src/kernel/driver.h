/* Types and constants shared between the generic and device dependent
 * device driver code.
 */

#include <minix/callnr.h>
#include <minix/com.h>
#include "proc.h"
#if (CHIP == INTEL)
#include <minix/partition.h>
#endif

/* Info about and entry points into the device dependent code. */
struct driver {
  _PROTOTYPE( char *(*dr_name), (void) );
  _PROTOTYPE( int (*dr_open), (struct driver *dp, message *m_ptr) );
  _PROTOTYPE( int (*dr_close), (struct driver *dp, message *m_ptr) );
  _PROTOTYPE( int (*dr_ioctl), (struct driver *dp, message *m_ptr) );
  _PROTOTYPE( struct device *(*dr_prepare), (int device) );
  _PROTOTYPE( int (*dr_schedule), (int proc_nr, struct iorequest_s *request) );
  _PROTOTYPE( int (*dr_finish), (void) );
  _PROTOTYPE( void (*dr_cleanup), (void) );
  _PROTOTYPE( void (*dr_geometry), (unsigned *chs) );
};

#if (CHIP == INTEL)

/* Number of bytes you can DMA before hitting a 64K boundary: */
#define dma_bytes_left(phys)    \
   ((unsigned) (sizeof(int) == 2 ? 0 : 0x10000) - (unsigned) ((phys) & 0xFFFF))

#endif /* CHIP == INTEL */

/* Base and size of a partition in bytes. */
struct device {
  unsigned long	dv_base;
  unsigned long dv_size;
};

#define NIL_DEV		((struct device *) 0)

/* Functions defined by driver.c: */
_PROTOTYPE( void driver_task, (struct driver *dr) );
_PROTOTYPE( int do_rdwt, (struct driver *dr, message *m_ptr) );
_PROTOTYPE( int do_vrdwt, (struct driver *dr, message *m_ptr) );
_PROTOTYPE( char *no_name, (void) );
_PROTOTYPE( int do_nop, (struct driver *dp, message *m_ptr) );
_PROTOTYPE( int nop_finish, (void) );
_PROTOTYPE( void nop_cleanup, (void) );
_PROTOTYPE( void clock_mess, (int ticks, watchdog_t func) );

#if (CHIP == INTEL)
_PROTOTYPE( void partition, (struct driver *dr, int device, int style) );
_PROTOTYPE( int do_diocntl, (struct driver *dr, message *m_ptr) );
#endif

/* Parameters for the disk drive. */
#define SECTOR_SIZE      512	/* physical sector size in bytes */
#define SECTOR_SHIFT       9	/* for division */
#define SECTOR_MASK      511	/* and remainder */

/* Size of the DMA buffer buffer in bytes. */
#define DMA_BUF_SIZE	(DMA_SECTORS * SECTOR_SIZE)

#if (CHIP == INTEL)
extern u8_t *tmp_buf;			/* the DMA buffer */
extern phys_bytes tmp_phys;		/* phys address of DMA buffer */
extern u16_t Ax, Bx, Cx, Dx, Es;	/* to hold registers for BIOS calls */

/* BIOS parameter table layout. */
#define bp_cylinders(t)		(* (u16_t *) (&(t)[0]))
#define bp_heads(t)		(* (u8_t *)  (&(t)[2]))
#define bp_reduced_wr(t)	(* (u16_t *) (&(t)[3]))
#define bp_precomp(t)		(* (u16_t *) (&(t)[5]))
#define bp_max_ecc(t)		(* (u8_t *)  (&(t)[7]))
#define bp_ctlbyte(t)		(* (u8_t *)  (&(t)[8]))
#define bp_landingzone(t)	(* (u16_t *) (&(t)[12]))
#define bp_sectors(t)		(* (u8_t *)  (&(t)[14]))

/* Miscellaneous. */
#define DEV_PER_DRIVE	(1 + NR_PARTITIONS)
#define MINOR_hd1a	128
#define MINOR_fd0a	(28<<2)
#define P_FLOPPY	0
#define P_PRIMARY	1
#define P_SUB		2

#else /* CHIP != INTEL */

extern u8_t tmp_buf[];			/* the DMA buffer */
extern phys_bytes tmp_phys;		/* phys address of DMA buffer */

#endif /* CHIP != INTEL */
