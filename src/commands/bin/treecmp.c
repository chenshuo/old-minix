/* treecmp - compare two trees		Author: Andy Tanenbaum */

/* This program recursively compares two trees and reports on differences.
 * It can be used, for example, when a project consists of a large number
 * of files and directories.  When a new release (i.e., a new tree) has been
 * prepared, the old and new tree can be compared to give a list of what has
 * changed.  The algorithm used is that the first tree is recursively
 * descended and for each file or directory found, the corresponding one in
 * the other tree checked.  The two arguments are not completely symmetric
 * because the first tree is descended, not the second one, but reversing
 * the arguments will still detect all the differences, only they will be
 * printed in a different order.  The program needs lots of stack space
 * because routines with local arrays are called recursively. The call is
 *    treecmp [-v] dir1 dir2
 * The -v flag (verbose) prints the directory names as they are processed.
 */

#include <sys/stat.h>

#define BUFSIZE 4096		/* size of file buffers */
#define MAXPATH 128		/* longest acceptable path */
#define DIRENTLEN 14		/* number of characters in a file name */

struct dirstruct {		/* layout of a directory entry */
  unsigned inum;
  char fname[DIRENTLEN];
};

struct stat stat1, stat2;	/* stat buffers */

char buf1[BUFSIZE];		/* used for comparing bufs */
char buf2[BUFSIZE];		/* used for comparing bufs */

int verbose;			/* set if mode is verbose */

main(argc, argv)
int argc;
char *argv[];
{
  char *p;

  if (argc < 3 || argc > 4) usage();
  p = argv[1];
  if (argc == 4) {
	if (*p == '-' && *(p+1) == 'v') 
		verbose++;
	else
		usage();
  }

  if (argc == 3)
	compare(argv[1], argv[2]);
  else
	compare(argv[2], argv[3]);

  exit(0);
}

compare(f1, f2)
char *f1, *f2;
{
/* This is the main comparision routine.  It gets two path names as arguments
 * and stats them both.  Depending on the results, it calls other routines
 * to compare directories or files.
 */

  int type1, type2;

  if (stat(f1, &stat1)  < 0) {
	printf("Cannot stat %s\n", f1);
	return;
  }

  if (stat(f2, &stat2)  < 0) {
	printf("Missing file: %s\n", f2);
	return;
  }

  /* Examine the types of the files. */
  type1 = stat1.st_mode & S_IFMT;
  type2 = stat2.st_mode & S_IFMT;
  if (type1 != type2) {
	printf("Type diff: %s and %s\n", f1, f2);
	return;
  }

  /* The types are the same. */
  switch(type1) {
	case S_IFREG:	regular(f1, f2);
			break;

	case S_IFDIR:	directory(f1, f2);
			break;

	case S_IFCHR:
	case S_IFBLK:	break;

	default:	printf("Unknown file type %o\n", type1);
  }
  return;
}

regular(f1, f2)
char *f1, *f2;
{
/* Compare to regular files.  If they are different, complain. */

  int fd1, fd2, n1, n2, i;
  unsigned bytes;
  long count;
  char *p1, *p2;

  if (stat1.st_size != stat2.st_size) {
	printf("Size diff: %s and %s\n", f1, f2);
	return;
  }

  /* The sizes are the same.  We actually have to read the files now. */
  fd1 = open(f1, 0);
  if (fd1 < 0) {
	printf("Cannot open %s for reading\n", f1);
	return;
  }

  fd2 = open(f2, 0);
  if (fd2 < 0) {
	printf("Cannot open %s for reading\n", f2);
	return;
  }

  count = stat1.st_size;
  while (count > 0L) {
	bytes = (unsigned) (count > BUFSIZE ? BUFSIZE : count);	/* rd count */
	n1 = read(fd1, buf1, bytes);
	n2 = read(fd2, buf2, bytes);
	if (n1 != n2) {
		printf("Length diff: %s and %s\n", f1, f2);
		close(fd1);
		close(fd2);
		return;
	}

	/* Compare the buffers. */
	i = n1;
	p1 = buf1;
	p2 = buf2;
	while (i--) {
		if (*p1++ != *p2++) {
			printf("File diff: %s and %s\n", f1, f2);
			close(fd1);
			close(fd2);
			return;
		}
	}
	count -= n1;
  }
  close(fd1);
  close(fd2);
}

