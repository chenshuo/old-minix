/* This file contains a collection of miscellaneous procedures.  Some of them
 * perform simple system calls.  Some others do a little part of system calls
 * that are mostly performed by the Memory Manager.
 *
 * The entry points into this file are
 *   do_dup:	  perform the DUP system call
 *   do_fcntl:	  perform the FCNTL system call
 *   lock_revive: revive processes when a lock is released
 *   do_sync:	  perform the SYNC system call
 *   do_fork:	  adjust the tables after MM has performed a FORK system call
 *   do_exec:	  handle files with FD_CLOEXEC on after MM has done an EXEC
 *   do_exit:	  a process has exited; note that in the tables
 *   do_set:	  set uid or gid for some process
 *   do_revive:	  revive a process that was waiting for something (e.g. TTY)
 */

#include "fs.h"
#include <fcntl.h>
#include <tiny-unistd.h>	/* cc runs out of memory with unistd.h :-( */
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/boot.h>
#include "buf.h"
#include "file.h"
#include "fproc.h"
#include "inode.h"
#include "param.h"

FORWARD _PROTOTYPE( int lock_op, (struct filp *f, int req)		);


/*===========================================================================*
 *				do_dup					     *
 *===========================================================================*/
PUBLIC int do_dup()
{
/* Perform the dup(fd) or dup(fd,fd2) system call. These system calls are
 * obsolete.  In fact, it is not even possible to invoke them using the
 * current library because the library routines call fcntl().  They are
 * provided to permit old binary programs to continue to run.
 */

  register int rfd;
  register struct filp *f;
  struct filp *dummy;
  int r;

  /* Is the file descriptor valid? */
  rfd = fd & ~DUP_MASK;		/* kill off dup2 bit, if on */
  if ((f = get_filp(rfd)) == NIL_FILP) return(err_code);

  /* Distinguish between dup and dup2. */
  if (fd == rfd) {			/* bit not on */
	/* dup(fd) */
	if ( (r = get_fd(0, 0, &fd2, &dummy)) != OK) return(r);
  } else {
	/* dup2(fd, fd2) */
	if (fd2 < 0 || fd2 >= OPEN_MAX) return(EBADF);
	if (rfd == fd2) return(fd2);	/* ignore the call: dup2(x, x) */
	fd = fd2;		/* prepare to close fd2 */
	(void) do_close();	/* cannot fail */
  }

  /* Success. Set up new file descriptors. */
  f->filp_count++;
  fp->fp_filp[fd2] = f;
  return(fd2);
}

/*===========================================================================*
 *				do_fcntl				     *
 *===========================================================================*/
PUBLIC int do_fcntl()
{
/* Perform the fcntl(fd, request, ...) system call. */

  register struct filp *f;
  int new_fd, r, fl;
  long cloexec_mask;		/* bit map for the FD_CLOEXEC flag */
  long clo_value;		/* FD_CLOEXEC flag in proper position */
  struct filp *dummy;

  /* Is the file descriptor valid? */
  if ((f = get_filp(fd)) == NIL_FILP) return(err_code);

  switch (request) {
     case F_DUPFD: 
	/* This replaces the old dup() system call. */
	if (addr < 0 || addr >= OPEN_MAX) return(EINVAL);
	if ((r = get_fd(addr, 0, &new_fd, &dummy)) != OK) return(r);
   	f->filp_count++;
  	fp->fp_filp[new_fd] = f;
  	return(new_fd);

     case F_GETFD: 
	/* Get close-on-exec flag (FD_CLOEXEC in POSIX Table 6-2). */
	return( ((fp->fp_cloexec >> fd) & 01) ? FD_CLOEXEC : 0);

     case F_SETFD: 
	/* Set close-on-exec flag (FD_CLOEXEC in POSIX Table 6-2). */
	cloexec_mask = 1L << fd;	/* singleton set position ok */
	clo_value = (addr & FD_CLOEXEC ? cloexec_mask : 0L);
	fp->fp_cloexec = (fp->fp_cloexec & ~cloexec_mask) | clo_value;
	return(OK);

     case F_GETFL: 
	/* Get file status flags (O_NONBLOCK and O_APPEND). */
	fl = f->filp_flags & (O_NONBLOCK | O_APPEND | O_ACCMODE);
	return(fl);	

     case F_SETFL: 
	/* Set file status flags (O_NONBLOCK and O_APPEND). */
	fl = O_NONBLOCK | O_APPEND;
	f->filp_flags = (f->filp_flags & ~fl) | (addr & fl);
	return(OK);

     case F_GETLK:
     case F_SETLK:
     case F_SETLKW:
	/* Set or clear a file lock. */
	r = lock_op(f, request);
	return(r);

     default:
	return(EINVAL);
  }
}


