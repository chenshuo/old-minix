/* cpdir - copy directory  	 	Author: Erik Baalbergen */

/* Use "cpdir [-v] src dst" to make a copy dst of directory src.
   Cpdir should behave like the UNIX shell command
	(cd src; tar cf - .) | (mkdir dst; cd dst; tar xf -)
   but the linking structure of the tree, and the owner and time
   information of files are not yet preserved. (See the work-yet-to-be-done list
   below.)
   The -v "verbose" flag enables you to see what's going on when running cpdir.

   Work yet to be done:
  - preserve link structure, times, etc...
  - 'stat' optimization (stat() is invoked twice for normal files)
  - link checks, i.e. am I not overwriting a file/directory by itself?
	* has been solved by letting 'cpdir' create the target directory
  - handle character and block special files
	* they're simply not copied

  Please report bugs and suggestions to erikb@cs.vu.nl
*/

#include "stdio.h"

#define MKDIR1 "/bin/mkdir"
#define MKDIR2 "/usr/bin/mkdir"
#ifdef UNIX
#include <sys/types.h>
#include <sys/stat.h>
#else !UNIX
#include "stat.h"
#endif

#define BUFSIZE 1024
#define PLEN	256
#define DIRSIZ	16

char *prog;
int vflag = 0;	/* verbose */
char *strcpy();

main(argc, argv)
	char *argv[];
{
	int rc = 0;
	char *p, *s;

	prog = *argv++;
	if ((p = *argv) && *p == '-') {
		argv++;
		argc--;
		while (*++p) {
			switch (*p) {
			case 'v':
				vflag = 1;
				break;
			default:
				fatal("illegal flag %s", p);
			}
		}
	}
	if (argc != 3)
		fatal("Usage: cpdir [-v] source destination");
	if (ftype(s = *argv++) != S_IFDIR)
		fatal("%s is not a directory", s);
	cpdir(s, *argv);
	exit(0);
}

cpdir(s, d)
	char *s, *d;
{
	char spath[PLEN], dpath[PLEN];
	char ent[DIRSIZ + 1];
	register char *send = spath, *dend = dpath;
	int fd, n;

	while (*send++ = *s++) {}
	send--;
	while (*dend++ = *d++) {}
	if ((fd = open(spath, 0)) < 0) {
		nonfatal("can't read directory %s", spath);
		return;
	}
	*send++ = '/';
	ent[DIRSIZ] = '\0';
	mkdir(dpath);
	dend[-1] = '/';
	while ((n = read(fd, ent, DIRSIZ)) == DIRSIZ) {
		if (!((ent[0] == '\0' && ent[1] == '\0')
		|| (ent[2] == '.') &&
			(ent[3] == '\0' || (ent[3] == '.' && ent[4] == '\0'))
		)) {
			strcpy(send, ent + 2);
			strcpy(dend, ent + 2);
			switch (ftype(spath)) {
			case S_IFDIR:
				cpdir(spath, dpath);
				break;
			case S_IFREG:
				cp(spath, dpath);
				break;
			default:
				nonfatal("can't copy special file %s", spath);
			}
		}
	}
	close(fd);
	if (n)
		fatal("error in reading directory %s", spath);
}

mkdir(s)
	char *s;
{
	int pid, status;

	if (vflag)
		printf("mkdir %s\n", s);
	if ((pid = fork()) == 0) {
		execl(MKDIR1, "mkdir", s, (char *)0);
		execl(MKDIR2, "mkdir", s, (char *)0);
		fatal("can't execute %s or %s", MKDIR1, MKDIR2);
	}
	if (pid == -1)
		fatal("can't fork", prog);
	wait(&status);
	if (status)
		fatal("can't create %s", s);
}

cp(s, d)
	char *s, *d;
{
	struct stat st;
	char buf[BUFSIZE];
	int sfd, dfd, n;

	if (vflag)
		printf("cp %s %s\n", s, d);
	if ((sfd = open(s, 0)) < 0)
		nonfatal("can't read %s", s);
	else {
		if (fstat(sfd, &st) < 0)
			fatal("can't get file status of %s", s);
		if ((dfd = creat(d, st.st_mode & 0777)) < 0)
			fatal("can't create %s", d);
		while ((n = read(sfd, buf, BUFSIZE)) > 0)
			write(dfd, buf, n);
		close(sfd);
		close(dfd);
		if (n)
			fatal("error in reading file %s", s);
	}
}

ftype(s)
	char *s;
{
	struct stat st;

	if (stat(s, &st) < 0)
		fatal("can't get file status of %s", s);
	return st.st_mode & S_IFMT;
}

nonfatal(s, a)
	char *s, *a;
{
	fprintf(stderr, "%s: ", prog);
	fprintf(stderr, s, a);
	fprintf(stderr, "\n");
}

fatal(s, a)
	char *s, *a;
{
	nonfatal(s, a);
	exit(1);
}


