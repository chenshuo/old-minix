/* This file contains a driver for a Floppy Disk Controller (FDC) using the
 * NEC PD765 chip.
 *
 * The driver supports the following operations (using message format m2):
 *
 *    m_type      DEVICE    PROC_NR     COUNT    POSITION  ADDRESS
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
 *   floppy_task:	main entry when system is brought up
 *
 *  Changes:
 *	27 Oct. 1986 by Jakob Schripsema: fdc_results fixed for 8 MHz
 *	28 Nov. 1986 by Peter Kay: better resetting for 386
 *	06 Jan. 1988 by Al Crew: allow 1.44 MB diskettes
 *		1989 by Bruce Evans: i/o vector to keep up with 1-1 interleave
 *		1990 by Bruce Evans: formatting
 */

#include "kernel.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/diskparm.h>

/* I/O Ports used by floppy disk task. */
#define DOR            0x3F2	/* motor drive control bits */
#define FDC_STATUS     0x3F4	/* floppy disk controller status register */
#define FDC_DATA       0x3F5	/* floppy disk controller data register */
#define FDC_RATE       0x3F7	/* transfer rate register */
#define DMA_ADDR       0x004	/* port for low 16 bits of DMA address */
#define DMA_TOP        0x081	/* port for top 4 bits of 20-bit DMA addr */
#define DMA_COUNT      0x005	/* port for DMA count (count =  bytes - 1) */
#define DMA_M2         0x00C	/* DMA status port */
#define DMA_M1         0x00B	/* DMA status port */
#define DMA_INIT       0x00A	/* DMA init port */
#define DMA_RESET_VAL   0x06

/* Status registers returned as result of operation. */
#define ST0             0x00	/* status register 0 */
#define ST1             0x01	/* status register 1 */
#define ST2             0x02	/* status register 2 */
#define ST3             0x00	/* status register 3 (return by DRIVE_SENSE) */
#define ST_CYL          0x03	/* slot where controller reports cylinder */
#define ST_HEAD         0x04	/* slot where controller reports head */
#define ST_SEC          0x05	/* slot where controller reports sector */
#define ST_PCN          0x01	/* slot where controller reports present cyl */

/* Fields within the I/O ports. */
/* Main status register. */
#define CTL_BUSY        0x10	/* bit is set when read or write in progress */
#define DIRECTION       0x40	/* bit is set when reading data reg is valid */
#define MASTER          0x80	/* bit is set when data reg can be accessed */

/* Digital output port (DOR). */
#define MOTOR_MASK      0xF0	/* these bits control the motors in DOR */
#define ENABLE_INT      0x0C	/* used for setting DOR port */

/* ST0. */
#define ST0_BITS        0xF8	/* check top 5 bits of seek status */
#define TRANS_ST0       0x00	/* top 5 bits of ST0 for READ/WRITE */
#define SEEK_ST0        0x20	/* top 5 bits of ST0 for SEEK */

/* ST1. */
#define BAD_SECTOR      0x05	/* if these bits are set in ST1, recalibrate */
#define WRITE_PROTECT   0x02	/* bit is set if diskette is write protected */

/* ST2. */
#define BAD_CYL         0x1F	/* if any of these bits are set, recalibrate */

/* ST3 (not used). */
#define ST3_FAULT       0x80	/* if this bit is set, drive is sick */
#define ST3_WR_PROTECT  0x40	/* set when diskette is write protected */
#define ST3_READY       0x20	/* set when drive is ready */

/* Floppy disk controller command bytes. */
#define FDC_SEEK        0x0F	/* command the drive to seek */
#define FDC_READ        0xE6	/* command the drive to read */
#define FDC_WRITE       0xC5	/* command the drive to write */
#define FDC_SENSE       0x08	/* command the controller to tell its status */
#define FDC_RECALIBRATE 0x07	/* command the drive to go to cyl 0 */
#define FDC_SPECIFY     0x03	/* command the drive to accept params */
#define FDC_READ_ID     0x4A	/* command the drive to read sector identity */
#define FDC_FORMAT      0x4D	/* command the drive to format a track */

/* DMA channel commands. */
#define DMA_READ        0x46	/* DMA read opcode */
#define DMA_WRITE       0x4A	/* DMA write opcode */

/* Parameters for the disk drive. */
#define SECTOR_SIZE      512	/* physical sector size in bytes */
#define HC_SIZE         2880	/* # sectors on largest legal disk (1.44MB) */
#define NR_HEADS        0x02	/* two heads (i.e., two tracks/cylinder) */
#define DTL             0xFF	/* determines data length (sector size) */
#define SPEC2           0x02	/* second parameter to SPECIFY */

#define MOTOR_OFF       3*HZ	/* how long to wait before stopping motor */

/* Error codes */
#define ERR_SEEK          -1	/* bad seek */
#define ERR_TRANSFER      -2	/* bad transfer */
#define ERR_STATUS        -3	/* something wrong when getting status */
#define ERR_RECALIBRATE   -4	/* recalibrate didn't work properly */
#define ERR_WR_PROTECT    -5	/* diskette is write protected */
#define ERR_DRIVE         -6	/* something wrong with a drive */
#define ERR_READ_ID       -7	/* bad read id */

/* Encoding of drive type in minor device number. */
#define DEV_TYPE_BITS   0x7C	/* drive type + 1, if nonzero */
#define DEV_TYPE_SHIFT     2	/* right shift to normalize type bits */
#define FORMAT_DEV_BIT  0x80	/* bit in minor to turn write into format */

