/* term - terminal simulator		Author: Andy Tanenbaum */

/* This program allows the user to turn a MINIX system into a dumb
 * terminal to communicate with a remote computer through one of the ttys.
 * It forks into two processes.  The parent sits in a tight loop copying
 * from stdin to the tty.  The child sits in a tight loop copying from
 * the tty to stdout.
 *
 * 2 Sept 88 BDE (Bruce D. Evans): Massive changes to make current settings the
 * default, allow any file as the "tty", support fancy baud rates and remove
 * references to and dependencies on modems and keyboards, so (e.g.)
 * a local login on /dev/tty1 can do an external login on /dev/tty2.
 *
 * 3 Sept 88 BDE: Split parent again to main process copies from stdin to a
 * pipe which is copied to the tty.  This stops a blocked write to the
 * tty from hanging the program.
 *
 * 11 Oct 88 BDE: Cleaned up baud rates and parity stripping.
 *
 * 09 Oct 90 MAT (Michael A. Temari): Fixed bug where terminal isn't reset
 * if an error occurs.
 *
 * Nov 90 BDE: Don't broadcast kill(0, SIGINT) since two or more of these
 * in a row will kill the parent shell.
 *
 * 19 Oct 89 RW (Ralf Wenk): Adapted to MINIX ST 1.1 + RS232 driver. Split
 * error into error_n and error. Added resetting of the terminal settings
 * in error.
 *
 * 24 Nov 90 RW: Adapted to MINIX ST 1.5.10.2. Forked processes are now
 * doing an exec to get a better performance. This idea is stolen from
 * a terminal program written by Felix Croes.
 *
 * 01 May 91 RW: Merged the MINIX ST patches with Andys current version.
 * Most of the 19 Oct 89 patches are deleted because they are already there.
 *
 * Example usage:
 *	term			: baud, bits/char, parity from /dev/tty1
 *	term 9600 7 even	: 9600 baud, 7 bits/char, even parity
 *	term odd 300 7		:  300 baud, 7 bits/char, odd parity
 *	term /dev/tty2		: use /dev/tty2 rather than /dev/tty1
 *				: Any argument starting with "/" is
 *				: taken as the communication device.
 */

#include <sys/types.h>
#include <minix/config.h>
#include <fcntl.h>
#include <sgtty.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXARGS  3		/* maximum number of uart params */
#define CHUNK 1024		/* how much to read at once */

#if 0
/* Hack some new baud rates for Minix. Minix uses a divide-by-100 encoding. */
#define B200     2
#define B600     6
#define B1800   18
#define B3600   36
#define B7200   72
#define B19200 192
#define EXTA   192

/* We can't handle some standard (slow) V7 speeds and speeds above 25500 since
 * since the speed is packed into a char :-(. Trap them with an illegal value.
 */
#define B50      0
#define B75      0
#define B134     0
#define EXTB     0
#ifndef B38400
#define B38400   0
#endif
#ifndef B57600
#define B57600   0
#endif
#ifndef B115200
#define B115200  0
#endif
#endif

#define TERM_LINE "/dev/tty1"	/* which serial port to use */

int commfd;			/* open file no. for comm device */
int readpid;			/* pid of child reading commfd */
struct sgttyb sgcommfd;		/* saved terminal parameters for commfd */
struct sgttyb sgstdin;		/* saved terminal parameters for stdin */
int writepid;			/* pid of child writing commfd */

/* Sequence to leave simulator.  It must arrive in a single read. */
#if (MACHINE == ATARI)
#if 0
char endseq[] = "\033OY";	/* 'F10' (pre 1.6.20) */
#endif
char endseq[] = "\033[20~";	/* 'F10' */
#endif
#if (CHIP == INTEL)
#if 0
char endseq[] = "\005";		/* Ctrl-E (if no numeric keypad) */
#endif
char endseq[] = "\033[G";	/* numeric keypad '5' */
#endif

