/* The <termios.h> header is used for controlling tty modes. */

#ifndef _TERMIOS_H
#define _TERMIOS_H

#define _TERMIOS_EMULATION 1	/* this is an emulation, not a real termios */

typedef unsigned short tcflag_t;
typedef unsigned char cc_t;
typedef unsigned long speed_t;

#define NCCS		11	/* size of c_cc array */

/* Primary terminal control structure.  POSIX Table 7-1. */
struct termios {
  tcflag_t c_iflag;		/* input modes */
  tcflag_t c_oflag;		/* output modes */
  tcflag_t c_cflag;		/* control modes */
  tcflag_t c_lflag;		/* local modes */
  cc_t c_cc[NCCS];		/* control characters */

  /* The rest of the structure is implementation-defined. */
  speed_t _c_ispeed;		/* input speed */
  speed_t _c_ospeed;		/* output speed */
};

/* Values for termios c_iflag bit map.  POSIX Table 7-2. */
#define BRKINT        000001	/* signal interrupt on break */
#define ICRNL         000002	/* map CR to NL on input */
#define IGNBRK        000004	/* ignore break */
#define IGNCR         000010	/* ignore CR */
#define IGNPAR        000020	/* ignore characters with parity errors */
#define INLCR         000100	/* map NL to CR on input */
#define INPCK         000200	/* enable input parity check */
#define ISTRIP        000400	/* mask off 8th bit */
#define IXOFF         001000	/* enable start/stop input control */
#define IXON          002000	/* enable start/stop output control */
#define PARMRK        004000	/* mark parity errors in the input queue */

/* Values for termios c_oflag bit map.  POSIX Sec. 7.1.2.3. */
#define OPOST         000001	/* perform output processing */

/* The following is stolen from <sgtty.h> and must match. */
#if defined(_MINIX) || !defined(_POSIX_SOURCE)
#define XTABS	     0006000	/* do tab expansion */
#define CRMOD	     0000020	/* map lf to cr + lf */
#endif

/* Values for termios c_cflag bit map.  POSIX Table 7-3. */
#define CLOCAL        000001	/* ignore modem status lines */
#define CREAD         000002	/* enable receiver */
#define CSIZE         000014	/* number of bits per character */
#define CSTOPB        000020	/* send 2 stop bits if set, else 1 */
#define HUPCL         000040	/* hang up on last close */
#define PARENB        000100	/* enable parity on output */
#define PARODD        000200	/* use odd parity if set, else even */

#define CS5           000000	/* if CSIZE is CS5, characters are 5 bits */
#define CS6           000004	/* if CSIZE is CS6, characters are 6 bits */
#define CS7           000010	/* if CSIZE is CS7, characters are 7 bits */
#define CS8           000014	/* if CSIZE is CS8, characters are 8 bits */

/* Values for termios c_lflag bit map.  POSIX Table 7-4. */
#define ECHO          000001	/* enable echoing of input characters */
#define ECHOE         000002	/* echo ERASE as backspace */
#define ECHOK         000004	/* echo KILL */
#define ECHONL        000010	/* echo NL */
#define ICANON        000020	/* canonical input (erase and kill enabled) */
#define IEXTEN        000040	/* enable extended functions */
#define ISIG          000100	/* enable signals */
#define NOFLSH        000200	/* disable flush after interrupt or quit */
#define TOSTOP        000400	/* send SIGTTOU (job cntrl, not implemented) */

