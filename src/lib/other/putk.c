#include <lib.h>
#include <unistd.h>

#define BUF_SIZE 1024
#define FD          1

PRIVATE char outbuf[BUF_SIZE];
PRIVATE char *bufp = outbuf;
_PROTOTYPE( void putk, (int n));

void putk(n)
int n;
{
/* Print one char on stdout. */

  if (n == 0) {
	/* putc(0) means flush the buffer. */
	if (bufp > outbuf) write(FD, outbuf, (int)(bufp - outbuf));
	bufp = outbuf;
	return;
  }

  *bufp++ = (char) n;
  if (bufp == &outbuf[BUF_SIZE]) {
	/* The buffer is full.  Flush it. */
	write(FD, outbuf, BUF_SIZE);
	bufp = outbuf;
  }
}
