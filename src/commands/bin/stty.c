/* stty - set terminal mode  	Author: Andy Tanenbaum */

#include <sgtty.h>
char *on[] = {"tabs",  "cbreak",  "raw",  "-nl",  "echo"};
char *off[]= {"-tabs", "", "", "nl", "-echo"};
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

main(argc, argv)
int argc;
char *argv[];
{

  /* stty with no arguments just reports on current status. */
  ioctl(FD, TIOCGETP, &args);
  ioctl(FD, TIOCGETC, &tch);
  if (argc == 1) {
	report();
	exit(0);
  }

  /* Process the options specified. */
  k = 1;
  while (k < argc) {
	option(argv[k], k+1 < argc ? argv[k+1] : "");
	k++;
  }
  ioctl(FD, TIOCSETP, &args);
  ioctl(FD, TIOCSETC, &tch);
  exit(0);
}



report()
{
  int mode, ispeed, ospeed;

  mode = args.sg_flags;
  pr(mode&XTABS, 0);
  pr(mode&CBREAK, 1);
  pr(mode&RAW, 2);
  pr(mode&CRMOD,3);
  pr(mode&ECHO,4);

  ispeed = 100 * ((int) args.sg_ispeed & BYTE);
  ospeed = 100 * ((int) args.sg_ospeed & BYTE);
  prints("\nkill = "); 	prctl(args.sg_kill);
  prints("\nerase = ");	prctl(args.sg_erase);
  prints("\nint = "); 	prctl(tch.t_intrc);
  prints("\nquit = "); 	prctl(tch.t_quitc);
  if (ispeed > 0) {
	/* Print # bits/char and parity */
	if ( (mode & BITS8) == BITS8) prints("\n8 bits/char,  ");
	if ( (mode & BITS8) == BITS7) prints("\n7 bits/char,  ");
	if ( (mode & BITS8) == BITS6) prints("\n6 bits/char,  ");
	if ( (mode & BITS8) == BITS5) prints("\n5 bits/char,  ");
	if (mode & EVENP) prints("even parity");
	else if (mode & ODDP) prints("odd parity");
	else prints("no parity");

	/* Print line speed. */
	prints("\nspeed = ");
	switch(ispeed) {
		case  100: prints("110");	break;
		case  300: prints("300");	break;
		case 1200: prints("1200");	break;
		case 2400: prints("2400");	break;
		case 4800: prints("4800");	break;
		case 9600: prints("9600");	break;
		default:   prints("unknown");
	}
  }
  prints("\n");
}

pr(f, n)
int f,n;
{
  if (f)
	prints("%s ",on[n]);
  else
	prints("%s ",off[n]);
}

option(opt, next)
char *opt, *next;
{
  if (match(opt, "-tabs"))	{args.sg_flags &= ~XTABS; return;}
  if (match(opt, "-odd"))	{args.sg_flags &= ~ODDP; return;}
  if (match(opt, "-even"))	{args.sg_flags &= ~EVENP; return;}
  if (match(opt, "-raw"))	{args.sg_flags &= ~RAW; return;}
  if (match(opt, "-cbreak"))	{args.sg_flags &= ~CBREAK; return;}
  if (match(opt, "-echo"))	{args.sg_flags &= ~ECHO; return;}
  if (match(opt, "-nl"))	{args.sg_flags |= CRMOD; return;}

  if (match(opt, "tabs"))	{args.sg_flags |= XTABS; return;}
  if (match(opt, "even"))	{clr2; args.sg_flags |= EVENP; return;}
  if (match(opt, "odd"))	{clr2; args.sg_flags |= ODDP; return;}
  if (match(opt, "raw"))	{args.sg_flags |= RAW; return;}
  if (match(opt, "cbreak"))	{args.sg_flags |= CBREAK; return;}
  if (match(opt, "echo"))	{args.sg_flags |= ECHO; return;}
  if (match(opt, "nl"))		{args.sg_flags &= ~CRMOD; return;}

  if (match(opt, "kill"))	{args.sg_kill = *next; k++; return;}
  if (match(opt, "erase"))	{args.sg_erase = *next; k++; return;}
  if (match(opt, "int"))	{tch.t_intrc = *next; k++; return;}
  if (match(opt, "quit"))	{tch.t_quitc = *next; k++; return;}

  if (match(opt, "5"))		{clr1; args.sg_flags |= BITS5; return;}
  if (match(opt, "6"))		{clr1; args.sg_flags |= BITS6; return;}
  if (match(opt, "7"))		{clr1; args.sg_flags |= BITS7; return;}
  if (match(opt, "8"))		{clr1; args.sg_flags |= BITS8; return;}

  if (match(opt, "110"))	{speed(B110); return;}
  if (match(opt, "300"))	{speed(B300); return;}
  if (match(opt, "1200"))	{speed(B1200); return;}
  if (match(opt, "2400"))	{speed(B2400); return;}
  if (match(opt, "4800"))	{speed(B4800); return;}
  if (match(opt, "9600"))	{speed(B9600); return;}

  if (match(opt, "default"))	{
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
	if (*s1 == 0 || *s2 == 0) return(0);
	if (*s1 != *s2) return(0);
	s1++;
	s2++;
  }
}

prctl(c)
char c;
{
  if (c < ' ')
	prints("^%c", 'A' + c - 1);
  else if (c == 0177)
	prints("DEL");
  else
	prints("%c", c);
}
