/* termios.c - fake termios interface using sgtty interface */

/* Force all Minix special symbols to be visible here. */
#undef  _POSIX_SOURCE		/* in case it is defined elsewhere */
#undef  _MINIX			/* ditto */
#define _POSIX_SOURCE	1
#define _MINIX		1

#include <errno.h>
#include <termios.h>
#include <sys/types.h>
#include <unistd.h>

/* Redefine some of the termios.h names with 'T_' prefixed to the name.
 * We will undefine them soon to avoid clashes with <sgtty.h>.
 */
#define T_ECHO		0000001
#define T_XTABS		0006000
#define T_CRMOD		0000020

#if !(T_ECHO == ECHO && T_XTABS == XTABS && T_CRMOD == CRMOD)
#include "error, guessed wrong values for ECHO, XTABS or CRMOD in termios.h"
#endif

/* Undefine everything that clashes with sgtty.h. */
#undef B0
#undef B50
#undef B75
#undef B110
#undef B134
#undef B150
#undef B200
#undef B300
#undef B600
#undef B1200
#undef B1800
#undef B2400
#undef B4800
#undef B9600
#undef B19200
#undef B28800
#undef B38400
#undef B57600
#undef B115200
#undef ECHO
#undef XTABS
#undef CRMOD

#include <sgtty.h>

#if !(T_XTABS == XTABS && T_CRMOD == CRMOD)
#include "error, guessed wrong values for XTABS or CRMOD in sgtty.h"
#endif

/* There is an error in <sgtty.h>.  It defines sg_ispeed and sg_ospeed as
 * char values.  At the same time it defines some values for them which they
 * could never give back when char is a signed type (B19200 is defined as 192).
 * Furthermore ACK has some problem with casting chars to unsigned chars.
 * It just ignores such a cast and
 *    char c = 192;
 *    int ic = (unsigned char)c;
 * won't work as expected (ic != 192).
 * The following is a workaround.
 */
#ifdef __ACK__
#define UCHAR(c)	((c) & 0xFF)
#else
#define UCHAR(c)	((unsigned char) (c))
#endif

/* The following is a hack for CRMOD setting.
 * Some programs activate OPOST because they want writing '\n' to position
 * the cursor to the beginning of the next line (this behaviour is not
 * guaranteed, because the effect of OPOST is implementation defined).  Some
 * programs clear ICRNL because they want to read CR as CR and not as LF.
 * It is impossible to satisfy both requirements (grumble), and this
 * implementation has an external variable __tios_crmod that specifies which
 * requirement has priority.  There are 3 possibilities:
 *
 *  __tios_crmod == _TC_COPY_CRMOD	copy CRMOD from c_oflag
 *  __tios_crmod == _TC_ALWAYS_CRMOD	set CRMOD for OPOST
 *  __tios_crmod == _TC_NEVER_CRMOD	set CRMOD for ICRNL only
 *
 * Here is the default method for the termios emulation if not specified
 * elsewhere.  I hope a program that requires an output of '\n' to act in
 * the usual way won't change any of the bits in c_oflag and so an active
 * CRMOD (from tcgetattr) will be kept.
 */
#ifndef DFLT_TIOS_CRMOD
#define DFLT_TIOS_CRMOD _TC_COPY_CRMOD
#endif

int __tios_crmod = DFLT_TIOS_CRMOD;

/* Various hacks are used for IXON control.  Turning off IXON is supposed to
 * stop output flow control, but there is no way to stop flow control in
 * cooked and cbreak modes using the sgtty interface.  The best we can do is
 * is to set the start and stop characters to an unusual character (the start
 * character will actually never be seen but any other character will restart
 * output).  Then it is difficult to restore the control characters.  We only
 * handle characters 0 to 0x1F, by ORing in 0xE0.  The mask might have to be
 * changed to avoid national characters (it can also be 0x80, 0xA0 or 0xC0).
 */
#define HIDDEN_TC(tc)	(UCHAR(tc) >= HIDE_TC(0) && UCHAR(tc) <= HIDE_TC(0x1F))
#define HIDE_TC(tc)	(UCHAR(tc) | HIDE_TC_MASK)
#define HIDE_TC_MASK	0xE0
#define UNHIDE_TC(tc)	(UCHAR(tc) & ~HIDE_TC_MASK)

#if (_POSIX_VDISABLE <= 0x80 \
     || _POSIX_VDISABLE >= HIDE_TC_MASK \
	&& _POSIX_VDISABLE <= HIDE_TC_MASK + 0x1F)
