/* getty - get tty speed			Author: Fred van Kempen */

/*
 * GETTY  -     Initialize and serve a login-terminal for INIT.
 *		Also, select the correct speed. The STTY() code
 *		was taken from stty(1).c; which was written by
 *		Andrew S. Tanenbaum.
 *
 * Usage:	getty [-c filename] [-h] [-k] [-t] line [speed]
 *
 * Version:	3.4	02/17/90
 *
 * Author:	F. van Kempen, MicroWalt Corporation
 *
 * Modifications:
 *		All the good stuff removed to get a minimal getty, because
 *		many modems don't like all that fancy speed detection stuff.
 *		03/03/91	Kees J. Bot (kjb@cs.vu.nl)
 *
 *		Uname(), termios.  More nonsense removed.  (The result has
 *		only 10% of the original functionality, but a 10x chance of
 *		working.)
 *		12/12/92	Kees J. Bot
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/utsname.h>

char LOGIN[] =		"/usr/bin/login";
char SHELL[] =		"/bin/sh";

#define ST_IDLE			 44	/* the getty is idle */
#define ST_RUNNING		 55	/* getty is now RUNNING */
#define ST_SUSPEND		 66	/* getty was SUSPENDed for dialout */


int state = ST_IDLE;		/* the IDLE/SUSPEND/RUNNING state flag */
char *tty_name;			/* name of the line */


void sigcatch(int sig)
{
/* Catch the signals that want to catch. */

  switch(sig) {
  case SIGEMT:	/* SIGEMT means SUSPEND */
	if (state == ST_IDLE) state = ST_SUSPEND;
	break;
  case SIGIOT:	/* SIGIOT means RESTART */
	if (state == ST_SUSPEND) state = ST_RUNNING;
	break;
  case SIGBUS:	/* SIGBUS means IGNORE ALL */
	signal(SIGEMT, SIG_IGN);
	signal(SIGIOT, SIG_IGN);
	state = ST_RUNNING;
	break;
  }
  signal(sig, sigcatch);
}


void std_out(char *s)
{
  write(1, s, strlen(s));
}


/* Read one character from stdin.
 */
int areadch(void)
{
  int st;
  char ch1;

  /* read character from TTY */
  st = read(0, &ch1, 1);
  if (st == 0) {
	std_out("\n");
	exit(0);
  }
  if (st < 0) {
	if (errno == EINTR) return(-1);		/* SIGNAL received! */
	if (errno != EINTR) {
		std_out("getty: ");
		std_out(tty_name);
		std_out(": read error\n");
		pause();
		exit(1);
	}
  }

  return(ch1 & 0xFF);
}


/* Handle the process of a GETTY.
 */
void do_getty(char *name, size_t len)
{
  register char *np, *s;
  int ch;
  struct utsname utsname;

  /* Display prompt. */
  ch = ' ';
  *name = '\0';
  while (ch != '\n') {
	/* Get data about this machine. */
	uname(&utsname);

	/* Give us a new line */
	std_out("\n");
	std_out(utsname.sysname);
	std_out("  Release ");
	std_out(utsname.release);
	std_out(" Version ");
	std_out(utsname.version);
	std_out("\n\n");
	std_out(utsname.nodename);
	std_out(" login: ");

	np = name;
	while (ch != '\n') {
		ch = areadch();	/* adaptive READ */
		switch (ch) {
		    case -1:	/* signalled! */
			if (state == ST_SUSPEND) {
				while (state != ST_IDLE) {
					pause();
					if (state == ST_RUNNING)
							state = ST_IDLE;
				}
			}
			ch = ' ';
			continue;
		    case '\n':
			*np = '\0';
			break;
		    default:
			if (np < name + len) *np++ = ch;
		}
	}
	if (*name == '\0') ch = ' ';	/* blank line typed! */
  }
}


/* Execute the login(1) command with the current
 * username as its argument. It will reply to the
 * calling user by typing "Password: "...
 */
void do_login(char *name)
{ 
  execl(LOGIN, LOGIN, name, (char *) NULL);
  /* Failed to exec login.  Impossible, but true.  Try a shell.  (This is
   * so unlikely that we forget about the security implications.)
   */
  execl(SHELL, SHELL, (char *) NULL);
}


int main(int argc, char **argv)
{
  register char *s;
  char name[30];

  tty_name = ttyname(0);
  if (tty_name == NULL) {
	std_out("getty: tty name unknown\n");
	pause();
	return(1);
  }

  chown(tty_name, 0, 0);	/* set owner of TTY to root */
  chmod(tty_name, 0600);	/* mode to max secure */

  /* Catch some of the available signals. */
  signal(SIGEMT, sigcatch);
  signal(SIGIOT, sigcatch);
  signal(SIGBUS, sigcatch);

  do_getty(name, sizeof(name));	/* handle getty() */
  name[29] = '\0';		/* make sure the name fits! */

  do_login(name);		/* and call login(1) if OK */

  return(1);			/* never executed */
}
