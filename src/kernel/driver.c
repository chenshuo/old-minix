/* This file contains device independent device driver interface.
 *							Author: Kees J. Bot.
 *
 * The drivers support the following operations (using message format m2):
 *
 *    m_type      DEVICE    PROC_NR     COUNT    POSITION  ADRRESS
 * ----------------------------------------------------------------
 * |  DEV_OPEN  | device  | proc nr |         |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_CLOSE | device  | proc nr |         |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_READ  | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_WRITE | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * |SCATTERED_IO| device  | proc nr | requests|         | iov ptr |
 * ----------------------------------------------------------------
 * |  DEV_IOCTL | device  | proc nr |func code|         | buf ptr |
 * ----------------------------------------------------------------
 *
 * The file contains one entry point:
 *
 *   driver_task:	called by the device dependent task entry
 *
 *
 * Constructed 92/04/02 by Kees J. Bot from the old AT wini and floppy driver.
 */

#include "kernel.h"
#include <sys/ioctl.h>
#include "driver.h"

#if (CHIP == INTEL)
#if ENABLE_ADAPTEC_SCSI && DMA_BUF_SIZE < 2048
/* A bit extra scratch for the Adaptec driver. */
#define BUF_EXTRA	(2048 - DMA_BUF_SIZE)
#else
#define BUF_EXTRA	0
#endif

/* Claim space for variables. */
PRIVATE u8_t buffer[(unsigned) 2 * DMA_BUF_SIZE + BUF_EXTRA];
u8_t *tmp_buf;			/* the DMA buffer eventually */
phys_bytes tmp_phys;		/* phys address of DMA buffer */
u16_t Ax, Bx, Cx, Dx, Es;	/* to hold registers for BIOS calls */

FORWARD _PROTOTYPE( void extpartition, (struct driver *dp, int extdev,
						unsigned long extbase) );
FORWARD _PROTOTYPE( int get_part_table, (struct driver *dp, int device,
			unsigned long offset, struct part_entry *table) );
FORWARD _PROTOTYPE( void sort, (struct part_entry *table) );

#else /* CHIP != INTEL */

/* Claim space for variables. */
u8_t tmp_buf[DMA_BUF_SIZE];	/* the DMA buffer */
phys_bytes tmp_phys;		/* phys address of DMA buffer */

#endif /* CHIP != INTEL */

FORWARD _PROTOTYPE( void init_buffer, (void) );


/*===========================================================================*
 *				driver_task				     *
 *===========================================================================*/
PUBLIC void driver_task(dp)
struct driver *dp;	/* Device dependent entry points. */
{
/* Main program of any device driver task. */

  int r, caller, proc_nr;
  message mess;

  init_buffer();	/* Get a DMA buffer. */


  /* Here is the main loop of the disk task.  It waits for a message, carries
   * it out, and sends a reply.
   */

