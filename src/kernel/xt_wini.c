/* This file contains a driver for the WD winchester controller from 
 * Western Digital (WX-2 and related controllers).
 *
 * Original code written by Adri Koppes.
 * Patches from Gary Oliver for use with the Western Digital WX-2.
 * Patches from Harry McGavran for robust operation on turbo clones.
 * Patches from Mike Mitchell for WX-2 auto configure operation.
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

#define MAX_DRIVES         2	/* only one supported (hd0 - hd4) */

/* I/O Ports used by winchester disk task. */
#define WIN_DATA       0x320	/* winchester disk controller data register */
#define WIN_STATUS     0x321	/* winchester disk controller status register */
#define WST_REQ	       0x001	/* Request bit */
#define WST_INPUT      0x002	/* Set if controller is writing to cpu */
#define WST_BUS	       0x004	/* Command/status bit */
#define WST_BUSY       0x008	/* Busy */
#define WST_INTERRUPT  0x020	/* Interrupt generated ?? */
#define WIN_SELECT     0x322	/* winchester disk controller select port */
#define WIN_DMA	       0x323	/* winchester disk controller dma register */
#define DMA_ADDR       0x006	/* port for low 16 bits of DMA address */
#define DMA_TOP	       0x082	/* port for top 4 bits of 20-bit DMA addr */
#define DMA_COUNT      0x007	/* port for DMA count (count =	bytes - 1) */
#define DMA_M2	       0x00C	/* DMA status port */
#define DMA_M1	       0x00B	/* DMA status port */
#define DMA_INIT       0x00A	/* DMA init port */

/* Winchester disk controller command bytes. */
#define WIN_RECALIBRATE	0x01	/* command for the drive to recalibrate */
#define WIN_SENSE	0x03	/* command for the controller to get its status */
#define WIN_READ	0x08	/* command for the drive to read */
#define WIN_WRITE	0x0a	/* command for the drive to write */
#define WIN_SPECIFY	0x0C	/* command for the controller to accept params	*/
#define WIN_ECC_READ	0x0D	/* command for the controller to read ecc length */

#define DMA_INT		   3 /* Command with dma and interrupt */
#define INT		   2	/* Command with interrupt, no dma */
#define NO_DMA_INT	   0	/* Command without dma and interrupt */

/* DMA channel commands. */
#define DMA_READ	0x47	/* DMA read opcode */
#define DMA_WRITE	0x4B	/* DMA write opcode */

/* Parameters for the disk drive. */
#define SECTOR_SIZE	 512	/* physical sector size in bytes */
#ifndef NR_SECTORS
/* For RLL drives NR_SECTORS has to be defined in the makefile or in config.h.
 * There is some hope of getting it from the parameter table for these drives,
 * and then this driver should use wn_maxsec like at_wini.c.
 * Unfortunately it is not standard in XT parameter tables.
 */
#define NR_SECTORS	0x11	/* number of sectors per track */
#endif

/* Error codes */
#define ERR		  -1	/* general error */

/* Miscellaneous. */
#define MAX_ERRORS	   4	/* how often to try rd/wt before quitting */
#define MAX_RESULTS	   4	/* max number of bytes controller returns */
#define DEV_PER_DRIVE   (1 + NR_PARTITIONS)	/* whole drive & each partn */
#define NR_DEVICES      (MAX_DRIVES * DEV_PER_DRIVE)
#define MAX_WIN_RETRY  32000	/* max # times to try to output to WIN */
#if AUTO_BIOS
#define AUTO_PARAM     0x1AD	/* drive parameter table starts here in sect 0	*/
#define AUTO_ENABLE	0x10	/* auto bios enabled bit from status reg */
/* some start up parameters in order to extract the drive parameter table */
/* from the winchester. these should not need changed. */
#define AUTO_CYLS	 306	/* default number of cylinders */
#define AUTO_HEADS	   4	/* default number of heads */
#define AUTO_RWC	 307	/* default reduced write cylinder */
#define AUTO_WPC	 307	/* default write precomp cylinder */
#define AUTO_ECC	  11	/* default ecc burst */
#define AUTO_CTRL	   5	/* default winchester stepping speed byte */
#endif