/* Miscellaneous. */
#define MOTOR_RUNNING   0xFF	/* message type for clock interrupt */
#define MAX_ERRORS         3	/* how often to try rd/wt before quitting */
#define MAX_RESULTS        7	/* max number of bytes controller returns */
#define NR_DRIVES          2	/* maximum number of drives */
#define DIVISOR          128	/* used for sector size encoding */
#define MAX_FDC_RETRY    100	/* max # times to try to output to FDC */
#define NT                 7	/* number of diskette/drive combinations */
#define AUTOMATIC	0x3F	/* bit map allowing both 3.5 and 5.25 disks */
				/* except for drive type 6, because that is */
				/* hard to distinguish from drive type 1 */
#define THREE_INCH	0x48	/* bit map allowing only 3.5 inch diskettes */
#define FIVE_INCH	0x37	/* bit map allowing only 5.25 inch diskettes */

/* Variables. */
PRIVATE struct floppy {		/* main drive struct, one entry per drive */
  int fl_opcode;		/* FDC_READ, FDC_WRITE or FDC_FORMAT */
  int fl_curcyl;		/* current cylinder */
  int fl_procnr;		/* which proc wanted this operation? */
  int fl_drive;			/* drive number addressed */
  int fl_cylinder;		/* cylinder number addressed */
  int fl_sector;		/* sector addressed */
  int fl_head;			/* head number addressed */
  int fl_count;			/* byte count */
  vir_bytes fl_address;		/* user virtual address */
  char fl_results[MAX_RESULTS];	/* the controller can give lots of output */
  char fl_calibration;		/* CALIBRATED or UNCALIBRATED */
  char fl_density;		/* 0 = 360K/360K; 1 = 360K/1.2M; etc. */
  char fl_auto_type;		/* nonzero to allow search for working type */
  struct disk_parameter_s fl_param;	/* parameters for format */
} floppy[NR_DRIVES];

#define UNCALIBRATED       0	/* drive needs to be calibrated at next use */
#define CALIBRATED         1	/* no calibration needed */

PRIVATE int motor_status;	/* current motor status is in 4 high bits */
PRIVATE int motor_goal;		/* desired motor status is in 4 high bits */
PRIVATE int prev_motor;		/* which motor was started last */
PRIVATE int need_reset;		/* set to 1 when controller must be reset */
PRIVATE int d;			/* diskette/drive combination */
PRIVATE int current_spec1;	/* latest spec1 sent to the controller */

PRIVATE message mess;		/* message buffer for in and out */

PRIVATE char len[] = {-1,0,1,-1,2,-1,-1,3,-1,-1,-1,-1,-1,-1,-1,4};
PRIVATE char base_sector = 1;	/* physical sectors form range starting here */

/* Seven combinations of diskette/drive are supported. 
 *
 * # Drive  diskette  Sectors  Tracks  Rotation Data-rate  Comment
 * 0  360K    360K      9       40     300 RPM  250 kbps   Standard PC DSDD
 * 1  1.2M    1.2M     15       80     360 RPM  500 kbps   AT disk in AT drive
 * 2  720K    360K      9       40     300 RPM  250 kbps   Quad density PC
 * 3  720K    720K      9       80     300 RPM  250 kbps   Toshiba, et al.
 * 4  1.2M    360K      9       40     360 RPM  300 kbps   PC disk in AT drive
 * 5  1.2M    720K      9       80     360 RPM  300 kbps   Toshiba in AT drive
 * 6  1.44M   1.44M    18	80     300 RPM  500 kbps   PS/2, et al.
 *
 * In addition, 720K diskettes can be read in 1.44MB drives, but that does 
 * not need a different set of parameters.  This combination uses
 *
 * X  1.44M   720K	9	80     300 RPM  250 kbps   PS/2, et al.
 */
PRIVATE int gap[NT] =
	{0x2A, 0x1B, 0x2A, 0x2A, 0x23, 0x23, 0x1B}; /* gap size */
PRIVATE int rate[NT] = 
	{0x02, 0x00, 0x02, 0x02, 0x01, 0x01, 0x00}; /* 2=250,1=300,0=500 kbps*/
PRIVATE int nr_sectors[NT] = 
	{9,    15,   9,    9,    9,    9,    18};   /* sectors/track */
PRIVATE int nr_blocks[NT] = 
	{720,  2400, 720,  1440, 720,  1440, 2880}; /* sectors/diskette*/
PRIVATE int steps_per_cyl[NT] = 
	{1,    1,    2,    1,    2,    1,     1};   /* 2 = dbl step */
PRIVATE int mtr_setup[NT] = 
	{1*HZ/4,3*HZ/4,1*HZ/4,4*HZ/4,3*HZ/4,3*HZ/4,4*HZ/4}; /* in ticks */
PRIVATE char spec1[NT] =
	{0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xAF}; /* step rate, etc. */

/* This driver hunts around for the proper density by simply trying them all
 * until it finds one that works.  By defining DEFAULT_CLASS, one can reduce 
 * the searching to only 5.25 inch or only 3.5 inch types.  The array
 * drive_class contains a bit map for each drive, telling which of the NT
 * combinations defined above should be tried.
 */