  while (TRUE) {
	/* First wait for a request to read or write a disk block. */
	receive(ANY, &mess);

	caller = mess.m_source;
	proc_nr = mess.PROC_NR;

	switch (caller) {
	case HARDWARE:
		/* Leftover interrupt. */
		continue;
	case FS_PROC_NR:
		/* The only legitimate caller. */
		break;
	default:
		printf("%s: got message from %d\n", (*dp->dr_name)(), caller);
		continue;
	}

	/* Now carry out the work. */
	switch(mess.m_type) {
	    case DEV_OPEN:	r = (*dp->dr_open)(dp, &mess);	break;
	    case DEV_CLOSE:	r = (*dp->dr_close)(dp, &mess);	break;
	    case DEV_IOCTL:	r = (*dp->dr_ioctl)(dp, &mess);	break;

	    case DEV_READ:
	    case DEV_WRITE:	r = do_rdwt(dp, &mess);		break;

	    case SCATTERED_IO:	r = do_vrdwt(dp, &mess);	break;
	    default:		r = EINVAL;			break;
	}

	/* Clean up leftover state. */
	(*dp->dr_cleanup)();

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
PUBLIC int do_rdwt(dp, m_ptr)
struct driver *dp;		/* device dependent entry points */
message *m_ptr;			/* pointer to read or write message */
{
/* Carry out a single read or write request. */
  struct iorequest_s ioreq;
  int r;

  if (m_ptr->COUNT <= 0) return(EINVAL);

  if ((*dp->dr_prepare)(m_ptr->DEVICE) == NIL_DEV) return(ENXIO);

  ioreq.io_request = m_ptr->m_type;
  ioreq.io_buf = m_ptr->ADDRESS;
  ioreq.io_position = m_ptr->POSITION;
  ioreq.io_nbytes = m_ptr->COUNT;

  r = (*dp->dr_schedule)(m_ptr->PROC_NR, &ioreq);

  if (r == OK) (void) (*dp->dr_finish)();

  r = ioreq.io_nbytes;
  return(r < 0 ? r : m_ptr->COUNT - r);
}


/*==========================================================================*
 *				do_vrdwt				    *
 *==========================================================================*/
PUBLIC int do_vrdwt(dp, m_ptr)
struct driver *dp;	/* device dependent entry points */
message *m_ptr;		/* pointer to read or write message */
{
/* Fetch a vector of i/o requests.  Handle requests one at a time.  Return
 * status in the vector.
 */

  struct iorequest_s *iop;
  static struct iorequest_s iovec[NR_IOREQS];
  phys_bytes iovec_phys;
  unsigned nr_requests;
  int request;
  int r;
  phys_bytes user_iovec_phys;

  nr_requests = m_ptr->COUNT;

  if (nr_requests > sizeof iovec / sizeof iovec[0])
	panic("FS passed too big an I/O vector", nr_requests);

  iovec_phys = vir2phys(iovec);
  user_iovec_phys = numap(m_ptr->PROC_NR, (vir_bytes) m_ptr->ADDRESS,
			 (vir_bytes) (nr_requests * sizeof iovec[0]));

  if (user_iovec_phys == 0)
	panic("FS passed a bad I/O vector", (int) m_ptr->ADDRESS);

  phys_copy(user_iovec_phys, iovec_phys,
			    (phys_bytes) nr_requests * sizeof iovec[0]);

  if ((*dp->dr_prepare)(m_ptr->DEVICE) == NIL_DEV) return(ENXIO);

  for (request = 0, iop = iovec; request < nr_requests; request++, iop++) {

	if ((r = (*dp->dr_schedule)(m_ptr->PROC_NR, iop)) != OK) break;
  }

  if (r == OK) (void) (*dp->dr_finish)();

  phys_copy(iovec_phys, user_iovec_phys,
			    (phys_bytes) nr_requests * sizeof iovec[0]);
  return(OK);
}


/*===========================================================================*
 *				no_name					     *
 *===========================================================================*/
PUBLIC char *no_name()
{
/* If no specific name for the device. */

  return(tasktab[proc_number(proc_ptr) + NR_TASKS].name);
}


/*============================================================================*
 *				do_nop					      *
 *============================================================================*/
PUBLIC int do_nop(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
/* Nothing there, or nothing to do. */

  switch (m_ptr->m_type) {
  case DEV_OPEN:	return(ENODEV);
  case DEV_CLOSE:	return(OK);
  case DEV_IOCTL:	return(ENOTTY);
  default:		return(EIO);
  }
}


/*===========================================================================*
 *				nop_prepare				     *
 *===========================================================================*/
PUBLIC struct device *nop_prepare(device)
{
/* Nothing to prepare for. */
  return(NIL_DEV);
}


/*===========================================================================*
 *				nop_finish				     *
 *===========================================================================*/
PUBLIC int nop_finish()
{
/* Nothing to finish, all the work has been done by dp->dr_schedule. */
  return(OK);
}


/*===========================================================================*
 *				nop_cleanup				     *
 *===========================================================================*/
PUBLIC void nop_cleanup()
{
/* Nothing to clean up. */
}


/*===========================================================================*
 *				nop_task				     *
 *===========================================================================*/
PUBLIC void nop_task()
{
/* Drivers that are configured out call this. */
  struct driver nop_tab = {
	no_name,
	do_nop,
	do_nop,
	do_nop,
	nop_prepare,
	NULL,
	nop_finish,
	nop_cleanup,
	NULL,
  };

  driver_task(&nop_tab);
}


/*===========================================================================*
 *				clock_mess				     *
 *===========================================================================*/
PUBLIC void clock_mess(ticks, func)
int ticks;			/* how many clock ticks to wait */
watchdog_t func;		/* function to call upon time out */
{
/* Send the clock task a message. */

  message mess;

  mess.m_type = SET_ALARM;
  mess.CLOCK_PROC_NR = proc_number(proc_ptr);
  mess.DELTA_TICKS = (long) ticks;
  mess.FUNC_TO_CALL = (sighandler_t) func;
  sendrec(CLOCK, &mess);
}


/*===========================================================================*
 *				init_buffer				     *
 *===========================================================================*/
PRIVATE void init_buffer()
{
/* Select a buffer that can safely be used for dma transfers.  It may also
 * be used to read partition tables and such.  Its absolute address is
 * 'tmp_phys', the normal address is 'tmp_buf'.
 */

#if (CHIP == INTEL)
  tmp_buf = buffer;
  tmp_phys = vir2phys(buffer);

  if (tmp_phys == 0) panic("no DMA buffer", NO_NUM);

  if (dma_bytes_left(tmp_phys) < DMA_BUF_SIZE) {
	/* First half of buffer crosses a 64K boundary, can't DMA into that */
	tmp_buf += DMA_BUF_SIZE;
	tmp_phys += DMA_BUF_SIZE;
  }
#else /* CHIP != INTEL */
  tmp_phys = vir2phys(tmp_buf);
#endif /* CHIP != INTEL */
}


#if (CHIP == INTEL)
/*============================================================================*
 *				partition				      *
 *============================================================================*/
PUBLIC void partition(dp, device, style)
struct driver *dp;	/* device dependent entry points */
int device;		/* device to partition */
int style;		/* partitioning style: floppy, primary, sub. */
{
/* This routine is called on first open to initialize the partition tables
 * of a device.  It makes sure that each partition falls safely within the
 * device's limits.  Depending on the partition style we are either making
 * floppy partitions, primary partitions or subpartitions.  Only primary
 * partitions are sorted, because they are shared with other operating
 * systems that expect this.
 */
  message mess;
  struct part_entry table[NR_PARTITIONS], *pe;
  int disk, par;
  struct device *dv;
  unsigned long base, limit, part_limit;

  /* Get the geometry of the device to partition */
  if ((dv = (*dp->dr_prepare)(device)) == NIL_DEV || dv->dv_size == 0) return;
  base = dv->dv_base >> SECTOR_SHIFT;
  limit = base + (dv->dv_size >> SECTOR_SHIFT);

  /* Read the partition table for the device. */
  if (!get_part_table(dp, device, 0L, table)) return;

  /* Compute the device number of the first partition. */
  switch (style) {
  case P_FLOPPY:
	device += MINOR_fd0a;
	break;
  case P_PRIMARY:
	sort(table);		/* sort a primary partition table */
	device += 1;
	break;
  case P_SUB:
	disk = device / DEV_PER_DRIVE;
	par = device % DEV_PER_DRIVE - 1;
	device = MINOR_hd1a + (disk * NR_PARTITIONS + par) * NR_PARTITIONS;
  }

  /* Find an array of devices. */
  if ((dv = (*dp->dr_prepare)(device)) == NIL_DEV) return;

  /* Set the geometry of the partitions from the partition table. */
  for (par = 0; par < NR_PARTITIONS; par++, dv++) {
	/* Shrink the partition to fit within the device. */
	pe = &table[par];
	part_limit = pe->lowsec + pe->size;
	if (part_limit < pe->lowsec) part_limit = limit;
	if (part_limit > limit) part_limit = limit;
	if (pe->lowsec < base) pe->lowsec = base;
	if (part_limit < pe->lowsec) part_limit = pe->lowsec;

	dv->dv_base = pe->lowsec << SECTOR_SHIFT;
	dv->dv_size = (part_limit - pe->lowsec) << SECTOR_SHIFT;

	if (style == P_PRIMARY) {
		/* Each Minix primary partition can be subpartitioned. */
		if (pe->sysind == MINIX_PART)
			partition(dp, device + par, P_SUB);

		/* An extended partition has logical partitions. */
		if (pe->sysind == EXT_PART)
			extpartition(dp, device + par, dv->dv_base);
	}
  }
}


/*============================================================================*
 *				extpartition				      *
 *============================================================================*/
PRIVATE void extpartition(dp, extdev, extbase)
struct driver *dp;	/* device dependent entry points */
int extdev;		/* extended partition to scan */
unsigned long extbase;	/* offset of the base extended partition */
{
/* Extended partitions cannot be ignored alas, because people like to move
 * files to and from DOS partitions.  Avoid reading this code, it's no fun.
 */
  message mess;
  struct part_entry table[NR_PARTITIONS], *pe;
  int subdev, disk, par;
  struct device *dv;
  unsigned long offset, nextoffset;

  disk = extdev / DEV_PER_DRIVE;
  par = extdev % DEV_PER_DRIVE - 1;
  subdev = MINOR_hd1a + (disk * NR_PARTITIONS + par) * NR_PARTITIONS;

  offset = 0;
  do {
	if (!get_part_table(dp, extdev, offset, table)) return;
	sort(table);

	/* The table should contain one logical partition and optionally
	 * another extended partition.  (It's a linked list.)
	 */
	nextoffset = 0;
	for (par = 0; par < NR_PARTITIONS; par++) {
		pe = &table[par];
		if (pe->sysind == EXT_PART) {
			nextoffset = pe->lowsec << SECTOR_SHIFT;
		} else
		if (pe->sysind != NO_PART) {
			if ((dv = (*dp->dr_prepare)(subdev)) == NIL_DEV) return;

			dv->dv_base = extbase + offset
					+ (pe->lowsec << SECTOR_SHIFT);
			dv->dv_size = pe->size << SECTOR_SHIFT;

			/* Out of devices? */
			if (++subdev % NR_PARTITIONS == 0) return;
		}
	}
  } while ((offset = nextoffset) != 0);
}


/*============================================================================*
 *				get_part_table				      *
 *============================================================================*/
PRIVATE int get_part_table(dp, device, offset, table)
struct driver *dp;
int device;
unsigned long offset;		/* offset to the table */
struct part_entry *table;	/* four entries */
{
/* Read the partition table for the device, return true iff there were no
 * errors.
 */
  message mess;

  mess.DEVICE = device;
  mess.POSITION = offset;
  mess.COUNT = SECTOR_SIZE;
  mess.ADDRESS = (char *) tmp_buf;
  mess.PROC_NR = proc_number(proc_ptr);
  mess.m_type = DEV_READ;

  if (do_rdwt(dp, &mess) != SECTOR_SIZE) {
	printf("%s: can't read partition table\n", (*dp->dr_name)());
	return 0;
  }
  if (tmp_buf[510] != 0x55 || tmp_buf[511] != 0xAA) {
	/* Invalid partition table. */
	return 0;
  }
  memcpy(table, (tmp_buf + PART_TABLE_OFF), NR_PARTITIONS * sizeof(table[0]));
  return 1;
}


/*===========================================================================*
 *				sort					     *
 *===========================================================================*/
PRIVATE void sort(table)
struct part_entry *table;
{
/* Sort a partition table. */
  struct part_entry *pe, tmp;
  int n = NR_PARTITIONS;

  do {
	for (pe = table; pe < table + NR_PARTITIONS-1; pe++) {
		if (pe[0].sysind == NO_PART
			|| (pe[0].lowsec > pe[1].lowsec
					&& pe[1].sysind != NO_PART)) {
			tmp = pe[0]; pe[0] = pe[1]; pe[1] = tmp;
		}
	}
  } while (--n > 0);
}


/*============================================================================*
 *				do_diocntl				      *
 *============================================================================*/
PUBLIC int do_diocntl(dp, m_ptr)
struct driver *dp;
message *m_ptr;			/* pointer to ioctl request */
{
/* Carry out a partition setting/getting request. */
  struct device *dv;
  phys_bytes user_phys, entry_phys;
  struct part_entry entry;
  unsigned chs[3];

  if (m_ptr->REQUEST != DIOCSETP && m_ptr->REQUEST != DIOCGETP) return(ENOTTY);

  /* Decode the message parameters. */
  if ((dv = (*dp->dr_prepare)(m_ptr->DEVICE)) == NIL_DEV) return(ENXIO);

  user_phys = numap(m_ptr->PROC_NR, (vir_bytes) m_ptr->ADDRESS, sizeof(entry));
  if (user_phys == 0) return(EFAULT);

  entry_phys = vir2phys(&entry);

  if (m_ptr->REQUEST == DIOCSETP) {
	/* Copy just this one partition table entry. */
	phys_copy(user_phys, entry_phys, (phys_bytes) sizeof(entry));
	dv->dv_base = entry.lowsec << SECTOR_SHIFT;
	dv->dv_size = entry.size << SECTOR_SHIFT;
  } else {
	/* Return a partition table entry and the geometry of the drive. */
	entry.lowsec = dv->dv_base >> SECTOR_SHIFT;
	entry.size = dv->dv_size >> SECTOR_SHIFT;
	(*dp->dr_geometry)(chs);
	entry.last_cyl = (chs[0]-1) & 0xFF;
	entry.last_head = chs[1]-1;
	entry.last_sec = chs[2] | (((chs[0]-1) >> 2) & 0xC0);
	phys_copy(entry_phys, user_phys, (phys_bytes) sizeof(entry));
  }
  return(OK);
}
#endif /* CHIP == INTEL */
