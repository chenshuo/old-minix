/* stty - set terminal mode	  	Author: Andy Tanenbaum */

#include <sgtty.h>
#include <stdlib.h>
#include <minix/minlib.h>
#include <stdio.h>

char *on[] = {"tabs", "cbreak", "raw", "-nl", "echo", "odd", "even"};
char *off[] = {"-tabs", "", "", "nl", "-echo", "", ""};
int k;

struct sgttyb args;
struct tchars tch;

#define STARTC	 021		/* CTRL-Q */
#define STOPC	 023		/* CTRL-S */
#define QUITC	 034		/* CTRL-\ */
#define EOFC	 004		/* CTRL-D */
#define DELC	0177		/* DEL */
#define BYTE    0377
#define FD         0		/* which file descriptor to use */

#define speed(s) args.sg_ispeed = s;  args.sg_ospeed = s
#define clr1 args.sg_flags &= ~(BITS5 | BITS6 | BITS7 | BITS8)
#define clr2 args.sg_flags &= ~(EVENP | ODDP)

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void report, (void));
_PROTOTYPE(void pr, (int f, int n));
_PROTOTYPE(void option, (char *opt, char *next));
_PROTOTYPE(int match, (char *s1, char *s2));
_PROTOTYPE(int interpret, (char *s));
_PROTOTYPE(void prctl, (int c));

int main(argc, argv)
int argc;
char *argv[];
{

  /* Stty with no arguments just reports on current status. */
  if (ioctl(FD,TIOCGETP,&args)<0 || ioctl(FD,TIOCGETC,(struct sgttyb*)&tch)<0){
	printf("%s: can't read ioctl parameters from stdin\n", argv[0]);
	exit(1);
  }
  if (argc == 1) {
	report();
	exit(0);
  }

  /* Process the options specified. */
  k = 1;
  while (k < argc) {
	option(argv[k], k + 1 < argc ? argv[k + 1] : "");
	k++;
  }
  if (ioctl(FD,TIOCSETP,&args)<0 || ioctl(FD,TIOCSETC,(struct sgttyb*)&tch)<0){
	printf("%s: can't write ioctl parameters to stdin\n", argv[0]);
	exit(2);
  }
  return(0);
}



void report()
{
  int mode, ispeed, ospeed;

  mode = args.sg_flags;
  pr(mode & XTABS, 0);
  pr(mode & CBREAK, 1);
  pr(mode & RAW, 2);
  pr(mode & CRMOD, 3);
  pr(mode & ECHO, 4);
  pr(mode & ODDP, 5);
  pr(mode & EVENP, 6);

  ispeed = 100 * ((int) args.sg_ispeed & BYTE);
  ospeed = 100 * ((int) args.sg_ospeed & BYTE);
  printf("\nkill = ");
  prctl(args.sg_kill);
  printf("\nerase = ");
  prctl(args.sg_erase);
  printf("\nintr = ");
  prctl(tch.t_intrc);
  printf("\nquit = ");
  prctl(tch.t_quitc);
  if (ispeed > 0) {
	printf("\nspeed = ");
	switch (ispeed) {
	    case 100:	printf("110");	break;
	    case 200:	printf("200");	break;
	    case 300:	printf("300");	break;
	    case 600:	printf("600");	break;
	    case 1200:	printf("1200");	break;
	    case 1800:	printf("1800");	break;
	    case 2400:	printf("2400");	break;
	    case 3600:	printf("3600");	break;
	    case 4800:	printf("4800");	break;
	    case 7200:	printf("7200");	break;
	    case 9600:	printf("9600");	break;
	    case 19200:	printf("19200");	break;
	    case 19300:	printf("115200");	break;
	    case 19400:	printf("57600");	break;
	    case 19500:	printf("38400");	break;
	    case 19600:	printf("28800");	break;
	    default:	printf("unknown");
	}
	switch (mode & BITS8) {
	    case BITS5:	printf("\nbits = 5");	break;
	    case BITS6:	printf("\nbits = 6");	break;
	    case BITS7:	printf("\nbits = 7");	break;
	    case BITS8:	printf("\nbits = 8");	break;
	}
  }
  printf("\n");
}

void pr(f, n)
int f, n;
{
  if (f)
	printf("%s ", on[n]);
  else
	printf("%s ", off[n]);
}

