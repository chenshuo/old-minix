#include <errno.h>
#include <termios.h>

int tcsendbreak(filedes, duration)
int filedes;
int duration;
{
    /* There is no way to generate a break condition. */
    errno = ENOSYS;
    return -1;
}