#include "warning, you should change _POSIX_VDISABLE in <unistd.h>"
#endif

static _PROTOTYPE( int tc_to_sg_speed, (speed_t speed) );
static _PROTOTYPE( speed_t sg_to_tc_speed, (int speed) );

/* Don't bother with the low speeds - Minix does not support them.  Add
 * support for higher speeds (speeds are now easy and don't need defines
 * because they are not encoded).
 */

static speed_t sg_to_tc_speed(speed)
int speed;
{
    /* The speed encodings in sgtty.h and termios.h are different.  Both are
     * inflexible.  Minix doesn't really support B0 but we map it through
     * anyway.  It doesn't support B50, B75 or B134.
     */
    switch (speed)
    {
	case B0: return 0;
	case B110: return 110;
	case B200: return 200;
	case B300: return 300;
	case B600: return 600;
	case B1200: return 1200;
	case B1800: return 1800;
	case B2400: return 2400;
	case B4800: return 4800;
	case B9600: return 9600;
	case B19200: return 19200;
#ifdef B28800
	case B28800: return 28800;
#endif
#ifdef B38400
	case B38400: return 38400;
#endif
#ifdef B57600
	case B57600: return 57600;
#endif
#ifdef B115200
	case B115200: return 115200;
#endif
	default: return (speed_t) -1;
    }
}

static int tc_to_sg_speed(speed)
speed_t speed;
{
    /* Don't use a switch here in case the compiler is 16-bit and doesn't
     * properly support longs (speed_t's) in switches.  It turns out the
     * switch is larger and slower for most compilers anyway!
     */
    if (speed == 0) return 0;
    if (speed == 110) return B110;
    if (speed == 200) return B200;
    if (speed == 300) return B300;
    if (speed == 600) return B600;
    if (speed == 1200) return B1200;
    if (speed == 1800) return B1800;
    if (speed == 2400) return B2400;
    if (speed == 4800) return B4800;
    if (speed == 9600) return B9600;
    if (speed == 19200) return B19200;
#ifdef B28800
    if (speed == 28800) return B28800;
#endif
#ifdef B38400
    if (speed == 38400) return B38400;
#endif
#ifdef B57600
    if (speed == 57600) return B57600;
#endif
#ifdef B115200
    if (speed == 115200) return B115200;
#endif
    return -1;
}

int tcgetattr(filedes, termios_p)
int filedes;
struct termios *termios_p;
{
    struct sgttyb sgbuf;
    struct tchars tcbuf;

    if (ioctl(filedes, TIOCGETP, &sgbuf) < 0
	|| ioctl(filedes, TIOCGETC, (struct sgttyb *) &tcbuf) < 0)
	return -1;

    /* Minix input flags:
     *   BRKINT:  forced off (break is not recognized)
     *   IGNBRK:  forced on (break is not recognized)
     *   ICRNL:   set if CRMOD is set and not RAW (CRMOD also controls output)
     *   IGNCR:   forced off (ignoring CR's is not supported)
     *   INLCR:   forced off (mapping NL's to CR's is not supported)
     *   ISTRIP:  forced off (should be off for consoles, on for rs232 no RAW)
     *   IXOFF:   forced off (rs232 uses CTS instead of XON/XOFF)
     *   IXON:    forced on if not RAW
     *   PARMRK:  forced off (no '\377', '\0', X sequence on errors)
     * ? IGNPAR:  forced off (input with parity/framing errors is kept)
     * ? INPCK:   forced off (input parity checking is not supported)
     */
    termios_p->c_iflag = IGNBRK;
    if (!(sgbuf.sg_flags & RAW))
    {
	if (!HIDDEN_TC(tcbuf.t_startc) || !HIDDEN_TC(tcbuf.t_stopc))
	    termios_p->c_iflag |= IXON;
	if (sgbuf.sg_flags & CRMOD)
	    termios_p->c_iflag |= ICRNL;
    }

    /* Minix output flags:
     *   OPOST:   set if CRMOD or XTABS is set
     *   XTABS:   copied from sg_flags
     *   CRMOD:	  copied from sg_flags
     */
    termios_p->c_oflag = sgbuf.sg_flags & (CRMOD | XTABS);
    if (termios_p->c_oflag)
	termios_p->c_oflag |= OPOST;

