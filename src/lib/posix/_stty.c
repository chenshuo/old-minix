#include <lib.h>
#define ioctl	_ioctl
#define stty	_stty
#include <sgtty.h>

_PROTOTYPE(int stty, (int, struct sgttyb *));

PUBLIC int stty(fd, argp)
int fd;
struct sgttyb *argp;
{
  return ioctl(fd, TIOCSETP, argp);
}
