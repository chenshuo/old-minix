/* NOTE: This driver doesn't work!  The original Minix 1.5 driver was only
 * used to transfer 1 kb blocks, but it should now be able to transfer any
 * number of sectors.  Alas I have no idea yet how to make it do that. (kjb)
 */

/* This file contains the device dependent part of a driver for the IBM PS/2
 * ST506 types 1 and 2 adapters.  The original version was written by Wim
 * van Leersum.  It has been modified extensively to make it run on PS/2s
 * that have the MCA.  The w_transfer routine and the code #ifdef'ed for
 * the PS/2 Model 30/286 comes from Rene Nieuwenhuizen's ps_wini.c.  This
 * driver has been tested on a Model 80, a Model 55/SX, and a Model 30/286.
 * It will hopefully run on a regular Model 30, but it hasn't been tested.
 *
 * The file contains one entry point:
 *
 *   ps_winchester_task:	main entry when system is brought up
 *
 *
 * Changes:
 *	 3 May 1992 by Kees J. Bot: device dependent/independent split.
 */

#include "kernel.h"
#include "driver.h"

#if ENABLE_PS_WINI

/* If the DMA buffer is large enough then use it always. */
#define USE_BUF		(DMA_BUF_SIZE > BLOCK_SIZE)

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
#define CSB	        0x40	/* Get controllers attention for a CSB */
#define DR	        0x10	/* Get controllers attention for data transfer*/
#define CCB	        0x80	/* same for command control block */
#define WIN_READ	0x15	/* command for the drive to read */
#define WIN_WRITE	0x95	/* command for the drive to write */
#define make_atten(cmd, drive)	(cmd | (drive << 2))

/* Error codes */
#define ERR		 (-1)	/* general error */
#define ERR_BAD_SECTOR	 (-2)	/* block marked bad detected */

/* Miscellaneous. */
#define MAX_DRIVES	2	/* this driver support two drives (hd0 - hd9) */
#define MAX_ERRORS	4	/* how often to try rd/wt before quitting */
#define NR_DEVICES      (MAX_DRIVES * DEV_PER_DRIVE)
#define SUB_PER_DRIVE	(NR_PARTITIONS * NR_PARTITIONS)
#define NR_SUBDEVS	(MAX_DRIVES * SUB_PER_DRIVE)
#define MAX_WIN_RETRY	10000	/* max # times to try to output to WIN */
#define NUM_COM_BYTES	14	/* number of command bytes controller expects*/
#define SYS_PORTA	0x92	/* MCA System Port A */
#define LIGHT_ON	0xC0    /* Bits to turn drive light on */

/* This driver uses DMA */
#define DMA_RESET_VAL   0x06    /* DMA reset value */
#define DMA_READ	0x47	/* DMA read opcode */
#define DMA_WRITE	0x4B	/* DMA write opcode */
#define DMA_ADDR       0x006	/* port for low 16 bits of DMA address */
#define DMA_TOP	       0x082	/* port for top 4 bits of 20-bit DMA addr */
#define DMA_COUNT      0x007	/* port for DMA count (count =	bytes - 1) */
#define DMA_FLIPFLOP   0x00C	/* DMA byte pointer flop-flop */
#define DMA_MODE       0x00B	/* DMA mode port */
#define DMA_INIT       0x00A	/* DMA init port */
#define DMA_STATUS	0x08    /* DMA status port for channels 0-3 */

/* I/O Ports used for Programmable Option Selection */
#define SBSER		0x94	/* System Board Setup Enable Register */
#define POS2		0x102	/* POS 2 register */
#define POS3		0x103	/* POS 3 register */

/*
 * This driver is written to run on all versions of the PS/2.  The
 * models < 50 are based on the XT.  The models >= 50 are based on the AT.
 * en_wini_int will use the appropriate irq to enable interrupts.
 */
#define	en_wini_int()	enable_irq(pc_at ? AT_WINI_IRQ : XT_WINI_IRQ)

