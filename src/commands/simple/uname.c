/*  uname - print system name			Author: Earl Chew */

/* The system name is printed based on the file /etc/uname. This should be
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

#include <sys/types.h>
#include <sys/utsname.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Define the uname components. */
#define ALL	 ((unsigned) ~0x0)
#define SYSNAME  ((unsigned) 0x01)
#define NODENAME ((unsigned) 0x02)
#define RELEASE  ((unsigned) 0x04)
#define VERSION  ((unsigned) 0x08)
#define MACHINE  ((unsigned) 0x10)

_PROTOTYPE(int main, (int argc, char **argv ));
_PROTOTYPE(void print, (int fd, ... ));
_PROTOTYPE(void usage, (void ));

#ifdef __STDC__
void print(int fd, ...)
#else
void print(fd)
int fd;
#endif
{
/* Print a sequence of strings onto the named channel. */
  va_list argp;
  char *p;

  va_start(argp, fd);
  while (1) {
	p = va_arg(argp, char *);
	if (p == (char *) NULL) break;
	write(fd, p, strlen(p));
  }
  va_end(argp);
}

void usage()
{
  print(STDERR_FILENO, "Usage: uname -amnrsv\n", (char *) NULL);
  exit(EXIT_FAILURE);
}

int main(argc, argv)
int argc;
char **argv;
{
  int info;
  char *p;
  struct utsname un;

  for (info = 0; argc > 1; argc--, argv++) {
  	if (argv[1][0] == '-') {
  		for (p = &argv[1][1]; *p; p++) {
  			switch (*p) {
				case 'a': info |= ALL;      break;
				case 'm': info |= MACHINE;  break;
				case 'n': info |= NODENAME; break;
				case 'r': info |= RELEASE;  break;
				case 's': info |= SYSNAME;  break;
				case 'v': info |= VERSION;  break;
				default: usage();
  			}
		}
	} else {
		usage();
	}
  }

  if (uname(&un) != 0) {
	print(STDERR_FILENO, "unable to determine uname values\n", (char *) NULL);
	exit(EXIT_FAILURE);
  }

  if (info == 0 || (info & SYSNAME) != 0)
	print(STDOUT_FILENO, un.sysname, (char *) NULL);
  if ((info & NODENAME) != 0) {
	if ((info & (SYSNAME)) != 0)
		print(STDOUT_FILENO, " ", (char *) NULL);
	print(STDOUT_FILENO, un.nodename, (char *) NULL);
  }
  if ((info & RELEASE) != 0) {
	if ((info & (SYSNAME|NODENAME)) != 0)
		print(STDOUT_FILENO, " ", (char *) NULL);
	print(STDOUT_FILENO, un.release, (char *) NULL);
  }
  if ((info & VERSION) != 0) {
	if ((info & (SYSNAME|NODENAME|RELEASE)) != 0)
		print(STDOUT_FILENO, " ", (char *) NULL);
	print(STDOUT_FILENO, un.version, (char *) NULL);
  }
  if ((info & MACHINE) != 0) {
	if ((info & (SYSNAME|NODENAME|RELEASE|VERSION)) != 0)
		print(STDOUT_FILENO, " ", (char *) NULL);
	print(STDOUT_FILENO, un.machine, (char *) NULL);
  }
  print(STDOUT_FILENO, "\n", (char *) NULL);
  return EXIT_SUCCESS;
}