/* Variables. */
PUBLIC int using_bios = FALSE;	/* this disk driver does not use the BIOS */

PRIVATE struct wini {		/* main drive struct, one entry per drive */
  int wn_opcode;		/* DISK_READ or DISK_WRITE */
  int wn_procnr;		/* which proc wanted this operation? */
  int wn_drive;			/* drive number addressed (<< 5) */
  int wn_cylinder;		/* cylinder number addressed */
  int wn_sector;		/* sector addressed */
  int wn_head;			/* head number addressed */
  int wn_heads;			/* maximum number of heads */
  int wn_ctrl_byte;		/* Control byte for COMMANDS (10-Apr-87 GO) */
  long wn_low;			/* lowest cylinder of partition */
  long wn_size;			/* size of partition in blocks */
  int wn_count;			/* byte count */
  vir_bytes wn_address;		/* user virtual address */
  char wn_results[MAX_RESULTS];	/* the controller can give lots of output */
} wini[NR_DEVICES];

PRIVATE int w_need_reset = FALSE;	 /* set to 1 when controller must be reset	*/
PRIVATE int nr_drives;		 /* Number of drives */

PRIVATE message w_mess;		/* message buffer for in and out */

PRIVATE int command[6];		/* Common command block */

PRIVATE unsigned char buf[BLOCK_SIZE] = { 0 };
				/* Buffer used by the startup routine */
				/* Initialized to try to avoid DMA wrap. */

PRIVATE struct param {
	int nr_cyl;		/* Number of cylinders */
	int nr_heads;		/* Number of heads */
	int reduced_wr;		/* First cylinder with reduced write current */
	int wr_precomp;		/* First cylinder with write precompensation */
	int max_ecc;		/* Maximum ECC burst length */
	int ctrl_byte;		/* Copied control-byte from bios tables */
} param0, param1;

FORWARD int check_init();
FORWARD int com_out();
FORWARD void copy_param();
FORWARD void copy_prt();
FORWARD int  hd_wait();
FORWARD void init_params();
FORWARD int old_win_results();
FORWARD int read_ecc();
FORWARD void sort();
FORWARD int w_do_rdwt();
FORWARD void w_dma_setup();
FORWARD int w_reset();
FORWARD int w_transfer();
FORWARD void w_wait_int();
FORWARD void win_out();
FORWARD int win_specify();
FORWARD int win_results();

/*=========================================================================*
 *							winchester_task					 			   * 
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
		printf("winchester task got message from %d ", w_mess.m_source);
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
 *								w_do_rdwt						 			* 
 *==========================================================================*/
PRIVATE int w_do_rdwt(m_ptr)
message *m_ptr;			/* pointer to read or write w_message */
{
/* Carry out a read or write request from the disk. */
  register struct wini *wn;
  int r, device, errors = 0;
  long sector;

  /* Decode the w_message parameters. */
  device = m_ptr->DEVICE;
  if (device < 0 || device >= NR_DEVICES)
	return(EIO);
  if (m_ptr->COUNT != BLOCK_SIZE)
	return(EINVAL);
  wn = &wini[device];		/* 'wn' points to entry for this drive */

  wn->wn_opcode = m_ptr->m_type;	/* DISK_READ or DISK_WRITE */
  if (m_ptr->POSITION % BLOCK_SIZE != 0)
	return(EINVAL);
  sector = m_ptr->POSITION/SECTOR_SIZE;
  if ((sector+BLOCK_SIZE/SECTOR_SIZE) > wn->wn_size)
	return(0);
  sector += wn->wn_low;
  wn->wn_cylinder = sector / (wn->wn_heads * NR_SECTORS);
  wn->wn_sector =  (sector % NR_SECTORS);
  wn->wn_head = (sector % (wn->wn_heads * NR_SECTORS) )/NR_SECTORS;
  wn->wn_count = m_ptr->COUNT;
  wn->wn_address = (vir_bytes) m_ptr->ADDRESS;
  wn->wn_procnr = m_ptr->PROC_NR;

  /* This loop allows a failed operation to be repeated. */
  while (errors <= MAX_ERRORS) {
	errors++;		/* increment count once per loop cycle */
	if (errors >= MAX_ERRORS)
		return(EIO);

	/* First check to see if a reset is needed. */
	if (w_need_reset) w_reset();

	/* Now set up the DMA chip. */
	w_dma_setup(wn);

	/* Perform the transfer. */
	r = w_transfer(wn);
	if (r == OK) break;	/* if successful, exit loop */

  }

  return(r == OK ? BLOCK_SIZE : EIO);
}


