/* ln - Link files			Author: V. Archer */

/* Copyright 1991 by Vincent Archer
 *	You may freely redistribute this software, in source or binary
 *	form, provided that you do not alter this copyright mention in any
 *	way.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <minix/minlib.h>
#include <stdio.h>


/* Forward declaration. */
_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void do_link, (char *file1, char *file2));
_PROTOTYPE(char *basename, (char *s));
_PROTOTYPE(void usage, (void));


/* Global variables needed. */
int sflag, cflag;
int fflag, dflag, error;
char name[PATH_MAX + 1];
struct stat st;


/* Main module. Only one option (-f) handled, so getopt() stuff not used.
 * The non-Posix & old-Minix compatible "ln x" construct is allowed as an
 * alias to "ln x ."
 */
int main(argc, argv)
int argc;
char **argv;
{
  char *ap;
  char *file;

  argc--;
  argv++;
  while (argc && argv[0][0] == '-') {
	argc--;
	ap = *argv++;
	while (*++ap) {
		switch (*ap) {
		    case 'f':	fflag = 1;	break;
#ifdef S_IFLNK
		    case 'c':
			cflag = 1;	/* Fall through to -s */
		    case 's':	sflag = 1;	break;
#endif
		    default:	usage();
		}
	}
  }

  switch (argc) {
      default:	file = argv[--argc];	break;
      case 1:
#ifndef S_IFLNK
	file = ".";
	break;
#else
	if (!cflag) {
		file = ".";
		break;
	}

	/* Fall through to error. */
#endif
      case 0:	usage();
}

  if (
#ifdef S_IFLNK
      !cflag &&
#endif
      !stat(file, &st) && S_ISDIR(st.st_mode))
	dflag = 1;
  else if (argc > 1)
	usage();

  while (argc--) do_link(*argv++, file);
  return(error);
}


/* Execute the linking between two files. Linking a directory will produce
 * a warning, but will be allowed if you are super user. This is not a
 * recommended practice, however...
 */
void do_link(file1, file2)
char *file1, *file2;
{
  if (!sflag) {
	if (stat(file1, &st)) {
		perror(file1);
		error = 1;
		return;
	}
	if (S_ISDIR(st.st_mode)) {
		errno = EISDIR;
		if (getuid() == 0) std_err("warning: ");
		perror(file1);
		if (getuid() != 0) {
			error = 1;
			return;
		}
	}
  }
#ifdef S_IFLNK
  /* Check to see if conditional symlink required. */
  if (cflag) {
	strcpy(name, " ");
	strcat(name, file1);
	file1 = name;
  }
#endif

  /* Check to see if target is a directory. */
  if (dflag) {
	strcpy(name, file2);
	strcat(name, "/");
	strcat(name, basename(file1));
	file2 = name;
  }
  if (
#ifdef S_IFLNK
      !lstat(file2, &st)
#else
      !stat(file2, &st)
#endif
	) {
	if (!fflag) {
		errno = EEXIST;
		perror(file2);
		error = 1;
		return;
	}
	if (unlink(file2)) {
		perror(file2);
		error = 1;
		return;
	}
  }
  if (
#ifdef S_IFLNK
      sflag && symlink(file1, file2) || !sflag && link(file1, file2)
#else
      link(file1, file2)
#endif
	) {
	perror(file2);
	error = 1;
  }
}


char *basename(s)
char *s;
{
/* Return pointer to last component of string. */
  char *b;

  if (b = strrchr(s, '/')) return(b + 1);
  return(s);
}


/* (Extended) Posix command prototype. */
void usage()
{
#ifdef S_IFLNK
  std_err("Usage: ln [-fs] file... [target]\n");
  std_err("       ln [-fc] conditional-link target\n");
#else
  std_err("Usage: ln [-f] file... [target]\n");
#endif
  exit(1);
}
