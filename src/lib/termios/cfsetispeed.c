#include <termios.h>

#undef cfsetispeed

int cfsetispeed(termios_p, speed)
struct termios *termios_p;
speed_t speed;
{
    termios_p->_c_ispeed = speed;
    return 0;
}