struct param_s {
  char *pattern;
  int value;
  char type;
#define BAD      0
#define BITS     1
#define NOSTRIP  2
#define PARITY   3
#define SPEED    4
}

 params[] =
{
  "5", BITS5, BITS,
  "6", BITS6, BITS,
  "7", BITS7, BITS,
  "8", BITS8, BITS,

  "even", EVENP, PARITY,
  "odd", ODDP, PARITY,
  "nostrip", 0, NOSTRIP,

  "50", B50, SPEED,
  "75", B75, SPEED,
  "110", B110, SPEED,
  "134", B134, SPEED,
  "200", B200, SPEED,
  "300", B300, SPEED,
  "600", B600, SPEED,
  "1200", B1200, SPEED,
  "1800", B1800, SPEED,
  "2400", B2400, SPEED,
  "3600", B3600, SPEED,
  "4800", B4800, SPEED,
  "7200", B7200, SPEED,
  "9600", B9600, SPEED,
  "19200", B19200, SPEED,
  "EXTA", EXTA, SPEED,
  "EXTB", EXTB, SPEED,
  "38400", B38400, SPEED,
  "57600", B57600, SPEED,
  "115200", B115200, SPEED,
  "", 0, BAD,			/* BAD type to end list */
};
unsigned char strip_parity = 1;	/* nonzero to strip high bits before output */

_PROTOTYPE(int main, (int argc, char *argv[]));
_PROTOTYPE(void set_uart, (int argc, char *argv[]));
_PROTOTYPE(void set_mode, (int fd, int speed, int parity, int bits,
			   struct sgttyb * sgsavep));
_PROTOTYPE(void copy, (int in, char *inname, int out,char *outname,char *end));
_PROTOTYPE(void error, (char *s1, char *s2));
_PROTOTYPE(void quit, (int dummy));
_PROTOTYPE(void write2sn, (char *s1, char *s2));

int main(argc, argv)
int argc;
char *argv[];
{
  char *commdev = (char *) NULL;
  int i;
  int pipefd[2];
#if ( CHIP == M68000 )
  char in[2], out[2];
  in[1] = out[1] = '\0';

/* Ugly way to process both hidden special arguments "read" or "write" early
 * enough. If one of the special arguments is given the other arguments are
 * ASCII values used by the copy() function. File descriptors are encoded
 * as characters starting with '0'.
 */
  if (argc > 1 &&
      (strcmp(argv[1], "-read") == 0 || strcmp(argv[1], "-write") == 0))
	copy(*argv[2] - '0', argv[3], *argv[4] - '0', argv[5], argv[6]);
#endif

  sync();
  for (i = 1; i < argc; ++i)
	if (argv[i][0] == '/') {
		if (commdev != (char *) NULL) {
			write2sn("Too many communication devices", "");
			exit(1);
		}
		commdev = argv[i];
	}
  if (commdev == (char *) NULL) {
	i = MAXARGS + 1;
	commdev = TERM_LINE;
  } else
	i = MAXARGS + 2;
  if (argc > i) {
	write2sn("Usage: term [baudrate] [data_bits] [parity]", "");
	exit(1);
  }
  commfd = open(commdev, O_RDWR);
  if (commfd < 0) {
	write2sn("Can't open ", commdev);
	exit(1);
  }

  /* Save state of both devices before altering either (may be identical!). */
  ioctl(0, TIOCGETP, &sgstdin);
  ioctl(commfd, TIOCGETP, &sgcommfd);
  set_mode(0, -1, -1, -1, &sgstdin);	/* RAW mode on stdin, others
					 * current */
  set_uart(argc, argv);

  /* Main body of the terminal simulator. */
  signal(SIGINT, quit);
  signal(SIGPIPE, quit);
  if (pipe(pipefd) < 0) error("Can't create pipe", "");
  switch ((writepid = fork())) {
      case -1:
	error("Can't create process to write to comm device", "");
      case 0:
	/* Piped stdin to tty */
	close(pipefd[1]);
#if (CHIP == M68000)
	in[0] = '0' + pipefd[0];
	out[0] = '0' + commfd;
	execlp(argv[0], argv[0], "-write", in, "piped stdin", out, commdev, "",
	       (char *) NULL);
	/* If execlp() failed, try the usual way. */
#endif
	copy(pipefd[0], "piped stdin", commfd, commdev, "");
  }
  close(pipefd[0]);
  switch ((readpid = fork())) {
      case -1:
	error("Can't create process to read from comm device", "");
      case 0:
	/* Tty to stdout */
#if (CHIP == M68000)
	in[0] = '0' + commfd;
	out[0] = '0' + 1;
	execlp(argv[0], argv[0], "-read", in, commdev, out, "stdout", "",
	       (char *) NULL);
	/* If execlp() failed, try the usual way. */
#endif
	copy(commfd, commdev, 1, "stdout", "");
  }

  /* Stdin to pipe */
  copy(0, "stdin", pipefd[1], "redirect stdin", endseq);
  return(0);
}