PRIVATE char drive_class[NR_DRIVES] = {
#if (DEFAULT_CLASS == 3)
	THREE_INCH, THREE_INCH	/* drive 0 = 3.5 inch, drive 1 also */
#endif
#if (DEFAULT_CLASS == 5)
	FIVE_INCH, FIVE_INCH	/* drive 0 = 5.25 inch, drive 1 also */
#endif
#if (DEFAULT_CLASS != 3 && DEFAULT_CLASS != 5)
	AUTOMATIC, AUTOMATIC	/* both drives can handle both types */
#endif
};

FORWARD void clock_mess();
FORWARD void dma_setup();
FORWARD int do1_rdwt();
FORWARD int do_rdwt();
FORWARD int f_do_vrdwt();
FORWARD void f_reset();
FORWARD void fdc_out();
FORWARD int fdc_results();
FORWARD int read_id();
FORWARD int recalibrate();
FORWARD int seek();
FORWARD void send_mess();
FORWARD void start_motor();
FORWARD void stop_motor();
FORWARD int transfer();

/*===========================================================================*
 *				floppy_task				     * 
 *===========================================================================*/
PUBLIC void floppy_task()
{
/* Main program of the floppy disk driver task. */

  int r, caller, proc_nr;

  cim_floppy();			/* ready for floppy interrupts */

  /* Here is the main loop of the disk task.  It waits for a message, carries
   * it out, and sends a reply.
   */
  while (TRUE) {
	/* First wait for a request to read or write a disk block. */
	receive(ANY, &mess);	/* get a request to do some work */
	if (mess.m_source < 0)
		panic("disk task got message from ", mess.m_source);

	/* Ignore any alarm to turn motor off, now there is work to do. */
	motor_goal = motor_status;

	caller = mess.m_source;
	proc_nr = mess.PROC_NR;

	/* Now carry out the work. */
	switch(mess.m_type) {
	    case DISK_READ:
	    case DISK_WRITE:	r = do_rdwt(&mess, FALSE);	break;
	    case SCATTERED_IO:	r = f_do_vrdwt(&mess);	break;
	    default:		r = EINVAL;		break;
	}

	/* Start watch_dog timer to turn all motors off in a few seconds. */
	motor_goal = ENABLE_INT;
	clock_mess(MOTOR_OFF, stop_motor);

	/* Finally, prepare and send the reply message. */
	mess.m_type = TASK_REPLY;	
	mess.REP_PROC_NR = proc_nr;
	mess.REP_STATUS = r;	/* # of bytes transferred or error code */
	send(caller, &mess);	/* send reply to caller */
  }
}


/*===========================================================================*
 *				do_rdwt					     * 
 *===========================================================================*/
