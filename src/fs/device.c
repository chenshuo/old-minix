/* When a needed block is not in the cache, it must be fetched from the disk.
 * Special character files also require I/O.  The routines for these are here.
 *
 * The entry points in this file are:
 *   dev_io:	 perform a read or write on a block or character device
 *   dev_opcl:   perform generic device-specific processing for open & close
 *   tty_open:   perform tty-specific processing for open
 *   tty_close:  perform tty-specific processing for close
 *   ctty_open:  perform controlling-tty-specific processing for open
 *   ctty_close: perform controlling-tty-specific processing for close
 *   do_ioctl:	 perform the IOCTL system call
 *   call_task:	 procedure that actually calls the kernel tasks
 *   rw_dev2:	 procedure that actually calls task for /dev/tty
 *   no_call:	 dummy procedure (e.g., used when device need not be opened)
 */

#include "fs.h"
#include <fcntl.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include "dev.h"
#include "file.h"
#include "fproc.h"
#include "inode.h"
#include "param.h"

PRIVATE message dev_mess;
PRIVATE major, minor, task;

FORWARD _PROTOTYPE( void find_dev, (Dev_t dev)				);

/*===========================================================================*
 *				dev_io					     *
 *===========================================================================*/
PUBLIC int dev_io(op, nonblock, dev, pos, bytes, proc, buff)
int op;				/* DEV_READ, DEV_WRITE, DEV_IOCTL, etc. */
int nonblock;			/* TRUE if nonblocking op */
dev_t dev;			/* major-minor device number */
off_t pos;			/* byte position */
int bytes;			/* how many bytes to transfer */
int proc;			/* in whose address space is buff? */
char *buff;			/* virtual address of the buffer */
{
/* Read or write from a device.  The parameter 'dev' tells which one. */

  find_dev(dev);		/* load the variables major, minor, and task */

  /* Set up the message passed to task. */
  dev_mess.m_type   = op;
  dev_mess.DEVICE   = (dev >> MINOR) & BYTE;
  dev_mess.POSITION = pos;
  dev_mess.PROC_NR  = proc;
  dev_mess.ADDRESS  = buff;
  dev_mess.COUNT    = bytes;
  dev_mess.TTY_FLAGS = nonblock; /* temporary kludge */

  /* Call the task. */
  (*dmap[major].dmap_rw)(task, &dev_mess);

  /* Task has completed.  See if call completed. */
  if (dev_mess.REP_STATUS == SUSPEND) {
	if (op == DEV_OPEN) task = XPOPEN;
	suspend(task);		/* suspend user */
  }

  return(dev_mess.REP_STATUS);
}


/*===========================================================================*
 *				dev_opcl				     *
 *===========================================================================*/
PUBLIC void dev_opcl(task_nr, mess_ptr)
int task_nr;			/* which task */
message *mess_ptr;		/* message pointer */
{
/* Called from the dmap struct in table.c on opens & closes of special files.*/

  int op;

  op = mess_ptr->m_type;	/* save DEV_OPEN or DEV_CLOSE for later */
  mess_ptr->DEVICE = (mess_ptr->DEVICE >> MINOR) & BYTE;
  mess_ptr->PROC_NR = fp - fproc;

  /* The interface could be greatly improved by using 2 functions,
   * dev_open(Dev_t dev, Mode_t mode) and dev_close(Dev_t dev) instead of
   * pseudo-message passing.  The conversion of the dev to major+minor+
   * task+function would be done here (and there would be another level
   * of function calls).  Checking the dev here (in find_dev) would stop
   * main.c crashing badly from bad boot/root dev numbers.
   *
   * Other problems: the printer driver is not called for open/close
   * processing.  It should be.
   *
   * find_dev should return ENODEV for bad devices and that result
   * should be tested for and passed back in the message (or in the
   * new function interface).  do_mount should respect this code, or
   * whatever is returned by the driver, and not convert it to EINVAL.
   *
   * This reminds me that EINVAL is a funny code for umount to return
   * when the dev is not mounted.
   */
  call_task(task_nr, mess_ptr);

  /* Task has completed.  See if call completed. */
  if (mess_ptr->REP_STATUS == SUSPEND) {
	if (op == DEV_OPEN) task_nr = XPOPEN;
	suspend(task_nr);	/* suspend user */
  }
}

