/* more - terminal pager		Author: Brandon S. Allbery  */

/* Pager commands:
 *	<space>	 display next page
 *	<return> scroll up 1 line
 *	q	 quit
*/

#define reverse()	write(1, "\033z\160", 3)	/* reverse video */
#define normal()	write(1, "\033z\7", 3)		/* undo reverse() */
#define clearln()	write(1, "\r\033~0", 4)		/* clear line */

#define LINES		23	/* lines/screen (- 1 to retain last line) */
#define COLS		80	/* columns/line */
#define TABSTOP		8	/* tabstop expansion */

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
	char ch;
	int fd, arg;

	signal(SIGINT, byebye);
	fd = 0;
	cbreak();
	if (argc < 2)
		while ((ch = input(fd)) != 0)
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
			while ((ch = input(fd)) != 0)
				output(ch);
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
					line = LINES - 1;
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
		return 0;
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
			return 0;
	}
	return ibuf[ibc++];
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
			if (++line == LINES) {
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
			if (++col == COLS) {
				col = 0;
				if (++line == LINES) {
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
			line = LINES - 1;
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