PRIVATE int do_rdwt(m_ptr, dont_retry)
message *m_ptr;			/* pointer to read or write message */
int dont_retry;			/* nonzero to skip retry after error */
{
/* Do a single read or write request. */

  register struct floppy *fp;
  int r, sectors, drive, errors;
  off_t block;
  unsigned dtype;
  phys_bytes param_phys;
  phys_bytes user_param_phys;

  /* Decode the message parameters. */
  drive = m_ptr->DEVICE & ~(DEV_TYPE_BITS | FORMAT_DEV_BIT);
  if (drive < 0 || drive >= NR_DRIVES) return(EIO);
  fp = &floppy[drive];		/* 'fp' points to entry for this drive */
  fp->fl_drive = drive;		/* save drive number explicitly */
  fp->fl_opcode = (m_ptr->m_type == DISK_READ ? FDC_READ : FDC_WRITE);
  if (m_ptr->DEVICE & FORMAT_DEV_BIT) {
	if (fp->fl_opcode == FDC_READ) return(EIO);
	fp->fl_opcode = FDC_FORMAT;
	param_phys = umap(proc_ptr, D, (vir_bytes) &fp->fl_param,
			  (vir_bytes) sizeof fp->fl_param);
	user_param_phys = numap(m_ptr->PROC_NR,
				(vir_bytes) (m_ptr->ADDRESS + BLOCK_SIZE / 2),
				(vir_bytes) sizeof fp->fl_param);
	phys_copy(user_param_phys, param_phys,(phys_bytes)sizeof fp->fl_param);

	/* Check that the number of sectors in the data is reasonable, to
	 * avoid division by 0.  Leave checking of other data to the FDC.
	 */
	if (fp->fl_param.sectors_per_cylinder == 0) return(EIO);
  }
  dtype = m_ptr->DEVICE & DEV_TYPE_BITS;
  if (dtype != 0) {
	dtype = (dtype >> DEV_TYPE_SHIFT) - 1;
	if (dtype >= NT) return(EIO);
	fp->fl_density = dtype;
	fp->fl_auto_type = FALSE;
  } else
	fp->fl_auto_type = TRUE;

  if (m_ptr->POSITION % BLOCK_SIZE != 0) return(EINVAL);
  block = m_ptr->POSITION/SECTOR_SIZE;
  if (block >= HC_SIZE) return(0);	/* sector is beyond end of all disks */

  d = fp->fl_density;		/* diskette/drive combination */
  if (fp->fl_auto_type) {
	/* Check bit map to skip illegal densities. */
	while ((drive_class[drive] & (1 << d)) == 0) d = (d + 1) % NT;
  }

  /* Store the message parameters in the fp->fl array. */
  fp->fl_density=d;
  sectors = (m_ptr->DEVICE & FORMAT_DEV_BIT ?
	     fp->fl_param.sectors_per_cylinder : nr_sectors[d]);
  fp->fl_cylinder = (int) (block / (NR_HEADS * sectors));
  fp->fl_sector = base_sector + (int) (block % sectors);
  fp->fl_head = (int) (block % (NR_HEADS * sectors)) / sectors;
  fp->fl_count = m_ptr->COUNT;
  fp->fl_address = (vir_bytes) m_ptr->ADDRESS;
  fp->fl_procnr = m_ptr->PROC_NR;
  if (fp->fl_count != BLOCK_SIZE) return(EINVAL);

  errors = 0;

  /* This loop allows a failed operation to be repeated.  It is really a
   * nested loop allowing MAX_ERRORS errors for each of NT drive types.
   */
  while (errors <= MAX_ERRORS * NT) {
	if (++errors % MAX_ERRORS == 0) {

#if 0 /* this works well except for programs like mkfs which seek to end 1st*/
		/* A lot of errors probably means that we are trying the
		 * wrong drive type.  Try another one if the cylinder is <= 1.
		 * Otherwise, assume the disk is unchanged and give up.
		 */
		if (fp->fl_cylinder > 1)
			return(block >= nr_blocks[d] ? 0 : EIO);
#else
		/* A lot of errors probably means that we are trying the
		 * wrong drive type.  Try another one.
		 */
#endif
		if (!fp->fl_auto_type) return(EIO);
                d++;

		/* Check bit map to skip illegal densities. */
		while ((drive_class[drive] & (1 << d)) == 0) d = (d + 1) % NT;
 		fp->fl_density = d;

		sectors = nr_sectors[d];
		fp->fl_cylinder = (int) (block / (NR_HEADS * sectors));
		fp->fl_sector = base_sector + (int) (block % sectors);
		fp->fl_head = (int)(block%(NR_HEADS*sectors)) / sectors;
 		need_reset = 1;
	}
  	if (block >= nr_blocks[d]) continue;

	/* First check to see if a reset is needed. */
	if (need_reset) f_reset();

	/* Set the stepping rate */
	if (current_spec1 != spec1[d]) {
		fdc_out(FDC_SPECIFY);
		fdc_out(current_spec1=spec1[d]);
		fdc_out(SPEC2);
	}

 	/* Set the data rate */
	if (pc_at) out_byte(FDC_RATE, rate[d]);

	/* Now set up the DMA chip. */
	dma_setup(fp);

	/* See if motor is running; if not, turn it on and wait */
	start_motor(fp);

	/* If we are going to a new cylinder, perform a seek. */
	r = seek(fp);

	/* Perform the transfer. */
	if (r == OK) r = transfer(fp);
	if (r == OK) break;	/* if successful, exit loop */
	if (dont_retry) break;	/* retries not wanted */
	if (r == ERR_WR_PROTECT) break;	/* retries won't help */
  }
  return(r == OK ? fp->fl_count : EIO);
}


/*===========================================================================*
 *				dma_setup				     * 
 *===========================================================================*/
PRIVATE void dma_setup(fp)
struct floppy *fp;		/* pointer to the drive struct */
{
/* The IBM PC can perform DMA operations by using the DMA chip.  To use it,
 * the DMA (Direct Memory Access) chip is loaded with the 20-bit memory address
 * to be read from or written to, the byte count minus 1, and a read or write
 * opcode.  This routine sets up the DMA chip.  Note that the chip is not
 * capable of doing a DMA across a 64K boundary (e.g., you can't read a 
 * 512-byte block starting at physical address 65520).
 */

  int mode, low_addr, high_addr, top_addr, low_ct, high_ct, top_end;
  vir_bytes vir, ct;
  phys_bytes user_phys;

  mode = (fp->fl_opcode == FDC_READ ? DMA_READ : DMA_WRITE);
  vir = (vir_bytes) fp->fl_address;
  ct = (vir_bytes) fp->fl_count;
  user_phys = numap(fp->fl_procnr, vir, ct);
  low_addr  = (int) (user_phys >>  0) & BYTE;
  high_addr = (int) (user_phys >>  8) & BYTE;
  top_addr  = (int) (user_phys >> 16) & BYTE;
  low_ct  = (int) ( (ct - 1) >> 0) & BYTE;
  high_ct = (int) ( (ct - 1) >> 8) & BYTE;

  /* Check to see if the transfer will require the DMA address counter to
   * go from one 64K segment to another.  If so, do not even start it, since
   * the hardware does not carry from bit 15 to bit 16 of the DMA address.
   * Also check for bad buffer address.  These errors mean FS contains a bug.
   */
  if (user_phys == 0) panic("FS gave floppy disk driver bad addr", (int) vir);
  top_end = (int) (((user_phys + ct - 1) >> 16) & BYTE);
  if (top_end != top_addr)panic("Trying to DMA across 64K boundary", top_addr);

  /* Now set up the DMA registers. */
  out_byte(DMA_INIT, DMA_RESET_VAL);        /* reset the dma controller */
  out_byte(DMA_M2, mode);	/* set the DMA mode */
  out_byte(DMA_M1, mode);	/* set it again */
  out_byte(DMA_ADDR, low_addr);	/* output low-order 8 bits */
  out_byte(DMA_ADDR, high_addr);/* output next 8 bits */
  out_byte(DMA_TOP, top_addr);	/* output highest 4 bits */
  out_byte(DMA_COUNT, low_ct);	/* output low 8 bits of count - 1 */
  out_byte(DMA_COUNT, high_ct);	/* output high 8 bits of count - 1 */
  out_byte(DMA_INIT, 2);	/* initialize DMA */
}