/*===========================================================================*
 *				tty_open				     *
 *===========================================================================*/
PUBLIC void tty_open(task_nr, mess_ptr)
int task_nr;
message *mess_ptr;
{
/* This procedure is called from the dmap struct in table.c on tty opens. */
  
  int r, ldr, ctl;
  dev_t dev;
  int ncount, proc;

  ldr = fp && (fp->fp_pid == fp->fp_pgrp);   /* is this a process grp leader?*/
  dev = (dev_t) mess_ptr->DEVICE;
  ncount = mess_ptr->COUNT;
  proc = fp - fproc;
  r = dev_io(DEV_OPEN, mode, dev, (off_t) ldr, ncount, proc, NIL_PTR);

  find_dev(dev);
  ctl = (major << MAJOR) | (r << MINOR);
  if ((! fp->fs_tty) && (r != NO_CTL_TTY)) fp->fs_tty = ctl;
  mess_ptr->REP_STATUS = OK;
}


/*===========================================================================*
 *				tty_close				     *
 *===========================================================================*/
PUBLIC void tty_close(task_nr, mess_ptr)
int task_nr;
message *mess_ptr;
{
/* This procedure is called from the dmap struct in table.c on tty closes.
 * When a process exits, MM calls FS to close all files, including stdin,
 * stdout, and stderr.  For these calls, who = MM, so we use fp-fproc in 
 * dev_io.
 */

  int r, ldr;
  dev_t dev;
  struct fproc *rfp;
  int ncount, proc;
  struct inode *ino;

  ncount=0;
  for (r=0; r<OPEN_MAX; r++) {
	if (r == fd) continue;
	if (fproc[fp-fproc].fp_filp[r] == NIL_FILP) continue;
	ino=fproc[fp-fproc].fp_filp[r]->filp_ino;
	if (ino == fproc[fp-fproc].fp_filp[fd]->filp_ino) ncount++;
  }
  ldr = (ncount == 0);   /* Is this the last ctty file to close? */
  dev = (dev_t) mess_ptr->DEVICE;
  ncount= mess_ptr->COUNT;
  proc = fp -fproc;
  r = dev_io(DEV_CLOSE, mode, dev, (off_t) ldr, ncount, proc, NIL_PTR);
  if ((r == NO_CTL_TTY) && (ldr)) {
	for (rfp = &fproc[INIT_PROC_NR + 1]; rfp < &fproc[NR_PROCS]; rfp++) {
		if (rfp->fs_tty == dev) rfp->fs_tty = 0;
	}
  }
}


/*===========================================================================*
 *				ctty_open				     *
 *===========================================================================*/
PUBLIC void ctty_open(task_nr, mess_ptr)
int task_nr;
message *mess_ptr;
{
/* This procedure is called from the dmap struct in table.c on controlling
 * tty opens.
 */
  
  if (fp->fs_tty == 0) { /* no controlling tty present; deny open */
	mess_ptr->REP_STATUS = ENXIO;
	return;
  }
  mess_ptr->REP_STATUS = OK;
}


/*===========================================================================*
 *				ctty_close				     *
 *===========================================================================*/
PUBLIC void ctty_close(task_nr, mess_ptr)
int task_nr;
message *mess_ptr;
{
/* This procedure is called from the dmap struct in table.c on controlling
 * tty closes.
 * When a process exits, MM calls FS to close all files, including stdin,
 * stdout, and stderr.  For these calls, who = MM, so we use fp-fproc in 
 * dev_io.
 */
}


/*===========================================================================*
 *				do_ioctl				     *
 *===========================================================================*/
