/* cp - Copy files					Author: V. Archer */

/* Copyright 1991 by Vincent Archer
 *	You may freely redistribute this software, in source or binary
 *	form, provided that you do not alter this copyright mention in any
 *	way.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <minix/minlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <utime.h>
#include <blocksize.h>
#include <unistd.h>
#include <stdio.h>

#define ALL_MODES (S_IRWXU|S_IRWXG|S_IRWXO)
#define NONE ((char *)0)

/* A link (for cpdir) descriptor. Link are never un-allocated, this allow
 * race conditions (a user creating links while another is busy copying the
 * hierarchy in which they reside, for example), at the price of memory
 * shortage...
 */
typedef struct _link_ {
  struct _link_ *next;
  dev_t st_dev;
  ino_t st_ino;
  char *path;
} LINK;

/* A tree (for cp -Rr/cpdir) descriptor. It is used when a directory cannot
 * be opened due to file descriptor shortage. For cp, it would be safe to
 * block on that condition. For mv (which calls cpdir -p), however, P1003.2
 * requires that the copy should NOT fail! Therefore, for each directory to
 * be opened, another might be closed. When we get back to that directory's
 * level, however, we'll have to reopen it and move to our previous position
 * within this directory.
 */
typedef struct _tree_ {
  struct _tree_ *next;
  DIR *dirp;
  off_t pos;
  struct stat st;
} TREE;

int cflag, dflag, fflag, iflag, pflag, rflag, rrflag, vflag;
int errors;

/* Common variables. The copy buffer is limited to PIPE_BUF to avoid errors
 * in cp -r (lowercase r) while copying from a pipe.
 */
char *dest;
char dst_path[PATH_MAX + 1], src_path[PATH_MAX + 1];
char buffer[PIPE_BUF];
LINK *links;
TREE *toplevel;
struct stat st2;
uid_t userid;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(int negative, (void));
_PROTOTYPE(char *octal, (Mode_t num));
_PROTOTYPE(void doing, (char *what, char *with, char *on));
_PROTOTYPE(void do_close, (void));
_PROTOTYPE(void similar, (struct stat *sp));
_PROTOTYPE(void do_cpfile, (int new, struct stat *st));
_PROTOTYPE(void do_cpdir, (int new, struct stat *oldmode, TREE *dotdot));
_PROTOTYPE(int do_cp, (char *source));
_PROTOTYPE(void usage, (void));

extern int optind, opterr;

/* Main module. If cp is invoked as "cpdir", the -R flag is automatically
 * turned on. Furthermore, the 'c' pseudo-flag is set, meaning that links
 * to the same source file should be preserved across the copy. 'c' cancels
 * any '-r' invocation.
 * The '-v' flag is maintained for compatibility with old cpdir.
 */
