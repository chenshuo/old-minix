/* This process is the father (mother) of all MINIX user processes.  When
 * MINIX comes up, this is process 2.  It executes the /etc/rc shell file and
 * then reads the /etc/ttys file to find out which terminals need a login
 * process.  The ttys file consists of 3-character lines as follows:
 *	abc
 * where
 *	a = 0 (line disabled = no shell), 1 (enabled = shell started)
 *	b = a-r defines UART paramers (baud, bits, parity), 0 for console
 *	c = line number
 *
 * The letters a-r correspond to the 18 entries of the uart table below.
 * For example, 'a' is 110 baud, 8 bits, no parity; 'b' is 300 baud, 8 bits,
 * no parity; 'j' is 2400 baud, 7 bits, even parity; etc.
 *
 * If the file /usr/adm/wtmp exists and is writable, init (with help from
 * login) maintains login accounting used by who(1).
 */

#include <signal.h>
#include <sgtty.h>

#define PIDSLOTS          10
#define NPARAMSETS  	  18
#define STACKSIZE        256
#define DIGIT              8
#define OFFSET             5
#define SHELL              1
#define NOPARAMS        -100
#define WTMPSIZE           8

extern long time();
extern long lseek();

struct uart {
  int baud;
  int flags;
} uart[NPARAMSETS] = {
	B110,   BITS8,		/*  110 baud, 8 bits, no parity */
	B300,   BITS8,		/*  300 baud, 8 bits, no parity */
	B1200,  BITS8,		/* 1200 baud, 8 bits, no parity */
	B2400,  BITS8,		/* 2400 baud, 8 bits, no parity */
	B4800,  BITS8,		/* 4800 baud, 8 bits, no parity */
	B9600,  BITS8,		/* 9600 baud, 8 bits, no parity */

	B110,   BITS7 | EVENP,	/*  110 baud, 7 bits, even parity */
	B300,   BITS7 | EVENP,	/*  300 baud, 7 bits, even parity */
	B1200,  BITS7 | EVENP,	/* 1200 baud, 7 bits, even parity */
	B2400,  BITS7 | EVENP,	/* 2400 baud, 7 bits, even parity */
	B4800,  BITS7 | EVENP,	/* 4800 baud, 7 bits, even parity */
	B9600,  BITS7 | EVENP,	/* 9600 baud, 7 bits, even parity */

	B110,   BITS7 | ODDP,	/*  110 baud, 7 bits, odd parity */
	B300,   BITS7 | ODDP,	/*  300 baud, 7 bits, odd parity */
	B1200,  BITS7 | ODDP,	/* 1200 baud, 7 bits, odd parity */
	B2400,  BITS7 | ODDP,	/* 2400 baud, 7 bits, odd parity */
	B4800,  BITS7 | ODDP,	/* 4800 baud, 7 bits, odd parity */
	B9600,  BITS7 | ODDP	/* 9600 baud, 7 bits, odd parity */
};

char wtmpfile[] = {"/usr/adm/wtmp"};
char name[] = {"/dev/tty?"};	/* terminal names */
int pid[PIDSLOTS];		/* pids of init's own children */
int save_params[PIDSLOTS];
int pidct;
extern int errno;

char stack[STACKSIZE];
char *stackpt = &stack[STACKSIZE];
char **environ;			/* declaration required by library routines */

struct sgttyb args;