/*===========================================================================*
 *				lock_op					     *
 *===========================================================================*/
PRIVATE int lock_op(f, req)
struct filp *f;
int req;			/* either F_SETLK or F_SETLKW */
{
/* Perform the advisory locking required by POSIX. */

  int r, ltype, i, conflict = 0, unlocking = 0;
  mode_t mo;
  off_t first, last;
  struct flock flock;
  vir_bytes user_flock, flock_size;
  struct file_lock *flp, *flp2, *empty;

  /* Fetch the flock structure from user space. */
  flock_size = (vir_bytes) sizeof(struct flock);
  user_flock = (vir_bytes) name1;
  r = rw_user(D, who, user_flock, flock_size, (char *) &flock, FROM_USER);
  if (r != OK) return(EINVAL);

  /* Make some error checks. */
  ltype = flock.l_type;
  mo = f->filp_mode;
  if (ltype != F_UNLCK && ltype != F_RDLCK && ltype != F_WRLCK) return(EINVAL);
  if (req == F_GETLK && ltype == F_UNLCK) return(EINVAL);
  if ( (f->filp_ino->i_mode & I_TYPE) != I_REGULAR) return(EINVAL);
  if (req != F_GETLK && ltype == F_RDLCK && (mo & R_BIT) == 0) return(EBADF);
  if (req != F_GETLK && ltype == F_WRLCK && (mo & W_BIT) == 0) return(EBADF);

  /* Compute the first and last bytes in the lock region. */
  switch (flock.l_whence) {
	case SEEK_SET:	first = 0; break;
	case SEEK_CUR:	first = f->filp_pos; break;
	case SEEK_END:	first = f->filp_ino->i_size; break;
	default:	return(EINVAL);
  }
  /* Check for overflow. */
  if (((long)flock.l_start > 0) && ((first + flock.l_start) < first))
	return(EINVAL);
  if (((long)flock.l_start < 0) && ((first + flock.l_start) > first))
	return(EINVAL);
  first = first + flock.l_start;
  last = first + flock.l_len - 1;
  if (flock.l_len == 0) last = MAX_FILE_POS;
  if (last < first) return(EINVAL);

  /* Check if this region conflicts with any existing lock. */
  empty = (struct file_lock *) 0;
  for (flp = &file_lock[0]; flp < & file_lock[NR_LOCKS]; flp++) {
	if (flp->lock_type == 0) {
		if (empty == (struct file_lock *) 0) empty = flp;
		continue;	/* 0 means unused slot */
	}
	if (flp->lock_inode != f->filp_ino) continue;	/* different file */
	if (last < flp->lock_first) continue;	/* new one is in front */
	if (first > flp->lock_last) continue;	/* new one is afterwards */
	if (ltype == F_RDLCK && flp->lock_type == F_RDLCK) continue;
	if (ltype != F_UNLCK && flp->lock_pid == fp->fp_pid) continue;
  
	/* There might be a conflict.  Process it. */
	conflict = 1;
	if (req == F_GETLK) break;

	/* If we are trying to set a lock, it just failed. */
	if (ltype == F_RDLCK || ltype == F_WRLCK) {
		if (req == F_SETLK) {
			/* For F_SETLK, just report back failure. */
			return(EAGAIN);
		} else {
			/* For F_SETLKW, suspend the process. */
			suspend(XLOCK);
			return(0);
		}
	}

	/* We are clearing a lock and we found something that overlaps. */
	unlocking = 1;
	if (first <= flp->lock_first && last >= flp->lock_last) {
		flp->lock_type = 0;	/* mark slot as unused */
		nr_locks--;		/* number of locks is now 1 less */
		continue;
	}

	/* Part of a locked region has been unlocked. */
	if (first <= flp->lock_first) {
		flp->lock_first = last + 1;
		continue;
	}

	if (last >= flp->lock_last) {
		flp->lock_last = first - 1;
		continue;
	}
	
	/* Bad luck. A lock has been split in two by unlocking the middle. */
	if (nr_locks == NR_LOCKS) return(ENOLCK);
	for (i = 0; i < NR_LOCKS; i++)
		if (file_lock[i].lock_type == 0) break;
	flp2 = &file_lock[i];
	flp2->lock_type = flp->lock_type;
	flp2->lock_pid = flp->lock_pid;
	flp2->lock_inode = flp->lock_inode;
	flp2->lock_first = last + 1;
	flp2->lock_last = flp->lock_last;
	flp->lock_last = first - 1;
	nr_locks++;
  }
  if (unlocking) lock_revive();

  if (req == F_GETLK) {
	if (conflict) {
		/* GETLK and conflict. Report on the conflicting lock. */
		flock.l_type = flp->lock_type;
		flock.l_whence = SEEK_SET;
		flock.l_start = flp->lock_first;
		flock.l_len = flp->lock_last - flp->lock_first + 1;
		flock.l_pid = flp->lock_pid;

	} else {
		/* It is GETLK and there is no conflict. */
		flock.l_type = F_UNLCK;
	}

	/* Copy the flock structure back to the caller. */
	r = rw_user(D, who, user_flock, flock_size, (char *) &flock, TO_USER);
	return(r);
  }

  if (ltype == F_UNLCK) return(OK);	/* unlocked a region with no locks */

  /* There is no conflict.  If space exists, store new lock in the table. */
  if (empty == (struct file_lock *) 0) return(ENOLCK);	/* table full */
  empty->lock_type = ltype;
  empty->lock_pid = fp->fp_pid;
  empty->lock_inode = f->filp_ino;
  empty->lock_first = first;
  empty->lock_last = last;
  nr_locks++;
  return(OK);
}

