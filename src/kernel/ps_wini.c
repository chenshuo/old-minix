/* This file contains a driver for the IBM PS/2 ST506 types 1 and 2 adapters.
 * The original version was written by Wim van Leersum.  It has been
 * modified extensively to make it run on PS/2s that have the MCA.
 * The w_transfer routine and the code #ifdef'ed for the PS/2 Model 30/286
 * comes from Rene Nieuwenhuizen's ps_wini.c.  This driver has been tested
 * on a Model 80, a Model 55/SX, and a Model 30/286.  It will hopefully
 * run on a regular Model 30, but it hasn't been tested.
 *
 *
 * The driver supports the following operations (using message format m2):
 *
 *    m_type      DEVICE    PROC_NR     COUNT    POSITION  ADRRESS
 * ----------------------------------------------------------------
 * |  DISK_READ | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * | DISK_WRITE | device  | proc nr |  bytes  |  offset | buf ptr |
 * ----------------------------------------------------------------
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
#define DATA		0x320	/* data register */
#define ASR		0x322	/* Attachment Status Register */
#define ATT_REG		0x324	/* Attention register */
#define ISR		0x324	/* Interrupt status register */
#define ACR		0x322	/* Attachment control register */

/* Winchester disk controller status bytes. */
#define BUSY		0x04	/* controller busy? */
#define DATA_REQUEST	0x10	/* controller asking for data */
#define IR		0x02	/* Interrupt Request */

/* Winchester disk controller command bytes. */
#define CSB	        0x40	/* Get controlers attention for a CSB */
#define DR	        0x10	/* Get controlers attention for data transfer*/
#define CCB	        0x80	/* same for command control block */
#define WIN_READ  	0x15	/* command for the drive to read */
#define WIN_WRITE 	0x95	/* command for the drive to write */

/* Parameters for the disk drive. */
#define SECTOR_SIZE      512	/* physical sector size in bytes */

/* Error codes */
#define ERR		  -1	/* general error */

/* Miscellaneous. */
#define MAX_ERRORS     	4	/* how often to try rd/wt before quitting */
#define MAX_DRIVES	2	/* maximum amount of drives we can handle */
#define NR_DEVICES      (MAX_DRIVES * DEV_PER_DRIVE)
#define MAX_WIN_RETRY 	10000	/* max # times to try to output to WIN */
#define DEV_PER_DRIVE   (1 + NR_PARTITIONS)	/* whole drive & each partn */
#define NUM_COM_BYTES	14	/* number of command bytes controller expects*/
#define SYS_PORTA	0x92	/* MCA System Port A */
#define LIGHT_ON	0xC0    /* Bits to turn drive light on */
#define DRIVE_2		0x4	/* Bit to select Drive_2 */

/* This driver uses DMA */
#define DMA_RESET_VAL   0x06    /* DMA reset value */
#define DMA_READ	0x47	/* DMA read opcode */
#define DMA_WRITE	0x4B	/* DMA write opcode */
#define DMA_ADDR       0x006	/* port for low 16 bits of DMA address */
#define DMA_TOP	       0x082	/* port for top 4 bits of 20-bit DMA addr */
#define DMA_COUNT      0x007	/* port for DMA count (count =	bytes - 1) */
#define DMA_M2	       0x00C	/* DMA status port */
#define DMA_M1	       0x00B	/* DMA status port */
#define DMA_INIT       0x00A	/* DMA init port */
#define DMA_STATUS	0x08    /* DMA status port for channels 0-3 */

/* I/O Ports used for Programmable Option Selection */
#define SBSER		0x94	/* System Board Setup Enable Register */
#define POS2		0x102	/* POS 2 register */
#define POS3		0x103	/* POS 3 register */

/*
 * This driver is written to run on all versions of the PS/2.  The
 * models < 50 are based on the XT.  The models >= 50 are based on the AT.
 * en_wini_int will call the appropriate cim routine to enable interrupts.
 */
#define	en_wini_int()	{ \
				if (pc_at) \
					cim_at_wini(); \
				else \
					cim_xt_wini(); \
			}