    /* Minix local flags:
     *   ECHO:    set if ECHO is set
     *   ECHOE:   set if ECHO is set (echo ERASE as error-correcting backspace)
     *   ECHOK:   set if ECHO is set ('\n' echoed after KILL char)
     *   ECHONL:  forced off ('\n' not echoed when ECHO isn't set)
     *   ICANON:  set if neither CBREAK nor RAW
     *   IEXTEN:  forced off
     *   ISIG:    set if not RAW
     *   NOFLSH:  forced off (input/output queues are always flushed)
     *   TOSTOP:  forced off (no job control)
     */
    termios_p->c_lflag = 0;
    if (sgbuf.sg_flags & ECHO)
	termios_p->c_lflag |= T_ECHO | ECHOE | ECHOK;
    if (!(sgbuf.sg_flags & RAW))
    {
	termios_p->c_lflag |= ISIG;
	if (!(sgbuf.sg_flags & CBREAK))
	    termios_p->c_lflag |= ICANON;
    }

    /* Minix control flags:
     *   CLOCAL:  forced on (ignore modem status lines - not quite right)
     *   CREAD:   forced on (receiver is always enabled)
     *   CSIZE:   CS5-CS8 correspond directly to BITS5-BITS8
     *   CSTOPB:  set for B110 (driver will generate 2 stop-bits than)
     *   HUPCL:   forced off
     *   PARENB:  set if EVENP or ODDP is set
     *   PARODD:  set if ODDP is set
     */
    termios_p->c_cflag = CLOCAL | CREAD;
    switch (sgbuf.sg_flags & BITS8)
    {
	case BITS5: termios_p->c_cflag |= CS5; break;
	case BITS6: termios_p->c_cflag |= CS6; break;
	case BITS7: termios_p->c_cflag |= CS7; break;
	case BITS8: termios_p->c_cflag |= CS8; break;
    }
    if (sgbuf.sg_flags & ODDP)
	termios_p->c_cflag |= PARENB | PARODD;
    if (sgbuf.sg_flags & EVENP)
	termios_p->c_cflag |= PARENB;
    if (sgbuf.sg_ispeed == B110)
	termios_p->c_cflag |= CSTOPB;

    /* Minix may give back different input and output baud rates,
     * but only the input baud rate is valid for both.
     * As our termios emulation will fail, if input baud rate differs
     * from output baud rate, force them to be equal.
     * Otherwise it would be very surprising not to be able to set
     * the terminal back to the state returned by tcgetattr :).
     */
    termios_p->_c_ospeed =
    termios_p->_c_ispeed = sg_to_tc_speed(UCHAR(sgbuf.sg_ispeed));

    /* Minix control characters correspond directly except VSUSP and the
     * important VMIN and VTIME are not really supported.
     */
    termios_p->c_cc[VEOF] = tcbuf.t_eofc;
    termios_p->c_cc[VEOL] = tcbuf.t_brkc;
    termios_p->c_cc[VERASE] = sgbuf.sg_erase;
    termios_p->c_cc[VINTR] = tcbuf.t_intrc;
    termios_p->c_cc[VKILL] = sgbuf.sg_kill;
    termios_p->c_cc[VQUIT] = tcbuf.t_quitc;
    if (HIDDEN_TC(tcbuf.t_startc) && HIDDEN_TC(tcbuf.t_stopc))
    {
	termios_p->c_cc[VSTART] = UNHIDE_TC(tcbuf.t_startc);
	termios_p->c_cc[VSTOP] = UNHIDE_TC(tcbuf.t_stopc);
    }
    else
    {
	termios_p->c_cc[VSTART] = tcbuf.t_startc;
	termios_p->c_cc[VSTOP] = tcbuf.t_stopc;
    }
    termios_p->c_cc[VMIN] = 1;
    termios_p->c_cc[VTIME] = 0;
    termios_p->c_cc[VSUSP] = 0;

    return 0;
}

