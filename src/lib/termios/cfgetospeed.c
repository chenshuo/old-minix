#include <termios.h>

#undef cfgetospeed

speed_t cfgetospeed(termios_p)
_CONST struct termios *termios_p;
{
    return termios_p->_c_ospeed;
}