/*===========================================================================*
 *				start_motor				     * 
 *===========================================================================*/
PRIVATE void start_motor(fp)
struct floppy *fp;		/* pointer to the drive struct */
{
/* Control of the floppy disk motors is a big pain.  If a motor is off, you
 * have to turn it on first, which takes 1/2 second.  You can't leave it on
 * all the time, since that would wear out the diskette.  However, if you turn
 * the motor off after each operation, the system performance will be awful.
 * The compromise used here is to leave it on for a few seconds after each
 * operation.  If a new operation is started in that interval, it need not be
 * turned on again.  If no new operation is started, a timer goes off and the
 * motor is turned off.  I/O port DOR has bits to control each of 4 drives.
 * The timer cannot go off while we are changing with the bits, since the
 * clock task cannot run while another (this) task is active, so there is no
 * need to lock().
 */

  int motor_bit, running;

  motor_bit = 1 << (fp->fl_drive + 4);	/* bit mask for this drive */
  running = motor_status & motor_bit;	/* nonzero if this motor is running */
  motor_goal = motor_bit | ENABLE_INT | fp->fl_drive;
  if (motor_status & prev_motor) motor_goal |= prev_motor;
  out_byte(DOR, motor_goal);
  motor_status = motor_goal;
  prev_motor = motor_bit;	/* record motor started for next time */

  /* If the motor was already running, we don't have to wait for it. */
  if (running) return;			/* motor was already running */
  clock_mess(mtr_setup[d], send_mess);	/* motor was not running */
  receive(CLOCK, &mess);		/* wait for clock interrupt */
}


/*===========================================================================*
 *				stop_motor				     * 
 *===========================================================================*/
PRIVATE void stop_motor()
{
/* This routine is called by the clock interrupt after several seconds have
 * elapsed with no floppy disk activity.  It checks to see if any drives are
 * supposed to be turned off, and if so, turns them off.
 */

  if ( (motor_goal & MOTOR_MASK) != (motor_status & MOTOR_MASK) ) {
	out_byte(DOR, motor_goal);
	motor_status = motor_goal;
  }
}


/*===========================================================================*
 *				seek					     * 
 *===========================================================================*/
PRIVATE int seek(fp)
struct floppy *fp;		/* pointer to the drive struct */
{
/* Issue a SEEK command on the indicated drive unless the arm is already 
 * positioned on the correct cylinder.
 */

  int r;

  /* Are we already on the correct cylinder? */
  if (fp->fl_calibration == UNCALIBRATED)
	if (recalibrate(fp) != OK) return(ERR_SEEK);
  if (fp->fl_curcyl == fp->fl_cylinder) return(OK);

  /* No.  Wrong cylinder.  Issue a SEEK and wait for interrupt. */
  fdc_out(FDC_SEEK);		/* start issuing the SEEK command */
  fdc_out( (fp->fl_head << 2) | fp->fl_drive);
  fdc_out(fp->fl_cylinder * steps_per_cyl[d]);
  if (need_reset) return(ERR_SEEK);	/* if controller is sick, abort seek */
  receive(HARDWARE, &mess);

  /* Interrupt has been received.  Check drive status. */
  fdc_out(FDC_SENSE);		/* probe FDC to make it return status */
  r = fdc_results(fp);		/* get controller status bytes */
  if ( (fp->fl_results[ST0] & ST0_BITS) != SEEK_ST0) r = ERR_SEEK;
  if (fp->fl_results[ST1] != fp->fl_cylinder * steps_per_cyl[d]) r = ERR_SEEK;
  if (r != OK) 
	if (recalibrate(fp) != OK) return(ERR_SEEK);
  fp->fl_curcyl = (r == OK ? fp->fl_cylinder : -1);
  if (r == OK && ((d == 6) || (d == 3))) {/* give head time to settle on 3.5 */
	clock_mess(2, send_mess);
	receive(CLOCK, &mess);
  }
  return(r);
}


/*===========================================================================*
 *				transfer				     * 
 *===========================================================================*/
