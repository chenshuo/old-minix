#include <lib.h>
#define isatty	_isatty
#include <sgtty.h>
#include <minix/com.h>
#include <unistd.h>

PUBLIC int isatty(fd)
int fd;
{
  message m;

  m.TTY_REQUEST = TIOCGETP;
  m.TTY_LINE = fd;
  if (_syscall(FS, IOCTL, &m) < 0) return(0);
  return(1);
}
