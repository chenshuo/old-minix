
/* This file contains some utility routines for MM.
 *
 * The entry points are:
 *   allowed:	see if an access is permitted
 *   mem_copy:	copy data from somewhere in memory to somewhere else
 *   no_sys:	this routine is called for invalid system call numbers
 *   panic:	MM has run aground of a fatal error and cannot continue
 *   sys_*:	various interfaces to sendrec(SYSTASK, ...)
 *   tell_fs:	interface to FS
 */

#include "mm.h"
#include <sys/stat.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <fcntl.h>
#include <signal.h>		/* needed only because mproc.h needs it */
#include <unistd.h>
#include "mproc.h"

/*===========================================================================*
 *				allowed					     *
 *===========================================================================*/
PUBLIC int allowed(name_buf, s_buf, mask)
char *name_buf;			/* pointer to file name to be EXECed */
struct stat *s_buf;		/* buffer for doing and returning stat struct*/
int mask;			/* R_BIT, W_BIT, or X_BIT */
{
/* Check to see if file can be accessed.  Return EACCES or ENOENT if the access
 * is prohibited.  If it is legal open the file and return a file descriptor.
 */

  int fd;
  int save_errno;

  /* Use the fact that mask for access() is the same as the permissions mask.
   * E.g., X_BIT in <minix/const.h> is the same as X_OK in <unistd.h> and
   * S_IXOTH in <sys/stat.h>.  tell_fs(DO_CHDIR, ...) has set MM's real ids
   * to the user's effective ids, so access() works right for setuid programs.
   */
  if (access(name_buf, mask) < 0) return(-errno);

  /* The file is accessible but might not be readable.  Make it readable. */
  tell_fs(SETUID, MM_PROC_NR, (int) SUPER_USER, (int) SUPER_USER);

  /* Open the file and fstat it.  Restore the ids early to handle errors. */
  fd = open(name_buf, O_RDONLY);
  save_errno = errno;		/* open might fail, e.g. from ENFILE */
  tell_fs(SETUID, MM_PROC_NR, (int) mp->mp_effuid, (int) mp->mp_effuid);
  if (fd < 0) return(-save_errno);
  if (fstat(fd, s_buf) < 0) panic("allowed: fstat failed", NO_NUM);

  /* Only regular files can be executed. */
  if (mask == X_BIT && (s_buf->st_mode & I_TYPE) != I_REGULAR) {
	close(fd);
	return(EACCES);
  }
  return(fd);
}


/*===========================================================================*
 *				find_proc  				     *
 *===========================================================================*/
PUBLIC struct mproc *find_proc(pid)
pid_t pid;
{
  register struct mproc *rmp;

  for (rmp = &mproc[INIT_PROC_NR + 1]; rmp < &mproc[NR_PROCS]; rmp++)
	if (rmp->mp_flags & IN_USE && rmp->mp_pid == pid) return(rmp);
  return(NIL_MPROC);
}


/*===========================================================================*
 *				mem_copy				     *
 *===========================================================================*/
PUBLIC int mem_copy(src_proc,src_seg, src_vir, dst_proc,dst_seg, dst_vir, bytes)
int src_proc;			/* source process */
int src_seg;			/* source segment: T, D, or S */
long src_vir;			/* source virtual address (phys addr for ABS)*/
int dst_proc;			/* dest process */
int dst_seg;			/* dest segment: T, D, or S */
long dst_vir;			/* dest virtual address (phys addr for ABS) */
long bytes;			/* how many bytes */
{
/* Transfer a block of data.  The source and destination can each either be a
 * process (including MM) or absolute memory, indicate by setting 'src_proc'
 * or 'dst_proc' to ABS.
 */

  message copy_mess;

  if (bytes == 0L) return(OK);
  copy_mess.SRC_SPACE = (char) src_seg;
  copy_mess.SRC_PROC_NR = src_proc;
  copy_mess.SRC_BUFFER = src_vir;

  copy_mess.DST_SPACE = (char) dst_seg;
  copy_mess.DST_PROC_NR = dst_proc;
  copy_mess.DST_BUFFER = dst_vir;

  copy_mess.COPY_BYTES = bytes;
  sys_copy(&copy_mess);
  return(copy_mess.m_type);
}