/*===========================================================================*
 *				lock_revive				     *
 *===========================================================================*/
PUBLIC void lock_revive()
{
/* Go find all the processes that are waiting for any kind of lock and 
 * revive them all.  The ones that are still blocked will block again when 
 * they run.  The others will complete.  This strategy is a space-time 
 * tradeoff.  Figuring out exactly which ones to unblock now would take 
 * extra code, and the only thing it would win would be some performance in 
 * extremely rare circumstances (namely, that somebody actually used 
 * locking).
 */

  int task;
  struct fproc *fptr;

  for (fptr = &fproc[INIT_PROC_NR + 1]; fptr < &fproc[NR_PROCS]; fptr++){
	task = -fptr->fp_task;
	if (fptr->fp_suspended == SUSPENDED && task == XLOCK) {
		revive( (int) (fptr - fproc), 0);
	}
  }
}

/*===========================================================================*
 *				do_sync					     *
 *===========================================================================*/
PUBLIC int do_sync()
{
/* Perform the sync() system call.  Flush all the tables. */

  register struct inode *rip;
  register struct buf *bp;

  /* The order in which the various tables are flushed is critical.  The
   * blocks must be flushed last, since rw_inode() leaves its results in
   * the block cache.
   */

  /* Write all the dirty inodes to the disk. */
  for (rip = &inode[0]; rip < &inode[NR_INODES]; rip++)
	if (rip->i_count > 0 && rip->i_dirt == DIRTY) rw_inode(rip, WRITING);

  /* Write all the dirty blocks to the disk, one drive at a time. */
  for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++)
	if (bp->b_dev != NO_DEV && bp->b_dirt == DIRTY) flushall(bp->b_dev);

  return(OK);		/* sync() can't fail */
}


/*===========================================================================*
 *				do_fork					     *
 *===========================================================================*/
PUBLIC int do_fork()
{
/* Perform those aspects of the fork() system call that relate to files.
 * In particular, let the child inherit its parent's file descriptors.
 * The parent and child parameters tell who forked off whom. The file
 * system uses the same slot numbers as the kernel.  Only MM makes this call.
 */

  register struct fproc *cp;
  int i;

  /* Only MM may make this call directly. */
  if (who != MM_PROC_NR) return(ERROR);

  /* Copy the parent's fproc struct to the child. */
  fproc[child] = fproc[parent];

  /* Increase the counters in the 'filp' table. */
  cp = &fproc[child];
  for (i = 0; i < OPEN_MAX; i++)
	if (cp->fp_filp[i] != NIL_FILP) cp->fp_filp[i]->filp_count++;

  /* Fill in new process id and, if necessary, process group. */
  cp->fp_pid = pid;
  if (parent == INIT_PROC_NR) {
	cp->fp_pgrp = pid;
  }

  /* Record the fact that both root and working dir have another user. */
  dup_inode(cp->fp_rootdir);
  dup_inode(cp->fp_workdir);
  return(OK);
}