main()
{
  char line[10];		/* /etc/ttys lines should be 3 chars */
  int rc, tty, k, status, ttynr, ct, i, params, shell;

  /* Carry out /etc/rc. */
  sync();			/* force buffers out onto RAM disk */

  /* Execute the /etc/rc file. */
  if (fork()) {
	/* Parent just waits. */
	wait(&k);
  } else {
	/* Child exec's the shell to do the work. */
	if (open("/etc/rc", 0) < 0) exit(-1);
	open("/dev/tty0", 1);	/* std output */
	open("/dev/tty0", 1);	/* std error */
	execn("/bin/sh");
	exit(-2);		/* impossible */
  }

  /* Make the /usr/adm/wtmp entry. */
  wtmp("~","");			/* log system reboot */

  /* Read the /etc/ttys file and fork off login processes. */
  if ( (tty = open("/etc/ttys", 0)) == 0) {
	/* Process /etc/ttys file. */
	while ( (ct = read(0, line, 4)) == 4) {
		/* Extract and check the 3 characters on each line. */
		shell = line[0] - '0';	/* 0, 1, 2 for disabled, sh, no sh */
		params = line[1] - 'a';	/* selects UART parameters */
		ttynr = line[2] - '0';	/* line number */
		if (shell <= 0 || shell > 1) continue;
		if (line[1] == '0') params = NOPARAMS;
		else if (params < 0 || params > NPARAMSETS) continue;
		if (ttynr < 0 || ttynr > PIDSLOTS) continue;

		save_params[ttynr] = params;
		startup(ttynr, params);
	}
  } else {
	tty = open("/dev/tty0", 1);
	write(tty, "Init can't open /etc/ttys\n", 26);
	while (1) ;		/* just hang -- system cannot be started */
  }
  close(tty);

  /* All the children have been forked off.  Wait for someone to terminate.
   * Note that it might be a child, in which case a new login process must be
   * spawned off, or it might be somebody's orphan, in which case ignore it.
   * First ignore all signals.
   */
  for (i = 1; i <= NR_SIGS; i++) signal(i, SIG_IGN);

  while (1) {
	sync();
	k = wait(&status);
	pidct--;

	/* Search to see which line terminated. */
	for (i = 0; i < PIDSLOTS; i++) {
		if (pid[i] == k) {
			name[DIGIT] = '0' + i;
			wtmp(&name[OFFSET], "");
			startup(i, save_params[i]);
		}
	}
  }
}


startup(linenr, params)
int linenr, params;
{
/* Fork off a process for the indicated line. */

  int k, n;

  if ( (k = fork()) != 0) {
	/* Parent */
	pid[linenr] = k;
	pidct++;
  } else {
	/* Child */
	close(0);		/* /etc/ttys may be open */
	name[DIGIT] = '0' + linenr;
	if (open(name, 2) != 0) exit(-3);	/* standard input */
	if (open(name, 2) != 1) exit(-3);	/* standard output */
	if (open(name, 2) != 2) exit(-3);	/* standard error */


	/* Set line parameters. */
  	if (params != NOPARAMS) {
		n = ioctl(0, TIOCGETP, &args);	/* get parameters */
		args.sg_ispeed = uart[params].baud;
		args.sg_ospeed = uart[params].baud;
		args.sg_flags = CRMOD | XTABS | ECHO | uart[params].flags;
		n = ioctl(0, TIOCSETP, &args);
	}

	/* Try to exec login, or in an emergency, exec the shell. */
	execn("/usr/bin/login");
	execn("/bin/login");
	execn("/bin/sh");	/* last resort, if mount of /usr failed */
	execn("/usr/bin/sh");	/* last resort, if mount of /usr failed */
	return;			/* impossible */
  }
}

wtmp(tty, name)
{
/* Make an entry in /usr/adm/wtmp. */

  int i, fd;
  long t, time();
  char ttybuff[WTMPSIZE], namebuff[WTMPSIZE];

  fd = open(wtmpfile, 2);
  if (fd < 0) return;		/* if wtmp does not exist, no accounting */
  i =lseek(fd, 0L, 2);		/* append to file */

  for (i = 0; i < WTMPSIZE; i++) {
	ttybuff[i] = 0;
	namebuff[i] = 0;
  }
  strncpy(ttybuff, tty, 8);
  strncpy(namebuff, name, 8);
  time(&t);
  write(fd, ttybuff, WTMPSIZE);
  write(fd, namebuff, WTMPSIZE);
  write(fd, &t, sizeof(t));
  close(fd);
}
