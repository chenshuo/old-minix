/*  uname - get system name			Author: Earl Chew */

/* The system name is read from the file /etc/uname. This should be
 * world readable but only writeable by system administrators. The
 * file contains lines of text. Lines beginning with # are treated
 * as comments. All other lines contain information to be read into
 * the uname structure. The sequence of the lines matches the sequence
 * in which the structure components are declared:
 *
 *	system name		Minix
 *	node name		waddles
 *	release name		1.5
 *	version			10
 *	machine name		IBM_PC
 *	serial number		N/A
 */

#include <lib.h>
#include <fcntl.h>
#include <string.h>
#define uname _uname
#include <unistd.h>
#include <sys/utsname.h>


#ifndef 	UFILENAME
#define UFILENAME	"/etc/uname"
#endif

struct buffer {
  char buf[1024];
  char *bp;
  char *ep;
};

_PROTOTYPE(int __upart, (int fd, struct buffer *b, char *s, size_t n));


int __upart(fd, b, s, n)
int fd;
struct buffer *b;
char *s;
size_t n;
{
  register int c;
  register char *p;
  char *e;

  e = &s[n];
  do {
	p = s;
	do {
		if (b->bp >= b->ep) {
			if ((c = read(fd, b->buf, sizeof b->buf)) <= 0) {
				if (p == s)
					return -1;
				else {
					b->buf[0] = '\n';
					c = 1;
				}
			}
			b->bp = b->buf;
			b->ep = b->buf + c;
		}
		c = *b->bp++;
		if (p < e) *p++ = c;
	} while (c != '\n');
	p[-1] = 0;
  } while (s[0] == '#');
  return 0;
}

int uname(up)
struct utsname *up;
{
  static int ufd = -1;
  int r;
  struct buffer buf;

  if (ufd != -1) {
	errno = EBUSY;
	return -1;
  }
  if ((ufd = open(UFILENAME, O_RDONLY)) < 0) {
	errno = EACCES;
	return -1;
  }
  r = 0;
  buf.bp = buf.ep = buf.buf;
  if (__upart(ufd, &buf, up->sysname,  sizeof(up->sysname))  ||
      __upart(ufd, &buf, up->nodename, sizeof(up->nodename)) ||
      __upart(ufd, &buf, up->release,  sizeof(up->release))  ||
      __upart(ufd, &buf, up->version,  sizeof(up->version))  ||
      __upart(ufd, &buf, up->machine,  sizeof(up->machine))  ||
      __upart(ufd, &buf, up->idnumber, sizeof(up->idnumber))) {
	errno = EIO;
	r = -1;
  }
  (void) close(ufd);
  ufd = -1;
  return r;
}
