#include <errno.h>
#include <termios.h>

int tcflow(filedes, act)
int filedes;
int act;
{
    /* There is no way to do flow control.  Just writing a start or stop
     * character is not enough.  There are a lot of reasons for this,
     * including: the start or stop character should be sent immediately
     * and not after all the pending output has been sent.  So we only
     * check for valid parameters here.
     */
    errno = act == TCOOFF || act == TCOON || act == TCIOFF || act == TCION
		? ENOSYS : EINVAL;
    return -1;
}