/* Variables. */
PRIVATE struct wini {		/* main drive struct, one entry per drive */
  unsigned wn_cylinders;	/* number of cylinders */
  unsigned wn_heads;		/* number of heads */
  unsigned wn_sectors;		/* number of sectors per track */
  unsigned wn_ctlbyte;		/* control byte (steprate) */
  unsigned wn_precomp;		/* write precompensation cylinder */
  unsigned wn_open_ct;		/* in-use count */
  struct device wn_part[DEV_PER_DRIVE];    /* primary partitions: hd[0-4] */
  struct device wn_subpart[SUB_PER_DRIVE]; /* subpartitions: hd[1-4][a-d] */
} wini[MAX_DRIVES], *w_wn;

PRIVATE struct trans {
  struct iorequest_s *tr_iop;	/* belongs to this I/O request */
  unsigned long tr_block;	/* first sector to transfer */
  unsigned tr_count;		/* byte count */
  phys_bytes tr_phys;		/* user physical address */
  phys_bytes tr_dma;		/* DMA physical address */
} wtrans[NR_IOREQS];

PRIVATE int w_need_reset = FALSE;	/* TRUE when controller must be reset */
PRIVATE int nr_drives;			/* Number of drives */
PRIVATE struct trans *w_tp;		/* to add transfer requests */
PRIVATE unsigned w_count;		/* number of bytes to transfer */
PRIVATE unsigned long w_nextblock;	/* next block on disk to transfer */
PRIVATE int w_opcode;			/* DEV_READ or DEV_WRITE */
PRIVATE int w_drive;			/* selected drive */
PRIVATE int w_irq;			/* configured irq */
PRIVATE struct device *w_dv;		/* device's base and size */

PRIVATE u8_t command[NUM_COM_BYTES];    /* Common command block */

FORWARD _PROTOTYPE( struct device *w_prepare, (int device) );
FORWARD _PROTOTYPE( char *w_name, (void) );
FORWARD _PROTOTYPE( int w_schedule, (int proc_nr, struct iorequest_s *iop) );
FORWARD _PROTOTYPE( int w_finish, (void) );
FORWARD _PROTOTYPE( int w_transfer, (struct trans *tp, unsigned count) );
FORWARD _PROTOTYPE( int w_reset, (void) );
FORWARD _PROTOTYPE( int win_init, (void) );
FORWARD _PROTOTYPE( int win_results, (void) );
FORWARD _PROTOTYPE( int controller_ready, (void) );
FORWARD _PROTOTYPE( int com_out, (int nr_words, int attention) );
FORWARD _PROTOTYPE( void init_params, (void) );
FORWARD _PROTOTYPE( int w_do_open, (struct driver *dp, message *m_ptr) );
FORWARD _PROTOTYPE( int w_do_close, (struct driver *dp, message *m_ptr) );
FORWARD _PROTOTYPE( void w_init, (void) );
FORWARD _PROTOTYPE( void ch_select, (void) );
FORWARD _PROTOTYPE( void ch_unselect, (void) );
FORWARD _PROTOTYPE( void w_dma_setup, (struct trans *tp, unsigned count) );
FORWARD _PROTOTYPE( void abort_com, (void) );
FORWARD _PROTOTYPE( int w_handler, (int irq) );
FORWARD _PROTOTYPE( void dump_isr, (void) );
FORWARD _PROTOTYPE( void w_geometry, (unsigned *chs));


/* Entry points to this driver. */
PRIVATE struct driver w_dtab = {
  w_name,	/* current device's name */
  w_do_open,	/* open or mount request, initialize device */
  w_do_close,	/* release device */
  do_diocntl,	/* get or set a partition's geometry */
  w_prepare,	/* prepare for I/O on a given minor device */
  w_schedule,	/* precompute cylinder, head, sector, etc. */
  w_finish,	/* do the I/O */
  nop_cleanup,	/* no cleanup needed */
  w_geometry	/* tell the geometry of the disk */
};


/*===========================================================================*
 *				ps_winchester_task			     *
 *===========================================================================*/