PRIVATE int transfer(fp)
register struct floppy *fp;	/* pointer to the drive struct */
{
/* The drive is now on the proper cylinder.  Read, write or format 1 block. */

  int r, s;

  /* Never attempt a transfer if the drive is uncalibrated or motor is off. */
  if (fp->fl_calibration == UNCALIBRATED) return(ERR_TRANSFER);
  if ( ( (motor_status>>(fp->fl_drive+4)) & 1) == 0) return(ERR_TRANSFER);

  /* The command is issued by outputting 9 bytes to the controller chip. */
  fdc_out(fp->fl_opcode);	/* issue the read, write or format command */
  fdc_out( (fp->fl_head << 2) | fp->fl_drive);
  if (fp->fl_opcode == FDC_FORMAT) {
	fdc_out(fp->fl_param.sector_size_code);
	fdc_out(fp->fl_param.sectors_per_cylinder);
	fdc_out(fp->fl_param.gap_length_for_format);
	fdc_out(fp->fl_param.fill_byte_for_format);
  } else {
	fdc_out(fp->fl_cylinder);
	fdc_out(fp->fl_head);
	fdc_out(fp->fl_sector);
	fdc_out( (int) len[SECTOR_SIZE/DIVISOR]);	/* sector size code */
	fdc_out(nr_sectors[d]);
	fdc_out(gap[d]);	/* sector gap */
	fdc_out(DTL);		/* data length */
  }

  /* Block, waiting for disk interrupt. */
  if (need_reset) return(ERR_TRANSFER);	/* if controller is sick, abort op */
  receive(HARDWARE, &mess);

  /* Get controller status and check for errors. */
  r = fdc_results(fp);
  if (r != OK) return(r);
  if ( (fp->fl_results[ST1] & BAD_SECTOR) || (fp->fl_results[ST2] & BAD_CYL) )
	fp->fl_calibration = UNCALIBRATED;
  if (fp->fl_results[ST1] & WRITE_PROTECT) {
	printf("Diskette in drive %d is write protected.\n", fp->fl_drive);
	return(ERR_WR_PROTECT);
  }
  if ((fp->fl_results[ST0] & ST0_BITS) != TRANS_ST0) return(ERR_TRANSFER);
  if (fp->fl_results[ST1] | fp->fl_results[ST2]) return(ERR_TRANSFER);

  if (fp->fl_opcode == FDC_FORMAT) return(OK);

  /* Compare actual numbers of sectors transferred with expected number. */
  s =  (fp->fl_results[ST_CYL] - fp->fl_cylinder) * NR_HEADS * nr_sectors[d];
  s += (fp->fl_results[ST_HEAD] - fp->fl_head) * nr_sectors[d];
  s += (fp->fl_results[ST_SEC] - fp->fl_sector);
  if (s * SECTOR_SIZE != fp->fl_count) return(ERR_TRANSFER);
  return(OK);
}


/*==========================================================================*
 *				fdc_results				    *
 *==========================================================================*/
PRIVATE int fdc_results(fp)
struct floppy *fp;		/* pointer to the drive struct */
{
/* Extract results from the controller after an operation, then allow floppy
 * interrupts again.
 */

  int result_nr;
  register int retries;
  register int status;

  /* Extract bytes from FDC until it says it has no more.  The loop is
   * really an outer loop on result_nr and an inner loop on status.
   */
  result_nr = 0;
  retries = MAX_FDC_RETRY;
  while (TRUE) {
	/* Reading one byte is almost a mirror of fdc_out() - the DIRECTION
	 * bit must be set instead of clear, but the CTL_BUSY bit destroys
	 * the perfection of the mirror.
	 */
	status = in_byte(FDC_STATUS) & (MASTER | DIRECTION | CTL_BUSY);
	if (status == (MASTER | DIRECTION | CTL_BUSY)) {
		if (result_nr >= MAX_RESULTS) break;	/* too many results */
		fp->fl_results[result_nr++] = in_byte(FDC_DATA);
		retries = MAX_FDC_RETRY;
		continue;
	}
	if (status == MASTER) {	/* all read */
		cim_floppy();
		return(OK);	/* only good exit */
	}
	if (--retries == 0) break;	/* time out */
  }
  need_reset = TRUE;		/* controller chip must be reset */
  cim_floppy();
  return(ERR_STATUS);
}


/*===========================================================================*
 *				fdc_out					     * 
 *===========================================================================*/
PRIVATE void fdc_out(val)
register int val;		/* write this byte to floppy disk controller */
{
/* Output a byte to the controller.  This is not entirely trivial, since you
 * can only write to it when it is listening, and it decides when to listen.
 * If the controller refuses to listen, the FDC chip is given a hard reset.
 */

  register int retries;

  if (need_reset) return;	/* if controller is not listening, return */

  /* It may take several tries to get the FDC to accept a command. */
  retries = MAX_FDC_RETRY;
  while ( (in_byte(FDC_STATUS) & (MASTER | DIRECTION)) != (MASTER | 0) )
	if (--retries == 0) {
  /* Controller is not listening.  Hit it over the head with a hammer. */
		need_reset = TRUE;
		return;
	}
  out_byte(FDC_DATA, val);
}


/*===========================================================================*
 *			 	recalibrate				     * 
 *===========================================================================*/
PRIVATE int recalibrate(fp)
register struct floppy *fp;	/* pointer tot he drive struct */
{
/* The floppy disk controller has no way of determining its absolute arm
 * position (cylinder).  Instead, it steps the arm a cylinder at a time and
 * keeps track of where it thinks it is (in software).  However, after a
 * SEEK, the hardware reads information from the diskette telling where the
 * arm actually is.  If the arm is in the wrong place, a recalibration is done,
 * which forces the arm to cylinder 0.  This way the controller can get back
 * into sync with reality.
 */

  int r;

  /* Issue the RECALIBRATE command and wait for the interrupt. */
  start_motor(fp);		/* can't recalibrate with motor off */
  fdc_out(FDC_RECALIBRATE);	/* tell drive to recalibrate itself */
  fdc_out(fp->fl_drive);	/* specify drive */
  if (need_reset) return(ERR_SEEK);	/* don't wait if controller is sick */
  receive(HARDWARE, &mess);	/* wait for interrupt message */

  /* Determine if the recalibration succeeded. */
  fdc_out(FDC_SENSE);		/* issue SENSE command to request results */
  r = fdc_results(fp);		/* get results of the FDC_RECALIBRATE command */
  fp->fl_curcyl = -1;		/* force a SEEK next time */
  if (r != OK ||		/* controller would not respond */
     (fp->fl_results[ST0]&ST0_BITS) != SEEK_ST0 || fp->fl_results[ST_PCN] !=0){
	/* Recalibration failed.  FDC must be reset. */
	need_reset = TRUE;
	fp->fl_calibration = UNCALIBRATED;
	return(ERR_RECALIBRATE);
  } else {
	/* Recalibration succeeded. */
	fp->fl_calibration = CALIBRATED;
	if (ps||((d == 6) || (d == 3))) {/* give head time to settle on 3.5 */
		clock_mess(2, send_mess);
		receive(CLOCK, &mess);
	}
#if RECORD_FLOPPY_SKEW
/* This might be used to determine nr_sectors.  This is not quite the right
 * place for it may be called for a format operation.  Then an error is
 * normal, but kills the operation.
 */
	{
		static char skew[32];
	
		for (r = 0; r < sizeof skew / sizeof skew[0]; ++r) {
			read_id(fp);
			skew[r] = fp->fl_results[5];
		}
	}
#endif
	return(OK);
  }
}