/* Indices into c_cc array.  Default values in parentheses. POSIX Table 7-5. */
#define VEOF               0	/* c_cc[VEOF] = EOF char (CTRL-D) */
#define VEOL               1	/* c_cc[VEOL] = EOL char (NUL, not impl) */
#define VERASE             2	/* c_cc[VERASE] = ERASE char (CTRL-H) */
#define VINTR              3	/* c_cc[VINTR] = INTR char (DEL) */
#define VKILL              4	/* c_cc[VKILL] = KILL char (@) */
#define VMIN               5	/* c_cc[VMIN] = MIN value for timer */
#define VQUIT              6	/* c_cc[VQUIT] = QUIT char (CTRL-\) */
#define VTIME              7	/* c_cc[VTIME] = TIME value for timer */
#define VSUSP              8	/* c_cc[VSUSP] = SUSP (job cntrl, not impl) */
#define VSTART             9	/* c_cc[VSTART] = START char (CTRL-S) */
#define VSTOP             10	/* c_cc[VSTOP] = STOP char (CTRL-Q) */

/* Values for the baud rate settings.  POSIX Table 7-6. */
/* Since we are reimplementing this, use a simple encoding.  Perhaps the
 * constants should be cast to speed_t since that is not an int.
 */
#define B0                 0	/* hang up the line */
#define B50               50
#define B75               75
#define B110             110
#define B134             134
#define B150             150
#define B200             200
#define B300             300
#define B600             600
#define B1200           1200
#define B1800           1800
#define B2400           2400
#define B4800           4800
#define B9600           9600
#define B19200         19200
#define B38400         38400
#if defined(_MINIX) || !defined(_POSIX_SOURCE)
#define B28800         28800	/* nonstandard */
#define B57600         57600	/* nonstandard */
#define B115200       115200	/* nonstandard */
#endif

/* Optional actions for tcsetattr().  POSIX Sec. 7.2.1.2. */
#define TCSANOW            1	/* changes take effect immediately */
#define TCSADRAIN          2	/* changes take effect after output is done */
#define TCSAFLUSH          3	/* wait for output to finish and flush input */

/* Queue_selector values for tcflush().  POSIX Sec. 7.2.2.2. */
#define TCIFLUSH           1	/* flush accumulated input data */
#define TCOFLUSH           2	/* flush accumulated output data */
#define TCIOFLUSH          3	/* flush accumulated input and output data */

/* Action values for tcflow().  POSIX Sec. 7.2.2.2. */
#define TCOOFF             1	/* suspend output */
#define TCOON              2	/* restart suspended output */
#define TCIOFF             3	/* transmit a STOP character on the line */
#define TCION              4	/* transmit a START character on the line */

#if defined(_MINIX) || !defined(_POSIX_SOURCE)
/* Kludge */
#define _TC_COPY_CRMOD     0
#define _TC_ALWAYS_CRMOD   1
#define _TC_NEVER_CRMOD    2
extern int __tios_crmod;
#endif

/* Function Prototypes. */
#ifndef _ANSI_H
#include <ansi.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
_PROTOTYPE( int tcsendbreak, (int _fildes, int _duration)		);
_PROTOTYPE( int tcdrain, (int _filedes)					);
_PROTOTYPE( int tcflush, (int _filedes, int _queue_selector)		);
_PROTOTYPE( int tcflow, (int _filedes, int _action)			);
_PROTOTYPE( speed_t cfgetospeed, (const struct termios *_termios_p)	);
_PROTOTYPE( int cfsetospeed, (struct termios *_termios_p, speed_t _speed) );
_PROTOTYPE( speed_t cfgetispeed, (const struct termios *_termios_p)	);
_PROTOTYPE( int cfsetispeed, (struct termios *_termios_p, speed_t _speed) );
_PROTOTYPE( int tcgetattr, (int _filedes, struct termios *_termios_p)	);
_PROTOTYPE( int tcsetattr, (int _filedes, int _optional_actions,
			    const struct termios *_termios_p)		);
#ifdef __cplusplus
}
#endif

#define cfgetispeed(termios_p)		((termios_p)->_c_ispeed)
#define cfgetospeed(termios_p)		((termios_p)->_c_ospeed)
#define cfsetispeed(termios_p, speed)	((termios_p)->_c_ispeed = (speed), 0)
#define cfsetospeed(termios_p, speed)	((termios_p)->_c_ospeed = (speed), 0)

#endif /* _TERMIOS_H */
