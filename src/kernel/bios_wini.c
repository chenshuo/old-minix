/* This file contains a hard disk driver that uses the ROM BIOS.  It makes
 * a call and just waits for the transfer to happen.  It is not interrupt
 * driven and thus will have poor performance.  The advantage is that it should
 * work on virtually any PC, XT, 386, PS/2 or clone.  The generic boot
 * diskette uses this driver.  It is suggested that all MINIX users try the
 * other drivers, and use this one only as a last resort, if all else fails.
 * This version automatically determines the drive parameters and uses them.
 *
 * The driver supports the following operations (using message format m2):
 *
 *	m_type	  DEVICE   PROC_NR	COUNT	 POSITION  ADRRESS
 * ----------------------------------------------------------------
 * |  DISK_READ | device  | proc nr |  bytes  |	 offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * | DISK_WRITE | device  | proc nr |  bytes  |	 offset | buf ptr |
 * ----------------------------------------------------------------
 * |SCATTERED_IO| device  | proc nr | requests|         | iov ptr |
 * ----------------------------------------------------------------
 *
 * The file contains one entry point:
 *
 *	 winchester_task:	main entry when system is brought up
 *
 */

#include "kernel.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/partition.h>

/* Error codes */
#define ERR		  -1	/* general error */

/* Parameters for the disk drive. */
#define MAX_DRIVES         2	/* this driver supports 2 drives (hd0 - hd9)*/
#define DEV_PER_DRIVE   (1 + NR_PARTITIONS)	/* whole drive & each partn */
#define NR_DEVICES      (MAX_DRIVES * DEV_PER_DRIVE)
#define SECTOR_SIZE	 512	/* physical sector size in bytes */

/* BIOS parameters */
#define BIOS_ASK        0x08	/* opcode for asking BIOS for parameters */
#define BIOS_RESET      0x00	/* opcode for resetting disk BIOS */
#define BIOS_READ       0x02	/* opcode for BIOS read */
#define BIOS_WRITE      0x03	/* opcode for BIOS write */
#define DRIVE           0x80	/* BIOS code for drive 0 */

PUBLIC int using_bios = TRUE;	/* this disk driver uses the BIOS */
PRIVATE unsigned char buf[BLOCK_SIZE] = { 1 };
				/* Buffer used by the startup routine */
				/* Initialized to try to avoid DMA wrap. */
PRIVATE message w_mess;		/* message buffer for in and out */

PRIVATE struct wini {		/* main drive struct, one entry per drive */
  int wn_opcode;		/* DISK_READ or DISK_WRITE */
  int wn_procnr;		/* which proc wanted this operation? */
  int wn_cylinder;		/* cylinder number addressed */
  int wn_sector;		/* sector addressed */
  int wn_head;			/* head number addressed */
  int wn_heads;			/* maximum number of heads */
  int wn_maxsec;		/* maximum number of sectors per track */
  long wn_low;			/* lowest cylinder of partition */
  long wn_size;			/* size of partition in sectors */
  int wn_count;			/* byte count */
  int wn_drive;			/* 0x80 or 0x81 */
  vir_bytes wn_address;		/* user virtual address */
} wini[NR_DEVICES];

PRIVATE struct param {
	int nr_cyl;		/* Number of cylinders */
	int nr_heads;		/* Number of heads */
	int nr_drives;		/* Number of drives on this controler */
	int nr_sectors;		/* Number of sectors per track */
} param0, param1;

PRIVATE int nr_drives;

FORWARD void copy_prt();
FORWARD void get_params();
FORWARD void init_params();
FORWARD void replace();
FORWARD void sort();
FORWARD int w_do_rdwt();

/*=========================================================================*
 *			winchester_task					   * 
 *=========================================================================*/
PUBLIC void winchester_task()
{
/* Main program of the winchester disk driver task. */

  int r, caller, proc_nr;

  /* First initialize the controller */
  init_params();

  /* Here is the main loop of the disk task.  It waits for a message, carries
   * it out, and sends a reply.
   */

  while (TRUE) {
	/* First wait for a request to read or write a disk block. */
	receive(ANY, &w_mess);	/* get a request to do some work */
	if (w_mess.m_source < 0) {
		printf("winchester task got message from %d\n",w_mess.m_source);
		continue;
	}

	caller = w_mess.m_source;
	proc_nr = w_mess.PROC_NR;

	/* Now carry out the work. */
	switch(w_mess.m_type) {
	    case DISK_READ:
	    case DISK_WRITE:	r = w_do_rdwt(&w_mess);	break;
	    case SCATTERED_IO:	r = do_vrdwt(&w_mess, w_do_rdwt); break;
	    default:		r = EINVAL;		break;
	}

	/* Finally, prepare and send the reply message. */
	w_mess.m_type = TASK_REPLY;	
	w_mess.REP_PROC_NR = proc_nr;

	w_mess.REP_STATUS = r;	/* # of bytes transferred or error code */
	send(caller, &w_mess);	/* send reply to caller */
  }
}