directory(f1, f2)
char *f1, *f2;
{
/* Recursively compare two directories by reading them and comparing their
 * contents.  The order of the entries need not be the same.
 */

  int fd1, fd2, n1, n2, ent1, ent2, i, used1 = 0, used2 = 0;
  char *dir1buf, *dir2buf;
  char name1buf[MAXPATH], name2buf[MAXPATH];
  struct dirstruct *dp1, *dp2;
  unsigned dir1bytes, dir2bytes;
  extern char *malloc();

  /* Allocate space to read in the directories */
  dir1bytes = (unsigned) stat1.st_size;
  dir1buf = malloc(dir1bytes);
  if (dir1buf == 0) {
	printf("Cannot process directory %s: out of memory\n", f1);
	return;
  }

  dir2bytes = (unsigned) stat2.st_size;
  dir2buf = malloc(dir2bytes);
  if (dir2buf == 0) {
	printf("Cannot process directory %s: out of memory\n", f2);
	free(dir1buf);
	return;
  }

  /* Read in the directories. */
  fd1 = open(f1, 0);
  if (fd1 > 0) n1 = read(fd1, dir1buf, dir1bytes);
  if (fd1 < 0 || n1 != dir1bytes) {
	printf("Cannot read directory %s\n", f1);
	free(dir1buf);
	free(dir2buf);
	if (fd1 > 0) close(fd1);
	return;
  }
  close(fd1);

  fd2 = open(f2, 0);
  if (fd2 > 0) n2 = read(fd2, dir2buf, dir2bytes);
  if (fd2 < 0 || n2 != dir2bytes) {
	printf("Cannot read directory %s\n", f2);
	free(dir1buf);
	free(dir2buf);
	close(fd1);
	if (fd2 > 0) close(fd2);
	return;
  }
  close(fd2);

  /* Linearly search directories */
  ent1 = dir1bytes/sizeof(struct dirstruct);
  dp1 = (struct dirstruct *) dir1buf;
  for (i = 0; i < ent1; i++) {
	if (dp1->inum != 0) used1++;
	dp1++;
  }

  ent2 = dir2bytes/sizeof(struct dirstruct);
  dp2 = (struct dirstruct *) dir2buf;
  for (i = 0; i < ent2; i++) {
	if (dp2->inum != 0) used2++;
	dp2++;
  }

  if (verbose) printf("Directory %s: %d entries\n", f1, used1);

  /* Check to see if any entries in dir2 are missing from dir1. */
  dp1 = (struct dirstruct *) dir1buf;
  dp2 = (struct dirstruct *) dir2buf;
  for (i = 0; i < ent2; i++) {
	if (dp2->inum == 0 || strcmp(dp2->fname, ".") == 0 || 
		strcmp(dp2->fname, "..") == 0) {
			dp2++;
			continue;
	}
	check(dp2->fname, dp1, ent1, f1);
	dp2++;
  }

  /* Recursively process all the entries in dir1. */
  dp1 = (struct dirstruct *) dir1buf;
  for (i = 0; i < ent1; i++) {
	if (dp1->inum == 0 || strcmp(dp1->fname, ".") == 0 || 
		strcmp(dp1->fname, "..") == 0) {
			dp1++;
			continue;
	}
	if (strlen(f1) + DIRENTLEN >= MAXPATH) {
		printf("Path too long: %s\n", f1);
		free(dir1buf);
		free(dir2buf);
		return;
	}
	if (strlen(f2) + DIRENTLEN >= MAXPATH) {
		printf("Path too long: %s\n", f2);
		free(dir1buf);
		free(dir2buf);
		return;
	}
	
	strcpy(name1buf, f1);
	strcat(name1buf, "/");
	strncat(name1buf, dp1->fname, DIRENTLEN);
	strcpy(name2buf, f2);
	strcat(name2buf, "/");
	strncat(name2buf, dp1->fname, DIRENTLEN);
 
	/* Here is the recursive call to process an entry. */
	compare(name1buf, name2buf);	/* recursive call */
	dp1++;
  }

  free(dir1buf);
  free(dir2buf);
}

check(s, dp1, ent1, f1)
char *s;
struct dirstruct *dp1;
int ent1;
char *f1;
{
/* See if the file name 's' is present in the directory 'dirbuf'. */
  int i;

  for (i = 0; i < ent1; i++) {
	if (strncmp(dp1->fname, s, DIRENTLEN) == 0) return;
	dp1++;
  }
  printf("Missing file: %s/%s\n", f1, s);
}

usage()
{
  printf("Usage: treecmp [-v] dir1 dir2\n");
  exit(0);
}

