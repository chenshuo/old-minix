/* This file contains a driver for the IBM-AT winchester controller.
 * It was written by Adri Koppes.
 *
 * The driver supports the following operations (using message format m2):
 *
 *    m_type      DEVICE    PROC_NR     COUNT    POSITION  ADRRESS
 * ----------------------------------------------------------------
 * |  DEV_OPEN  |         |         |         |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_CLOSE |         |         |         |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_READ  | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_WRITE | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * |SCATTERED_IO| device  | proc nr | requests|         | iov ptr |
 * ----------------------------------------------------------------
 *
 * The file contains one entry point:
 *
 *   winchester_task:	main entry when system is brought up
 *
 */

#include "kernel.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/partition.h>

/* I/O Ports used by winchester disk controller. */

#define WIN_REG1       0x1f0
#define WIN_REG2       0x1f1
#define WIN_REG3       0x1f2
#define WIN_REG4       0x1f3
#define WIN_REG5       0x1f4
#define WIN_REG6       0x1f5
#define WIN_REG7       0x1f6
#define WIN_REG8       0x1f7
#define WIN_REG9       0x3f6

/* Winchester disk controller command bytes. */
#define WIN_FORMAT      0x50	/* command for the drive to format track */
#define WIN_RECALIBRATE	0x10	/* command for the drive to recalibrate */
#define WIN_READ        0x20	/* command for the drive to read */
#define WIN_WRITE       0x30	/* command for the drive to write */
#define WIN_SPECIFY     0x91	/* command for the controller: accept params */

/* Parameters for the disk drive. */
#define SECTOR_SIZE      512	/* physical sector size in bytes */

/* Error codes */
#define ERR		  -1	/* general error */

/* Miscellaneous. */
#define MAX_ERRORS         4	/* how often to try rd/wt before quitting */
#define MAX_DRIVES         2	/* this driver supports 2 drives (hd0 - hd9)*/
#define NR_DEVICES      (MAX_DRIVES * DEV_PER_DRIVE)
#define MAX_WIN_RETRY  10000	/* max # times to try to output to WIN */
#define DEV_PER_DRIVE   (1 + NR_PARTITIONS)	/* whole drive & each partn */

/* Variables. */
PRIVATE struct wini {		/* main drive struct, one entry per drive */
  int wn_opcode;		/* DEV_READ or DEV_WRITE */
  int wn_procnr;		/* which proc wanted this operation? */
  int wn_drive;			/* drive number addressed */
  int wn_cylinder;		/* cylinder number addressed */
  int wn_sector;		/* sector addressed */
  int wn_head;			/* head number addressed */
  int wn_heads;			/* maximum number of heads */
  int wn_maxsec;		/* maximum number of sectors per track */
  int wn_ctlbyte;		/* control byte (steprate) */
  int wn_precomp;		/* write precompensation cylinder / 4 */
  long wn_low;			/* lowest cylinder of partition */
  long wn_size;			/* size of partition in blocks */
  int wn_count;			/* byte count */
  vir_bytes wn_address;		/* user virtual address */
} wini[NR_DEVICES];

PUBLIC int using_bios = FALSE;	/* this disk driver does not use the BIOS */

PRIVATE int w_need_reset = FALSE;	 /* set to 1 when controller must be reset */
PRIVATE int nr_drives;		 /* Number of drives */

PRIVATE message w_mess;		/* message buffer for in and out */

PRIVATE int command[8];		/* Common command block */

PRIVATE unsigned char buf[BLOCK_SIZE]; /* Buffer used by the startup routine */

FORWARD _PROTOTYPE( int com_out, (void) );
FORWARD _PROTOTYPE( int controller_ready, (void) );
FORWARD _PROTOTYPE( void copy_params, (unsigned char *src, struct wini *dest));
FORWARD _PROTOTYPE( void copy_prt, (int base_dev) );
FORWARD _PROTOTYPE( int drive_busy, (void) );
FORWARD _PROTOTYPE( void init_params, (void) );
FORWARD _PROTOTYPE( void sort, (struct wini wn[]) );
FORWARD _PROTOTYPE( int w_do_rdwt, (message *m_ptr) );
FORWARD _PROTOTYPE( int w_reset, (void) );
FORWARD _PROTOTYPE( int w_transfer, (struct wini *wn) );
FORWARD _PROTOTYPE( int win_init, (void) );
FORWARD _PROTOTYPE( int win_results, (void) );

