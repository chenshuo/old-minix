#include <termios.h>

#undef cfsetospeed

int cfsetospeed(termios_p, speed)
struct termios *termios_p;
speed_t speed;
{
    termios_p->_c_ospeed = speed;
    return 0;
}
