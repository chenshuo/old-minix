/* mknod - build a special file		Author: Andy Tanenbaum */

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <minix/minlib.h>
#include <errno.h>
#include <stdio.h>

#ifdef _MINIX
#define MKNOD(p,M,m,s) mknod4(p,M,m,s)
#else
#define MKNOD(p,M,m,s) mknod(p,M,m)
#endif

_PROTOTYPE(int main, (int argc, char *argv []));
_PROTOTYPE(void badcomm, (void));
_PROTOTYPE(void badfifo, (void));
_PROTOTYPE(void badchar, (void));
_PROTOTYPE(void badblock, (void));

int main(argc, argv)
int argc;
char *argv[];
{
/* Mknod name b/c major minor [size] makes a node. */

  int mode, major, minor, dev, e;
  long size;

  if (argc < 3) badcomm();
  if (*argv[2] != 'b' && *argv[2] != 'c' && *argv[2] != 'p') badcomm();
  if (*argv[2] == 'p' && argc != 3) badfifo();
  if (*argv[2] == 'c' && argc != 5) badchar();
  if (*argv[2] == 'b' && argc != 6) badblock();
  if (*argv[2] == 'p') {
	mode = 010666;
	dev = 0;
	size = 0;
  } else {
	mode = (*argv[2] == 'b' ? 060666 : 020666);
	major = atoi(argv[3]);
	minor = atoi(argv[4]);
	size = (*argv[2] == 'b' ? atol(argv[5]) : 0);
	if (major - 1 > 0xFE || minor > 0xFF) badcomm();
	dev = (major << 8) | minor;
  }
  e = MKNOD(argv[1], mode, dev, size);
  if (e < 0 && *argv[2] != 'p' && errno == EPERM)
	std_err("mknod: Inode not made. Only the superuser can make inodes\n");
  else if (e < 0)
	perror("mknod");
  return(0);
}

void badcomm()
{
  std_err("Usage: mknod name b/c/p [major minor [size_in_blocks]]\n");
  exit(1);
}

void badfifo()
{
  std_err("Usage: mknod name p\n");
  exit(1);
}

void badchar()
{
  std_err("Usage: mknod name c major minor\n");
  exit(1);
}

void badblock()
{
  std_err("Usage: mknod name b major minor size_in_blocks\n");
  exit(1);
}