/*===========================================================================*
 *				winchester_task				     * 
 *===========================================================================*/
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
		printf("winchester task got message from %d ",w_mess.m_source);
		continue;
	}
	caller = w_mess.m_source;
	proc_nr = w_mess.PROC_NR;

	/* Now carry out the work. */
	switch(w_mess.m_type) {
	    case DEV_OPEN:	r = OK;				  break;
	    case DEV_CLOSE:	r = OK;				  break;

	    case DEV_READ:
	    case DEV_WRITE:	r = w_do_rdwt(&w_mess);		  break;

	    case SCATTERED_IO:	r = do_vrdwt(&w_mess, w_do_rdwt); break;
	    default:		r = EINVAL;			  break;
	}

	/* Finally, prepare and send the reply message. */
	w_mess.m_type = TASK_REPLY;	
	w_mess.REP_PROC_NR = proc_nr;

	w_mess.REP_STATUS = r;	/* # of bytes transferred or error code */
	send(caller, &w_mess);	/* send reply to caller */
  }
}


/*===========================================================================*
 *				w_do_rdwt				     * 
 *===========================================================================*/
PRIVATE int w_do_rdwt(m_ptr)
message *m_ptr;			/* pointer to read or write w_message */
{
/* Carry out a read or write request from the disk. */
  register struct wini *wn;
  int r, device, errors = 0;
  long sector, s;

  /* Decode the w_message parameters. */
  device = m_ptr->DEVICE;
  if (device < 0 || device >= NR_DEVICES)
	return(EIO);
  if (m_ptr->COUNT != BLOCK_SIZE)
	return(EINVAL);
  wn = &wini[device];		/* 'wn' points to entry for this drive */
  wn->wn_drive = device/DEV_PER_DRIVE;	/* save drive number */
  if (wn->wn_drive >= nr_drives)
	return(EIO);
  wn->wn_opcode = m_ptr->m_type;	/* DEV_READ or DEV_WRITE */
  if (m_ptr->POSITION % BLOCK_SIZE != 0)
	return(EINVAL);
  s = m_ptr->POSITION;
  if (s < 0 || s > wn->wn_size * SECTOR_SIZE - BLOCK_SIZE)
	return(0);
  sector = s/SECTOR_SIZE + wn->wn_low;
  wn->wn_cylinder = sector / (wn->wn_heads * wn->wn_maxsec);
  wn->wn_sector =  (sector % wn->wn_maxsec) + 1;
  wn->wn_head = (sector % (wn->wn_heads * wn->wn_maxsec) )/wn->wn_maxsec;
  wn->wn_count = m_ptr->COUNT;
  wn->wn_address = (vir_bytes) m_ptr->ADDRESS;
  wn->wn_procnr = m_ptr->PROC_NR;

  /* This loop allows a failed operation to be repeated. */
  while (errors <= MAX_ERRORS) {
	errors++;		/* increment count once per loop cycle */
	if (errors > MAX_ERRORS)
		return(EIO);

	/* First check to see if a reset is needed. */
	if (w_need_reset) w_reset();

	/* Perform the transfer. */
	r = w_transfer(wn);
	if (r == OK) break;	/* if successful, exit loop */

  }

  return(r == OK ? BLOCK_SIZE : EIO);
}

/*===========================================================================*
 *				w_transfer				     * 
 *===========================================================================*/
PRIVATE int w_transfer(wn)
register struct wini *wn;	/* pointer to the drive struct */
{
  phys_bytes usr_buf;
  register int i;
  int r = 0;

  /* The command is issued by outputing 7 bytes to the controller chip. */

  usr_buf = numap(wn->wn_procnr, wn->wn_address, BLOCK_SIZE);
  if (usr_buf == (phys_bytes)0)
	return(ERR);
  command[0] = wn->wn_ctlbyte;
  command[1] = wn->wn_precomp;
  command[2] = BLOCK_SIZE/SECTOR_SIZE;
  command[3] = wn->wn_sector;
  command[4] = wn->wn_cylinder & 0xFF;
  command[5] = (wn->wn_cylinder >> 8) & BYTE;
  command[6] = (wn->wn_drive << 4) | wn->wn_head | 0xA0;
  command[7] = (wn->wn_opcode == DEV_READ ? WIN_READ : WIN_WRITE);

  if (com_out() != OK)
	return(ERR);

  /* Block, waiting for disk interrupt. */
  if (wn->wn_opcode == DEV_READ) {
	for (i=0; i<BLOCK_SIZE/SECTOR_SIZE; i++) {
		receive(HARDWARE, &w_mess);
		if (win_results() != OK) {
			w_need_reset = TRUE;
			return(ERR);
		}
		port_read(WIN_REG1, usr_buf, (unsigned) SECTOR_SIZE);
		usr_buf += SECTOR_SIZE;
	}
	r = OK;
  } else {
	for (i=0; i<MAX_WIN_RETRY && (r&8) == 0; i++)
		r = in_byte(WIN_REG8);
	if ((r&8) == 0) {
		w_need_reset = TRUE;
		return(ERR);
	}

	/* There will be an interrupt for each sector, except the format-track
	 * command only interrupts once.  Formatting may be done just like
	 * writing by setting command[7] = WIN_FORMAT.  There is no clean way
	 * to do this yet.
	 */
	if (command[7] == WIN_FORMAT)
		i = 1;
	else
		i = BLOCK_SIZE / SECTOR_SIZE;
	do {
		port_write(WIN_REG1, usr_buf, (unsigned) SECTOR_SIZE);
		usr_buf += SECTOR_SIZE;
		receive(HARDWARE, &w_mess);
		if (win_results() != OK) {
			w_need_reset = TRUE;
			return(ERR);
		}
	} while (--i != 0);
	r = OK;
  }

  if (r == ERR)
	w_need_reset = TRUE;
  return(r);
}

