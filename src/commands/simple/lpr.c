/* lpr - line printer front end		Author: Andy Tanenbaum */

#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <minix/minlib.h>

#define BLOCK 1024

char in_buf[BLOCK], out_buf[BLOCK];
int cur_in, in_count, out_count, column;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void copy, (int fd));
_PROTOTYPE(void myputchar, (int c));
_PROTOTYPE(void flush, (void));

int main(argc, argv)
int argc;
char *argv[];
{
/* This program copies files to the line printer.  It expands tabs and converts
 * line feeds to carriage returns + line feeds.
 */

  int i, fd;

  close(1);
  if (open("/dev/lp", O_WRONLY) < 0) {
	std_err("lpr: can't open /dev/lp\n");
	exit(1);
  }
  if (argc == 1) {
	copy(0);		/* standard input only */
  } else {
	for (i = 1; i < argc; i++) {
		if ((fd = open(argv[i], O_RDONLY)) < 0) {
			std_err("lpr: can't open ");
			std_err(argv[1]);
			std_err("\n");
			exit(1);
		} else {
			copy(fd);
			close(fd);
			cur_in = 0;
			in_count = 0;
		}
	}
  }
  return(0);
}


void copy(fd)
int fd;
{
/* Print a file, adding carriage returns and expanding tabs. */

  char c;

  while (1) {
	if (cur_in == in_count) {
		in_count = read(fd, in_buf, BLOCK);
		if (in_count == 0) {
			flush();
			return;
		}
		cur_in = 0;
	}
	c = in_buf[cur_in++];
	if (c == '\n') {
		myputchar('\r');
		myputchar('\n');
	} else if (c == '\t') {
		do {
			myputchar(' ');
		} while (column & 07);
	} else
		myputchar(c);
  }
}

void myputchar(c)
char c;
{
  out_buf[out_count++] = c;
  if (c == '\n')
	column = 0;
  else
	column++;
  if (out_count == BLOCK) {
	flush();
  }
}

void flush()
{
  int n, count = 0;

  if (out_count == 0) return;
  while (1) {
	n = write(1, out_buf, out_count);
	if (n == out_count) break;
	if (n != EAGAIN) {
		std_err("Printer error\n");
		exit(1);
	}
	if (count > 5) {
		std_err("Printer keeps returning busy status\n");
		exit(1);
	}
	count++;
	sleep(1);
  }
  out_count = 0;
}