/* Variables. */
PRIVATE struct wini {		/* main drive struct, one entry per drive */
  int wn_opcode;		/* DISK_READ or DISK_WRITE */
  int wn_procnr;		/* which proc wanted this operation? */
  int wn_drive;			/* drive number addressed */
  int wn_cylinder;		/* cylinder number addressed */
  int wn_sector;		/* sector addressed */
  int wn_head;			/* head number addressed */
  int wn_heads;			/* maximum number of heads */
  int wn_maxsec;		/* maximum number of sectors per track */
  int wn_maxcyl;		/* maximum number of cylinders */
  int wn_ctlbyte;		/* control byte (steprate) */
  int wn_precomp;		/* write precompensation cylinder */
  long wn_low;			/* lowest cylinder of partition */
  long wn_size;			/* size of partition in blocks */
  int wn_count;			/* byte count */
  vir_bytes wn_address;		/* user virtual address */
} wini[NR_DEVICES];

PUBLIC int using_bios = FALSE;	/* this disk driver does not use the BIOS */

PRIVATE int w_need_reset = FALSE;      /* TRUE when controller must be reset */
PRIVATE int nr_drives;		       /* Number of drives */

PRIVATE message w_mess;		       /* message buffer for in and out */

PRIVATE int command[NUM_COM_BYTES];    /* Common command block */

PRIVATE unsigned char buf[BLOCK_SIZE]; /* Buffer used by the startup routine */

FORWARD void ch_select();
FORWARD void ch_unselect();
FORWARD int com_out();
FORWARD int controller_ready();
FORWARD void copy_params();
FORWARD void copy_prt();
FORWARD int drive_busy();
FORWARD void init_params();
FORWARD void sort();
FORWARD int w_do_rdwt();
FORWARD int w_reset();
FORWARD int w_transfer();
FORWARD int win_init();
FORWARD int win_results();
FORWARD void set_command();
FORWARD void w_dma_setup();
FORWARD void abort_com();
FORWARD int status();

/*===========================================================================*
 *				winchester_task				     * 
 *===========================================================================*/
PUBLIC void winchester_task()
{
/* Main program of the winchester disk driver task. */

  int r, caller, proc_nr;

  /* Initialize the controller */
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


/*===========================================================================*
 *				w_do_rdwt				     * 
 *===========================================================================*/
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
  wn->wn_drive = device/DEV_PER_DRIVE;	/* save drive number */
  if (wn->wn_drive >= nr_drives)
	return(EIO);
  wn->wn_opcode = m_ptr->m_type;	/* DISK_READ or DISK_WRITE */
  if (m_ptr->POSITION % BLOCK_SIZE != 0)
	return(EINVAL);
  sector = m_ptr->POSITION/SECTOR_SIZE;
  if ((sector+BLOCK_SIZE/SECTOR_SIZE) > wn->wn_size)
	return(0);
  sector += wn->wn_low;
  wn->wn_cylinder = sector / (wn->wn_heads * wn->wn_maxsec);
  wn->wn_sector =  (sector % wn->wn_maxsec) + 1;
  wn->wn_head = (sector % (wn->wn_heads * wn->wn_maxsec) )/wn->wn_maxsec;
  wn->wn_count = m_ptr->COUNT;
  wn->wn_address = (vir_bytes) m_ptr->ADDRESS;
  wn->wn_procnr = m_ptr->PROC_NR;

  ch_select();		/* Select fixed disk chip */

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

  ch_unselect();		/* Do not select fixed disk chip anymore */

  return(r == OK ? BLOCK_SIZE : EIO);
}

/*===========================================================================*
 *				w_transfer				     * 
 *===========================================================================*/