/*==========================================================================*
 *								w_dma_setup					 				* 
 *==========================================================================*/
PRIVATE void w_dma_setup(wn)
struct wini *wn;		/* pointer to the drive struct */
{
/* The IBM PC can perform DMA operations by using the DMA chip.	 To use it,
 * the DMA (Direct Memory Access) chip is loaded with the 20-bit memory address
 * to by read from or written to, the byte count minus 1, and a read or write
 * opcode.	This routine sets up the DMA chip.	Note that the chip is not
 * capable of doing a DMA across a 64K boundary (e.g., you can't read a 
 * 512-byte block starting at physical address 65520).
 */

  int mode, low_addr, high_addr, top_addr, low_ct, high_ct, top_end;
  vir_bytes vir, ct;
  phys_bytes user_phys;

  mode = (wn->wn_opcode == DISK_READ ? DMA_READ : DMA_WRITE);
  vir = (vir_bytes) wn->wn_address;
  ct = (vir_bytes) wn->wn_count;
  user_phys = numap(wn->wn_procnr, vir, ct);
  low_addr	= (int) user_phys & BYTE;
  high_addr = (int) (user_phys >>  8) & BYTE;
  top_addr	= (int) (user_phys >> 16) & BYTE;
  low_ct  = (int) (ct - 1) & BYTE;
  high_ct = (int) ( (ct - 1) >> 8) & BYTE;

  /* Check to see if the transfer will require the DMA address counter to
   * go from one 64K segment to another.  If so, do not even start it, since
   * the hardware does not carry from bit 15 to bit 16 of the DMA address.
   * Also check for bad buffer address.	 These errors mean FS contains a bug.
   */
  if (user_phys == 0)
	  panic("FS gave winchester disk driver bad addr", (int) vir);
  top_end = (int) (((user_phys + ct - 1) >> 16) & BYTE);
  if (top_end != top_addr) panic("Trying to DMA across 64K boundary", top_addr);

  /* Now set up the DMA registers. */
  out_byte(DMA_M2, mode);	/* set the DMA mode */
  out_byte(DMA_M1, mode);	/* set it again */
  out_byte(DMA_ADDR, low_addr);	/* output low-order 8 bits */
  out_byte(DMA_ADDR, high_addr);/* output next 8 bits */
  out_byte(DMA_TOP, top_addr);	/* output highest 4 bits */
  out_byte(DMA_COUNT, low_ct);	/* output low 8 bits of count - 1 */
  out_byte(DMA_COUNT, high_ct);	/* output high 8 bits of count - 1 */
}

/*=========================================================================*
 *								w_transfer								   *
 *=========================================================================*/
PRIVATE int w_transfer(wn)
register struct wini *wn;	/* pointer to the drive struct */
{
/* The drive is now on the proper cylinder.	 Read or write 1 block. */

  /* The command is issued by outputing 6 bytes to the controller chip. */
  command[0] = (wn->wn_opcode == DISK_READ ? WIN_READ : WIN_WRITE);
  command[1] = wn->wn_head | wn->wn_drive;
  command[2] = (((wn->wn_cylinder & 0x0300) >> 2) | wn->wn_sector);
  command[3] = (wn->wn_cylinder & 0xFF);
  command[4] = BLOCK_SIZE/SECTOR_SIZE;
  command[5] = wn->wn_ctrl_byte;

  if (com_out(DMA_INT) != OK)
	return(ERR);

  out_byte(DMA_INIT, 3);	/* initialize DMA */
  /* Block, waiting for disk interrupt. */
  w_wait_int();

  /* Get controller status and check for errors. */
  if (win_results(wn) == OK)
	return(OK);
  if ((wn->wn_results[0] & 63) == 24)
	read_ecc();
  else
	w_need_reset = TRUE;
  return(ERR);
}


