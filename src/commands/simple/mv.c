/* mv - move files			Author: V. Archer */

/* Copyright 1991 by Vincent Archer
 *	You may freely redistribute this software, in source or binary
 *	form, provided that you do not alter this copyright mention in any
 *	way.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <minix/minlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

/* Global variables. */
int dflag, fflag, iflag, tflag;
struct stat st, st2;

extern int optind, opterr;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(int do_it, (char *cmd, char *options, char *file, char *target));
_PROTOTYPE(int negative, (void));
_PROTOTYPE(char *octal, (Mode_t num));
_PROTOTYPE(int do_mv, (char *source, char *dest));
_PROTOTYPE(void usage, (void));

/* Main module. The internal '-t' flag is set if the file descriptor 0
 * refers to a tty device. It is not used if '-i' is set.
 */
int main(argc, argv)
int argc;
char *argv[];
{
  char *dest;
  int c;

  opterr = 0;
  while ((c = getopt(argc, argv, "fi")) != EOF) switch (c) {
	    case 'f':
		fflag = 1;
		iflag = 0;
		break;
	    case 'i':
		iflag = 1;
		fflag = 0;
		break;
	    default:	usage();	break;
	}
  argc -= optind;
  argv += optind;
  if (argc < 2) usage();

  dest = argv[--argc];
  if (stat(dest, &st)) {
	if (argc > 1) {
		perror(dest);
		return(1);
	}
  } else if (!S_ISDIR(st.st_mode)) {
	if (argc > 1) {
		errno = ENOTDIR;
		perror(dest);
		return(1);
	}
  } else
	dflag = 1;
  if (!fflag) tflag = isatty(0);

  c = 0;
  while (argc--) c |= do_mv(*argv++, dest);
  return(c);
}


/* This replaces a bulky, slow, and unnecessary system() call in the
 * specific cases needed by a mv across file systems (rm/cpdir calls).
 */
int do_it(cmd, options, file, target)
char *cmd, *options, *file, *target;
{
  pid_t pid, pid2;
  int status;

  if ((pid = fork()) != 0) {
	if (pid < 0) {
		std_err(file);
		perror(":fork()");
		return(1);
	}
	while ((pid2 = wait(&status)) != pid)
		if (pid2 <= 0 && errno != EAGAIN) {
			std_err(file);
			perror(":wait()");
			return(1);
		}
	if (WIFEXITED(status) && !WEXITSTATUS(status)) return(0);
	return(1);
  }
  execlp(cmd, cmd, options, file, target, (char *) 0);

  /* Control only passes here in the event execlp failed. */
  std_err(file);
  std_err(":execlp():");
  perror(cmd);
  exit(1);
  return(0);
}


/* Wait for a user answer from the stdin stream (but do not use stdio which
 * is bulky and unneeded in most tools). An error (or end of file) on file
 * descriptor 0 is assumed to mean a NEGATIVE answer. LC_* locale could be
 * handled here.
 */
int negative()
{
  char c, t;

  if (read(0, &c, 1) != 1) return(1);
  t = c;
  while (t != '\n')
	if (read(0, &t, 1) != 1) break;
  return(c != 'y' && c != 'Y');
}


/* Quick transformation of a mode_t in 3-digits octal form. */
char *octal(num)
mode_t num;
{
  static char a[4];

  a[0] = (((num >> 6) & 7) + '0');
  a[1] = (((num >> 3) & 7) + '0');
  a[2] = ((num & 7) + '0');
  a[3] = 0;
  return(a);
}


/* Do the actual directory entry movement. This rely upon the rename()
 * system call heavily.
 */
int do_mv(source, dest)
char *source, *dest;
{
  int newfile;
  char path[PATH_MAX + 1];

  if (dflag) {
	strcpy(path, dest);
	if ((dest = strrchr(source, '/')) != NULL)
		strcat(path, dest);
	else {
		strcat(path, "/");
		strcat(path, source);
	}
	dest = path;
  }
  if (access(source, 0) < 0) {
	perror(source);
	return(1);
  }
  if (
#ifdef S_IFLNK
      !(newfile = lstat(dest, &st))
#else
      !(newfile = stat(dest, &st))
#endif
     ) {
	if (S_ISDIR(st.st_mode)) {
		errno = EISDIR;
		perror(dest);
		return(1);
	}
	if (!fflag)
		if (
#ifdef S_IFLNK
		    !S_ISLNK(st.st_mode) &&
#endif
		    access(dest, 2) && tflag) {
			std_err(dest);
			std_err(": replace (mode = ");
			std_err(octal(st.st_mode));
			std_err(") ? ");
			if (negative()) return(0);
		} else if (iflag) {
			std_err(dest);
			std_err(": replace ? ");
			if (negative()) return(0);
		}
  }
  if (rename(source, dest) == 0) return(0);
  if (errno != EXDEV) {
	std_err(source);
	std_err(", ");
	perror(dest);
	return(1);
  }

  /* A copy is necessary. Make sure that the source and destination have
   * the same "type" (i.e. they're both directories or non-directories).
   */
  if (stat(source, &st2)) {
	perror(source);
	return(1);
  }
  if (!newfile) {
	if (S_ISDIR(st2.st_mode) && !S_ISDIR(st.st_mode)) {
		errno = ENOTDIR;
		perror(dest);
		return(1);
	} else if (!S_ISDIR(st2.st_mode) && S_ISDIR(st.st_mode)) {
		errno = EISDIR;
		perror(dest);
		return(1);
	}
  }
  if (!newfile && do_it("rm", "-Rf", dest, (char *) 0)) return(1);
  if (do_it("cpdir", "-fp", source, dest) ||
      do_it("rm", "-Rf", source, (char *) 0))
	return(1);
  return(0);
}


/* Posix command prototype. */
void usage()
{
  std_err("Usage: mv [-fi] source_file... target\n");
  exit(1);
}
