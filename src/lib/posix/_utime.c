/* utime(2) for POSIX		Authors: Terrence W. Holm & Edwin L. Froese */

#include <lib.h>
#define time	_time
#define utime	_utime
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <utime.h>

PUBLIC int utime(name, timp)
_CONST char *name;
_CONST struct utimbuf *timp;
{
  time_t current_time;
  message m;

  if (timp == (struct utimbuf *) NULL) {
	current_time = time( (time_t *) NULL);
	m.m2_l1 = current_time;
	m.m2_l2 = current_time;
  } else {
	m.m2_l1 = timp->actime;
	m.m2_l2 = timp->modtime;
  }
  m.m2_i1 = strlen(name) + 1;
  m.m2_p1 = (char *) name;
  return(_syscall(FS, UTIME, &m));
}