/*===========================================================================*
 *				old_win_results					 * 
 *===========================================================================*/
PRIVATE int old_win_results(wn)
register struct wini *wn;	/* pointer to the drive struct */
{
/* Extract results from the controller after an operation. */

  register int i;
  int status;

  status = in_byte(WIN_DATA);
  out_byte(WIN_DMA, 0);
  if (!(status & 2))		/* Test "error" bit */
	return(OK);
  command[0] = WIN_SENSE;
  command[1] = wn->wn_drive;
  if (com_out(NO_DMA_INT) != OK)
	return(ERR);

  /* Loop, extracting bytes from WIN */
  for (i = 0; i < MAX_RESULTS; i++) {
	if (hd_wait(WST_REQ) != OK)
		return(ERR);
	status = in_byte(WIN_DATA);
	wn->wn_results[i] = status & BYTE;
  }
  if(hd_wait(WST_REQ) != OK)	/* Missing from			*/
	 return (ERR);		/* Original.  11-Apr-87 G.O.	*/

  status = in_byte(WIN_DATA);	 /* Read "error" flag */

  if(((status & 2) != 0) || (wn->wn_results[0] & 0x3F)) {
	return(ERR);
  } else
	return(OK);
}


/*===========================================================================*
 *				win_out						 * 
 *===========================================================================*/
PRIVATE void win_out(val)
int val;			/* write this byte to winchester disk controller */
{
/* Output a byte to the controller.	 This is not entirely trivial, since you
 * can only write to it when it is listening, and it decides when to listen.
 * If the controller refuses to listen, the WIN chip is given a hard reset.
 */
  int r;

  if (w_need_reset) return;	/* if controller is not listening, return */

  do {
	r = in_byte(WIN_STATUS);
  } while((r & (WST_REQ | WST_BUSY)) == WST_BUSY);

	out_byte(WIN_DATA, val);
}

/*===========================================================================*
 *				w_reset						 * 
 *===========================================================================*/
PRIVATE int w_reset()
{
/* Issue a reset to the controller.	 This is done after any catastrophe,
 * like the controller refusing to respond.
 */

  int r = 0, i;

  /* Strobe reset bit low. */
  out_byte(WIN_STATUS, 0);

  for(i = MAX_WIN_RETRY/10; i; --i)
	;	/* Spin loop for a while */

  out_byte(WIN_SELECT, 0);	/* Issue select pulse */
  for (i = 0; i < MAX_WIN_RETRY; i++) {
	r = in_byte(WIN_STATUS);
	if(r & 0x30)		/* What is 10? 20 = INTERRUPT */
		return (ERR);

	if((r & (WST_BUSY | WST_BUS | WST_REQ)) ==
		(WST_BUSY | WST_BUS | WST_REQ))
		break;
  }

  if (i == MAX_WIN_RETRY) {
	printf("Hard disk won't reset, status = %x\n", r);
	return(ERR);
  }

  /* Reset succeeded.  Tell WIN drive parameters. */
  w_need_reset = FALSE;

  if(win_specify(0, &param0) != OK)
	return (ERR);


  if ((nr_drives > 1) && (win_specify(1, &param1) != OK))
	return(ERR);


  for (i=0; i<nr_drives; i++) {
	command[0] = WIN_RECALIBRATE;
	command[1] = i << 5;
	command[5] = wini[i * DEV_PER_DRIVE].wn_ctrl_byte;


	if (com_out(INT) != OK)
		return(ERR);

	w_wait_int();

	if (win_results(&wini[i * DEV_PER_DRIVE]) != OK) {
		w_need_reset = TRUE;
		return(ERR);
	}
	 }
	 return(OK);
}


