/* term - terminal simulator		Author: Andy Tanenbaum */

/* This program allows the user to turn a MINIX system into a dumb
 * terminal to communicate with a remote computer over a modem.  It
 * forks into two processes.  The parent sits in a tight loop copying
 * from the keyboard to the modem.  The child sits in a tight loop
 * copying from the modem to the screen.
 *
 * Example usage:
 *	term			: 1200 baud, 8 bits/char, no parity
 *	term 9600 7 even	: 9600 baud, 7 bits/char, even parity
 *	term odd 300 7		:  300 baud, 7 bits/char, odd parity
 */

#include <signal.h>
#include <sgtty.h>

#define MAXARGS 3		/* maximum number of uart params */
#define NCHECKS 10
#define BAD -1
#define GOOD 1
#define DEF_SPEED B1200		/* default baudrate */
#define DEF_BITS BITS8		/* default bits/char */
#define MODEM "/dev/tty1"	/* special file attached to the modem */
#define ESC 033			/* character to hit to leave simulator */
#define LIMIT 3			/* how often do you have to hit  ESC to exit*/
#define CHUNK 1024		/* how much to read at once */

int modem, pid;			/* file descriptor for modem */
char *pat[NCHECKS] = 
	{"5", "6", "7", "8", "110", "300", "1200", "2400", "4800", "9600"};

int value[NCHECKS] = 
	{BITS5, BITS6, BITS7, BITS8, B110, B300, B1200, B2400, B4800, B9600};

int hold[MAXARGS];
struct sgttyb sgtty, sgsave1, sgsave2;

main(argc, argv)
int argc;
char *argv[];
{

  sync();
  modem = open(MODEM, 2);
  if (modem < 0) {
	printf("Can't open modem on %s\n", MODEM);
	exit(1);
  }
  set_uart(argc, argv);

  /* Main body of the terminal simulator. */
  if ( (pid = fork()))
	copy(0, modem, ESC);	/* copy from stdin to modem */
  else
	copy(modem, 1, -1);	/* copy from modem to stdout */
}

set_uart(argc, argv)
int argc;
char *argv[];
{
/* Set up the UART parameters. */

  int i, k, v, nspeeds = 0, speed, nbits = 0, bits, parity = 0;

  if (argc > MAXARGS + 1)
	error("Usage: term [baudrate] [data_bits] [parity]\n");

  /* Examine all the parameters and check for validity. */
  speed = DEF_SPEED;		/* default line speed */
  bits = DEF_BITS;		/* default bits/char */
  for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "even") == 0) {parity = EVENP; continue;}
	if (strcmp(argv[i], "odd") == 0)  {parity = ODDP; continue;}
	v = validity(argv[i]);
	if (v == BAD) {
		printf("Invalid parameter: %s\n", argv[i]);
		exit(1);
	}
	k = atoi(argv[i]);
	if (k > 100) {
		speed = value[v];
		nspeeds++;
	}
	if ( k < 10) {
		bits = value[v];
		nbits++;
	}
	if (nspeeds > 1) error("Too many speeds\n");
	if (nbits > 1) error("Too many character sizes\n");
  }

  /* Fetch the modem parameters, save them, and set new ones. */
  ioctl(modem, TIOCGETP, &sgtty);
  sgsave1 = sgtty;		/* modem parameters */
  sgtty.sg_ispeed = speed;
  sgtty.sg_ospeed = speed;
  sgtty.sg_flags = RAW | parity | bits;
  ioctl(modem, TIOCSETP, &sgtty);
  
  /* Fetch the keyboard parameters, save them, and set new ones. */
  ioctl(0, TIOCGETP, &sgtty);
  sgsave2 = sgtty;		/* modem parameters */
  sgtty.sg_flags = (sgtty.sg_flags & 01700) + RAW;
  ioctl(0, TIOCSETP, &sgtty);
}


int validity(s)
char *s;
{
/* Check parameter for legality. */

  int i;

  for (i = 0; i < NCHECKS; i++) {
	if (strcmp(s, pat[i]) == 0) return(i);
  }
  return(BAD);
}
 

copy(in, out, end)
int in, out, end;
{
/* Copy from the keyboard to the modem or vice versa. If the end character
 * is seen LIMIT times in a row, quit.  For the traffic from the modem, the
 * end character is -1, which cannot occur since the characters from the
 * modem are unsigned integers in the range 0 to 255.
 */

  int t, count, state = 0;
  char buf[CHUNK], *p;

  while (1) {
	if ( (count = read(in, buf, CHUNK)) < 0) {
		printf("Can't read from modem\r\n");
		quit();
	}

	if (end > 0) {
		for (p = &buf[0]; p < &buf[count]; p++) {
			t = *p & 0377;		/* t is unsigned int 0 - 255 */
			if (t == end) {
				if (++state == LIMIT) quit();
			} else {
				state = 0;
			}
		}
	}
	write(out, buf, count);
  }
}


error(s)
char *s;
{
  printf("%s", s);
  exit(1);
}

quit()
{
  ioctl(modem, TIOCSETP, &sgsave1);
  ioctl(0, TIOCSETP, &sgsave2);
  if (getpid() != pid) kill(pid, SIGINT);
  exit(0);
}