PUBLIC void ps_winchester_task()
{
  init_params();

  put_irq_handler(pc_at ? AT_WINI_IRQ : XT_WINI_IRQ, w_handler);

  driver_task(&w_dtab);
}


/*===========================================================================*
 *				w_prepare				     *
 *===========================================================================*/
PRIVATE struct device *w_prepare(device)
int device;
{
/* Prepare for I/O on a device. */

  /* Nothing to transfer as yet. */
  w_count = 0;

  if (device < NR_DEVICES) {			/* hd0, hd1, ... */
	w_drive = device / DEV_PER_DRIVE;	/* save drive number */
	w_wn = &wini[w_drive];
	w_dv = &w_wn->wn_part[device % DEV_PER_DRIVE];
  } else
  if ((unsigned) (device -= MINOR_hd1a) < NR_SUBDEVS) {	/* hd1a, hd1b, ... */
	w_drive = device / SUB_PER_DRIVE;
	w_wn = &wini[w_drive];
	w_dv = &w_wn->wn_subpart[device % SUB_PER_DRIVE];
  } else {
	return(NIL_DEV);
  }
  return(w_drive < nr_drives ? w_dv : NIL_DEV);
}


/*===========================================================================*
 *				w_name					     *
 *===========================================================================*/
PRIVATE char *w_name()
{
/* Return a name for the current device. */
  static char name[] = "ps-hd5";

  name[5] = '0' + w_drive * DEV_PER_DRIVE;
  return name;
}


/*===========================================================================*
 *				w_schedule				     *
 *===========================================================================*/
PRIVATE int w_schedule(proc_nr, iop)
int proc_nr;			/* process doing the request */
struct iorequest_s *iop;	/* pointer to read or write request */
{
/* Gather I/O requests on consecutive blocks so they may be read/written
 * in one command if using a buffer.  Check and gather all the requests
 * and try to finish them as fast as possible if unbuffered.
 */
  int r, opcode;
  unsigned long pos;
  unsigned nbytes, count, dma_count;
  unsigned long block;
  phys_bytes user_phys, dma_phys;

  /* This many bytes to read/write */
  nbytes = iop->io_nbytes;
  if ((nbytes & SECTOR_MASK) != 0) return(iop->io_nbytes = EINVAL);

  /* From/to this position on the device */
  pos = iop->io_position;
  if ((pos & SECTOR_MASK) != 0) return(iop->io_nbytes = EINVAL);

  /* To/from this user address */
  user_phys = numap(proc_nr, (vir_bytes) iop->io_buf, nbytes);
  if (user_phys == 0) return(iop->io_nbytes = EINVAL);

  /* Read or write? */
  opcode = iop->io_request & ~OPTIONAL_IO;

  /* Which block on disk and how close to EOF? */
  if (pos >= w_dv->dv_size) return(OK);		/* At EOF */
  if (pos + nbytes > w_dv->dv_size) nbytes = w_dv->dv_size - pos;
  block = (w_dv->dv_base + pos) >> SECTOR_SHIFT;

  if (USE_BUF && w_count > 0 && block != w_nextblock) {
	/* This new request can't be chained to the job being built */
	if ((r = w_finish()) != OK) return(r);
  }

  /* The next consecutive block */
  if (USE_BUF) w_nextblock = block + (nbytes >> SECTOR_SHIFT);