/*==========================================================================*
 *				w_do_rdwt				    * 
 *==========================================================================*/
PRIVATE int w_do_rdwt(m_ptr)
message *m_ptr;			/* pointer to read or write w_message */
{
/* Carry out a read or write request from the disk. */
  register struct wini *wn;
  vir_bytes vir, ct;
  unsigned locyl, hicyl, c1, c2, c3;
  int r, device;
  long sector;
  phys_bytes user_phys;

  /* Decode the w_message parameters. */
  device = m_ptr->DEVICE;	/* minor device #.  1-4 are partitions */
  if (device < 0 || device >= NR_DEVICES) return(EIO);
  if (m_ptr->COUNT != BLOCK_SIZE) return(EINVAL);
  wn = &wini[device];		/* 'wn' points to entry for this drive */

  /* Set opcode to BIOS_READ or BIOS_WRITE. Check for bad starting addr. */
  wn->wn_opcode = (m_ptr->m_type == DISK_WRITE ? BIOS_WRITE : BIOS_READ);
  if (m_ptr->POSITION % BLOCK_SIZE != 0) return(EINVAL);

  /* Calculate the physical parameters */
  sector = m_ptr->POSITION/SECTOR_SIZE;	/* relative sector within partition */
  if ((sector+BLOCK_SIZE/SECTOR_SIZE) > wn->wn_size) return(0);
  sector += wn->wn_low;		/* absolute sector number */
  wn->wn_cylinder = sector / (wn->wn_heads * wn->wn_maxsec);
  wn->wn_sector =  (sector % wn->wn_maxsec);
  wn->wn_head = (sector % (wn->wn_heads * wn->wn_maxsec) )/wn->wn_maxsec;
  wn->wn_count = m_ptr->COUNT;
  wn->wn_address = (vir_bytes) m_ptr->ADDRESS;
  wn->wn_procnr = m_ptr->PROC_NR;

  /* Do the transfer */
  vir = (vir_bytes) wn->wn_address;
  ct = (vir_bytes) wn->wn_count;
  user_phys = numap(wn->wn_procnr, vir, ct);
  Ax = (wn->wn_opcode << 8) | 2;	/* read or write 2 sectors */
  Bx = (unsigned) user_phys % HCLICK_SIZE;	/* low order 4 bits */
  Es = physb_to_hclick(user_phys);	/* high order 16 bits */
  hicyl = (wn->wn_cylinder >> 8) & 03;	/* two high-order bits */
  locyl = (wn->wn_cylinder & 0xFF);	/* 8 low-order bits */
  c1 = (locyl<<8);
  c2 = (hicyl<<6);
  c3 = ((unsigned) wn->wn_sector) + 1;
  Cx = c1 | c2 | c3;
  Dx = (wn->wn_head<<8) | wn->wn_drive;
  bios13();
  r = (Ax >> 8) & 0xFF;
  return(r == 0 ? BLOCK_SIZE : EIO);
}


/*===========================================================================*
 *				get_params				     *
 *===========================================================================*/
PRIVATE void get_params(dr, params)
int dr;
struct param *params;
{
   Dx = dr + DRIVE;
   Ax = (BIOS_ASK << 8);
   bios13();
   params->nr_heads = ((Dx >> 8) & 0xFF) + 1;
   params->nr_sectors = (Cx & 0x3F);
   params->nr_cyl = ((Cx & 0xC0) << 2) + ((Cx >> 8) & 0xFF);
   params->nr_drives = (Dx & 0xFF);
}

/*===========================================================================*
 *				init_params				     *
 *===========================================================================*/