PRIVATE int w_transfer(wn)
register struct wini *wn;	/* pointer to the drive struct */
{
	register int i, j, r;	/* indices */
	message dummy;		/* dummy message to recieve interrupts */

	set_command(wn);	/* setup command block */

	if (com_out(6, (wn->wn_drive) ? CCB | DRIVE_2 : CCB) != OK) 
		return(ERR);	/* output CCB to controller */

	for (i = 0; i < MAX_WIN_RETRY; i++)
		if (in_byte(ASR) & IR) break;	/* wait for acknowledgment */

	if (i == MAX_WIN_RETRY) {
		w_need_reset = TRUE;	/* ACK! this shouldn't happen -- bug?*/
		return(ERR);
	}

	if (win_results() != OK) {
		abort_com();
		return(ERR);
	}

	r = OK;				/* be optimistic! */
	w_dma_setup(wn);		/* setup DMA */

	out_byte(ACR, 0x03);		/* turn on interrupts and DMA */
  	out_byte(DMA_INIT, 3);		/* initialize DMA controller */
	en_wini_int();			/* OK to receive interrupts */

	if (com_out(0, DR) != OK) return(ERR); /* ask for data transfer */

	receive(HARDWARE, &dummy);	/* receive interrupt */

	out_byte(ACR, 0x0);		/* disable interrupt and dma */
	out_byte(DMA_INIT, 7);		/* shut off DMA controller */

  	if (win_results() != OK) {
		abort_com();		/* stop the current command */
		r = ERR;
  	} 

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
  message dummy;

  out_byte(ACR, 0x80);	/* Strobe reset bit high. */
  out_byte(ACR, 0);	/* Strobe reset bit low. */

  for (i = 0; i < MAX_WIN_RETRY; i++) {
		if ((in_byte(ASR) & BUSY) != BUSY) break;
		milli_delay(20);
  }

  if (i == MAX_WIN_RETRY) panic("Winchester won't reset!", 0);

  /* Reset succeeded.  Tell WIN drive parameters. */
  if (win_init() != OK) {		/* Initialize the controler */
	printf("Winchester wouldn't accept parameters\n");
	return(ERR);
  }
  
  w_need_reset = FALSE;
  return(OK);
}

/*===========================================================================*
 *				win_init				     * 
 *===========================================================================*/
PRIVATE int win_init()
{
/* Routine to initialize the drive parameters after boot or reset */

  register int i, cyl;
  message dummy;

/*
 * Command Specify Bytes
 */
#define CSB0	0x8F		/* Command Specify Block byte 0 */
#define	CSB1	0x83		/* ST506/2 Interface, 5 Mbps transfer rate */
#define CSB2	0x0B		/* Drive Gap 1 (value gotten from bios) */
#define CSB3	0x04		/* Drive Gap 2 (value gotten from bios) */
#define CSB4	0x1B		/* Drive Gap 3 (value gotten from bios) */
#define CSB5	0x0D		/* Sync field length */
#define CSB6	0x0		/* Step rate in 50 microseconds */
#define CSB7	0x0		/* IBM reserved */
	
  command[0] = CSB0;

  if (pc_at) { 
	/* Setup the Command Specify Block */
  	command[1] = CSB1;
  	command[2] = CSB2;
  	command[3] = CSB3;
  	command[4] = CSB4;
  	command[5] = CSB5;
  	command[6] = CSB6;
  	command[7] = CSB7;
  	command[8] = (unsigned) wini[0].wn_precomp < wini[0].wn_maxcyl ?
			(wini[0].wn_precomp >> 8) & BYTE : 
			(wini[0].wn_maxcyl >> 8) & BYTE;
  	command[9] = (unsigned) wini[0].wn_precomp < wini[0].wn_maxcyl ?
			wini[0].wn_precomp & BYTE :
			wini[0].wn_maxcyl & BYTE;
  	command[10] = (wini[0].wn_maxcyl >> 8) & BYTE;
  	command[11] = wini[0].wn_maxcyl & BYTE;
  	command[12] = wini[0].wn_maxsec;
  	command[13] = wini[0].wn_heads;
  } else {
	/*
	 * The original ps_wini driver set all the bytes in the Command
	 * Specify Block to 0.  According to my documentation, the adapter
  	 * should have complained.  However, supposedly it did work on a
	 * model 30, so if we're running on a NON-mca machine (i.e. model 30)
	 * set the CSB to all 0's
	 */
	for (i = 1; i < 14; i ++)
		command[i] = 0;
  }

  out_byte(ACR, 0);			/* Make sure accepts 8 bits */
  if (com_out(14, CSB) != OK)		/* Output command block */
	return(ERR);

  out_byte(ACR, 2);			/* turn on interrupts */
  en_wini_int();
  receive(HARDWARE, &dummy);	 	/* receive interrupt */
  if (win_results() != OK) {
	w_need_reset = TRUE;
	return(ERR);
  }

  if (nr_drives > 1) {
  	command[8] = (unsigned) wini[5].wn_precomp < wini[5].wn_maxcyl ?
			(wini[5].wn_precomp >> 8) & BYTE : 
			(wini[5].wn_maxcyl >> 8) & BYTE;
  	command[9] = (unsigned) wini[5].wn_precomp < wini[5].wn_maxcyl ?
			wini[5].wn_precomp & BYTE :
			wini[5].wn_maxcyl & BYTE;
	command[10] = (wini[5].wn_maxcyl >> 8) & BYTE;
	command[11] = wini[5].wn_maxcyl & BYTE;
  	command[12] = wini[5].wn_maxsec;
  	command[13] = wini[5].wn_heads;

	if (com_out(14, CSB | DRIVE_2) != OK)	/* Output command block */
		return(ERR);

	en_wini_int();				/* enable interrupts */
	receive(HARDWARE, &dummy);	        /* receive interrupt */
	if (win_results() != OK) {
		w_need_reset = TRUE;
		return(ERR);
	}

  }

  out_byte(ACR, 0);		/* no interrupts and no DMA */
  return(OK);
}