/*===========================================================================*
 *				w_reset					     * 
 *===========================================================================*/
PRIVATE int w_reset()
{
/* Issue a reset to the controller.  This is done after any catastrophe,
 * like the controller refusing to respond.
 */

  int i, r;

  /* Strobe reset bit low. */
  out_byte(WIN_REG9, 4);
  for (i = 0; i < 10; i++)
	 ;
  out_byte(WIN_REG9, wini[0].wn_ctlbyte & 0x0F);
  for (i = 0; i < MAX_WIN_RETRY && drive_busy(); i++)
	;
  if (drive_busy()) {
	printf("Winchester wouldn't reset, drive busy\n");
	return(ERR);
  }
  r = in_byte(WIN_REG2);
  if (r != 1) {
	printf("Winchester wouldn't reset, drive error\n");
	return(ERR);
  }

  /* Reset succeeded.  Tell WIN drive parameters. */

  w_need_reset = FALSE;
  return(win_init());
}

/*===========================================================================*
 *				win_init				     * 
 *===========================================================================*/
PRIVATE int win_init()
{
/* Routine to initialize the drive parameters after boot or reset */

  register int i;

  command[0] = wini[0].wn_ctlbyte;
  command[1] = wini[0].wn_precomp;
  command[2] = wini[0].wn_maxsec;
  command[4] = 0;
  command[6] = (wini[0].wn_heads - 1) | 0xA0;
  command[7] = WIN_SPECIFY;		/* Specify some parameters */

  if (com_out() != OK)	/* Output command block */
	return(ERR);

  receive(HARDWARE, &w_mess);
  if (win_results() != OK) {	/* See if controller accepted parameters */
	w_need_reset = TRUE;
	return(ERR);
  }

  if (nr_drives > 1) {
	command[0] = wini[5].wn_ctlbyte;
	command[1] = wini[5].wn_precomp;
	command[2] = wini[5].wn_maxsec;
	command[4] = 0;
	command[6] = (wini[5].wn_heads - 1) | 0xB0;
	command[7] = WIN_SPECIFY;		/* Specify some parameters */

	if (com_out() != OK)			/* Output command block */
		return(ERR);
	receive(HARDWARE, &w_mess);
	if (win_results() != OK) {  /* See if controller accepted parameters */
		w_need_reset = TRUE;
		return(ERR);
	}
  }
  for (i=0; i<nr_drives; i++) {
	command[0] = wini[i*5].wn_ctlbyte;
	command[6] = i << 4 | 0xA0;
	command[7] = WIN_RECALIBRATE;
	if (com_out() != OK)
		return(ERR);
	receive(HARDWARE, &w_mess);
	if (win_results() != OK) {
		w_need_reset = TRUE;
		return(ERR);
	}
  }
  return(OK);
}

/*===========================================================================*
 *				win_results				     *
 *===========================================================================*/
PRIVATE int win_results()
{
/* Extract results from the controller after an operation, then allow wini
 * interrupts again.
 */

  register int r;

  r = in_byte(WIN_REG8);
  if ( (r & 0x80) == 0 && (r & (0x40 | 0x20 | 0x10 | 0x01)) != (0x40 | 0x10)) {
	if (r & 0x01) in_byte(WIN_REG2);
	cim_at_wini();
	return(ERR);
  }
  cim_at_wini();
  return(OK);
}

/*==========================================================================*
 *				controller_ready			    *
 *==========================================================================*/
PRIVATE int controller_ready()
{
/* Wait until controller is ready for output; return zero if this times out. */

#define MAX_CONTROLLER_READY_RETRIES	1000	/* should calibrate this */
#define WIN_BUSY			0x80
#define WIN_OUTREADY			0x40	/* Bruce's guess based on FDC*/
#define WIN_STATUS			WIN_REG8

  register int retries;

  retries = MAX_CONTROLLER_READY_RETRIES + 1;
  while (--retries != 0 &&
	 (in_byte(WIN_STATUS) & WIN_BUSY) == WIN_BUSY)
	;			/* wait until not busy */
  return(retries);		/* nonzero if ready */
}

