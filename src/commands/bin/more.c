/* more - terminal pager		Author: Brandon S. Allbery  */

/*  Temporary fixes:  efth   1988-Aug-16
     -  don't display directories and special files
     -  don't die on files with '\0' in them
     -  don't print non-ASCII characters
     -  use termcap for #lines, normal, reverse, clear-line
 */


/* Pager commands:
 *	<space>	 display next page
 *	<return> scroll up 1 line
 *	q	 quit
*/

char *SO, *SE, *CD;

#define reverse()	write(1, SO, strlen(SO))	/* reverse video */
#define normal()	write(1, SE, strlen(SE))	/* undo reverse() */
#define clearln()	write(1,"\r",1); \
			write(1, CD, strlen(CD))	/* clear line */

int  lines;			/* lines/screen (- 2 to retain last line) */

#define COLS		80	/* columns/line */
#define TABSTOP		8	/* tabstop expansion */

#include <sys/types.h>
#include <sys/stat.h>
#include <sgtty.h>
#include <signal.h>

extern int byebye();
extern char *index();

int line = 0;			/* current terminal line */
int col = 0;			/* current terminal column */
int fd = -1;			/* terminal file descriptor (/dev/tty) */
struct sgttyb ttymode;		/* and the terminal modes */
char ibuf[1024];		/* input buffer */
char obuf[1024];		/* output buffer */
int ibl = 0;			/* chars in input buffer */
int ibc = 0;			/* position in input buffer */
int obc = 0;			/* position in output buffer (== chars in) */
int isrewind = 0;		/* flag: ' command -- next input() rewind */
int isdone = 0;			/* flag: return EOF next read even if not */

main(argc, argv)
char **argv; {
	int ch;
	int fd, arg;
	struct stat s;

	get_termcap();

	signal(SIGINT, byebye);
	fd = 0;
	cbreak();
	if (argc < 2)
		while ((ch = input(fd)) != -1)
			output(ch);
	else
		for (arg = 1; argv[arg] != (char *) 0; arg++) {
			if ((fd = open(argv[arg], 0)) == -1) {
				write(1, "more: cannot open ", 18);
				write(1, argv[arg], strlen(argv[arg]));
				write(1, "\n", 1);
				nocbreak();
				exit(1);
			}

			if ( fstat(fd,&s) < 0 ) {
				write( 1, "more: can not fstat file\n", 25 );
				nocbreak();
				exit(1);
			}

			if ( (s.st_mode & S_IFMT) == S_IFREG )
				while ((ch = input(fd)) != -1)
					output(ch);
			else
				write(1, "not file\n", 9 );

			close(fd);
			if (argv[arg + 1] != (char *) 0) {
				oflush();
				if (isdone) {	/* 'n' command */
					reverse();
					write(1, "*** Skipping to next file ***\n", 30);
					normal();
					isdone = 0;
				}
				reverse();
				write(1, "--More-- (Next file: ", 21);
				write(1, argv[arg + 1], strlen(argv[arg + 1]));
				write(1, ")", 1);
				normal();
				switch (wtch()) {
				case ' ':
				case '\'':
				case 'n':
				case 'N':
					line = 0;
					break;
				case '\r':
				case '\n':
					line = lines - 1;
					break;
				case 'q':
				case 'Q':
					clearln();
					byebye();
				}
				clearln();
			}
		}
	oflush();
	byebye();
}

input(fd) {
	if (isdone) {
		ibl = 0;
		ibc = 0;
		return -1;
	}
	if (isrewind) {
		lseek(fd, 0L, 0);
		ibl = 0;
		ibc = 0;
		isrewind = 0;
	}
	if (ibc == ibl) {
		ibc = 0;
		if ((ibl = read(fd, ibuf, sizeof ibuf)) <= 0)
			return -1;
	}
	return ibuf[ibc++] & 0xff;
}

output(c)
char c; {
	if (obc == sizeof obuf) {
		lwrite(1, obuf, sizeof obuf);
		obc = 0;
	}
	if (!isrewind)
		obuf[obc++] = c;
}

oflush() {
	if (!isdone)
		lwrite(1, obuf, obc);
	obc = 0;
}

lwrite(fd, buf, len)
char *buf;
unsigned len; {
	unsigned here, start;
	char cmd;

	start = 0;
	here = 0;
	while (here != len) {
		cmd = '\0';
		switch (buf[here++]) {
		case '\n':
			col = 0;
			if (++line == lines) {
				write(fd, buf + start, here - start);
				reverse();
				write(1, "--More--", 8);
				normal();
				cmd = wtch();
				clearln();
				line = 0;
				start = here;
			}
			break;
		case '\r':
			col = 0;
			break;
		case '\b':
			if (col != 0)
				col--;
			else {
				line--;
				col = COLS - 1;
			}
			break;
		case '\t':
			do {
				col++;
			} while (col % TABSTOP != 0);
			break;
		default:
			if ( buf[here-1] < ' ' || (buf[here-1] & 0x80) )
			   buf[here-1] = '?';
			if (++col == COLS) {
				col = 0;
				if (++line == lines) {
					write(fd, buf + start, here - start);
					reverse();
					write(1, "--More--", 8);
					normal();
					cmd = wtch();
					clearln();
					line = 0;
					start = here;
				}
			}
		}
		switch (cmd) {
		case '\0':
			break;
		case ' ':
			line = 0;
			break;
		case '\r':
		case '\n':
			line = lines - 1;
			break;
		case 'q':
		case 'Q':
			byebye();
		case '\'':
			isrewind = 1;
			reverse();
			write(1, "*** Back ***\n", 13);
			normal();
			return;
		case 'n':
		case 'N':
			isdone = 1;
			return;
		default:
			break;
		}
	}
	if (here != start)
		write(fd, buf + start, here - start);
}

wtch() {
	char ch;

	do {
		read(fd, &ch, 1);
	} while (index(" \r\nqQ'nN", ch) == (char *) 0);
	return ch;
}

cbreak() {
	if (fd != -1)
		return;
	if ((fd = open("/dev/tty", 0)) == -1) {
		write(2, "OOPS -- can't open /dev/tty\n", 28);
		exit(1);
	}
	ioctl(fd, TIOCGETP, &ttymode);
	ttymode.sg_flags |= CBREAK;
	ttymode.sg_flags &= ~ECHO;
	ioctl(fd, TIOCSETP, &ttymode);	/* NB: add TIOCSETN! */
}

nocbreak() {
	if (fd == -1)
		return;
	ttymode.sg_flags &= ~CBREAK;
	ttymode.sg_flags |= ECHO;
	ioctl(fd, TIOCSETP, &ttymode);
	close(fd);
	fd = -1;
}

byebye() {
	nocbreak();
	exit(0);
}



get_termcap()
  {
  static char termbuf[50];
  extern char *tgetstr(), *getenv();
  char *loc = termbuf;
  char entry[1024];

  if (tgetent(entry, getenv("TERM")) <= 0) {
  	printf("Unknown terminal.\n");
  	exit(1);
  }

  lines = tgetnum("li" ) - 2;

  SO = tgetstr("so", &loc);
  SE = tgetstr("se", &loc);
  CD = tgetstr("cd", &loc);

  if ( CD == (char *) 0 )
    CD = "             \r";
  }