/*===========================================================================*
 *				w_wait_int					 *
 *===========================================================================*/
PRIVATE void w_wait_int()
{
   /*DEBUG: loop looking for 0x20 in status (I don't know what that is!!) */
   /*		 10-Apr-87. G. Oliver					  */
   int r, i; /* Some local storage */

   receive(HARDWARE, &w_mess);

   out_byte(DMA_INIT, 0x07);	/* Disable int from DMA */

   for(i=0; i<MAX_WIN_RETRY; ++i) {
	r = in_byte(WIN_STATUS);
	if(r & WST_INTERRUPT)
		break;		/* Exit if end of int */
  }

#if	 MONITOR
   if(i > 10) {		/* Some arbitrary limit below which we don't really care */
	if(i == MAX_WIN_RETRY)
		printf("wini: timeout waiting for INTERRUPT status\n");
	else
		printf("wini: %d loops waiting for INTERRUPT status\n", i);
   }
#endif	/* MONITOR */
}


/*============================================================================*
 *				win_specify					  *
 *============================================================================*/
PRIVATE int win_specify(drive, paramp)
int drive;
struct param *paramp;
{
  command[0] = WIN_SPECIFY;		/* Specify some parameters */
  command[1] = drive << 5;		/* Drive number */

	if (com_out(NO_DMA_INT) != OK)		/* Output command block */
		return(ERR);

	/* No. of cylinders (high byte) */
  win_out(paramp->nr_cyl >> 8);

	/* No. of cylinders (low byte) */
  win_out(paramp->nr_cyl);

	/* No. of heads */
  win_out(paramp->nr_heads);

	/* Start reduced write (high byte) */
  win_out(paramp->reduced_wr >> 8);

	/* Start reduced write (low byte) */
  win_out(paramp->reduced_wr);

	/* Start write precompensation (high byte) */
  win_out(paramp->wr_precomp >> 8);

	/* Start write precompensation (low byte) */
  win_out(paramp->wr_precomp);

	/* Ecc burst length */
  win_out(paramp->max_ecc);

	if (check_init() != OK) {  /* See if controller accepted parameters */
		w_need_reset = TRUE;
		return(ERR);
	}
  else
  return(OK);
}

/*============================================================================*
 *				check_init					  *
 *============================================================================*/
PRIVATE int check_init()
{
/* Routine to check if controller accepted the parameters */
  int r, s;

  if (hd_wait(WST_REQ | WST_INPUT) == OK) {
	  r = in_byte(WIN_DATA);

	   do {
		s = in_byte(WIN_STATUS);
	   } while(s & WST_BUSY);		/* Loop while still busy */

	   if (r & 2)		/* Test error bit */
		{
		return(ERR);
		}
	  else
		return(OK);
  } else
	{
	return (ERR);	/* Missing from original: 11-Apr-87 G.O. */
  }
}

/*============================================================================*
 *				read_ecc					  *
 *============================================================================*/
PRIVATE int read_ecc()
{
/* Read the ecc burst-length and let the controller correct the data */

  int r;

  command[0] = WIN_ECC_READ;
  if (com_out(NO_DMA_INT) == OK && hd_wait(WST_REQ) == OK) {
	r = in_byte(WIN_DATA);
	if (hd_wait(WST_REQ) == OK) {
		r = in_byte(WIN_DATA);
		if (r & 1)
			w_need_reset = TRUE;
	}
  }
  return(ERR);
}

/*============================================================================*
 *				hd_wait						  *
 *============================================================================*/
PRIVATE int hd_wait(bits)
register int bits;
{
/* Wait until the controller is ready to receive a command or send status */

  register int i = 0;
  int r;

  do {
	r = in_byte(WIN_STATUS) & bits;
  } while ((i++ < MAX_WIN_RETRY) && r != bits);		/* Wait for ALL bits */

  if (i >= MAX_WIN_RETRY) {
	w_need_reset = TRUE;
	return(ERR);
  } else
	return(OK);
}

/*============================================================================*
 *				com_out						  *
 *============================================================================*/