  /* While there are "unscheduled" bytes in the request: */
  do {
	count = nbytes;

	if (USE_BUF) {
		if (w_count == DMA_BUF_SIZE) {
			/* Can't transfer more than the buffer allows. */
			if ((r = w_finish()) != OK) return(r);
		}

		if (w_count + count > DMA_BUF_SIZE)
			count = DMA_BUF_SIZE - w_count;
	} else {
		if (w_tp == wtrans + NR_IOREQS) {
			/* All transfer slots in use. */
			if ((r = w_finish()) != OK) return(r);
		}
	}

	if (w_count == 0) {
		/* The first request in a row, initialize. */
		w_opcode = opcode;
		w_tp = wtrans;
	}

	if (USE_BUF) {
		dma_phys = tmp_phys + w_count;
	} else {
		/* Note that the PS/2 MCA chip IS capable of doing a DMA
		 * across a 64K boundary, but not sure about the model 30,
		 * so test for it anyway.
		 */
		dma_phys = user_phys;
		dma_count = dma_bytes_left(dma_phys);

		if (dma_count < count) {
			/* Nearing a 64K boundary. */
			if (dma_count >= SECTOR_SIZE) {
				/* Can read a few sectors before hitting the
				 * boundary.
				 */
				count = dma_count & ~SECTOR_MASK;
			} else {
				/* Must use the special buffer for this. */
				count = SECTOR_SIZE;
				dma_phys = tmp_phys;
			}
		}
	}

	/* Store I/O parameters */
	w_tp->tr_iop = iop;
	w_tp->tr_block = block;
	w_tp->tr_count = count;
	w_tp->tr_phys = user_phys;
	w_tp->tr_dma = dma_phys;

	/* Update counters */
	w_tp++;
	w_count += count;
	block += count >> SECTOR_SHIFT;
	user_phys += count;
	nbytes -= count;
  } while (nbytes > 0);

  return(OK);
}


/*===========================================================================*
 *				w_finish				     *
 *===========================================================================*/
PRIVATE int w_finish()
{
/* Carry out the I/O requests gathered in wtrans[]. */

  struct trans *tp = wtrans, *tp2;
  unsigned count;
  int r, errors = 0, many = USE_BUF;

  if (w_count == 0) return(OK);	/* Spurious finish. */

  ch_select();		/* Select fixed disk chip */

  do {
	if (w_opcode == DEV_WRITE) {
		tp2 = tp;
		count = 0;
		do {
			if (USE_BUF || tp2->tr_dma == tmp_phys) {
				phys_copy(tp2->tr_phys, tp2->tr_dma,
						(phys_bytes) tp2->tr_count);
			}
			count += tp2->tr_count;
			tp2++;
		} while (many && count < w_count);
	} else {
		count = many ? w_count : tp->tr_count;
	}

	/* First check to see if a reset is needed. */
	if (w_need_reset) w_reset();

	/* Perform the transfer. */
	r = w_transfer(tp, count);

	if (r != OK) {
		/* An error occurred, try again block by block unless */
		if (r == ERR_BAD_SECTOR || ++errors == MAX_ERRORS) {
			tp->tr_iop->io_nbytes = r = EIO;
			break;
		}

		/* Reset if halfway, but bail out if optional I/O. */
		if (errors == MAX_ERRORS / 2) {
			w_need_reset = TRUE;
			if (tp->tr_iop->io_request & OPTIONAL_IO) {
				tp->tr_iop->io_nbytes = r = EIO;
				break;
			}
		}
		many = 0;
		continue;
	}
	errors = 0;

	w_count -= count;

	do {
		if (w_opcode == DEV_READ) {
			if (USE_BUF || tp->tr_dma == tmp_phys) {
				phys_copy(tp->tr_dma, tp->tr_phys,
						(phys_bytes) tp->tr_count);
			}
		}
		tp->tr_iop->io_nbytes -= tp->tr_count;
		count -= tp->tr_count;
		tp++;
	} while (count > 0);
  } while (w_count > 0);

  ch_unselect();	/* Do not select fixed disk chip anymore */

  return(r);
}


/*===========================================================================*
 *				w_transfer				     *
 *===========================================================================*/