/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
PUBLIC int no_sys()
{
/* A system call number not implemented by MM has been requested. */

  return(EINVAL);
}


/*===========================================================================*
 *				panic					     *
 *===========================================================================*/
PUBLIC void panic(format, num)
char *format;			/* format string */
int num;			/* number to go with format string */
{
/* Something awful has happened.  Panics are caused when an internal
 * inconsistency is detected, e.g., a programming error or illegal value of a
 * defined constant.
 */

  printf("Memory manager panic: %s ", format);
  if (num != NO_NUM) printf("%d",num);
  printf("\n");
  tell_fs(SYNC, 0, 0, 0);	/* flush the cache to the disk */
  sys_abort();
}



/*===========================================================================*
 *				sys_exec				     *
 *===========================================================================*/
PUBLIC void sys_exec(proc, ptr, traced, prog_name, initpc)
int proc;			/* process that did exec */
char *ptr;			/* new stack pointer */
int traced;			/* is tracing enabled? */
char *prog_name;		/* name of the new program */
vir_bytes initpc;
{
/* A process has exec'd.  Tell the kernel. */

  message m;

  m.m1_i1 = proc;
  m.m1_i2 = traced;
  m.m1_p1 = ptr;
  m.m1_p2 = prog_name;
  m.m1_p3 = (char *)initpc;
  _taskcall(SYSTASK, SYS_EXEC, &m);
}


/*===========================================================================*
 *				sys_fork				     *
 *===========================================================================*/
PUBLIC int sys_fork(parent, child, pid, child_base_or_shadow)
int parent;			/* process doing the fork */
int child;			/* which proc has been created by the fork */
int pid;			/* process id assigned by MM */
phys_clicks child_base_or_shadow;	/* position for child [VM386];
				 * memory allocated for shadow [68000] */
{
/* A process has forked.  Tell the kernel. */

  message m;

  m.m1_i1 = parent;
  m.m1_i2 = child;
  m.m1_i3 = pid;
  m.m1_p1 = (char *) child_base_or_shadow;
  return(_taskcall(SYSTASK, SYS_FORK, &m));
}

/*===========================================================================*
 *				sys_getsp				     *
 *===========================================================================*/
PUBLIC void sys_getsp(proc, newsp)
int proc;			/* process whose sp is wanted */
vir_bytes *newsp;		/* place to put sp read from kernel */
{
/* Ask the kernel what the sp is. */

  message m;

  m.m1_i1 = proc;
  _taskcall(SYSTASK, SYS_GETSP, &m);
  *newsp = (vir_bytes) m.STACK_PTR;
}

/*===========================================================================*
 *				sys_newmap				     *
 *===========================================================================*/
PUBLIC void sys_newmap(proc, ptr)
int proc;			/* process whose map is to be changed */
struct mem_map *ptr;		/* pointer to new map */
{
/* A process has been assigned a new memory map.  Tell the kernel. */

  message m;

  m.m1_i1 = proc;
  m.m1_p1 = (char *) ptr;
  _taskcall(SYSTASK, SYS_NEWMAP, &m);
}


/*===========================================================================*
 *				sys_sendsig				     *
 *===========================================================================*/
PUBLIC void sys_sendsig(proc, smp)
int proc;
struct sigmsg *smp;
{
  message m;

  m.m1_i1 = proc;
  m.m1_p1 = (char *) smp;
  _taskcall(SYSTASK, SYS_SENDSIG, &m);
}

/*===========================================================================*
 *				sys_oldsig				     *
 *===========================================================================*/
PUBLIC void sys_oldsig(proc, sig, sighandler)
int proc;			/* process to be signaled  */
int sig;			/* signal number: 1 to _NSIG */
sighandler_t sighandler;	/* pointer to signal handler in user space */
{
/* A proc has to be signaled.  Tell the kernel. This function is obsolete. */

  message m;

  m.m6_i1 = proc;
  m.m6_i2 = sig;
  m.m6_f1 = sighandler;
  _taskcall(SYSTASK, SYS_OLDSIG, &m);
}