PRIVATE void init_params()
{
/* This routine is called at startup to initialize the partition table,
 * the number of drives and the controller
*/
  unsigned int i;

  /* Give control to the BIOS interrupt handler. */
  if (pc_at) {
	replace( (AT_WINI_IRQ & 0x07) + BIOS_IRQ8_VEC, AT_WINI_VECTOR);
	cim_at_wini();		/* ready for AT wini interrupts */
  } else {
	replace( (XT_WINI_IRQ & 0x07) + BIOS_IRQ0_VEC, XT_WINI_VECTOR);
	cim_xt_wini();		/* ready for XT wini interrupts */
  }

  /* Copy the parameters to the structures */
  get_params(0, &param0);
  get_params(1, &param1);

  /* Get the number of drives */
  nr_drives = param0.nr_drives;
  if (nr_drives > MAX_DRIVES) nr_drives = MAX_DRIVES;

  /* Set the parameters in the drive structure */
  for (i = 0; i < DEV_PER_DRIVE; i++)
  {
	wini[i].wn_heads = param0.nr_heads;
	wini[i].wn_maxsec = param0.nr_sectors;
	wini[i].wn_drive = DRIVE;
  }
  wini[0].wn_low = wini[DEV_PER_DRIVE].wn_low = 0L;
  wini[0].wn_size = (long)((long)param0.nr_cyl
      * (long)param0.nr_heads * (long)param0.nr_sectors);
  for (i = DEV_PER_DRIVE; i < (2*DEV_PER_DRIVE); i++)
  {
	wini[i].wn_heads = param1.nr_heads;
	wini[i].wn_maxsec = param1.nr_sectors;
	wini[i].wn_drive = DRIVE + 1;
  }
  wini[DEV_PER_DRIVE].wn_size = (long)((long)param1.nr_cyl
       * (long)param1.nr_heads * (long)param1.nr_sectors);


  /* Read the partition table for each drive and save them */
  for (i = 0; i < nr_drives; i++) {
	w_mess.DEVICE = i * 5;
	w_mess.POSITION = 0L;
	w_mess.COUNT = BLOCK_SIZE;
	w_mess.ADDRESS = (char *) buf;
	w_mess.PROC_NR = WINCHESTER;
	w_mess.m_type = DISK_READ;
	if (w_do_rdwt(&w_mess) != BLOCK_SIZE)
		panic("Can't read partition table of winchester ", i);
	copy_prt(i * DEV_PER_DRIVE);
  }
}

/*===========================================================================*
 *				copy_prt				     *
 *===========================================================================*/
PRIVATE void copy_prt(base_dev)
int base_dev;			/* base device for drive */
{
/* This routine copies the partition table for the selected drive to
 * the variables wn_low and wn_size
 */

  register struct part_entry *pe;
  register struct wini *wn;

  for (pe = (struct part_entry *) &buf[PART_TABLE_OFF],
       wn = &wini[base_dev + 1];
       pe < ((struct part_entry *) &buf[PART_TABLE_OFF]) + NR_PARTITIONS;
       ++pe, ++wn) {
	wn->wn_low = pe->lowsec;
	wn->wn_size = pe->size;

	/* Adjust low sector to a multiple of (BLOCK_SIZE/SECTOR_SIZE) for old
	 * Minix partitions only.  We can assume the ratio is 2 and round to
	 * even, which is slightly simpler.
	 */
	if (pe->sysind == OLD_MINIX_PART && wn->wn_low & 1) {
		++wn->wn_low;
		--wn->wn_size;
	}
  }
  sort(&wini[base_dev + 1]);
}

/*===========================================================================*
 *					sort				     *
 *===========================================================================*/
PRIVATE void sort(wn)
register struct wini wn[];
{
  register int i,j;
  struct wini tmp;

  for (i = 0; i < NR_PARTITIONS; i++)
	for (j = 0; j < NR_PARTITIONS-1; j++)
		if ((wn[j].wn_low == 0 && wn[j+1].wn_low != 0) ||
		    (wn[j].wn_low > wn[j+1].wn_low && wn[j+1].wn_low != 0)) {
			tmp = wn[j];
			wn[j] = wn[j+1];
			wn[j+1] = tmp;
		}
}

/*===========================================================================*
 *				replace					     *
 *===========================================================================*/
PRIVATE void replace(from, to)
int from;			/* vector to get replacement from */
int to;				/* vector to replace */
{
/* Replace the vector 'to' in the interrupt table with its original BIOS
 * vector 'from' in vec_table (they differ since the 8259 was reprogrammed).
 * On the first call only, also restore all software interrupt vectors from
 * vec_table except the trap vectors and SYS_VECTOR.
 * (Doing it only on the first call is redundant, since this is only called
 * once! We ought to swap our vectors back just before bios_wini replies.
 * Then this routine should be made more efficient.)
 */

  phys_bytes phys_b;
  static int repl_called = FALSE;	/* set on first call of replace */
  int vec;

  phys_b = umap(proc_ptr, D, (vir_bytes) vec_table, VECTOR_BYTES);
  if (!repl_called) {
	for (vec = 16; vec < VECTOR_BYTES / 4; ++vec)
		if (vec != SYS_VECTOR &&
		    !(vec >= IRQ0_VECTOR && vec < IRQ0_VECTOR + 8) &&
		    !(vec >= IRQ8_VECTOR && vec < IRQ8_VECTOR + 8))
			phys_copy(phys_b + 4L * vec, 4L * vec, 4L);
	repl_called = TRUE;
  }
  lock();
  phys_copy(phys_b + 4L * from, 4L * to, 4L);
  unlock();
}