PUBLIC int do_ioctl()
{
/* Perform the ioctl(ls_fd, request, argx) system call (uses m2 fmt). */

  struct filp *f;
  register struct inode *rip;
  dev_t dev;

  if ( (f = get_filp(ls_fd)) == NIL_FILP) return(err_code);
  rip = f->filp_ino;		/* get inode pointer */
  if ( (rip->i_mode & I_TYPE) != I_CHAR_SPECIAL
	&& (rip->i_mode & I_TYPE) != I_BLOCK_SPECIAL) return(ENOTTY);
  dev = (dev_t) rip->i_zone[0];
  find_dev(dev);

  dev_mess= m;

  dev_mess.m_type  = DEV_IOCTL;
  dev_mess.PROC_NR = who;
  dev_mess.TTY_LINE = minor;	

  /* Call the task. */
  (*dmap[major].dmap_rw)(task, &dev_mess);

  /* Task has completed.  See if call completed. */
  if (dev_mess.REP_STATUS == SUSPEND) {
	if (f->filp_flags & O_NONBLOCK) {
		/* Not supposed to block. */
		dev_mess.m_type = CANCEL;
		dev_mess.PROC_NR = who;
		dev_mess.TTY_LINE = minor;
		(*dmap[major].dmap_rw)(task, &dev_mess);
		if (dev_mess.REP_STATUS == EINTR) dev_mess.REP_STATUS = EAGAIN;
	} else {
		suspend(task);
	}
  }
					/* User must be suspended. */
  m1.TTY_SPEK = dev_mess.TTY_SPEK;	/* erase and kill */
  m1.TTY_FLAGS = dev_mess.TTY_FLAGS;	/* flags */
  return(dev_mess.REP_STATUS);
}


/*===========================================================================*
 *				find_dev				     *
 *===========================================================================*/
PRIVATE void find_dev(dev)
dev_t dev;			/* device */
{
/* Extract the major and minor device number from the parameter. */

  major = (dev >> MAJOR) & BYTE;	/* major device number */
  minor = (dev >> MINOR) & BYTE;	/* minor device number */
  if (major >= max_major) {
	major = minor = 0;		/* will fail with ENODEV */
  }
  task = dmap[major].dmap_task;	/* which task services the device */
}


/*===========================================================================*
 *				call_task				     *
 *===========================================================================*/
PUBLIC void call_task(task_nr, mess_ptr)
int task_nr;			/* which task to call */
message *mess_ptr;		/* pointer to message for task */
{
/* All file system I/O ultimately comes down to I/O on major/minor device
 * pairs.  These lead to calls on the following routines via the dmap table.
 */

  int r;
  message local_m;

  while ((r = sendrec(task_nr, mess_ptr)) == ELOCKED) {
	/* sendrec() failed to avoid deadlock. The task 'task_nr' is
	 * trying to send a REVIVE message for an earlier request.
	 * Handle it and go try again.
	 */
	if (receive(task_nr, &local_m) != OK)
		panic("call_task: can't receive", NO_NUM);

	/* If we're trying to send a cancel message to a task which has just
	 * sent a completion reply, ignore the reply and abort the cancel
	 * request. The caller will do the revive for the process. 
	 */
	if (mess_ptr->m_type == CANCEL
	    && local_m.REP_PROC_NR == mess_ptr->PROC_NR)
		return;
	revive(local_m.REP_PROC_NR, local_m.REP_STATUS);
  }
  if (r != OK) panic("call_task: can't send", NO_NUM);
}


/*===========================================================================*
 *				rw_dev2					     *
 *===========================================================================*/
PUBLIC void rw_dev2(task_nr, mess_ptr)
int task_nr;			/* not used - for compatibility with dmap_t */
message *mess_ptr;		/* pointer to message for task */
{
/* This routine is only called for one device, namely /dev/tty.  Its job
 * is to change the message to use the controlling terminal, instead of the
 * major/minor pair for /dev/tty itself.
 */

  int major_device;
 
  if (fp->fs_tty == 0) { /* no controlling tty present; deny open */
	if (mess_ptr->m_type == DEV_READ) {
	    mess_ptr->REP_STATUS = 0; 	/* 0 bytes read; EOF */
	} else {
	    mess_ptr->REP_STATUS = EIO;
	}
	return;
  }
  major_device = (fp->fs_tty >> MAJOR) & BYTE;
  task_nr = dmap[major_device].dmap_task;	/* task for controlling tty */
  mess_ptr->DEVICE = (fp->fs_tty >> MINOR) & BYTE;
  call_task(task_nr, mess_ptr);
}


/*===========================================================================*
 *				no_call					     *
 *===========================================================================*/
PUBLIC void no_call(task_nr, m_ptr)
int task_nr;			/* not used - for compatibility with dmap_t */
message *m_ptr;			/* message pointer */
{
/* Null operation always succeeds. */

  m_ptr->REP_STATUS = OK;
}