PRIVATE int com_out(mode)
int mode;
{
/* Output the command block to the winchester controller and return status */

	register int i;
	int r;

	out_byte(WIN_DMA, mode);
	out_byte(WIN_SELECT, mode);
	for (i=0; i<MAX_WIN_RETRY; i++) {
		r = in_byte(WIN_STATUS);
		if (r & WST_BUSY)
			break;
	}

	if (i == MAX_WIN_RETRY) {
		w_need_reset = TRUE;
		return(ERR);
	}


	for (i=0; i<6; i++) {
		if(hd_wait(WST_REQ) != OK)
			break;		/* No data request pending */

		r = in_byte(WIN_STATUS);

		if((r & (WST_BUSY | WST_BUS | WST_INPUT)) !=
			(WST_BUSY | WST_BUS))
			break;

		out_byte(WIN_DATA, command[i]);
	}

	if(i != 6) {
		return(ERR);
	}
	else
		return(OK);
}

/*============================================================================*
 *				init_params				      *
 *===========================================================================*/

PRIVATE void init_params()
{
/* This routine is called at startup to initialize the partition table,
 * the number of drives and the controller
*/
  unsigned int i, segment, offset;
  int type_0, type_1;
  phys_bytes address;

  /* Get the number of drives from the bios */
  phys_copy(0x475L, umap(proc_ptr, D, buf, 1), 1L);
  nr_drives = *buf & 0xFF;
  if (nr_drives > MAX_DRIVES) nr_drives = MAX_DRIVES;

  /* Read the switches from the controller */
  i = in_byte(WIN_SELECT);

#if AUTO_BIOS
  /* Get the drive parameters from sector zero of the drive if the */
  /* autoconfig mode of the controller has been selected */

  if(i & AUTO_ENABLE) {

	/* set up some phoney parameters so that we can read the first sector */
	/* from the winchester. all drives will have one cylinder and one head */
	/* but set up initially to the mini scribe drives from ibm */
	param1.nr_cyl = param0.nr_cyl = AUTO_CYLS;
	param1.nr_heads = param0.nr_heads = AUTO_HEADS;
	param1.reduced_wr = param0.reduced_wr = AUTO_RWC;
	param1.wr_precomp = param0.wr_precomp = AUTO_WPC;
	param1.max_ecc = param0.max_ecc = AUTO_ECC;
	param1.ctrl_byte = param0.ctrl_byte = AUTO_CTRL;
	wini[DEV_PER_DRIVE].wn_heads = wini[0].wn_heads = param0.nr_heads;
	wini[DEV_PER_DRIVE].wn_low = wini[0].wn_low = 0L;
	wini[DEV_PER_DRIVE].wn_size = wini[0].wn_size
		   = (long)AUTO_CYLS * (long)AUTO_HEADS * (long)NR_SECTORS;
	wini[DEV_PER_DRIVE].wn_drive = 1 << 5; /* set drive number */
	cim_xt_wini();		/* ready for XT wini interrupts */
	if(w_reset() != OK)
	  panic("cannot setup for reading winchester parameter tables",0);

	if (nr_drives > 1) {
	  
	  /* generate the request to read the first sector from the winchester */
	  w_mess.DEVICE = DEV_PER_DRIVE;
	  w_mess.POSITION = 0L;
	  w_mess.COUNT = BLOCK_SIZE;
	  w_mess.ADDRESS = (char *) buf;
	  w_mess.PROC_NR = WINCHESTER;
	  w_mess.m_type = DISK_READ;
	  if(w_do_rdwt(&w_mess) != BLOCK_SIZE)
		panic("cannot read drive parameters from winchester",DEV_PER_DRIVE);

	  /* copy the parameter tables into the structures for later use */
	  copy_param(&buf[AUTO_PARAM], &param1);

	}

	/* generate the request to read the first sector from the winchester */
	w_mess.DEVICE = 0;
	w_mess.POSITION = 0L;
	w_mess.COUNT = BLOCK_SIZE;
	w_mess.ADDRESS = (char *) buf;
	w_mess.PROC_NR = WINCHESTER;
	w_mess.m_type = DISK_READ;
	if(w_do_rdwt(&w_mess) != BLOCK_SIZE)
	  panic("cannot read drive parameters from winchester", 0);

	/* copy the parameter tables into the structures for later use */
	copy_param(&buf[AUTO_PARAM], &param0);

	   
   /* whoever compiled the kernel wanted the auto bios code included. if it
	* turns out that the tables should be read from the rom, then handle
	* this case the regular way */
  } else {
#endif

  /* Calculate the drive types */
  type_0 = i & 3;
  type_1 = (i >> 2) & 3;

  /* Copy the parameter vector from the saved vector table */
  offset = vec_table[2 * WINI_0_PARM_VEC];
  segment = vec_table[2 * WINI_0_PARM_VEC + 1];

  /* Calculate the address off the parameters and copy them to buf */
  address = hclick_to_physb(segment) + offset;
  phys_copy(address, umap(proc_ptr, D, buf, 64), 64L);

  /* Copy the parameters to the structures */
  copy_param(&buf[type_0 * 16], &param0);
  copy_param(&buf[type_1 * 16], &param1);

#if AUTO_BIOS
  /* close up the code to be executed when the controller has not been
   * set up to for auto configuration */
  }
#endif

  /* Set the parameters in the drive structure */
  for (i = 0; i < DEV_PER_DRIVE; i++) {
	wini[i].wn_heads = param0.nr_heads;
	wini[i].wn_ctrl_byte = param0.ctrl_byte;
	wini[i].wn_drive = 0 << 5;	/* Set drive number */
  }

  wini[0].wn_low = wini[DEV_PER_DRIVE].wn_low = 0L;
  wini[0].wn_size = (long)((long)param0.nr_cyl * 
				   (long)param0.nr_heads * (long)NR_SECTORS);

  for (i = DEV_PER_DRIVE; i < (2*DEV_PER_DRIVE); i++) {
	wini[i].wn_heads = param1.nr_heads;
	wini[i].wn_ctrl_byte = param1.ctrl_byte;
	wini[i].wn_drive = 1 << 5;	/* Set drive number */
  }
  wini[DEV_PER_DRIVE].wn_size =
	(long)((long)param1.nr_cyl * (long)param1.nr_heads * (long)NR_SECTORS);

  /* Initialize the controller */
  cim_xt_wini();		/* ready for XT wini interrupts */
  if ((nr_drives > 0) && (w_reset() != OK)) nr_drives = 0;

  /* Read the partition table for each drive and save them */
  for (i = 0; i < nr_drives; i++) {
	w_mess.DEVICE = i * DEV_PER_DRIVE;
	w_mess.POSITION = 0L;
	w_mess.COUNT = BLOCK_SIZE;
	w_mess.ADDRESS = (char *) buf;
	w_mess.PROC_NR = WINCHESTER;
	w_mess.m_type = DISK_READ;
	if (w_do_rdwt(&w_mess) != BLOCK_SIZE) {
		printf("Can't read partition table of winchester %d ", i);
		continue;
	}
	copy_prt(i * DEV_PER_DRIVE);
  }
}

/*==========================================================================*
 *								copy_param					 				*
 *==========================================================================*/
PRIVATE void copy_param(src, dest)
register unsigned char *src;
register struct param *dest;
{
/* This routine copies the parameters from src to dest
 * and sets the parameters for partition 0 and DEV_PER_DRIVE
*/

  dest->nr_cyl = *(u16_t *)src;
  dest->nr_heads = (int)src[2];
  dest->reduced_wr = *(u16_t *)&src[3];
  dest->wr_precomp = *(u16_t *)&src[5];
  dest->max_ecc = (int)src[7];
  dest->ctrl_byte = (int)src[8];
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

/*==========================================================================*
 *				win_results				    *
 *==========================================================================*/
PRIVATE int win_results(wn)
register struct wini *wn;	/* pointer to the drive struct */
{
/* Extract results from the controller after an operation, then allow wini
 * interrupts again.
 */

  int r;

  r = old_win_results(wn);
  cim_xt_wini();
  return(r);
}