int tcsetattr(filedes, opt_actions, termios_p)
int filedes;
int opt_actions;
_CONST struct termios *termios_p;
{
    struct sgttyb sgbuf;
    struct tchars tcbuf;
    int sg_ispeed;
    int sg_ospeed;

    /* Posix 1003.1-1988 page 135 says:
     *   Attempts to set unsupported baud rates shall be ignored, and it is
     *   implementation-defined whether an error is returned by any or all of
     *   cfsetispeed(), cfsetospeed(), or tcsetattr().  This refers both to
     *   changes to baud rates not supported by the hardware, and to changes
     *   setting the input and output baud rates to different values if the
     *   hardware does not support it.
     *
     * In this implementation, tcsetattr may return an error but the cfset
     * functions do not.  The driver will use the input speed for both input
     * and output if split speeds are not supported (no error).
     */
    sg_ospeed = tc_to_sg_speed(termios_p->_c_ospeed);
    if (termios_p->_c_ispeed == 0)
	sg_ispeed = sg_ospeed;
    else
	sg_ispeed = tc_to_sg_speed(termios_p->_c_ispeed);
    if (sg_ispeed == -1 || sg_ospeed == -1)
    {
	errno = EINVAL;
	return -1;
    }
    sgbuf.sg_ispeed = sg_ispeed;
    sgbuf.sg_ospeed = sg_ospeed;
    sgbuf.sg_flags = 0;

    /* I don't know what should happen with requests that are not supported by
     * old Minix drivers and therefore cannot be emulated.
     * Returning an error may confuse the application (the values aren't really
     * invalid or unsupported by the hardware, they just cannot be satisfied
     * by the driver).  Not returning an error might be even worse because the
     * driver will act different to what the application requires it to act
     * after successfully setting the attributes as specified.
     * Settings that cannot be emulated fully include:
     *   _c_ospeed != 110 && c_cflag & CSTOPB
     *   _c_ospeed == 110 && ! c_cflag & CSTOPB
     *   (c_cc[VMIN] != 1 || c_cc[VTIME] != 0) && ! c_lflag & ICANON
     *   c_lflag & ICANON && ! c_lflag & ISIG
     * For the moment I just ignore these conflicts.  BTW `good' programs will
     * set values with tcsetattr and then check again with tcgetattr.
     */

    if (termios_p->c_oflag & OPOST)
    {
	if ((__tios_crmod == _TC_COPY_CRMOD && (termios_p->c_oflag & CRMOD))
	    || __tios_crmod == _TC_ALWAYS_CRMOD)
		sgbuf.sg_flags |= CRMOD;

	if (termios_p->c_oflag & XTABS)
		sgbuf.sg_flags |= XTABS;
    }

    if (termios_p->c_iflag & ICRNL)
	/* We couldn't do it better :-(. */
	sgbuf.sg_flags |= CRMOD;

    if (termios_p->c_lflag & T_ECHO)
	sgbuf.sg_flags |= ECHO;
    if (!(termios_p->c_lflag & ICANON))
    {
	if (termios_p->c_lflag & ISIG)
	     sgbuf.sg_flags |= CBREAK;
	else
	     sgbuf.sg_flags |= RAW;
    }

    switch (termios_p->c_cflag & CSIZE)
    {
	case CS5: sgbuf.sg_flags |= BITS5; break;
	case CS6: sgbuf.sg_flags |= BITS6; break;
	case CS7: sgbuf.sg_flags |= BITS7; break;
	case CS8: sgbuf.sg_flags |= BITS8; break;
    }
    if (termios_p->c_cflag & PARENB)
    {
	if (termios_p->c_cflag & PARODD)
	    sgbuf.sg_flags |= ODDP;
	else
	    sgbuf.sg_flags |= EVENP;
    }

    sgbuf.sg_erase = termios_p->c_cc[VERASE];
    sgbuf.sg_kill = termios_p->c_cc[VKILL];
    tcbuf.t_intrc = termios_p->c_cc[VINTR];
    tcbuf.t_quitc = termios_p->c_cc[VQUIT];
    if (termios_p->c_iflag & IXON)
    {
	tcbuf.t_startc = termios_p->c_cc[VSTART];
	tcbuf.t_stopc = termios_p->c_cc[VSTOP];
    }
    else
    {
	tcbuf.t_startc = HIDE_TC(termios_p->c_cc[VSTART]);
	tcbuf.t_stopc = HIDE_TC(termios_p->c_cc[VSTOP]);
    }
    tcbuf.t_eofc = termios_p->c_cc[VEOF];
    tcbuf.t_brkc = termios_p->c_cc[VEOL];

    /* Now we have prepared the new settings we must decide when to change
     * them.  POSIX specifies 3 different optional actions:
     *   setting them now,
     *   wait for all output processed and then set them,
     *   wait for all output processed, discard all input and then set them.
     * The last 2 cannot be handled with old Minix tty drivers because
     * there is no way to wait for all output to be processed and there is
     * only a request to flush both input and output.  Nevertheless I have
     * decided to flush input and output when flushing of input is requested.
     * Wait for output is simply ignored.
     */
#ifdef TIOCFLUSH
    if (opt_actions == TCSAFLUSH)
	ioctl(filedes, TIOCFLUSH, (struct sgttyb *)0);
#endif

    return ioctl(filedes, TIOCSETP, &sgbuf) < 0 ||
	   ioctl(filedes, TIOCSETC, (struct sgttyb *) &tcbuf) < 0 ?
		-1 : 0;
}
