#include <termios.h>

#undef cfgetispeed

speed_t cfgetispeed(termios_p)
_CONST struct termios *termios_p;
{
    return termios_p->_c_ispeed;
}
