/* When a needed block is not in the cache, it must be fetched from the disk.
 * Special character files also require I/O.  The routines for these are here.
 *
 * The entry points in this file are:
 *   dev_open:	 called when a special file is opened
 *   dev_close:  called when a special file is closed
 *   dev_io:	 perform a read or write on a block or character device
 *   do_ioctl:	 perform the IOCTL system call
 *   rw_dev:	 procedure that actually calls the kernel tasks
 *   rw_dev2:	 procedure that actually calls task for /dev/tty
 *   no_call:	 dummy procedure (e.g., used when device need not be opened)
 *   tty_open:   a tty has been opened
 *   tty_exit:   a process with pid=pgrp has exited.
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/com.h"
#include "../h/error.h"
#include "const.h"
#include "type.h"
#include "dev.h"
#include "file.h"
#include "fproc.h"
#include "glo.h"
#include "inode.h"
#include "param.h"

PRIVATE message dev_mess;
PRIVATE major, minor, task;
extern max_major;

/*===========================================================================*
 *				dev_open				     *
 *===========================================================================*/
PUBLIC int dev_open(dev, mod)
dev_nr dev;			/* which device to open */
int mod;			/* how to open it */
{
/* Special files may need special processing upon open. */

  find_dev(dev);
  dev_mess.DEVICE = dev;
  (*dmap[major].dmap_open)(task, &dev_mess);
  return(dev_mess.REP_STATUS);
}


/*===========================================================================*
 *				dev_close				     *
 *===========================================================================*/
PUBLIC dev_close(dev)
dev_nr dev;			/* which device to close */
{
/* This procedure can be used when a special file needs to be closed. */

  find_dev(dev);
  (*dmap[major].dmap_close)(task, &dev_mess);
}


/*===========================================================================*
 *				dev_io					     *
 *===========================================================================*/
PUBLIC int dev_io(rw_flag, dev, pos, bytes, proc, buff)
int rw_flag;			/* READING or WRITING */
dev_nr dev;			/* major-minor device number */
long pos;			/* byte position */
int bytes;			/* how many bytes to transfer */
int proc;			/* in whose address space is buff? */
char *buff;			/* virtual address of the buffer */
{
/* Read or write from a device.  The parameter 'dev' tells which one. */

  find_dev(dev);

  /* Set up the message passed to task. */
  dev_mess.m_type   = (rw_flag == READING ? DISK_READ : DISK_WRITE);
  dev_mess.DEVICE   = (dev >> MINOR) & BYTE;
  dev_mess.POSITION = pos;
  dev_mess.PROC_NR  = proc;
  dev_mess.ADDRESS  = buff;
  dev_mess.COUNT    = bytes;

  /* Call the task. */
  (*dmap[major].dmap_rw)(task, &dev_mess);

  /* Task has completed.  See if call completed. */
  if (dev_mess.REP_STATUS == SUSPEND) suspend(task);	/* suspend user */

  return(dev_mess.REP_STATUS);
}


/*===========================================================================*
 *				do_ioctl				     *
 *===========================================================================*/
PUBLIC do_ioctl()
{
/* Perform the ioctl(ls_fd, request, argx) system call (uses m2 fmt). */

  struct filp *f;
  register struct inode *rip;
  extern struct filp *get_filp();

  if ( (f = get_filp(ls_fd)) == NIL_FILP) return(err_code);
  rip = f->filp_ino;		/* get inode pointer */
  if ( (rip->i_mode & I_TYPE) != I_CHAR_SPECIAL) return(ENOTTY);
  find_dev(rip->i_zone[0]);

  dev_mess.m_type  = TTY_IOCTL;
  dev_mess.PROC_NR = who;
  dev_mess.TTY_LINE = minor;	
  dev_mess.TTY_REQUEST = m.TTY_REQUEST;
  dev_mess.TTY_SPEED = m.TTY_SPEED;
  dev_mess.TTY_SPEK = m.TTY_SPEK;
  dev_mess.TTY_FLAGS = m.TTY_FLAGS;

  /* Call the task. */
  (*dmap[major].dmap_rw)(task, &dev_mess);

  /* Task has completed.  See if call completed. */
  if (dev_mess.m_type == SUSPEND) suspend(task);  /* User must be suspended. */
  m1.TTY_SPEK = dev_mess.TTY_SPEK;	/* erase and kill */
  m1.TTY_FLAGS = dev_mess.TTY_FLAGS;	/* flags */
  return(dev_mess.REP_STATUS);
}


/*===========================================================================*
 *				find_dev				     *
 *===========================================================================*/
