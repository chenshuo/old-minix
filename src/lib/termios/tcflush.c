#include <errno.h>
#include <termios.h>

int tcflush(filedes, queue_selector)
int filedes;
int queue_selector;
{
    /* Only flushing both input and output is supported by the old drivers.
     * But we still check for valid parameters.
     */
    if (queue_selector == TCIFLUSH)
	errno = ENOSYS;
    else if (queue_selector == TCOFLUSH)
	errno = ENOSYS;
    else if (queue_selector == TCIOFLUSH)
    {
#ifdef TIOCFLUSH
	return ioctl(filedes, TIOCFLUSH, (struct sgttyb *)0) < 0
		? -1 : 0;
#else
	errno = ENOSYS;
#endif
    }
    else
	errno = EINVAL;
    return -1;
}