/*===========================================================================*
 *				reset					     * 
 *===========================================================================*/
PRIVATE void f_reset()
{
/* Issue a reset to the controller.  This is done after any catastrophe,
 * like the controller refusing to respond.
 */

  int i;

  /* Disable interrupts and strobe reset bit low. */
  need_reset = FALSE;

  /* It is not clear why the next lock is needed.  Writing 0 to DOR causes
   * interrupt, while the PC documentation says turning bit 8 off disables
   * interrupts.  Without the lock:
   *   1) the interrupt handler sets the floppy mask bit in the 8259.
   *   2) writing ENABLE_INT to DOR causes the FDC to assert the interrupt
   *      line again, but the mask stops the cpu being interrupted.
   *   3) the sense interrupt clears the interrupt (not clear which one).
   * and for some reason the reset does not work.
   */
  lock();
  motor_status = 0;
  motor_goal = 0;
  out_byte(DOR, 0);		/* strobe reset bit low */
  out_byte(DOR, ENABLE_INT);	/* strobe it high again */
  unlock();
  receive(HARDWARE, &mess);	/* collect the RESET interrupt */
  fdc_out(FDC_SENSE);		/* probe FDC to make it return status */
  fdc_results(&floppy[0]);	/* flush controller using scratch structure */
  for (i=0; i<NR_DRIVES; i++)	/* Clear each drive. */
	floppy[i].fl_calibration = UNCALIBRATED;

  /* Tell FDC drive parameters. */
  fdc_out(FDC_SPECIFY);		/* specify some timing parameters */
  fdc_out(current_spec1=spec1[d]); /* step-rate and head-unload-time */
  fdc_out(SPEC2);		/* head-load-time and non-dma */
}


/*===========================================================================*
 *				clock_mess				     * 
 *===========================================================================*/
PRIVATE void clock_mess(ticks, func)
int ticks;			/* how many clock ticks to wait */
void (*func)();			/* function to call upon time out */
{
/* Send the clock task a message. */

  mess.m_type = SET_ALARM;
  mess.CLOCK_PROC_NR = FLOPPY;
  mess.DELTA_TICKS = (long) ticks;
  mess.FUNC_TO_CALL = func;
  sendrec(CLOCK, &mess);
}


/*===========================================================================*
 *				send_mess				     * 
 *===========================================================================*/
PRIVATE void send_mess()
{
/* This routine is called when the clock task has timed out on motor startup.*/

  send(FLOPPY, &mess);
}


/*==========================================================================*
 *				f_do_vrdwt				    *
 *==========================================================================*/