/*===========================================================================*
 *				drive_busy				     *
 *===========================================================================*/
PRIVATE int drive_busy()
{
/* Wait until the controller is ready to receive a command or send status */

  controller_ready();
  if ( (in_byte(WIN_REG8) & (0x80 | 0x40 | 0x10)) != (0x40 | 0x10)) {
	w_need_reset = TRUE;
	return(ERR);
  }
  return(OK);
}

/*===========================================================================*
 *				com_out					     *
 *===========================================================================*/
PRIVATE int com_out()
{
/* Output the command block to the winchester controller and return status */

  register int *commandp;
  register int port;

  if (!controller_ready()) {
	printf("Controller not ready in com_out\n");
	w_need_reset = TRUE;
	return(ERR);
  }
  out_byte(WIN_REG9, command[0]);
  for (commandp = &command[1], port = WIN_REG2; port <= WIN_REG8; ++port)
	out_byte(port, *commandp++);
  return(OK);
}

/*===========================================================================*
 *				init_params				     *
 *===========================================================================*/
PRIVATE void init_params()
{
/* This routine is called at startup to initialize the partition table,
 * the number of drives and the controller
 */
  unsigned int i, segment, offset;
  phys_bytes address;

  /* Copy the parameter vector from the saved vector table */
  offset = vec_table[2 * WINI_0_PARM_VEC];
  segment = vec_table[2 * WINI_0_PARM_VEC + 1];

  /* Calculate the address off the parameters and copy them to buf */
  address = hclick_to_physb(segment) + offset;
  phys_copy(address, umap(proc_ptr, D, (vir_bytes)buf, 16), 16L);

  /* Copy the parameters to the structures */
  copy_params(buf, &wini[0]);

  /* Copy the parameter vector from the saved vector table */
  offset = vec_table[2 * WINI_1_PARM_VEC];
  segment = vec_table[2 * WINI_1_PARM_VEC + 1];

  /* Calculate the address off the parameters and copy them to buf */
  address = hclick_to_physb(segment) + offset;
  phys_copy(address, umap(proc_ptr, D, (vir_bytes)buf, 16), 16L);

  /* Copy the parameters to the structures */
  copy_params(buf, &wini[5]);

  /* Get the nummer of drives from the bios */
  phys_copy(0x475L, umap(proc_ptr, D, (vir_bytes)buf, 1), 1L);
  nr_drives = (int) *buf;
  if (nr_drives > MAX_DRIVES) nr_drives = MAX_DRIVES;

  /* Set the parameters in the drive structure */
  wini[0].wn_low = wini[5].wn_low = 0L;

  /* Initialize the controller */
  cim_at_wini();		/* ready for AT wini interrupts */
  if (nr_drives > 0 && win_init() != OK && w_reset() != OK) nr_drives = 0;

  /* Read the partition table for each drive and save them */
  for (i = 0; i < nr_drives; i++) {
	w_mess.DEVICE = i * 5;
	w_mess.POSITION = 0L;
	w_mess.COUNT = BLOCK_SIZE;
	w_mess.ADDRESS = (char *) buf;
	w_mess.PROC_NR = WINCHESTER;
	w_mess.m_type = DEV_READ;
	if (w_do_rdwt(&w_mess) != BLOCK_SIZE) {
		printf("Can't read partition table on winchester %d\n",i);
		milli_delay(20000);
		continue;
	}
	if (buf[510] != 0x55 || buf[511] != 0xAA) {
		printf("Invalid partition table on winchester %d\n",i);
		milli_delay(20000);
		continue;
	}
	copy_prt((int)i*5);
  }
}

/*===========================================================================*
 *				copy_params				     *
 *===========================================================================*/
PRIVATE void copy_params(src, dest)
register unsigned char *src;
register struct wini *dest;
{
/* This routine copies the parameters from src to dest
 * and sets the parameters for partition 0 and 5
*/
  register int i;
  long cyl, heads, sectors;

  for (i=0; i<5; i++) {
	dest[i].wn_heads = (int)src[2];
	dest[i].wn_precomp = *(u16_t *)&src[5] >> 2;
	dest[i].wn_ctlbyte = (int)src[8];
	dest[i].wn_maxsec = (int)src[14];
  }
  cyl = (long)(*(u16_t *)src);
  heads = (long)dest[0].wn_heads;
  sectors = (long)dest[0].wn_maxsec;
  dest[0].wn_size = cyl * heads * sectors;
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