PRIVATE find_dev(dev)
dev_nr dev;			/* device */
{
/* Extract the major and minor device number from the parameter. */

  major = (dev >> MAJOR) & BYTE;	/* major device number */
  minor = (dev >> MINOR) & BYTE;	/* minor device number */
  if (major == 0 || major >= max_major) panic("bad major dev", major);
  task = dmap[major].dmap_task;	/* which task services the device */
}


/*===========================================================================*
 *				rw_dev					     *
 *===========================================================================*/
PUBLIC rw_dev(task_nr, mess_ptr)
int task_nr;			/* which task to call */
message *mess_ptr;		/* pointer to message for task */
{
/* All file system I/O ultimately comes down to I/O on major/minor device
 * pairs.  These lead to calls on the following routines via the dmap table.
 */
  int r;
  message m;

  while ((r = sendrec(task_nr, mess_ptr)) == E_LOCKED) {
	/* sendrec() failed to avoid deadlock. The task 'task_nr' is
	 * trying to send a REVIVE message for an earlier request.
	 * Handle it and go try again.
	 */
	if (receive(task_nr, &m) != OK) panic("rw_dev: can't receive", NO_NUM);
	revive(m.REP_PROC_NR, m.REP_STATUS);
  }
  if (r != OK) panic("rw_dev: can't send", NO_NUM);
}


/*===========================================================================*
 *				rw_dev2					     *
 *===========================================================================*/
PUBLIC rw_dev2(dummy, mess_ptr)
int dummy;			/* not used - for compatibility with rw_dev() */
message *mess_ptr;		/* pointer to message for task */
{
/* This routine is only called for one device, namely /dev/tty.  Its job
 * is to change the message to use the controlling terminal, instead of the
 * major/minor pair for /dev/tty itself.
 */

  int task_nr, major_device;

  if (fp->fs_tty == 0) {
	mess_ptr->DEVICE = NULL_DEV;
	rw_dev(MEM, mess_ptr);
	return;
  }
  major_device = (fp->fs_tty >> MAJOR) & BYTE;
  task_nr = dmap[major_device].dmap_task;	/* task for controlling tty */
  mess_ptr->DEVICE = (fp->fs_tty >> MINOR) & BYTE;
  rw_dev(task_nr, mess_ptr);
}


/*===========================================================================*
 *				no_call					     *
 *===========================================================================*/
PUBLIC int no_call(task_nr, m_ptr)
int task_nr;			/* which task */
message *m_ptr;			/* message pointer */
{
/* Null operation always succeeds. */

  m_ptr->REP_STATUS = OK;
}

/*===========================================================================*
 *				tty_open				     *
 *===========================================================================*/
PUBLIC tty_open(task_nr, mess_ptr)
int task_nr;
message *mess_ptr;
{
  register struct fproc *rfp;
  int major;

  mess_ptr->REP_STATUS = OK;

  /* Is this a process group leader? */
  if (fp->fp_pid != fp->fp_pgrp) return;

  /* Is there a current control terminal? */
  if (fp->fs_tty != 0) return;
  /* Is this one already allocated to another process? */
  for (rfp = &fproc[INIT_PROC_NR + 1]; rfp < &fproc[NR_PROCS]; rfp++)
	if (rfp->fs_tty == mess_ptr->DEVICE) return;

  /* All conditions satisfied.  Make this a control terminal. */
  fp->fs_tty = mess_ptr->DEVICE;
  major = (mess_ptr->DEVICE >> MAJOR) & BYTE;
  mess_ptr->DEVICE = (mess_ptr->DEVICE >> MINOR) & BYTE;
  mess_ptr->m_type = TTY_SETPGRP;
  mess_ptr->PROC_NR = who;
  mess_ptr->TTY_PGRP = who;
  (*dmap[major].dmap_rw)(task_nr, mess_ptr);
}

/*===========================================================================*
 *				tty_exit				     *
 *===========================================================================*/
PUBLIC tty_exit()
{
/* Process group leader exits. Remove its control terminal
 * from any processes currently running.
 */

  register struct fproc *rfp;
  register dev_nr ttydev;

  ttydev = fp->fs_tty;
  for (rfp = &fproc[INIT_PROC_NR + 1]; rfp < &fproc[NR_PROCS]; rfp++)
	if (rfp->fs_tty == ttydev)
		rfp->fs_tty = 0;
  /* Inform the terminal driver. */
  find_dev(ttydev);
  dev_mess.m_type = TTY_SETPGRP;
  dev_mess.DEVICE = (ttydev >> MINOR) & BYTE;
  dev_mess.PROC_NR = who;
  dev_mess.TTY_PGRP = 0;
  (*dmap[major].dmap_rw)(task, &dev_mess);
  return(OK);
}

