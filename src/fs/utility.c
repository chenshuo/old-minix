/* This file contains a few general purpose utility routines.
 *
 * The entry points into this file are
 *   clock_time:  ask the clock task for the real time
 *   copy:	  copy a block of data
 *   fetch_name:  go get a path name from user space
 *   no_sys:      reject a system call that FS does not handle
 *   panic:       something awful has occurred;  MINIX cannot continue
 *   conv2:	  do byte swapping on a 16-bit int
 *   conv4:	  do byte swapping on a 32-bit long
 */

#include "fs.h"
#include <minix/com.h>
#include <minix/boot.h>
#include "buf.h"
#include "file.h"
#include "fproc.h"
#include "inode.h"
#include "param.h"

PRIVATE int panicking;		/* inhibits recursive panics during sync */
PRIVATE message clock_mess;

/*===========================================================================*
 *				clock_time				     *
 *===========================================================================*/
PUBLIC time_t clock_time()
{
/* This routine returns the time in seconds since 1.1.1970.  MINIX is an
 * astrophysically naive system that assumes the earth rotates at a constant
 * rate and that such things as leap seconds do not exist.
 */

  register int k;

  clock_mess.m_type = GET_TIME;
  if ( (k = sendrec(CLOCK, &clock_mess)) != OK) panic("clock_time err", k);

  return( (time_t) clock_mess.NEW_TIME);
}


/*===========================================================================*
 *				fetch_name				     *
 *===========================================================================*/
PUBLIC int fetch_name(path, len, flag)
char *path;			/* pointer to the path in user space */
int len;			/* path length, including 0 byte */
int flag;			/* M3 means path may be in message */
{
/* Go get path and put it in 'user_path'.
 * If 'flag' = M3 and 'len' <= M3_STRING, the path is present in 'message'.
 * If it is not, go copy it from user space.
 */

  register char *rpu, *rpm;
  int r, n;
  vir_bytes vpath;

  /* Check name length for validity. */
  if (len <= 0) {
	err_code = EINVAL;
	return(ERROR);
  }

  if (len > PATH_MAX) {
	err_code = ENAMETOOLONG;
	return(ERROR);
  }

  n = len - 1;			/* # chars not including 0 byte */
  if (flag == M3 && len <= M3_STRING) {
	/* Just copy the path from the message to 'user_path'. */
	rpu = &user_path[0];
	rpm = pathname;		/* contained in input message */
	do { *rpu++ = *rpm++; } while (--len);
	r = OK;
  } else {
	/* String is not contained in the message.  Get it from user space. */
	vpath = (vir_bytes) path;
	r = rw_user(D, who, vpath, (vir_bytes) len, user_path, FROM_USER);
  }

  /* Paths that end in "/." like "/a/b/." or "/" are a pain. Get rid of them */
  if (r == OK) {
	rpu = &user_path[n - 1]; /* points to last char */
	while (n > 2) {
		if (*rpu == '/') 
		{
		    *rpu = '\0';	 /* remove the "/" */
		    n -= 1;
		    rpu -= 1;
		} else
		if (*rpu == '.' && *(--rpu) == '/') {
			*rpu = '\0';	 /* remove the "/." */
			n -= 2;
			rpu -= 1;
		} else break;
	}
  }
  return(r);
}


/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
PUBLIC int no_sys()
{
/* Somebody has used an illegal system call number */

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

  if (panicking) return;	/* do not panic during a sync */
  panicking = TRUE;		/* prevent another panic during the sync */
  printf("File system panic: %s ", format);
  if (num != NO_NUM) printf("%d",num); 
  printf("\n");
  (void) do_sync();		/* flush everything to the disk */
  sys_abort();
}



/*===========================================================================*
 *				conv2					     *
 *===========================================================================*/
PUBLIC unsigned conv2(norm, w)
int norm;			/* TRUE if no swap, FALSE for byte swap */
int w;				/* promotion of 16-bit word to be swapped */
{
/* Possibly swap a 16-bit word between 8086 and 68000 byte order. */

  if (norm) return( (unsigned) w & 0xFFFF);
  return( ((w&BYTE) << 8) | ( (w>>8) & BYTE));
}

/*===========================================================================*
 *				conv4					     *
 *===========================================================================*/
PUBLIC long conv4(norm, x)
int norm;			/* TRUE if no swap, FALSE for byte swap */
long x;				/* 32-bit long to be byte swapped */
{
/* Possibly swap a 32-bit long between 8086 and 68000 byte order. */

  unsigned lo, hi;
  long l;
  
  if (norm) return(x);			/* byte order was already ok */
  lo = conv2(FALSE, (int) x & 0xFFFF);	/* low-order half, byte swapped */
  hi = conv2(FALSE, (int) (x>>16) & 0xFFFF);	/* high-order half, swapped */
  l = ( (long) lo <<16) | hi;
  return(l);
}

/*===========================================================================*
 *				sys_times				     *
 *===========================================================================*/
PUBLIC void sys_times(proc, ptr)
int proc;			/* proc whose times are needed */
clock_t ptr[5];			/* pointer to time buffer */
{
/* Fetch the accounting info for a proc. */
  message m;

  m.m1_i1 = proc;
  m.m1_p1 = (char *)ptr;
  (void) _taskcall(SYSTASK, SYS_TIMES, &m);
  ptr[0] = m.USER_TIME;
  ptr[1] = m.SYSTEM_TIME;
  ptr[2] = m.CHILD_UTIME;
  ptr[3] = m.CHILD_STIME;
  ptr[4] = m.BOOT_TICKS;
}

/*===========================================================================*
 *				sys_kill				     *
 *===========================================================================*/
PUBLIC void sys_kill(proc, signr)
int proc;			/* which proc has exited */
int signr;			/* signal number: 1 - 16 */
{
/* A proc has to be signaled via MM.  Tell the kernel. */
  message m;

  m.m6_i1 = proc;
  m.m6_i2 = signr;
  (void) _taskcall(SYSTASK, SYS_KILL, &m);
}