PRIVATE int w_transfer(tp, count)
struct trans *tp;		/* pointer to the transfer struct */
unsigned count;			/* bytes to transfer */
{
  int i, r;			/* indices */
  message dummy;		/* dummy message to receive interrupts */
  unsigned cylinder, sector, head;
  unsigned secspcyl = w_wn->wn_heads * w_wn->wn_sectors;

  cylinder = tp->tr_block / secspcyl;
  sector = (tp->tr_block % w_wn->wn_sectors) + 1;
  head = (tp->tr_block % secspcyl) / w_wn->wn_sectors;

  /* Compute command to transfer count bytes.  (I am using the top four bits
   * of the cylinder number using a 0x0F mask instead of 0x03 (KJB)).
   */
  command[0] = w_opcode == DEV_WRITE ? WIN_WRITE : WIN_READ;
  command[1] = ((head << 4) & 0xF0) | ((cylinder >> 8) & 0x0F);
  command[2] = cylinder & BYTE;
  command[3] = sector;
  command[4] = 2;
  command[5] = count >> SECTOR_SHIFT;

  if (com_out(6, make_atten(CCB, w_drive)) != OK)
	return(ERR);		/* output CCB to controller */

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
  w_dma_setup(tp, count);		/* setup DMA */

  out_byte(ACR, 0x03);			/* turn on interrupts and DMA */
  out_byte(DMA_INIT, 3);		/* initialize DMA controller */
  en_wini_int();			/* OK to receive interrupts */

  if (com_out(0, DR) != OK) return(ERR); /* ask for data transfer */

  receive(HARDWARE, &dummy);		/* receive interrupt */

  out_byte(ACR, 0x0);			/* disable interrupt and dma */
  out_byte(DMA_INIT, 7);		/* shut off DMA controller */

  if (win_results() != OK) {
	abort_com();			/* stop the current command */
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

  int i;

  out_byte(ACR, 0x80);	/* Strobe reset bit high. */
  out_byte(ACR, 0);	/* Strobe reset bit low. */

  for (i = 0; i < MAX_WIN_RETRY; i++) {
	if ((in_byte(ASR) & BUSY) != BUSY) break;
	milli_delay(20);
  }

  if (i == MAX_WIN_RETRY) panic("Winchester won't reset!", 0);

  /* Reset succeeded.  Tell WIN drive parameters. */
  if (win_init() != OK) {		/* Initialize the controller */
	printf("%s: parameters not accepted\n", w_name());
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
/* (I would like to put the two initializations in for loop on the drive
 * number, but the out_byte(ACR, ?)'s don't match, and the possible zeroing
 * of the command bytes is only done for drive 0??? (KJB)).
 */

  register int i;
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
	command[8] = (unsigned) wini[0].wn_precomp < wini[0].wn_cylinders ?
			(wini[0].wn_precomp >> 8) & BYTE :
			(wini[0].wn_cylinders >> 8) & BYTE;
	command[9] = (unsigned) wini[0].wn_precomp < wini[0].wn_cylinders ?
			wini[0].wn_precomp & BYTE :
			wini[0].wn_cylinders & BYTE;
	command[10] = (wini[0].wn_cylinders >> 8) & BYTE;
	command[11] = wini[0].wn_cylinders & BYTE;
	command[12] = wini[0].wn_sectors;
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
  if (com_out(14, make_atten(CSB, 0)) != OK)	/* Output command block */
	return(ERR);

  out_byte(ACR, 2);			/* turn on interrupts */
  en_wini_int();
  receive(HARDWARE, &dummy);		/* receive interrupt */
  if (win_results() != OK) {
	w_need_reset = TRUE;
	return(ERR);
  }

  if (nr_drives > 1) {
	command[8] = (unsigned) wini[1].wn_precomp < wini[1].wn_cylinders ?
			(wini[1].wn_precomp >> 8) & BYTE :
			(wini[1].wn_cylinders >> 8) & BYTE;
	command[9] = (unsigned) wini[1].wn_precomp < wini[1].wn_cylinders ?
			wini[1].wn_precomp & BYTE :
			wini[1].wn_cylinders & BYTE;
	command[10] = (wini[1].wn_cylinders >> 8) & BYTE;
	command[11] = wini[1].wn_cylinders & BYTE;
	command[12] = wini[1].wn_sectors;
	command[13] = wini[1].wn_heads;

	if (com_out(14, make_atten(CSB, 1)) != OK)/* Output command block */
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
  while ((--retries != 0) && ((in_byte(ASR) & BUSY) == BUSY))
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
	printf("%s: controller not ready\n", w_name());
	w_need_reset = TRUE;
	return(ERR);
  }

  out_byte(ATT_REG, attention);	/* get controller's attention */

  if (nr_words == 0) return(OK);	/* may not want to output command */

  for (i = 0; i < nr_words; i++) {	/* output command block */
	for (j = 0; j < MAX_WIN_RETRY; j++)	/* wait  */
		if (in_byte(ASR) & DATA_REQUEST) break;

	if (j == MAX_WIN_RETRY) {
		w_need_reset = TRUE;
		dump_isr();
		return(ERR);
	}
	out_byte(DATA, command[i]);	/* issue command */
  }

  return(OK);
}


/*============================================================================*
 *				init_params				      *
 *============================================================================*/
PRIVATE void init_params()
{
/* This routine is called at startup to initialize the drive parameters. */

  u16_t parv[2];
  unsigned int vector;
  int drive;
  phys_bytes param_phys;
  unsigned char params[16];
  struct wini *wn;

  param_phys = vir2phys(params);

  /* Get the number of drives from the bios */
  phys_copy(0x475L, param_phys, 1L);
  nr_drives = params[0];
  if (nr_drives > MAX_DRIVES) nr_drives = MAX_DRIVES;

  for (drive = 0, wn = wini; drive < nr_drives; drive++, wn++) {
	/* Copy the BIOS parameter vector */
	vector = drive == 0 ? WINI_0_PARM_VEC : WINI_1_PARM_VEC;
	phys_copy(vector * 4L, vir2phys(parv), 4L);

	/* Calculate the address of the parameters and copy them */
	address = hclick_to_physb(parv[1]) + parv[0];
	phys_copy(address, param_phys, 16L);

	/* Copy the parameters to the structures of the drive */
	wn->wn_heads = bp_heads(params);
	wn->wn_precomp = bp_precomp(params) >> 2;
	wn->wn_ctlbyte = bp_ctlbyte(params);
	wn->wn_sectors = bp_sectors(params);
	wn->wn_cylinders = bp_cylinders(params);

	/* Base and size of the whole drive */
	wn->wn_part[0].dv_base = 0;
	wn->wn_part[0].dv_size = ((unsigned long) wn->wn_cylinders
			* wn->wn_heads * wn->wn_sectors) << SECTOR_SHIFT;
  }
}


/*============================================================================*
 *				w_do_open				      *
 *============================================================================*/
PRIVATE int w_do_open(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
/* Device open: Initialize the controller and read the partition table. */

  static int init_done = FALSE;

  if (!init_done) { w_init(); init_done = TRUE; }

  if (w_prepare(m_ptr->DEVICE) == NIL_DEV) return(ENXIO);

  if (w_wn->wn_open_ct++ == 0) {
	/* Partition the disk. */
	partition(&w_dtab, w_drive * DEV_PER_DRIVE, P_FLOPPY);
  }
  return(OK);
}


/*============================================================================*
 *				w_do_close				      *
 *============================================================================*/
PRIVATE int w_do_close(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
/* Device close: Release a device. */

  if (w_prepare(m_ptr->DEVICE) == NIL_DEV) return(ENXIO);
  w_wn->wn_open_ct--;
  return(OK);
}


/*============================================================================*
 *				w_init					      *
 *============================================================================*/
PRIVATE void w_init()
{
/* This routine is called to initialize the controller. */
  int drive;

  /* Initialize the controller */
  if (nr_drives > 0) {
	ch_select();  /* select fixed disk chip */
	if (win_init() != OK) {
		/* Probably controller not a ST506 */
		nr_drives = 0;
		printf("%s: controller does not appear to be a ST506\n",
								w_name());
	}
	ch_unselect(); /* unselect the fixed disk chip */
  }

  /* Tell the geometry of each disk. */
  for (drive = 0; drive < nr_drives; drive++) {
	(void) w_prepare(drive * DEV_PER_DRIVE);
	printf("%s: %d cylinders, %d heads, %d sectors per track\n",
		w_name(), w_wn->wn_cylinders, w_wn->wn_heads, w_wn->wn_sectors);
  }
}


/*===========================================================================*
 *				ch_select				     *
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
	   Seems to be some magic to get controller on the phone. */
	lock();
	out_byte(SBSER, 0x20);
	out_byte(POS2, (in_byte(POS2) | 1));
	out_byte(POS3, (in_byte(POS3) | 8));
	out_byte(SBSER, 0xA0);
	unlock();
 } else		/* else bit 1 of Planar Control Reg selects it (disk?) */
        out_byte(PCR, in_byte(PCR) | 1);	/* This must be a Model 30 */

}