int main(argc, argv)
int argc;
char *argv[];
{
  char *s;
  int c;
  struct stat st;

  if ((s = strrchr(*argv, '/')) != NULL)
	s++;
  else
	s = *argv;
  if (strcmp(s, "cpdir") == 0) {
	cflag = 1;
	rflag = 1;
	rrflag = 1;
  }
  opterr = 0;
  while ((c = getopt(argc, argv, "Rfimprsv")) != EOF) switch (c) {
	    case 'f':
		fflag = 1;
		iflag = 0;
		break;
	    case 'i':
		iflag = 1;
		fflag = 0;
		break;
	    case 'm':
		break;
	    case 's':
	    case 'p':	pflag = 1;	break;
	    case 'v':	vflag = 1;	break;
	    case 'R':	rrflag = 1;
	    case 'r':	rflag = 1;	break;
	    default:	usage();
	}
  argc -= optind;
  if (argc < 2) usage();
  if (argc > 2 && cflag) usage();
  argv += optind;
  dest = argv[--argc];
  userid = getuid();

  if (!cflag)
	if (stat(dest, &st)) {
		if (argc > 1) {
			perror(dest);
			return(1);
		}
	} else if (S_ISDIR(st.st_mode))
		dflag = 1;
	else if (argc > 1 || rflag) {
		errno = ENOTDIR;
		perror(dest);
		return(1);
	}
  while (argc--) errors |= do_cp(*argv++);
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


/* Verbose output of the operation. mknod4 could be better shown than thru
 * this, but I don't care...
 */
void doing(what, with, on)
char *what, *with, *on;
{
  std_err(what);
  std_err(with);
  if (on) {
	std_err(" ");
	std_err(on);
  }
  std_err("\n");
}


/* Close a previously opened directory stream when file descriptors are
 * needed. The streams are closed in a First-Open-First-Closed (FIFO) order,
 * because high-level directories are less likely to be needed soon than
 * lower-level directories.
 */
void do_close()
{
  TREE *sweep;

  for (sweep = toplevel; sweep; sweep = sweep->next)
	if (sweep->dirp) {
		closedir(sweep->dirp);
		sweep->dirp = (DIR *) 0;
		return;
	}
  std_err("FATAL:");
  perror("cpdir");
  exit(1);
}


/* This function handles the "-p" option. */
void similar(sp)
struct stat *sp;
{
  struct utimbuf timer;

  timer.actime = sp->st_atime;
  timer.modtime = sp->st_mtime;
  if (utime(dst_path, &timer)) {
	perror(dst_path);
	errors = 1;
  }
  if (chown(dst_path, sp->st_uid, sp->st_gid) || userid != 0)
	sp->st_mode &= ALL_MODES;
  if (chmod(dst_path, sp->st_mode)) {
	perror(dst_path);
	errors = 1;
  }
}


/* This copy a directory entry (non-directory inode) to a (possibly new)
 * destination. Prompting, linking and mkfifo/mknod4 are done here.
 */
void do_cpfile(new, st)
int new;
struct stat *st;
{
  int fd, fd2, n, m;
  char *bufp;
  long s;
  LINK *linkp;

  if (!new) {
	if (iflag) {
		std_err(dst_path);
		std_err(": replace ? ");
		if (negative()) return;
	}
	if (access(dst_path, 2)) {
		perror(dst_path);
		errors = 1;
		return;
	}
  }
  if (cflag && st->st_nlink > 1) {
	n = 0;
	for (linkp = links; linkp; linkp = linkp->next)
		if (linkp->st_dev == st->st_dev &&
		    linkp->st_ino == st->st_ino) {
			if (!new) {
				if (vflag) doing("unlink ", dst_path, NONE);
				if (unlink(dst_path)) {
					perror(dst_path);
					errors = 1;
					return;
				} else
					new = 1;
			}
			if (vflag) doing("link ", linkp->path, dst_path);
			if (!link(linkp->path, dst_path)) {
				return;
			}
			if (errno != EXDEV) {
				std_err(src_path);
				std_err(", ");
				perror(dst_path);
			} else if (!n) {
				std_err(dst_path);
				std_err(": cross device link snapped\n");
			}
			n = 1;	/* display snap once */
		}
	if ((linkp = (LINK *) malloc(sizeof(LINK))) == (LINK *) 0)
		perror(dst_path);
	else if ((linkp->path = (char *)malloc(strlen(dst_path) + 1)) == NONE){
		perror(dst_path);
		free(linkp);
	} else {
		strcpy(linkp->path, dst_path);
		linkp->st_dev = st->st_dev;
		linkp->st_ino = st->st_ino;
		linkp->next = links;
		links = linkp;
	}
  }
  if (
#ifdef S_IFLNK
      S_ISLNK(st->st_mode) ||
#endif
      rrflag && (S_ISBLK(st->st_mode) ||
		S_ISCHR(st->st_mode) || S_ISFIFO(st->st_mode))) {
	if (!new) {
		if (vflag) doing("unlink ", dst_path, NONE);
		if (unlink(dst_path)) {
			perror(dst_path);
			errors = 1;
			return;
		}
	}
	if (S_ISFIFO(st->st_mode)) {
		if (vflag) doing("mkfifo ", dst_path, octal(st->st_mode));
		if (mkfifo(dst_path, st->st_mode & ALL_MODES)) {
			perror(dst_path);
			errors = 1;
			return;
		}
	} else if (S_ISBLK(st->st_mode) || S_ISCHR(st->st_mode)) {
		if (vflag) doing("mknod4 ", dst_path, octal(st->st_mode));
		s = (long) st->st_size / BLOCK_SIZE;
		if (mknod4(dst_path, st->st_mode, st->st_rdev, s)) {
			perror(dst_path);
			errors = 1;
			return;
		}
	}
#ifdef S_IFLNK
	else if (S_ISLNK(st->st_mode)) {
		static char linkname[PATH_MAX + 1];
		int len;

		len = readlink(src_path, linkname, sizeof(linkname) - 1);
		if (len < 0) {
			perror(src_path);
			errors = 1;
			return;
		}
		linkname[len] = 0;
		if (vflag) doing("symlink ", linkname, dst_path);
		if (symlink(linkname, dst_path)) {
			perror(dst_path);
			errors = 1;
			return;
		}
	}
#endif
	if (pflag) similar(st);
	return;
  }
  while ((fd = open(src_path, O_RDONLY)) < 0) {
	if (errno != EMFILE && errno != ENFILE) {
		perror(src_path);
		errors = 1;
		return;
	}
	do_close();
  }

  if (vflag) doing("cp ", src_path, dst_path);
  if (new) {
	while ((fd2 = open(dst_path, O_WRONLY | O_CREAT,
			   st->st_mode & ALL_MODES)) < 0)
		if (errno == EMFILE || errno == ENFILE)
			do_close();
		else
			break;
  } else {
	while ((fd2 = open(dst_path, O_WRONLY | O_TRUNC)) < 0)
		if (errno == EMFILE || errno == ENFILE)
			do_close();
		else
			break;
  }
  if (fd2 < 0) {
	perror(dst_path);
	close(fd);
	errors = 1;
	return;
  }
  while ((n = read(fd, buffer, sizeof(buffer))) != 0) {
	if (n < 0) {
		perror(src_path);
		errors = 1;
		break;
	}
	bufp = buffer;
	while (n) {
		m = write(fd2, bufp, n);
		if (!m) {
			m = -1;
			errno = 0;
		}
		if (m < 0) {
			perror(dst_path);
			errors = 1;
			n = -1;
			break;
		}
		n -= m;
		bufp += m;
	}
	if (n < 0) break;
  }

  close(fd);
  close(fd2);
  if (new && pflag) similar(st);
}


/* Copying a directory's contents is done here. If necessary, the
 * directory will be created, but protected by 'a=,u=rwx' mode until the
 * entire copy is done (or failed, somehow). The mode is the reverted to
 * the "correct" creation mode at the end of the directory copy.
 */
void do_cpdir(new, oldmode, dotdot)
int new;
struct stat *oldmode;
TREE *dotdot;
{
  TREE d;
  char *src, *dst;
  mode_t oldmask;
  int new2;
  struct dirent *entp;

  if (vflag) doing("cpdir ", src_path, dst_path);
  if (new) {
	oldmask = umask(0);
	if (vflag) doing("mkdir ", dst_path, NONE);
	if (mkdir(dst_path, S_IRWXU)) {
		perror(dst_path);
		umask(oldmask);
		errors = 1;
		return;
	}
	umask(oldmask);
  }
  while ((d.dirp = opendir(src_path)) == (DIR *) 0)
	if (errno == EMFILE || errno == ENFILE || errno == ENOMEM)
		do_close();
	else {
		perror(src_path);
		if (new) {
			if (pflag)
				similar(oldmode);
			else
				chmod(dst_path, oldmode->st_mode & ALL_MODES & ~oldmask);
		}
		errors = 1;
		return;
	}

  src = src_path + strlen(src_path);
  dst = dst_path + strlen(dst_path);
  *src++ = '/';
  *dst++ = '/';

  if (dotdot)
	dotdot->next = &d;
  else
	toplevel = &d;

  d.pos = 0;

  for (;;) {
	if (d.dirp == (DIR *) 0) {
		dst[-1] = '\0';
		if ((d.dirp = opendir(dst_path)) == (DIR *) 0) {
			perror(dst_path);
			errors = 1;
			break;
		}
		dst[0] = '/';
		seekdir(d.dirp, d.pos);
		if ((entp = readdir(d.dirp)) && entp->d_off == d.pos)
			entp = readdir(d.dirp);	/* Already processed! */
	} else
		entp = readdir(d.dirp);

	if (entp == (struct dirent *) 0) break;

	if (entp->d_name[0] != '.' ||
	    (entp->d_name[1] &&
	     (entp->d_name[1] != '.' || entp->d_name[2]))) {
		strcpy(src, entp->d_name);
		strcpy(dst, entp->d_name);
		if (
#ifdef S_IFLNK
		    cflag && lstat(src_path, &d.st) ||
		    !cflag && stat(src_path, &d.st)
#else
		    stat(src_path, &d.st)
#endif
		   ) {
			perror(src);
			errors = 1;
			continue;
		}
		d.pos = entp->d_off;

		if ((new2 = stat(dst_path, &st2)) != 0) {
			if (S_ISDIR(d.st.st_mode)) {
				do_cpdir(new2, &d.st, &d);
				continue;
			}
		} else if (S_ISDIR(d.st.st_mode)) {
			if (S_ISDIR(st2.st_mode)) {
				do_cpdir(new2, &d.st, &d);
				continue;
			}
			errno = EISDIR;
			perror(dst_path);
			errors = 1;
			continue;
		}
		do_cpfile(new2, &d.st);
	}
  }

  closedir(d.dirp);
  *--src = '\0';
  *--dst = '\0';
  if (dotdot)
	dotdot->next = (TREE *) 0;
  else
	toplevel = (TREE *) 0;

  if (new) {
	if (pflag)
		similar(oldmode);
	else
		chmod(dst_path, oldmode->st_mode & ALL_MODES & ~oldmask);
  }
}


/* This is called to copy each of the ARGUMENTS (specified in cp) to its
 * destination, either the 2nd argument to cp, or somewhere in the
 * specified directory.
 */
int do_cp(source)
char *source;
{
  struct stat st, st2;
  char *s;
  int newfile;

  if (stat(source, &st)) {
	perror(source);
	return(1);
  }
  strcpy(dst_path, dest);
  if (dflag) {
	if ((s = strrchr(source, '/')) != NULL)
		strcat(dst_path, s);
	else {
		strcat(dst_path, "/");
		strcat(dst_path, source);
	}
  }
  if (!(newfile = stat(dst_path, &st2)))
	if (st.st_dev == st2.st_dev && st.st_ino == st2.st_ino) {
		std_err(source);
		std_err(",");
		std_err(dst_path);
		std_err(": Same file\n");
		return(1);
	}
  strcpy(src_path, source);
  if (S_ISDIR(st.st_mode)) {
	if (!rflag) {
		errno = EISDIR;
		perror(source);
		return(1);
	}
	if (!newfile && !S_ISDIR(st2.st_mode)) {
		errno = ENOTDIR;
		perror(dst_path);
		return(1);
	}
	do_cpdir(newfile, &st, (TREE *) 0);
	return(0);
  }
  do_cpfile(newfile, &st);
  return(0);
}


/* Posix prototype of the command */
void usage()
{
  std_err(cflag ? "Usage: cpdir [-fipv] source_dir target_dir\n"
	: "Usage: cp [-R|-r] [-fip] source_file... target\n");
  exit(1);
}