/*===========================================================================*
 *				sys_endsig				     *
 *===========================================================================*/
PUBLIC void sys_endsig(proc)
int proc;
{
  message m;

  m.m1_i1 = proc;
  _taskcall(SYSTASK, SYS_ENDSIG, &m);
}

/*===========================================================================*
 *				sys_sigreturn				     *
 *===========================================================================*/
PUBLIC void sys_sigreturn(proc, scp, flags)
int proc;
vir_bytes scp;
int flags;
{
  message m;

  m.m1_i1 = proc;
  m.m1_i2 = flags;
  m.m1_p1 = (char *) scp;
  _taskcall(SYSTASK, SYS_SIGRETURN, &m);
}

/*===========================================================================*
 *				sys_trace				     *
 *===========================================================================*/
PUBLIC int sys_trace(req, procnr, addr, data_p)
int req, procnr;
long addr, *data_p;
{
  message m;
  int r;

  m.m2_i1 = procnr;
  m.m2_i2 = req;
  m.m2_l1 = addr;
  if (data_p) m.m2_l2 = *data_p;
  r = _taskcall(SYSTASK, SYS_TRACE, &m);
  if (data_p) *data_p = m.m2_l2;
  return(r);
}

/*===========================================================================*
 *				sys_xit					     *
 *===========================================================================*/
PUBLIC void sys_xit(parent, proc, basep, sizep)
int parent;			/* parent of exiting process */
int proc;			/* which process has exited */
phys_clicks *basep;		/* where to return base of shadow [68000] */
phys_clicks *sizep;		/* where to return size of shadow [68000] */
{
/* A process has exited.  Tell the kernel. */

  message m;

  m.m1_i1 = parent;
  m.m1_i2 = proc;
  _taskcall(SYSTASK, SYS_XIT, &m);
  *basep = (phys_clicks) m.m1_i1;
  *sizep = (phys_clicks) m.m1_i2;
}


/*===========================================================================*
 *				sys_fresh				     *
 *===========================================================================*/
#if (CHIP == M68000) /* funny, the text cache version simply used sys_newmap */
PUBLIC void sys_fresh(proc, ptr, dc, basep, sizep)
int proc;			/* process whose map is to be changed */
struct mem_map *ptr;		/* pointer to new map */
phys_clicks dc;			/* size of initialized data */
phys_clicks *basep, *sizep;	/* base and size for free_mem() */
{
/* Create a fresh process image for exec().  Tell the kernel. */

  message m;

  m.m1_i1 = proc;
  m.m1_i2 = (int) dc;
  m.m1_p1 = (char *) ptr;
  _taskcall(SYSTASK, SYS_FRESH, &m);
  *basep = (phys_clicks) m.m1_i1;
  *sizep = (phys_clicks) m.m1_i2;
}
#endif


/*===========================================================================*
 *				tell_fs					     *
 *===========================================================================*/
PUBLIC void tell_fs(what, p1, p2, p3)
int what, p1, p2, p3;
{
/* This routine is only used by MM to inform FS of certain events:
 *      tell_fs(CHDIR, slot, dir, 0)
 *      tell_fs(EXEC, proc, 0, 0)
 *      tell_fs(EXIT, proc, 0, 0)
 *      tell_fs(FORK, parent, child, pid)
 *      tell_fs(SETGID, proc, realgid, effgid)
 *      tell_fs(SETUID, proc, realuid, effuid)
 *      tell_fs(SYNC, 0, 0, 0)
 *      tell_fs(UNPAUSE, proc, signr, 0)
 *      tell_fs(SETPGRP, proc, 0, 0)
 */

  message m;

  m.m1_i1 = p1;
  m.m1_i2 = p2;
  m.m1_i3 = p3;
  _taskcall(FS_PROC_NR, what, &m);
}