/*==========================================================================*
 *			ch_unselect					    *
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
	   Seems to be some magic to get controller on the phone. */
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
 *				w_dma_setup				    *
 *==========================================================================*/
PRIVATE void w_dma_setup(tp, count)
struct trans *tp;		/* pointer to the transfer struct */
unsigned count;			/* bytes to transfer */
{
/* The IBM PC can perform DMA operations by using the DMA chip.  To use it,
 * the DMA (Direct Memory Access) chip is loaded with the 20-bit memory address
 * to by read from or written to, the byte count minus 1, and a read or write
 * opcode.  This routine sets up the DMA chip.
 */

  (void) in_byte(DMA_STATUS);	/* clear the status byte */

  /* Set up the DMA registers. */
  out_byte(DMA_INIT, DMA_RESET_VAL);	/* reset the dma controller */
  out_byte(DMA_FLIPFLOP, 0);		/* write anything to reset it */
  out_byte(DMA_MODE, w_opcode == DEV_WRITE ? DMA_WRITE : DMA_READ);
  out_byte(DMA_ADDR, (int) tp->tr_dma >>  0);
  out_byte(DMA_ADDR, (int) tp->tr_dma >>  8);
  out_byte(DMA_TOP, (int) (tp->tr_dma >> 16));
  out_byte(DMA_COUNT, (count - 1) >> 0);
  out_byte(DMA_COUNT, (count - 1) >> 8);
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
	panic("Winchester controller not accepting abort command", NO_NUM);

  en_wini_int();		/* can now allow interrupts */
  receive(HARDWARE, &dummy);	/* wait for the interrupt */

  if (win_results() != OK)
	w_reset();		/* this should not be necessary */

  out_byte(ACR, 0x0);	/* shut off interrupts */
}