/*============================================================================*
 *				win_results				      *
 *============================================================================*/
PRIVATE int win_results()
{
/* Extract results from the controller after an operation.
 */

	if ((in_byte(ISR) & 0xE3) != 0) return(ERR);
	return(OK);
}

/*==========================================================================*
 *				controller_ready			    *
 *==========================================================================*/
PRIVATE int controller_ready()
{
/* Wait until controller is ready for output; return zero if this times out. */

#define MAX_CONTROLLER_READY_RETRIES	1000	/* should calibrate this */

  register int retries;

  retries = MAX_CONTROLLER_READY_RETRIES + 1;
  while ((--retries != 0) && ((status() & BUSY) == BUSY))
	;			/* wait until not busy */
  return(retries);		/* nonzero if ready */
}

/*============================================================================*
 *				com_out					      *
 *============================================================================*/
PRIVATE int com_out(nr_words, attention)
int nr_words;
int attention;
{
/* Output the command block to the winchester controller and return status */

	register int i, j;

  	if (!controller_ready()) {
		printf("Controller not ready in com_out\n");
		w_need_reset = TRUE;
		return(ERR);
  	}

	out_byte(ATT_REG, attention);	/* get controller's attention */

	if (nr_words == 0) return(OK);	/* may not want to output command */

	for (i = 0; i < nr_words; i++) {	/* output command block */
		for (j = 0; j < MAX_WIN_RETRY; j++)	/* wait  */
			if (status() & DATA_REQUEST) break;

		if (j == MAX_WIN_RETRY) {
			w_need_reset = TRUE;
			dump_isr();
			return(ERR);
		}
		out_byte(DATA, command[i]);   	/* issue command */
  	}

  return(OK);

}

