#include <errno.h>
#include <termios.h>

int tcdrain(filedes)
int filedes;
{
    /* There is no way to wait for all output to be sent. */
    errno = ENOSYS;
    return -1;
}