void set_uart(argc, argv)
int argc;
char *argv[];
{
/* Set up the UART parameters. */

  int i, j, bits, nbits, parity, nparities, speed, nspeeds;
  char *arg;
  register struct param_s *param;

  /* Examine all the parameters and check for validity. */
  nspeeds = nparities = nbits = 0;
  speed = parity = bits = -1;	/* -1 means use current value */
  for (i = 1; i < argc; ++i) {
	if ((arg = argv[i])[0] == '/') continue;

	/* Check parameter for legality. */
	for (j = 0, param = &params[0];
	     param->type != BAD && strcmp(arg, param->pattern) != 0;
	     ++j, ++param);
	switch (param->type) {
	    case BAD:
		error("Invalid parameter: ", arg);
	    case BITS:
		bits = param->value;
		if (++nbits > 1) error("Too many character sizes", "");
		break;
	    case PARITY:
		parity = param->value;
		if (++nparities > 1) error("Too many parities", "");
		break;
	    case SPEED:
		speed = param->value;
		if (speed == 0) error("Invalid speed: ", arg);
		if (++nspeeds > 1) error("Too many speeds", "");
		break;
	    case NOSTRIP:	strip_parity = 0;	break;
	}
  }
  set_mode(commfd, speed, parity, bits, &sgcommfd);
}


void set_mode(fd, speed, parity, bits, sgsavep)
int fd;
int speed;
int parity;
int bits;
struct sgttyb *sgsavep;
{
  /* Set open file fd to RAW mode with the given other modes. If fd is
   * not a tty, this may do nothing but connecting ordinary files as
   * ttys may have some use. */

  struct sgttyb sgtty;
  int tabs;

  sgtty = *sgsavep;
  tabs = sgtty.sg_flags & XTABS;
  if (speed == -1) speed = sgtty.sg_ispeed;
  if (parity == -1) parity = sgtty.sg_flags & (EVENP | ODDP);
  if (bits == -1)
	bits = sgtty.sg_flags & BITS8;	/* BITS8 is actually a mask */
  sgtty.sg_ispeed = speed;
  sgtty.sg_ospeed = speed;
  sgtty.sg_flags = RAW | parity | bits | tabs;
  ioctl(fd, TIOCSETP, &sgtty);
}


void copy(in, inname, out, outname, end)
int in;
char *inname;
int out;
char *outname;
char *end;
{
/* Copy from one open file to another. If the 'end' sequence is not "", and
 * precisely matches the input, terminate the copy and various children.
 * The end sequence is best provided by keyboard input from one of the
 * special keys which always produces chars in a bunch. RAW mode almost
 * guarantees exactly one keystroke's worth of input at a time.
 */

  static char buf[CHUNK];
  char *bufend;
  register char *bufp;
  int count;
  int len;

  len = strlen(end);
  while (1) {
	if ((count = read(in, buf, CHUNK)) <= 0) {
		write2sn("Can't read from ", inname);
		quit(0);
	}
	if (count == len && strncmp(buf, end, (size_t) count) == 0) quit(0);
	if (strip_parity) for (bufp = buf, bufend = bufp + count;
		     bufp < bufend; ++bufp)
			*bufp &= 0x7F;
	if (write(out, buf, (size_t) count) != count) {
		write2sn("Can't write to ", outname);
		quit(0);
	}
  }
}


void error(s1, s2)
char *s1;
char *s2;
{
  ioctl(commfd, TIOCSETP, &sgcommfd);
  ioctl(0, TIOCSETP, &sgstdin);
  write2sn(s1, s2);
  exit(1);
}


void quit(s)
int s;				/* not used, just to make prototype ok */
{
  if (readpid != 0 && writepid != 0) {
	/* This is the parent of the other two processes.  It cleans up. */
	ioctl(commfd, TIOCSETP, &sgcommfd);
	ioctl(0, TIOCSETP, &sgstdin);
	kill(readpid, SIGINT);
	kill(writepid, SIGINT);
  }
  exit(0);
}


void write2sn(s1, s2)
char *s1;
char *s2;
{
  write(1, s1, strlen(s1));
  write(1, s2, strlen(s2));
  write(1, "\r\n", 2);
}