/*===========================================================================*
 *				no_dev					     *
 *===========================================================================*/
PUBLIC void no_dev(task_nr, m_ptr)
int task_nr;			/* not used - for compatibility with dmap_t */
message *m_ptr;			/* message pointer */
{
/* No device there. */

  m_ptr->REP_STATUS = ENODEV;
}


#if ENABLE_NETWORKING
/*===========================================================================*
 *				net_open				     *
 *===========================================================================*/
PUBLIC void net_open(task_nr, mess_ptr)
int task_nr;			/* task to send message to */
message *mess_ptr;		/* pointer to message to send */
{
/* network files may need special processing upon open. */

  dev_t dev;
  struct inode *rip, *nrip;
  int result;
  int ncount, proc;

  rip= fp->fp_filp[fd]->filp_ino; 

  nrip= alloc_inode(rip->i_dev, ALL_MODES | I_CHAR_SPECIAL);
  if (nrip == NIL_INODE) {
	mess_ptr->REP_STATUS= err_code;
	return;
  }

  dev = (dev_t) mess_ptr->DEVICE;
  ncount= mess_ptr->COUNT;
  proc = fp - fproc;
  result= dev_io(DEV_OPEN, mode, dev, (off_t) 0, ncount, proc, NIL_PTR);

  if (result < 0)
  {
	put_inode(nrip);
	mess_ptr->REP_STATUS= result;
	return;
  }

  dev= rip->i_zone[0]; 
  dev= (dev & ~(BYTE << MINOR)) | ((result & BYTE) << MINOR); 

  nrip->i_zone[0]= dev;
  put_inode (rip);
  fp->fp_filp[fd]->filp_ino= nrip;
  mess_ptr->REP_STATUS= OK;
}

/*===========================================================================*
 *				net_rw					     *
 *===========================================================================*/
PUBLIC void net_rw(task_nr, mess_ptr)
int task_nr;			/* which task to call */
message *mess_ptr;		/* pointer to message for task */
{
/* All file system I/O ultimately comes down to I/O on major/minor device
 * pairs.  These lead to calls on the following routines via the dmap table.
 */

  int r, proc_nr;
  message m;

  do {
	r = send (task_nr, mess_ptr);
	if (r != ELOCKED) break;

	if (receive(task_nr, &m) != OK) panic("net_rw: can't receive", NO_NUM);
	if (mess_ptr->m_type == CANCEL && m.REP_PROC_NR == mess_ptr->PROC_NR)
		return;

	if (m.m_type != REVIVE)	{
		printf("%s, %d: got a strange message (m_type == %d)\n",
			__FILE__, __LINE__, m.m_type);
		continue;
	}
	revive(m.REP_PROC_NR, m.REP_STATUS);
  } while (TRUE);

  if (r != OK) {
	printf("send(%d, ...)= %d\r\n", task_nr, r);
	panic("net_rw: can't send", NO_NUM);
  }

  proc_nr= mess_ptr->PROC_NR;
  while (TRUE) {
	r = receive (task_nr, mess_ptr);
	
	if (r != OK) panic("net_rw: can't receive", NO_NUM);

  	if (mess_ptr->REP_PROC_NR == proc_nr)
		return;

	if (mess_ptr->m_type != REVIVE) {
		printf("%s, %d: got a strange message for %d (m_type == %d)\n",
		  __FILE__, __LINE__, mess_ptr->REP_PROC_NR, mess_ptr->m_type);
		printf("REP_PROC_NR= %d, expecting %d\n",mess_ptr->REP_PROC_NR,
			proc_nr);
		continue;
	}
	revive(mess_ptr->REP_PROC_NR, mess_ptr->REP_STATUS);
  }
}

PUBLIC void net_close (task_nr, mess_ptr)
int task_nr;
message *mess_ptr;
{
  dev_t dev;
  int ncount, proc;

  dev = (dev_t) mess_ptr->DEVICE;
  ncount= mess_ptr->COUNT;
  proc = fp - fproc;
  if (ncount) {
	/* more users present */
	mess_ptr->REP_STATUS= OK;
	return;
  }
  (void) dev_io(DEV_CLOSE, mode, dev, (off_t) 0, ncount, proc, NIL_PTR);
}
#endif
