#include <lib.h>
#define ioctl	_ioctl
#define stty	_stty
#include <sgtty.h>

PUBLIC int stty(fd, argp)
int fd;
struct sgttyb *argp;
{
  return ioctl(fd, TIOCSETP, argp);
}