/*============================================================================*
 *				init_params				      *
 *============================================================================*/
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
  if (nr_drives > 0) {
  	ch_select();  /* select fixed disk chip */
  	if (win_init() != OK) {	
		/* Probably controller not a ST506 */
		nr_drives = 0;
		printf("Controller does not appear to be a ST506\n");
	}
  	ch_unselect(); /* unselect the fixed disk chip */
  }

  /* Read the partition table for each drive and save them */
  for (i = 0; i < nr_drives; i++) {
	w_mess.DEVICE = i * 5;
	w_mess.POSITION = 0L;
	w_mess.COUNT = BLOCK_SIZE;
	w_mess.ADDRESS = (char *) buf;
	w_mess.PROC_NR = WINCHESTER;
	w_mess.m_type = DISK_READ;
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

/*============================================================================*
 *				copy_params				      *
 *============================================================================*/
PRIVATE void copy_params(src, dest)
register unsigned char *src;
register struct wini *dest;
{
/* This routine copies the parameters from src to dest
 * and sets the parameters for partition 0 and 5
*/
  register int i;
  long cyl, heads, sectors;

  cyl = (long)(*(u16_t *)src);

  for (i=0; i<5; i++) {
	dest[i].wn_heads = (int)src[2];
	dest[i].wn_precomp = *(u16_t *)&src[5];
	dest[i].wn_ctlbyte = (int)src[8];
	dest[i].wn_maxsec = (int)src[14];
	dest[i].wn_maxcyl = (int)cyl;
  }

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

/*===========================================================================*
 *				ch_select	 		  	     * 
 *===========================================================================*/
PRIVATE void ch_select() 
{
/*
 * The original comment said that this function select the fixed disk
 * drive "chip".  I have no idea what this does, but it sounds important.
 * For mca, this function will turn on the fixed disk activity light.
 */

 if (ps_mca)	/* If MCA, turn on fixed disk activity light */
	out_byte(SYS_PORTA, in_byte(SYS_PORTA) | LIGHT_ON); 
 else if (pc_at) {
	/* Select fixed disk chip. Extracted from a debugged BIOS listing.
   	   Seems to be some magic to get controler on the phone. */
	lock();
	out_byte(SBSER, 0x20);
	out_byte(POS2, (in_byte(POS2) | 1));
	out_byte(POS3, (in_byte(POS3) | 8));
	out_byte(SBSER, 0xA0);
	unlock();
 } else		/* else bit 1 of Planar Control Reg selects it (disk?) */
        out_byte(PCR, in_byte(PCR) | 1); 	/* This must be a Model 30 */

}

/*==========================================================================*
 *			ch_unselect				 	    * 
 *==========================================================================*/
PRIVATE void ch_unselect() 
{
/*
 * The original comment said that this function unselects the fixed disk
 * drive "chip".  I have no idea what this does, but it sounds important.
 * For mca, this function will turn off the fixed disk activity light.
 */
 if (ps_mca)	/* If MCA, turn off fixed disk activity light */
	out_byte(SYS_PORTA, in_byte(SYS_PORTA)  & ~LIGHT_ON); 
 else if (pc_at) {
	/* This must be a Model 30/286 
	   Select fixed disk chip. Extracted from a debugged BIOS listing.
	   Seems to be some magic to get controler on the phone. */
	lock();
	out_byte(SBSER, 0x20);
	out_byte(POS2, (in_byte(POS2) | 1));
	out_byte(POS3, (in_byte(POS3) & 7));
	out_byte(SBSER, 0xA0);
	unlock();
 } else		/* else bit 1 of Planar Control Reg selects it (disk?) */
 	out_byte(PCR, in_byte(PCR) & ~1);	/* This must be a Model 30 */
}

/*==========================================================================*
 *			w_dma_setup				  	    * 
 *==========================================================================*/
PRIVATE void w_dma_setup(wn)
register struct wini *wn;
{
/* The IBM PC can perform DMA operations by using the DMA chip.	 To use it,
 * the DMA (Direct Memory Access) chip is loaded with the 20-bit memory address
 * to by read from or written to, the byte count minus 1, and a read or write
 * opcode.  This routine sets up the DMA chip.	Note that the PS/2 MCA 
 * chip IS capable of doing a DMA across a 64K boundary, but not sure about
 * model 30, so test for it anyway.
 */

  int mode, low_addr, high_addr, top_addr, low_ct, high_ct, top_end;
  vir_bytes vir, ct;
  phys_bytes user_phys;

  mode = (wn->wn_opcode == DISK_READ ? DMA_READ : DMA_WRITE);
  vir = (vir_bytes) wn->wn_address;
  ct = (vir_bytes) BLOCK_SIZE;
  user_phys = numap(wn->wn_procnr, vir, BLOCK_SIZE);

  low_addr  = (int) user_phys & BYTE;
  high_addr = (int) (user_phys >>  8) & BYTE;
  top_addr  = (int) (user_phys >> 16) & BYTE;
  low_ct    = (int) (ct - 1) & BYTE;
  high_ct   = (int) ((ct - 1) >> 8) & BYTE;

  /* Check to see if the transfer will require the DMA address counter to
   * go from one 64K segment to another.  If so, do not even start it, since
   * the hardware does not carry from bit 15 to bit 16 of the DMA address.
   * Also check for bad buffer address.	 These errors mean FS contains a bug.
   */
  if (user_phys == 0)
	  panic("FS gave winchester disk driver bad addr", (int) vir);
  top_end = (int) (((user_phys + ct - 1) >> 16) & BYTE);
  if (top_end != top_addr) 
	panic("Trying to DMA across 64K boundary", top_addr);

  (void)in_byte(DMA_STATUS);	/* clear the status byte */

  /* Now set up the DMA registers. */
  out_byte(DMA_INIT, DMA_RESET_VAL); /* reset the dma controller */
  out_byte(DMA_M2, mode);	/* set the DMA mode */
  out_byte(DMA_M1, mode);	/* set it again */
  out_byte(DMA_ADDR, low_addr);	/* output low-order 8 bits */
  out_byte(DMA_ADDR, high_addr);/* output next 8 bits */
  out_byte(DMA_TOP, top_addr);	/* output highest 4 bits */
  out_byte(DMA_COUNT, low_ct);	/* output low 8 bits of count - 1 */
  out_byte(DMA_COUNT, high_ct);	/* output high 8 bits of count - 1 */
}

/*===========================================================================*
 *				abort_com				     *
 *===========================================================================*/
PRIVATE void abort_com()
{
/*
 * Abort_com will terminate the current command the controller is working
 * on.  Most likely, a bad sector has been found on the disk.
 */

	message dummy;		/* dummy message for receive() */
	
	out_byte(ACR, 0x2);	/* Turn off everything except interrupts */
	if (com_out(0, 0x1) != OK)
		panic("Winchester controller not accepting abort command", 0);

	en_wini_int();			/* can now allow interrupts */
	receive(HARDWARE, &dummy);	/* wait for the interrupt */

	if (win_results() != OK)
		w_reset();		/* this should not be necessary */

	out_byte(ACR, 0x0);	/* shut off interrupts */

}

/*===========================================================================*
 *				dump_isr				     *
 *===========================================================================*/
PRIVATE dump_isr()
{
/*
 * Dump_isr will print out an informative message of what the controller
 * has reported to the system.  This is for use in debugging and in case
 * of hardware error.
 */
	register int stat = in_byte(ISR);

#define ISR_TERM_ERROR(x)	(x & 0x80)
#define	ISR_INVALID_COM(x)	(x & 0x40)
#define ISR_COMMAND_REJ(x)	(x & 0x20)
#define ISR_DRIVE(x)		(x & 0x04) ? 2 : 1
#define ISR_ERROR_REC(x)	(x & 0x02)
#define ISR_EQP_CHECK(x)	(x & 0x01)

	printf("Drive #: %d, ST506 WINI adapter reports:\n", ISR_DRIVE(stat));
	if (ISR_TERM_ERROR(stat)) printf("\t\tTermination Error\n");
	if (ISR_INVALID_COM(stat)) printf("\t\tInvalid Command Sent\n");
	if (ISR_COMMAND_REJ(stat)) printf("\t\tCommand was rejected\n");
	if (ISR_ERROR_REC(stat)) printf("\t\tError rec. procedure invoked\n");
	if (ISR_EQP_CHECK(stat)) printf("\t\tEquipment check, reset needed\n");

}

/*===========================================================================*
 *				set_command			             *
 *===========================================================================*/
PRIVATE void set_command(wn)
register struct wini *wn;
{
	command[0] = wn->wn_opcode == DISK_READ ? WIN_READ : WIN_WRITE;
	command[1] = ((wn->wn_head << 4) & 0xF0) | 
					((wn->wn_cylinder >> 8) & 0x03);
	command[2] = wn->wn_cylinder & BYTE;
	command[3] = wn->wn_sector;
	command[4] = 2;
	command[5] = BLOCK_SIZE/SECTOR_SIZE;	
}


/*===========================================================================*
 *				status					     *
 *===========================================================================*/
PRIVATE int status()
{
/* Get status of the controler */

  return in_byte(ASR);
}