void option(opt, next)
char *opt, *next;
{
  if (match(opt, "-tabs")) {
	args.sg_flags &= ~XTABS;
	return;
  }
  if (match(opt, "-odd")) {
	args.sg_flags &= ~ODDP;
	return;
  }
  if (match(opt, "-even")) {
	args.sg_flags &= ~EVENP;
	return;
  }
  if (match(opt, "-raw")) {
	args.sg_flags &= ~RAW;
	return;
  }
  if (match(opt, "-cbreak")) {
	args.sg_flags &= ~CBREAK;
	return;
  }
  if (match(opt, "-echo")) {
	args.sg_flags &= ~ECHO;
	return;
  }
  if (match(opt, "-nl")) {
	args.sg_flags |= CRMOD;
	return;
  }
  if (match(opt, "tabs")) {
	args.sg_flags |= XTABS;
	return;
  }
  if (match(opt, "even")) {
	clr2;
	args.sg_flags |= EVENP;
	return;
  }
  if (match(opt, "odd")) {
	clr2;
	args.sg_flags |= ODDP;
	return;
  }
  if (match(opt, "raw")) {
	args.sg_flags |= RAW;
	return;
  }
  if (match(opt, "cbreak")) {
	args.sg_flags |= CBREAK;
	return;
  }
  if (match(opt, "echo")) {
	args.sg_flags |= ECHO;
	return;
  }
  if (match(opt, "nl")) {
	args.sg_flags &= ~CRMOD;
	return;
  }
  if (match(opt, "kill")) {
	args.sg_kill = interpret(next);
	k++;
	return;
  }
  if (match(opt, "erase")) {
	args.sg_erase = interpret(next);
	k++;
	return;
  }
  if (match(opt, "intr")) {
	tch.t_intrc = interpret(next);
	k++;
	return;
  }
  if (match(opt, "quit")) {
	tch.t_quitc = interpret(next);
	k++;
	return;
  }
  if (match(opt, "5")) {
	clr1;
	args.sg_flags |= BITS5;
	return;
  }
  if (match(opt, "6")) {
	clr1;
	args.sg_flags |= BITS6;
	return;
  }
  if (match(opt, "7")) {
	clr1;
	args.sg_flags |= BITS7;
	return;
  }
  if (match(opt, "8")) {
	clr1;
	args.sg_flags |= BITS8;
	return;
  }
  if (match(opt, "110")) {
	speed(B110);
	return;
  }
  if (match(opt, "200")) {
	speed(2);
	return;
  }
  if (match(opt, "300")) {
	speed(B300);
	return;
  }
  if (match(opt, "600")) {
	speed(6);
	return;
  }
  if (match(opt, "1200")) {
	speed(B1200);
	return;
  }
  if (match(opt, "1800")) {
	speed(18);
	return;
  }
  if (match(opt, "2400")) {
	speed(B2400);
	return;
  }
  if (match(opt, "3600")) {
	speed(36);
	return;
  }
  if (match(opt, "4800")) {
	speed(B4800);
	return;
  }
  if (match(opt, "7200")) {
	speed(72);
	return;
  }
  if (match(opt, "9600")) {
	speed(B9600);
	return;
  }
  if (match(opt, "19200")) {
	speed(192);
	return;
  }
  if (match(opt, "115200")) {
	speed(193);
	return;
  }
  if (match(opt, "57600")) {
	speed(194);
	return;
  }
  if (match(opt, "38400")) {
	speed(195);
	return;
  }
  if (match(opt, "28800")) {
	speed(196);
	return;
  }
  if (match(opt, "default")) {
	args.sg_flags = ECHO | CRMOD | XTABS | BITS8;
	args.sg_ispeed = B1200;
	args.sg_ospeed = B1200;
	args.sg_kill = '@';
	args.sg_erase = '\b';
	tch.t_intrc = DELC;
	tch.t_quitc = QUITC;
	tch.t_startc = STARTC;
	tch.t_stopc = STOPC;
	tch.t_eofc = EOFC;
	return;
  }
  std_err("unknown mode: ");
  std_err(opt);
  std_err("\n");

}

int match(s1, s2)
char *s1, *s2;
{

  while (1) {
	if (*s1 == 0 && *s2 == 0) return(1);
	if (*s1 == 0 || *s2 == 0) return (0);
	if (*s1 != *s2) return (0);
	s1++;
	s2++;
  }
}

int interpret(s)
char *s;
{
  if (s[0] == '^' && s[1] != 0)
	return(s[1] == '?' ? 0177 : s[1] & 037);
  else
	return(s[0]);
}

void prctl(c)
char c;
{
  if (c < ' ')
	printf("^%c", 'A' + c - 1);
  else if (c == 0177)
	printf("DEL");
  else
	printf("%c", c);
}
