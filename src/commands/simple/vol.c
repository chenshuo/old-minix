/* vol - break stdin into volumes	Author: Andy Tanenbaum */

/* This program reads standard input and writes it onto diskettes, pausing
 * at the start of each one.  It's main use is for saving files that are
 * larger than a single diskette.  Vol just writes its standard input onto
 * a diskette, and prompts for a new one when it is full.  This mechanism
 * is transparent to the process producing vol's standard input. For example,
 *	tar c - . | vol -w 360 /dev/fd0
 * puts the tar output as as many diskettes as needed.  To read them back in,
 * use
 *	vol -r 360 /dev/fd0 | tar x -
 *
 * Changed 17 Nov 1993 by Kees J. Bot to handle buffering to slow devices.
 * Changed 27 Jul 1994 by Kees J. Bot to auto discover data direction + -rw.
 */

#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

#define CHUNK_SIZE	8192
#define NCHUNKS_BLK	1
#if __minix_vmd
#define NCHUNKS_CHR	128
#else
#define NCHUNKS_CHR	(sizeof(size_t) == 2 ? 3 : 6)
#endif

char *buffer;
size_t buffer_size;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void usage, (void));
_PROTOTYPE(void diskio, (long size, int fd1, int fd2, char *errstr1,
			 char *errstr2));

int rflag, wflag;
int raw;

int main(argc, argv)
int argc;
char *argv[];
{
  int volume = 1, fd, tty, i;
  long size;
  char *p, *name;
  struct stat stb;

  /* Fetch and verify the arguments. */
  i = 1;
  while (i < argc && argv[i][0] == '-') {
	p = argv[i++];
	if (p[1] == '-' && p[2] == 0) {
		/* -- */
		i++;
		break;
	}
	while (*++p != '\0') switch (*p) {
		case 'r':
		case 'u':	rflag = 1; break;
		case 'w':	wflag = 1; break;
		default:	usage();
	}
  }
  size = LONG_MAX;
  if (i < argc - 1) {
	size = atol(argv[i]);
	if (size <= 0 || size > LONG_MAX / 1024) {
		fprintf(stderr, "vol: %s: bad volume size\n", argv[i]);
		exit(1);
	}
	size *= 1024;
	i++;
  }
  if (i != argc - 1) usage();
  name = argv[i];

  if (!rflag && !wflag) {
	/* Auto direction.  If there is a terminal at one side then data is
	 * to go out at the other side.
	 */
	if (isatty(0)) rflag = 1;
	if (isatty(1)) wflag = 1;
  }

  if (rflag == wflag) {
	fprintf(stderr, "vol: should %s be read or written?\n", name);
	usage();
  }

  if (stat(name, &stb) < 0) {
	fprintf(stderr, "vol: %s: %s\n", name, strerror(errno));
	exit(1);
  }
  if (!S_ISBLK(stb.st_mode) && !S_ISCHR(stb.st_mode)) {
	fprintf(stderr, "vol: %s is not a device\n", name);
	exit(1);
  }
  raw = S_ISCHR(stb.st_mode);

  tty = open("/dev/tty", O_RDONLY);
  if (tty < 0) {
	fprintf(stderr, "vol: cannot open /dev/tty\n");
	exit(1);
  }

  /* Allocate buffer space, more for a raw device. */
  buffer_size = (raw ? NCHUNKS_CHR : NCHUNKS_BLK) * CHUNK_SIZE;
  buffer = (char *) malloc(buffer_size);
  if (buffer == NULL) {
	fprintf(stderr, "vol: cannot allocate a %luk buffer\n",
		(unsigned long) buffer_size / 1024);
	exit(1);
  }

  while (1) {
	sleep(1);
	fprintf(stderr, "\007Please insert %sput volume %d and hit return\n",
		rflag ? "in" : "out", volume);
	read(tty, buffer, buffer_size);
	volume++;

	/* Open the special file. */
	fd = open(name, rflag ? O_RDONLY : O_WRONLY);
	if (fd < 0) {
		fprintf(stderr, "vol: %s: %s\n", name, strerror(errno));
		exit(1);
	}

	/* Read or write the requisite number of blocks. */
	if (rflag)
		diskio(size, fd, 1, name, "stdout");	/* vol -u | tar xf - */
	else
		diskio(size, 0, fd, "stdin", name);	/* tar cf - | vol */

	close(fd);
  }
}

void usage()
{
  fprintf(stderr, "Usage: vol [-rw] [size] block-special\n", "");
  exit(1);
}

void diskio(size, fd1, fd2, errstr1, errstr2)
long size;
int fd1, fd2;
char *errstr1, *errstr2;
{
/* Read 'size' bytes from 'fd1' and write them on 'fd2'.  Watch out for
 * the fact that reads on pipes can return less than the desired data.
 */

  ssize_t n, in_needed, in_count, out_count;
  long needed = size;
  int eof = 0;

  while (needed > 0) {
	in_count = 0;
	in_needed = needed > buffer_size ? buffer_size : needed;
	while (in_count < in_needed) {
		n = in_needed - in_count;
		if (n > CHUNK_SIZE) n = CHUNK_SIZE;
		n = eof ? 0 : read(fd1, buffer + in_count, n);
		if (n == 0) {
			eof = 1;
			if (raw && (n = in_count % CHUNK_SIZE) > 0) {
				n = CHUNK_SIZE - n;
				memset(buffer + in_count, '\0', n);
				if ((in_count += n) > in_needed)
					in_count = in_needed;
			}
			break;
		}
		if (n < 0) {
			fprintf(stderr, "vol: %s: %s\n",
						errstr1, strerror(errno));
			exit(1);
		}
		in_count += n;
	}
	if (in_count == 0) exit(0);	/* EOF */
	out_count = 0;
	while (out_count < in_count) {
		n = in_count - out_count;
		if (n > CHUNK_SIZE) n = CHUNK_SIZE;
		n = write(fd2, buffer + out_count, n);
		if (n < 0) {
			fprintf(stderr, "vol: %s: %s\n",
						errstr2, strerror(errno));
			exit(1);
		}
		out_count += n;
	}
	needed -= in_count;
  }
}
