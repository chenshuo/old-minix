#include <lib.h>
#include <minix/com.h>
#include <minix/syslib.h>

/*----------------------------------------------------------------------------
		Messages to SYSTASK that are used by both MM and FS
----------------------------------------------------------------------------*/

PUBLIC void sys_abort()
{
/* Something awful has happened.  Abandon ship. */

  message m;

  _taskcall(SYSTASK, SYS_ABORT, &m);
}

PUBLIC void sys_copy(mptr)
message *mptr;			/* pointer to message */
{
/* A server wants something copied. */

  /* Make this routine better.  Also check other guys' error handling.
   * DEBUG.
   */
  if (_taskcall(SYSTASK, SYS_COPY, mptr) != 0)
	panic("sys_copy can't send", NO_NUM);
}
