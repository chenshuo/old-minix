/* rm - remove directory entries	Author: V. Archer */

/* Copyright 1991 by Vincent Archer
 *	You may freely redistribute this software, in source or binary
 *	form, provided that you do not alter this copyright mention in any
 *	way.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <minix/minlib.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>


/* Global variables. */
int errors = 0;
int fflag = 0;
int iflag = 0;
int rflag = 0;
int tflag = -1;
char *pgm;
char path[PATH_MAX + 1];
struct stat st;

extern int optind, opterr;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(int negative, (void));
_PROTOTYPE(char *octal, (Mode_t num));
_PROTOTYPE(void rm_fork, (char *dir));
_PROTOTYPE(void removal, (char *name));
_PROTOTYPE(void stdprot, (char *name, char *prompt, Mode_t mode));
_PROTOTYPE(void usage, (void));

/* Main module. The internal '-t' flag is set if the file descriptor 0 is a
 * tty, rather than from the command line options. It is not used when '-f'
 * is specified.
 */
int main(argc, argv)
int argc;
char *argv[];
{
  int c;

  if (argc == 2 && *(argv[1]) == '-')
	usage();		/* file name required */
  pgm = argv[0];
  opterr = 0;
  while ((c = getopt(argc, argv, "Rfir")) != EOF) switch (c) {
	    case 'f':
		fflag = 1;
		iflag = 0;
		break;
	    case 'i':
		iflag = 1;
		fflag = 0;
		break;
	    case 'R':
	    case 'r':	rflag = 1;	break;
	    default:	usage();
	}
  if (optind >= argc) usage();

  tflag = isatty(0);
  while (optind < argc) removal(argv[optind++]);

  return(errors);
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


/* P1003.2 requires "rm -r directory" to be able to descend to arbitrary
 *	depth if a file hierarchy, and not to fail due to file descriptors
 *	loss or path length limitations (except PATH_MAX). (4.53.2)
 */
void rm_fork(dir)
char *dir;
{
  pid_t pid, pid2;
  int status;
  int i;
  char options[4];

  if (pid = fork()) {
	if (pid < 0) {
		perror("fork()");
		errors = 1;
		return;
	}
	while ((pid2 = wait(&status)) != pid)
		if (pid2 < 0) {
			errors = 1;
			return;
		}
	if (!WIFEXITED(status) || WEXITSTATUS(status)) errors = 1;
	return;
  }
  for (i = 3; i < OPEN_MAX; i++) close(i);
  options[0] = '-';
  options[1] = 'R';
  options[3] = '\0';
  if (fflag)
	options[2] = 'f';
  else if (iflag)
	options[2] = 'i';
  else
	options[2] = '\0';
  execlp(pgm, pgm, options, dir, (char *) 0);

  std_err(dir);
  std_err(": ");
  perror(pgm);
  exit(1);
}


/* Remove a single argument, as prescribed in P1003.2 (4.53.2) */
void removal(name)
char *name;
{
  DIR *dirp;
  struct dirent *entp;
  char *namp;

  if (
#ifdef S_IFLNK
      lstat(name, &st)
#else
      stat(name, &st)
#endif
	) {
	if (!fflag) {
		perror(name);
		errors = 1;
	}
	return;
  }
  if (namp = strrchr(name, '/'))
	namp++;
  else
	namp = name;
  if (namp[0] == '.' &&
      (!namp[1] || (namp[1] == '.' && !namp[2]))) {
	std_err(name);
	std_err(": cannot remove\n");
	errors = 1;
	return;
  }
  if (S_ISDIR(st.st_mode)) {
	if (!rflag) {
		errno = EISDIR;
		perror(name);
		errors = 1;
		return;
	}
	if (!fflag)
		if (access(name, 2) && tflag) {
			stdprot(name, ": remove contents", st.st_mode);
			if (negative()) return;
		} else if (iflag) {
			std_err(name);
			std_err(": remove contents ? ");
			if (negative()) return;
		}
	if (!(dirp = opendir(name))) {
		if (errno != EMFILE) {
			perror(name);
			errors = 1;
			return;
		}
		rm_fork(name);
		return;
	} else {
		if (name != path) strcpy(path, name);
		namp = path + strlen(path);
		*namp++ = '/';
		while (entp = readdir(dirp))
			if (entp->d_name[0] != '.' ||
			    (entp->d_name[1] &&
			     (entp->d_name[1] != '.' || entp->d_name[2]))) {
				strcpy(namp, entp->d_name);
				removal(path);
			}
		closedir(dirp);
		*--namp = '\0';
	}
	if (iflag) {
		std_err(name);
		std_err(": remove directory ? ");
		if (negative()) return;
	}
	if (rmdir(name)) {
		perror(name);
		errors = 1;
	}
  } else {
	if (!fflag)
		if (
#ifdef S_IFLNK
		    !S_ISLNK(st.st_mode) &&
#endif
		    access(name, 2) && tflag) {
			stdprot(name, ": remove", st.st_mode);
			if (negative()) return;
		} else if (iflag) {
			std_err(name);
			std_err(": remove ? ");
			if (negative()) return;
		}
	if (unlink(name)) {
		perror(name);
		errors = 1;
		return;
	}
  }
}


/* Standard prompt when mode does not allow writing to something. */
void stdprot(name, prompt, mode)
char *name, *prompt;
mode_t mode;
{
  std_err(name);
  std_err(prompt);
  std_err(" (mode = ");
  std_err(octal(mode));
  std_err(") ? ");
}


/* Posix command prototype. */
void usage()
{
  std_err("Usage: rm [-fiRr] file...\n");
  exit(1);
}