/*===========================================================================*
 *				do_exec					     *
 *===========================================================================*/
PUBLIC int do_exec()
{
/* Files can be marked with the FD_CLOEXEC bit (in fp->fp_cloexec).  When
 * MM does an EXEC, it calls FS to allow FS to find these files and close them.
 */

  register int i;
  long bitmap;

  /* Only MM may make this call directly. */
  if (who != MM_PROC_NR) return(ERROR);

  /* The array of FD_CLOEXEC bits is in the fp_cloexec bit map. */
  fp = &fproc[slot1];		/* get_filp() needs 'fp' */
  bitmap = fp->fp_cloexec;
  if (bitmap == 0) return(OK);	/* normal case, no FD_CLOEXECs */

  /* Check the file desriptors one by one for presence of FD_CLOEXEC. */
  for (i = 0; i < OPEN_MAX; i++) {
	fd = i;
	if ( (bitmap >> i) & 01) (void) do_close();
  }

  return(OK);
}


/*===========================================================================*
 *				do_exit					     *
 *===========================================================================*/
PUBLIC int do_exit()
{
/* Perform the file system portion of the exit(status) system call. */

  register int i, exitee, task;

  /* Only MM may do the EXIT call directly. */
  if (who != MM_PROC_NR) return(ERROR);

  /* Nevertheless, pretend that the call came from the user. */
  fp = &fproc[slot1];		/* get_filp() needs 'fp' */
  exitee = slot1;

  if (fp->fp_suspended == SUSPENDED) {
	task = -fp->fp_task;
	if (task == XPIPE || task == XPOPEN) susp_count--;
	pro = exitee;
	(void) do_unpause();	/* this always succeeds for MM */
	fp->fp_suspended = NOT_SUSPENDED;
  }

  /* Loop on file descriptors, closing any that are open. */
  for (i = 0; i < OPEN_MAX; i++) {
	fd = i;
	(void) do_close();
  }

  /* Release root and working directories. */
  put_inode(fp->fp_rootdir);
  put_inode(fp->fp_workdir);
  fp->fp_rootdir = NIL_INODE;
  fp->fp_workdir = NIL_INODE;

  return(OK);
}


/*===========================================================================*
 *				do_set					     *
 *===========================================================================*/
PUBLIC int do_set()
{
/* Set uid_t or gid_t field. */

  register struct fproc *tfp;

  /* Only MM may make this call directly. */
  if (who != MM_PROC_NR) return(ERROR);

  tfp = &fproc[slot1];
  if (fs_call == SETUID) {
	tfp->fp_realuid = (uid_t) real_user_id;
	tfp->fp_effuid =  (uid_t) eff_user_id;
  }
  if (fs_call == SETGID) {
	tfp->fp_effgid =  (gid_t) eff_grp_id;
	tfp->fp_realgid = (gid_t) real_grp_id;
  }
  return(OK);
}


/*===========================================================================*
 *				do_revive				     *
 *===========================================================================*/
PUBLIC int do_revive()
{
/* A task, typically TTY, has now gotten the characters that were needed for a
 * previous read.  The process did not get a reply when it made the call.
 * Instead it was suspended.  Now we can send the reply to wake it up.  This
 * business has to be done carefully, since the incoming message is from
 * a task (to which no reply can be sent), and the reply must go to a process
 * that blocked earlier.  The reply to the caller is inhibited by setting the
 * 'dont_reply' flag, and the reply to the blocked process is done explicitly
 * in revive().
 */

#if !ALLOW_USER_SEND
  if (who >= LOW_USER) return(EPERM);
#endif

  revive(m.REP_PROC_NR, m.REP_STATUS);
  dont_reply = TRUE;		/* don't reply to the TTY task */
  return(OK);
}