/*==========================================================================*
 *				w_handler				    *
 *==========================================================================*/
PRIVATE int w_handler(irq)
int irq;
{
/* Disk interrupt, send message to winchester task. */

  interrupt(WINCHESTER);
  return 0;
}


/*===========================================================================*
 *				dump_isr				     *
 *===========================================================================*/
PRIVATE void dump_isr()
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

  printf("%s: ST506 adapter reports:\n", w_name(), ISR_DRIVE(stat));
  if (ISR_TERM_ERROR(stat)) printf("\t\tTermination Error\n");
  if (ISR_INVALID_COM(stat)) printf("\t\tInvalid Command Sent\n");
  if (ISR_COMMAND_REJ(stat)) printf("\t\tCommand was rejected\n");
  if (ISR_ERROR_REC(stat)) printf("\t\tError rec. procedure invoked\n");
  if (ISR_EQP_CHECK(stat)) printf("\t\tEquipment check, reset needed\n");
}


/*============================================================================*
 *				w_geometry				      *
 *============================================================================*/
PRIVATE void w_geometry(chs)
unsigned *chs;			/* {cylinder, head, sector} */
{
  chs[0] = w_wn->wn_cylinders;
  chs[1] = w_wn->wn_heads;
  chs[2] = w_wn->wn_sectors;
}
#endif /* ENABLE_PS_WINI */
