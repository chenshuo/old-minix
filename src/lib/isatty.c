#include "lib.h"
#include <minix/com.h>
#include <sgtty.h>

int isatty(fd)
int fd;
{
  M.TTY_REQUEST = TIOCGETP;
  M.TTY_LINE = fd;
  if (callx(FS, IOCTL) < 0)
	return(0);
  return(1);
}