PRIVATE int f_do_vrdwt(m_ptr)
message *m_ptr;			/* pointer to read or write message */
{
/* Carry out a scattered read or write request. */

  int base;
  off_t block;
  int cylinder;
  int dist;
  struct floppy *fp;
  static struct iorequest_s iovec[NR_BUFS];
  phys_bytes iovec_phys;
  int last_plus1;
  int limit;
  int mindist;
  unsigned nr_requests;
  int nsector;
  int request;
  phys_bytes user_iovec_phys;
  message vmessage;

  vmessage = *m_ptr;		/* global message will be clobbered */
  m_ptr = &vmessage;

  /* Fetch i/o vector from caller's space. */
  nr_requests = m_ptr->COUNT;
  if (nr_requests > sizeof iovec / sizeof iovec[0])
	panic("FS gave floppy driver too big an i/o vector", nr_requests);
  iovec_phys = umap(proc_ptr, D, (vir_bytes) iovec, (vir_bytes) sizeof iovec);
  user_iovec_phys = numap(m_ptr->PROC_NR, (vir_bytes) m_ptr->ADDRESS,
			  (vir_bytes) (nr_requests * sizeof iovec[0]));
  if (user_iovec_phys == 0)
	panic("FS gave floppy driver bad i/o vector", (int) m_ptr->ADDRESS);
  phys_copy(user_iovec_phys, iovec_phys,
	    (phys_bytes) nr_requests * sizeof iovec[0]);

  /* Determine the number of sectors and the last sector.  It only hurts
   * efficiency if these are wrong after a disk change.
   */
  fp = &floppy[(m_ptr->DEVICE & ~(DEV_TYPE_BITS|FORMAT_DEV_BIT)) % NR_DRIVES];
  nsector = nr_sectors[fp->fl_density];
  read_id(fp);			/* should reorganize and seek() before this */
  fp->fl_sector = fp->fl_results[5];	/* last sector accessed */

  for (base = 0; base < nr_requests; base = limit) {

	/* Handle all requests on the same cylinder as the base request. */
	block = iovec[base].io_position / SECTOR_SIZE;
	cylinder = (int) (block / (NR_HEADS * nsector));

	/* Find the request for the closest sector on the base cylinder. */
	for (request = limit = base, mindist = 9999; limit < nr_requests;
	     ++limit) {
		block = iovec[limit].io_position / SECTOR_SIZE;
		if (cylinder != (int) (block / (NR_HEADS * nsector))) break;
		dist = base_sector + (int) (block % nsector) - fp->fl_sector;
		if (dist < 0) dist += nsector;
		if (dist > 0 && dist < mindist) {
			/* Closer.  Ignore dist == 0 which is furthest! */
			request = limit;
			mindist = dist;
		}
	}

	/* Do the actual i/o in the good order just found. */
	last_plus1 = (request == base) ? limit : request;
	do {
		if (request >= limit) request = base;
		if (do1_rdwt(&iovec[request], m_ptr) != OK) {
			/* Abort both loops, to avoid reading-ahead of
			 * bad blocks, especially off the end of the disk.
			 */
			request = last_plus1 - 1;
			limit = nr_requests;
		}
	} while (++request != last_plus1);

	/* Advance last sector from the base sector of the last block.
	 * Advance another sector after that to allow time for seek, except
	 * for the last sectors on a cylinder, for which the gap at the end
	 * gives enough time.
	 */
	fp->fl_sector += BLOCK_SIZE / SECTOR_SIZE;
	while (fp->fl_sector >= nsector + base_sector)
		fp->fl_sector -= nsector;
	if (fp->fl_sector == base_sector)
		fp->fl_sector = base_sector + nsector - 1;
  }

  /* Return most results in caller's i/o vector. */
  phys_copy(iovec_phys, user_iovec_phys,
	    (phys_bytes) nr_requests * sizeof iovec[0]);
  return(OK);
}


/*==========================================================================*
 *				do1_rdwt				    *
 *==========================================================================*/
PRIVATE int do1_rdwt(iop, m_ptr)
register struct iorequest_s *iop;
register message *m_ptr;
{
/* Convert from scattered i/o entry to partially built message and do i/o.
 * There are too many conversions, so to keep up on slow machines it will
 * be necessary to do more preparation at a high level, e.g., when looping
 * over sectors on the same cylinder, do not recompute the cylinder over
 * and over, and avoid all long arithmetic.
 */

  int result;
#if FLOPPY_TIMING
#define MAX_TIMED_BLOCK (HC_SIZE / (BLOCK_SIZE / SECTOR_SIZE))
  off_t block;
  static struct {
	unsigned short start;
	unsigned short finish;
  } fl_times[MAX_TIMED_BLOCK];
#endif

  m_ptr->POSITION = iop->io_position;
  m_ptr->ADDRESS = iop->io_buf;
  m_ptr->COUNT = iop->io_nbytes;
  m_ptr->m_type = iop->io_request & ~OPTIONAL_IO;
#if FLOPPY_TIMING
  block = m_ptr->POSITION/BLOCK_SIZE;
  if (block < MAX_TIMED_BLOCK) fl_times[block].start = read_counter();
#endif
  result = do_rdwt(m_ptr, iop->io_request & OPTIONAL_IO);
#if FLOPPY_TIMING
  if (block < MAX_TIMED_BLOCK) fl_times[block].finish = read_counter();
#endif
  if (result == 0) return(!OK);	/* EOF */
  if (result < 0) {
	iop->io_nbytes = result;
	if (iop->io_request & OPTIONAL_IO) return(!OK);	/* abort if optional */
  } else
	iop->io_nbytes -= result;
  return(OK);
}


/*==========================================================================*
 *				read_id					    *
 *==========================================================================*/
PRIVATE int read_id(fp)
register struct floppy *fp;	/* pointer to the drive struct */
{
/* Determine current cylinder and sector. */

  int result;

  /* Never attempt a read id if the drive is uncalibrated or motor is off. */
  if (fp->fl_calibration == UNCALIBRATED) return(ERR_READ_ID);
  if ( ( (motor_status>>(fp->fl_drive+4)) & 1) == 0) return(ERR_READ_ID);

  /* The command is issued by outputting 2 bytes to the controller chip. */
  fdc_out(FDC_READ_ID);		/* issue the read id command */
  fdc_out( (fp->fl_head << 2) | fp->fl_drive);

  /* Block, waiting for disk interrupt. */
  if (need_reset) return(ERR_READ_ID);	/* if controller is sick, abort op */
  receive(HARDWARE, &mess);

  /* Get controller status and check for errors. */
  result = fdc_results(fp);
  if (result != OK) return(result);
  if ( (fp->fl_results[ST1] & BAD_SECTOR) || (fp->fl_results[ST2] & BAD_CYL) )
	fp->fl_calibration = UNCALIBRATED;
  if ((fp->fl_results[ST0] & ST0_BITS) != TRANS_ST0) return(ERR_READ_ID);
  if (fp->fl_results[ST1] | fp->fl_results[ST2]) return(ERR_READ_ID);

  return(OK);
}
