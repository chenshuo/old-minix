/* ls - list directory			Author: C. E. Chew */

/*			List Directory Entries
 *
 * (C) Copyright C E Chew
 *
 * Feel free to copy and use this software provided:
 *
 *	1. you do not pretend that you wrote it
 *	2. you leave this copyright notice intact.
 *
 * This is an implementation of a BSD style ls(1) for Minix. This
 * implementation is faster than the original ls(1) for Minix. There
 * are no restrictions with regard to the size of the directory file
 * since memory is allocated dynamically.
 */

#ifndef		PASS1		/* hack for small compilers */
# ifndef	PASS2
#   define	PASS1
#   define	PASS2
# endif
#endif

#ifdef	PASS1
char Version_[] = "@(#)els 3.17a 03-Jan-1993 (C) C E Chew";
#endif

/* Edit History:
 * 03-Jan-1993  Merged Minix 1.6.23 changes (-DAST, larger field width for
 *		file lengths, and silly parens for && within ||).
 * 17-May-1992	Accommodated Minix 1.6.16 hacks and Bruce's minor patches.
 * 02-May-1992	Bug with -f flag following null pointer.
 *		Fix typo in blank padding calculation.
 * 11-Jul-1991	Could not follow relative symlinks when invoked with
 *		ls -l /usr/local/lib/mushtool.
 * 14-Apr-1991	Incorporated mntent library source.
 * 06-Apr-1991	Fix `ls -ld' problem.
 * 30-Mar-1991	Fix uninitialised cw pointer in acrosspage().
 * 12-Mar-1991	Strip out non-Minix conditional code into ls.h
 * 08-Mar-1991	Need const qualifier in stringlength().
 *		Fix for Mips gcc complaint about wrong printf format.
 *		Don't initialise blanks[] at runtime.
 * 06-Mar-1991	Fix date initialisation.
 * 05-Mar-1991	Fix typos and avoid getcwd() and time() if possible.
 * 03-Mar-1991	Add -0 option to reset options (to unset LSOPTS).
 *		Add usage information.
 * 22-Feb-1991	Add -S option to squeeze column widths.
 * 21-Feb-1991	Use STDC locally to avoid __STDC__ brain damage.
 *		Change order of dolsopts() and docmdname() scan.
 * 20-Feb-1991	Some Coherent port problems.
 * 15-Feb-1991	Coherent port included.
 * 27-Dec-1990	Upgrade for Minix 2.0 filesystem.
 * 30-Nov-1990	Fix nil dereference problem.
 * 21-Nov-1990	Use lstat() on target of symlink so that multilevel
 *		links are visible. Don't give up on failed readlink().
 * 23-Jul-1990	POSIXfication.
 * 10-May-1990	Miscellaneous patches.
 * 23-Apr-1989	Reorder includes for 1.5.8. Support sticky and locking
 *		bits. Support st_ctime and st_atime. Support for name
 *		aliases.
 * 20-Oct-1989	Change suffix for failed symbolic links.
 * 21-Sep-1989	Changed names to _BSD and _MINIX. Use #ifdef for
 *		portability.
 * 19-Sep-1989	Sizes in kb rather than `blocks'.
 * 14-Sep-1989	No longer need to define MINIX. Get rid of itoa().
 *		Pyramid BSD coercions. Symbolic links and sockets.
 * 27-Aug-1989	Added declaration of errno for old errno.h and
 *		char *malloc() for old include files. Added
 *		support for named FIFO's. Changed user and group
 *		name format to %-6.6s so that long names won't
 *		muck up columns. Later will have to add
 *		symbolic link support for -l and -L.
 * 16-May-1989	Option -n shouldn't look in /etc/passwd. Use new
 *		strerror in string.h, add prototype for itoa().
 * 30-Apr-1989	Changed stat failure message to not found. Include
 *		dirent.h after stdio.h so that NULL is defined.
 * 22-Mar-1989	Incompatible options processing, long sizes instead
 *		of int and removed TURBOC conditional.
 * 02-Mar-1989	Sort command line arguments.
 * 22-Feb-1989	Fixed up bug regarding device number printing.
 * 10-Sep-1988	Cleaned up for lint.
 * 05-Sep-1988	Tidied up for network release.
 */

#ifndef		_SYSV
# ifndef	_BSD
#   ifndef	COHERENT
#     ifndef	_MINIX
#       define 	_MINIX
#     endif
#   endif
# endif
#endif

#define		TERMCAP		/* consult termcap for columns */
/*efine		USESTDLIB*/	/* stdlib.h available for use */
/*efine		USESTDDEF*/	/* stddef.h available for use */

#ifdef __STDC__			/* fix brain damage */
# if __STDC__ != 0
#   define STDC
# endif
#endif
#ifdef	STDC
# define CONST		const
# define P(x)		x
# define F4(t1,n1,t2,n2,t3,n3,t4,n4)	(t1 n1,t2 n2,t3 n3,t4 n4)
# define F3(t1,n1,t2,n2,t3,n3)		(t1 n1,t2 n2,t3 n3)
# define F2(t1,n1,t2,n2)		(t1 n1,t2 n2)
# define F1(t1,n1)			(t1 n1)
# define F0()				(void)
#else
# define CONST
# define P(x)		()
# define F4(t1,n1,t2,n2,t3,n3,t4,n4)	(n1,n2,n3,n4)t1 n1;t2 n2;t3 n3;t4 n4;
# define F3(t1,n1,t2,n2,t3,n3)		(n1,n2,n3)t1 n1;t2 n2;t3 n3;
# define F2(t1,n1,t2,n2)		(n1,n2)t1 n1;t2 n2;
# define F1(t1,n1)			(n1)t1 n1;
# define F0()				()
#endif

#include <sys/types.h>
#include <sys/stat.h>

#ifndef		_MINIX
# include "ls.h"
#else
# ifndef	_POSIX_SOURCE
#   define	_POSIX_SOURCE
#   define	_POSIX_1_SOURCE 2
# endif
# include <stdlib.h>
# define FREE_T void *
# include <limits.h>
# ifdef	PASS1
#   include <ctype.h>
#   include <grp.h>
#   include <pwd.h>
#   include <errno.h>
#   include <dirent.h>
#   include <string.h>
#   include <time.h>
#   include <stddef.h>
#   define OFFSETOF(t,n)	offsetof(t,n)
#   define USRGRPOTH(m)		(m)
# endif
# ifdef	PASS2
#   include <limits.h>
#   include <minix/config.h>
#   include <minix/const.h>
#   include <minix/type.h>
#   include "../fs/const.h"
#   include "../fs/type.h"
#   undef printf
#   define STD_BLK	BLOCK_SIZE
#   define BYTESPERBLK	512
#   ifndef V1_NR_DZONES
#     define V1_NR_TZONES NR_ZONE_NUMS
#     define V1_NR_DZONES NR_DZONE_NUM
#     define V1_INDIRECTS NR_INDIRECTS
#   else
#     define USEMTAB
#     ifdef USEMNTENTLIB
#	include <mntent.h>
#     else
#	define MTAB		"/etc/mtab"
struct mntent {
  char *mnt_fsname;			/* device */
  char *mnt_dir;			/* mount point */
  char *mnt_type;			/* filesystem type */
  char *mnt_opts;			/* options */
};
#   include <stdio.h>			/* we were trying to include this last
					 * to avoid Minix header braindamage
					 * (multiple definitions of NULL), but
					 * FILE * is needed now */
#   undef NULL				/* fix the header braindamage */
FILE *setmntent P((char *_file, char *));
struct mntent *getmntent P((FILE *_mf));
void endmntent P((FILE *_mf));
#	define setmntent(n,t)	(fopen((n),(t)))
#	define endmntent(f)	((void) fclose(f))
#     endif
#   endif
# endif
# include <unistd.h>
# include <stdio.h>
#endif

#ifdef PASS2
# ifdef	_POSIX_SOURCE
#   undef BYTESPERBLK
#   define BYTESPERBLK	512
# endif
#endif

/***********************************************************************\
 *                            Common Definitions                       *
\***********************************************************************/

#define NIL		(0)	/* nil pointer */

/* Mounted file system parameter table */

typedef struct filesystem {
  struct filesystem *next;		/* link to next */
  dev_t dev;				/* mounted device */
  unsigned long *fs;			/* structure */
} FILESYSTEM;

/***********************************************************************\
 *                             Portable Code                           *
\***********************************************************************/
#ifdef		PASS1

#ifndef		SUPER_USER
# define SUPER_USER	(0)
#endif

#define DEFAULTDIR	"."	/* default for ls without args */
#define ENVNAME		"LSOPTS"/* ls options */
#define COLNAME		"COLUMNS"/* columns option */
#define TERMNAME	"TERM"	/* name of terminal */
#define LINKPOINTER	" -> "	/* symlink pointer indicator */
#define COLUMNS		80	/* columns on terminal */
#define INTERSPACE	2	/* spacing between columns */
#define INODEWIDTH	5	/* width of field for inode */
#define BLOCKWIDTH	6	/* width of field for blocks */

#define HALFYEAR	((long) 60*60*24*365/2)	/* half year in seconds */
#define BASEYEAR	1900	/* struct tm.tm_year base */

/* Flags are maintained in a bitmap. */
#define BPC		CHAR_BIT/* bits per character */
#define BITTEST(m,b)	(m[b/BPC] & (1<<(b%BPC)))
#define BITSET(m,b)	(m[b/BPC] |= (1<<(b%BPC)))
#define BITCLEAR(m,b)	(m[b/BPC] &= ~(1<<(b%BPC)))
#define BITCHARS(b)	((b+BPC-1)/BPC)

#define TEST(b)		BITTEST(flags,b)
#define SET(b)		BITSET(flags,b)
#define CLEAR(b)	BITCLEAR(flags,b)
#define FLAGS		(1<<BPC)

/* These macros permit the shortens the short circuiting of
 * complicated boolean expressions. This is particularly
 * useful when working with the flags since these are
 * read-only.
 */
#define BOOL(b)		static char b = 0
#define EVAL(b,a)	((b ? b : (b = (a)+1)) > 1)

/* A descriptor is kept for each file in the directory. The
 * descriptors are linked together in a linked list.
 */
struct direntry {
  struct direntry *next;	/* link list */
  char *name;			/* name of entry */
  char *suffix;			/* entry suffix */
  int length;			/* length of name and suffix */
  struct direntry *parent;	/* pointer to parent */
  struct stat *status;		/* pointer to status */
};

typedef struct direntry DIRENTRY;	/* entry */
typedef struct {
  DIRENTRY *head;		/* head of list */
  DIRENTRY **tail;		/* insertion point at tail */
} DIRLIST;			/* list of entries */

#define EMPTY_dl(d)	((d).head=(DIRENTRY *)NIL,(d).tail=(&(d).head))
#define APPEND_dl(d,p)	(*(d).tail=(p),(d).tail=(&(p)->next))
#define TIE_dl(d,p)	APPEND_dl(d,p); \
		while (*(d).tail) (d).tail=(&(*(d).tail)->next)
#define HEAD_dl(d)	(d).head
#define LINKOFFSET	(OFFSETOF(DIRENTRY, next))

/* Name statistics are required to support multi-column output.
 * The statistics are used to compute the optimal number of
 * columns required for output.
 */
typedef struct namestats {
  int n;		/* number of entries */
  int minwidth;		/* minimum width seen */
  int maxwidth;		/* maximum width seen */
} NAMESTATS;

#define INIT_NS(x)	((x).n = 0, (x).minwidth = INT_MAX, (x).maxwidth = 0)
#define MAX_NS(x,y)	((x)>(y)?(x):(y))
#define MIN_NS(x,y)	((x)<(y)?(x):(y))
#define UPDATE_NS(x,w)	((x).n++, \
			 (x).minwidth = MIN_NS((x).minwidth, (w)), \
                         (x).maxwidth = MAX_NS((x).maxwidth, (w)))

/* These status bits are to help compute the correct suffix to append
 * to a file name listing. The current working directory is required
 * when following relative symbolic links.
 */
#define LF_NORMAL	0x00		/* Normal unknown disposition */
#define LF_SYMLINK	0x01		/* Known to be a symbolic link */
#define LF_BADCWD	0x02		/* Current directory wrong */

/* Function Pointers */

typedef int (*statfunc) P((const char *, struct stat *));
typedef int (*cmpfunc) P((void *, void *));
typedef int (*strlenfunc) P((CONST char *));
typedef void (*putstrfunc) P((char *));

/* External Functions */

int getopt P((int, char **, char *));	/* parse the options */

#ifdef	TERMCAP
int tgetent P((char *, char *));/* get entry from termcap database */
int tgetnum P((char *));	/* get numeric capability */
#endif

/* Symbolic Links. */

#ifdef		S_IFLNK
# define OPAQUESTAT	((statfunc) lstat)
#else
# define OPAQUESTAT	((statfunc) stat)
#endif
#define TRANSPARENTSTAT	((statfunc) stat)

/* Permission Classes */

#define	PERMCLASSES	3	/* Number of permission classes */
#define	PERMUSR		2
#define	PERMGRP		1
#define	PERMOTH		0

#define	PERMBITS	3	/* Number of bits per class */

/* Permission Strings */

#define PERM_NORMAL	(0 * (1 << PERMBITS) * sizeof(permstrings[0]))
#define PERM_SUID	(1 * (1 << PERMBITS) * sizeof(permstrings[0]))
#define PERM_STICKY	(2 * (1 << PERMBITS) * sizeof(permstrings[0]))
#ifdef	_SYSV
# define PERM_LOCK	(3 * (1 << PERMBITS) * sizeof(permstrings[0]))
#endif

static char permstrings[][PERMBITS + 1] = {	/* permission strings */
	      "---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx"
	     ,"--S", "--s", "-wS", "-ws", "r-S", "r-s", "rwS", "rws"
	     ,"--T", "--t", "-wT", "-wt", "r-T", "r-t", "rwT", "rwt"
#ifdef	_SYSV
	     ,"--l", "--s", "-wl", "-ws", "r-l", "r-s", "rwl", "rws"
#endif
};

/* Symbolic link indicator. */

char linkpointer[] = LINKPOINTER;/* common link pointer string */
#define ARROW		(linkpointer)
#define ARROWLENGTH	(sizeof(ARROW)-1)

/* External Variables. */

extern int optind;		/* next argument */
extern int errno;		/* error code */
extern int sys_nerr;		/* errors */
extern char *sys_errlist[];	/* errors by name */

/* Forward declarations. */

void acrosspage P((DIRENTRY *, DIRLIST *, NAMESTATS *));/* list across */
void downpage P((DIRENTRY *, DIRLIST *, NAMESTATS *));	/* list down */
void streamoutput P((DIRENTRY *, DIRLIST *));	/* stream output */
void dolsopts P((void));	/* do environment variable */
void docmdname P((char *));	/* do command name aliasing */
void parse P((int, char **));	/* parse argument list */
void setflag P((int));		/* set flag */
void checkflag P((int, char *));/* check flag for incompatibilies */
void init P((void));		/* initialise globals */
void initpath P((DIRENTRY *));	/* initialise pathname */
void initcols P((void));	/* initialise columns */
int stringlength P((CONST char *));/* length of string with feeling */
void printstring P((char *));	/* print string without feeling */
void bprintstring P((char *));	/* print string with feeling */
void qprintstring P((char *));	/* print string with feeling */
unsigned long bytestoblk P((struct stat *));	/* convert bytes to blks */
void error P((char *));		/* error message with prefix */
FILESYSTEM *scanmtab P((void));	/* create file system database */
void date P((time_t));		/* make date readable */
char *makepath P((DIRENTRY *));	/* form the path */
void longprint P((struct stat *));	/* long format */
void printentry P((DIRENTRY *, DIRENTRY *, int, int));	/* print this entry */
int optcols P((DIRLIST *, NAMESTATS *, DIRENTRY **, int **));
				/* optimise number of columns */
DIRENTRY *newentry P((char *));	/* malloc a new entry */
void freelist P((DIRLIST *));	/* free entries */
void freeentry P((DIRENTRY *));	/* free an entry */
int alphacmp P((DIRENTRY *, DIRENTRY *));/* alphabetic comparison */
int mtimecmp P((DIRENTRY *, DIRENTRY *));/* compare by modification time */
int ctimecmp P((DIRENTRY *, DIRENTRY *));/* compare by change time */
int atimecmp P((DIRENTRY *, DIRENTRY *));/* compare by access time */
void list P((DIRENTRY *, DIRLIST *, NAMESTATS *));/* file listing routine */
void suffix P((DIRENTRY *, int));	/* do suffixes */
int filestat P((int, statfunc, char *, struct stat *));	/* get status of file */
void *safemalloc P((size_t));	/* malloc with feeling */
DIRENTRY *makelist P((char *, NAMESTATS *,
	       unsigned long *));	/* create list of entries */
void *lsort P((void *, int, cmpfunc));	/* linked list sort */

#ifdef	S_IFLNK
char *followlink P((char *, int *));	/* follow symbolic link */
#endif

/* Uninitialised Globals. */

DIRLIST rlist;			/* temporary recursive list */
DIRLIST dlist;			/* list of directories */
time_t today;			/* today's date */
cmpfunc compare;		/* sort criteria */
strlenfunc strlength;		/* (visible) string length */
putstrfunc putstring;		/* (visible) string output */
int columns;			/* number of columns */
char *pathname;			/* current path name */
FILESYSTEM *filesystems;	/* file system data base */
FILESYSTEM *cfs;		/* current file system */

/* Initialised Globals. */

unsigned char flags[BITCHARS(FLAGS)] = { 0 }; /* switches */
int exitstatus = EXIT_SUCCESS;                /* exit status */
char cwd[2 * PATH_MAX + 1] = {0};             /* cwd and current pathname */
int extrawidth = INTERSPACE;	/* implied extra space to add */
char blanks[] =			/* blanks for quick padding */
"                                                               ";
char *incompatible[] = {	/* mutually exclusive flags */
		"aA", "mx1Cglno", "bq", "pF", "cu", (char *) NIL
};
char optlist[] = "abcdfgilmnopqrstux01ACFLRS";/* list of options */
char *summary[] = {		/* summary of options */
"",
"a  All files             A  All except . and ..   R  Recursive",
"d  No directory scan     f  Force directory scan  L  Opaque symlink",
"0  Reset options",
"",
"l  Long format           g  Print group only      o  Print owner only",
"x  Multicolumn across    C  Multicolumn down      S  Squeeze columns",
"1  Single column         m  Stream format",
"",
"t  Sort by time          r  Reverse sort",
"c  Status change time    u  Last access time",
"",
"i  Print inode number    s  Print size in blocks  n  Numeric uid and gid",
"p  Suffix /              F  Suffix / @ = | *",
"",
"b  Non-graphic \\ddd      q  Non-graphic ?",
(char *) NIL
};

int main F2(int, argc, char **, argv)
{
  DIRENTRY *dp;			/* directory scanner */
  DIRLIST nlist;		/* list of files */
  int showdir;			/* print directory name */
  int notfirst;			/* printing first directory */
  BOOL(dototal);		/* show totals */
  BOOL(dostat);			/* need to do a stat */
  int isdir;			/* current argument is a directory name */
  int symlinked;		/* file a symbolic link */
  char *absname;		/* absolute current path name */
  NAMESTATS ns;			/* statistics */

  unsigned long blk;		/* blks in total */
#ifdef	S_IFLNK
  struct stat lsb;		/* stat of link */
#endif

/* Initialise by emptying the directory lists */
  EMPTY_dl(dlist), EMPTY_dl(nlist);
  showdir = 0;
  notfirst = 0;

/* Parse the command line */
  dolsopts();
  docmdname(argv[0]);
  parse(argc, argv);
  init();
  if (TEST('C') || TEST('x') || TEST('m')) initcols();

/* Insert arguments into the current list */
  INIT_NS(ns);
  do {

	dp = newentry((optind >= argc) ? DEFAULTDIR : argv[optind]);
	symlinked = LF_NORMAL;

	if (EVAL(dostat, (!TEST('f') && !TEST('d')) ||
			  TEST('t') ||  TEST('g') ||
			  TEST('o') ||  TEST('s') ||
			  TEST('F') ||  TEST('p') ||
			  TEST('R') ||  TEST('i'))) {
		dp->status = (struct stat *)
			     safemalloc(sizeof(*dp->status));
		if (filestat(0, OPAQUESTAT, dp->name, dp->status)) {
			freeentry(dp);
			continue;
		}
	}

/* Determine if this argument is a directory */
	if (TEST('f'))
		isdir = 1;

	else if (TEST('d'))
		isdir = 0;

	else {

#ifdef	S_IFLNK
		if (!TEST('L') && S_ISLNK(dp->status->st_mode) &&
		    !filestat(1, TRANSPARENTSTAT, dp->name, &lsb) &&
		    S_ISDIR(lsb.st_mode)) {
			symlinked = LF_SYMLINK;
			*dp->status = lsb;
		}
#endif

		if (!S_ISDIR(dp->status->st_mode))
			isdir = 0;
		else {
			if (compare == (cmpfunc) alphacmp) {
				free((FREE_T) dp->status);
				dp->status = NIL;
			}
			isdir = 1;
		}

	}
	if (isdir) {
		if (HEAD_dl(dlist)) showdir = 1;
		APPEND_dl(dlist, dp);

		continue;
	}

/* This argument is not a directory */
	suffix(dp, symlinked | LF_BADCWD);
	UPDATE_NS(ns, dp->length);
	APPEND_dl(nlist, dp);

  } while (++optind < argc);

/* List all the non-directory files */
  if (ns.n) {
	showdir = 1;
	notfirst = 1;
  }
  HEAD_dl(nlist) = (DIRENTRY *) lsort((void *) HEAD_dl(nlist),
				      LINKOFFSET, compare);
  list((DIRENTRY *) NIL, &nlist, &ns);

/* List all directories */
  if (!TEST('f'))
	HEAD_dl(dlist) = (DIRENTRY *) lsort((void *) HEAD_dl(dlist),
					    LINKOFFSET, compare);

  dp = HEAD_dl(dlist);
  initpath(dp);
  while (dp) {
	freelist(&nlist);
	absname = makepath(dp);

	if (showdir) {
		if (notfirst) (void) putchar('\n');
		(void) printf("%s:\n", pathname);
	}
	HEAD_dl(nlist) = makelist(absname, &ns, &blk);

	if (EVAL(dototal, TEST('g') || TEST('o') || TEST('s')))
		(void) printf("total %lu\n", blk);

	list(dp, &nlist, &ns);

	notfirst = showdir = 1;
	dp = dp->next;
  }

  return exitstatus;
}

/* Scan the options from the environment variable. This is
 * done in a similar fashion to the command line option
 * scan but no errors are flagged.
 */

void dolsopts F0()
{
  register char *env;		/* environment options */
  register int sw;		/* switch from environment */

/* Output to tty allows environment settable options */
  if (isatty(fileno(stdout))) {
	setflag('q');
	if ((env = getenv(ENVNAME)) != NIL) {
		while ((sw = *env) > 0 && sw < FLAGS) {
			setflag(sw);
			env++;
		}
	}
  }
}

/* Look at the command name. The following options are set
 * according to the name:
 *
 *	Name		Option
 *	ls		NONE
 *	l		-m
 *	ll		-l
 *	lx		-x
 *	lc		-C
 *	lf		-F
 *	lr		-R
 */

void docmdname F1(char *, name)
{
  register char *bp;		/* point to basename */

  if ((bp = strrchr(name, '/')) == NIL)
	bp = name;
  else
	bp++;

  if (bp[0] != 0) {
	switch (bp[1]) {
	    case '\0':	setflag('m');	break;
	    case 'l':	setflag('l');	break;
	    case 'x':	setflag('x');	break;
	    case 'c':	setflag('C');	break;
	    case 'f':	setflag('F');	break;
	    case 'r':	setflag('R');	break;
	}
  }
}

/* Determine the number of columns on the output terminal.
 * The environment variable COLUMNS is preferred. If this
 * is not set, and output is to a terminal, the termcap database
 * is consulted. Failing this, a default assumed.
 */
void initcols F0()
{
  register char *env;		/* environment variable */
  register int cols;		/* columns */
#ifdef	TERMCAP
  char tspace[1024];		/* termcap work space */
#endif

/* Check environment first */
  if ((env = getenv(COLNAME)) != NIL && (cols = atoi(env)) > 0)
	columns = cols;

#ifdef	TERMCAP
/* Try termcap library */
  else if ((env = getenv(TERMNAME)) != NIL &&
	 tgetent(tspace, env) > 0 &&
	 (cols = tgetnum("co")) > 0)
	columns = cols;
#endif

  else
	columns = COLUMNS;
}

/* Parse the command line arguments. This function will set
 * the switches in the global variable flags. No interpretation
 * of the switches is performed at this point.
 */

void parse F2(int, argc, char **, argv)
{
  register int swch;		/* switch character */
  register int i;		/* index */

  while ((swch = getopt(argc, argv, optlist)) != EOF) {
  	if (swch == '0') {
  		for (i = 0; i < sizeof(flags)/sizeof(flags[0]); i++)
  			flags[i] = 0;
  	} else if (swch != '?')
		setflag(swch);
	else {
		fprintf(stderr, "Usage: %s [-%s] [names]\n", argv[0], optlist);
		for (i = 0; summary[i] != NIL; i++)
			fprintf(stderr, "%s\n", summary[i]);
		exit(EXIT_FAILURE);
	}
  }
}

/* Set the specified option switch. This function knows about
 * mutually exclusive options and will turn off options which
 * are not compatible with the current one.
 */

void setflag F1(int, ch)
{
  register char **p;		/* scanner */

  for (p = incompatible; *p != NIL; p++) checkflag(ch, *p);
  SET(ch);
}

/* Check the specified flag against the list of mutually exclusive
 * flags. If the flag appears, then all other flags in the list are
 * turned off. If not, then nothing is done.
 */

void checkflag F2(int, ch, register char *, p)
{
  if (strchr(p, ch) != NIL) {
	while (*p != 0) {
		if (*p++ != ch) CLEAR(p[-1]);
	}
  }
}

/* Scan the switches and initialise globals. This function will
 * do some preliminary work on the switches to determine which
 * globals will be needed and also which switches need to be
 * set or cleared in addition to the current settings.
 */

void init F0()
{
/* Turn on -A if we're the super user */
  if (!TEST('a') && getuid() == SUPER_USER) SET('A');

/* Visible string length */
  strlength = TEST('b') ? stringlength : (strlenfunc) strlen;

/* Visible string output */
  putstring = TEST('q') ? qprintstring : TEST('b') ? bprintstring : printstring;

/* Force raw directory listing */
  if (TEST('f')) {
	CLEAR('l');
	CLEAR('n');
	CLEAR('o');
	CLEAR('g');
	CLEAR('t');
	CLEAR('s');
	CLEAR('r');
	CLEAR('F');
	CLEAR('p');
	CLEAR('A');
	SET('a');
  }

/* Sort criterion */
  compare = (cmpfunc) (TEST('t') ? TEST('u') ? atimecmp
		                             : TEST('c') ? ctimecmp : mtimecmp
		                 : alphacmp);

/* Open the password and group files if required */
  if (TEST('l') || TEST('n')) {
	SET('o');
/* Use -DAST to suppress groups on ls -l  (V7 style). */
#ifndef AST
	SET('g');
#endif
  }

/* Accumulate extra width if required */
  if (TEST('i')) extrawidth += INODEWIDTH + 1;
  if (TEST('s')) extrawidth += BLOCKWIDTH + 1;

/* Get today's date */
  if (TEST('o') || TEST('g')) today = time((time_t *) 0);

/* Initialise file system data base */
  if (TEST('g') || TEST('o') || TEST('s'))
	cfs = filesystems = scanmtab();
}

/* Form the name of the current directory. Before each directory
 * is scanned, its absolute name is formed. The name of the
 * current directory is prefixed to relative names to obtain
 * this.
 */
void initpath F1(DIRENTRY *, dp)
{
  if (dp == NIL || (dp->next == NIL && !TEST('R')))
	pathname = cwd;
  else {
	if (getcwd(cwd, sizeof(cwd)) == NIL) {
	      error("cannot locate cwd");
	      exit(EXIT_FAILURE);
	}
	pathname = strchr(cwd, 0);
	pathname[0] = '/';
	pathname[1] = 0;
	pathname++;
  }
}

/* Make a linked list of entries using specified directory. The
 * directory is rewound before being scanned. The function
 * returns a pointer to the head of the list of entries. The
 * function gathers two important statistics as the list
 * is created. It will return the width required to print
 * the files, and also the number of files in the list.
 *
 * The list will be sorted according to the current sorting
 * criteria.
 */

DIRENTRY *makelist F3(char *, dirname, NAMESTATS *, np, unsigned long *, blk)
{
  DIR *dirp;			/* directory to scan */
  register struct dirent *cp;	/* current entry */
  DIRLIST nlist;		/* files in directory */
  register DIRENTRY *p;		/* new entry */
  BOOL(dostat);			/* perform a stat */
  BOOL(doblock);		/* check block sizes */

  EMPTY_dl(nlist);
  INIT_NS(*np);
  *blk = 0;

  if ((dirp = opendir(dirname)) == NIL || chdir(dirname)) {
	error(dirname);
	return NIL;
  }
  while ((cp = readdir(dirp)) != NIL) {
	if (cp->d_name[0] != '.' || TEST('a') ||
	    (cp->d_name[1] != 0 && (cp->d_name[1] != '.' || cp->d_name[2] != 0)
	     && TEST('A'))) {

		p = newentry(cp->d_name);

		if (EVAL(dostat, TEST('t') || TEST('g') ||
				 TEST('o') || TEST('s') ||
				 TEST('F') || TEST('p') ||
				 TEST('R') || TEST('i'))) {
			p->status = (struct stat *)
				    safemalloc(sizeof(*p->status));
			if (filestat(0, OPAQUESTAT, p->name, p->status)) {
				freeentry(p);
				continue;
			}
		}
		suffix(p, LF_NORMAL);

		UPDATE_NS(*np, p->length);

		if (EVAL(doblock, TEST('g') || TEST('o') || TEST('s')))
			*blk += bytestoblk(p->status);

		APPEND_dl(nlist, p);
	}
  }

#ifndef		_MINIX
  (void) closedir(dirp);
#else
  if (closedir(dirp)) error(dirname);
#endif

  return TEST('f') ? HEAD_dl(nlist)
	: (DIRENTRY *) lsort((void *) HEAD_dl(nlist),
			     LINKOFFSET, compare);
}

/* This function performs does the formatting and output for ls(1).
 * The function is passed a list of files (not necessarily sorted)
 * to list. All files will be listed. . and .. removal should have
 * been done BEFORE calling this function.
 */

void list F3(DIRENTRY *, parent, DIRLIST *, lp, NAMESTATS *, np)
{
  if (np->n) {

/* Empty recursive directory list */
	EMPTY_dl(rlist);

/* Select the correct output format */
	if (TEST('m'))
		streamoutput(parent, lp);
	else if (!TEST('C'))
		acrosspage(parent, lp, np);
	else
		downpage(parent, lp, np);

	(void) putchar('\n');

/* Check recursive list */
	if (HEAD_dl(rlist)) {
		if (!TEST('f'))
			HEAD_dl(rlist) = (DIRENTRY *)
					 lsort((void *) HEAD_dl(rlist),
					       LINKOFFSET, compare);
		TIE_dl(dlist, HEAD_dl(rlist));
	}
  }
}

/* List the entries across the page. Single column output is
 * treated as a special case of this. This is the easiest
 * since the list of files can be scanned and dumped.
 */

void acrosspage F3(DIRENTRY *, parent, DIRLIST *, lp, NAMESTATS *, np)
{
  register DIRENTRY *p;		/* entries to print */
  int cols;			/* columns to print in */
  int *cw;			/* column widths */
  register int colno;		/* which column we're printing */

  if (!TEST('x')) {
    cols = 1;
    cw = NIL;
  } else {
    cols = optcols(lp, np, (DIRENTRY **) NIL, &cw);
  }

  for (colno = 0, p = HEAD_dl(*lp); p; p = p->next) {
	if (++colno > cols) {
		colno = 1;
		(void) putchar('\n');
	}
	printentry(parent, p, cw ? cw[colno-1] : np->maxwidth, colno != cols);
  }
}

/* Print the entries down the page. This is the default format
 * for multicolumn listings. It's unfortunate that this is
 * the most acceptable to the user, but it causes the program
 * a little grief since the list of files is not in the
 * most convenient order. Most of this code is taken up
 * with rearranging the list to suit the output format.
 */

void downpage F3(DIRENTRY *, parent, DIRLIST *, lp, NAMESTATS *, np)
{
  static DIRENTRY **c = NIL;	/* column pointers */
  int *cw;			/* column width vector */
  int cols;			/* columns to print in */
  int rows;			/* number of rows per column */
  register int i, j;		/* general counters */

  if (c == NIL) {
	cols = (columns+INTERSPACE-1) / INTERSPACE;
	c = (DIRENTRY **) safemalloc(sizeof(*c) * cols);
  }

  cols = optcols(lp, np, c, &cw);
  rows = (np->n + cols - 1) / cols;

/* Scan and print the listing */
  for (i = 0; i < rows; i++) {
	if (i) (void) putchar('\n');

	for (j = 0; j < cols; j++) {
		if (c[j]) {
			printentry(parent, c[j],
				   cw ? cw[j] : np->maxwidth, j != cols - 1);
			c[j] = c[j]->next;
		}
	}
  }
}

/* List the files using stream format. This code looks a bit
 * kludgy because it is necessary to KNOW how wide the next
 * entry is going to be. If the width would case the printout
 * to exceed the width of the display, the entry must be printed
 * on the next line.
 */

void streamoutput F2(DIRENTRY *, parent, DIRLIST *, lp)
{
  register DIRENTRY *p;		/* entries to print */
  int colno;			/* current column */
  int midline;			/* in middle of line */
  register int tw;		/* width of this entry */
  int x;			/* inode calculation */
  BOOL(dopretty);		/* pretty print */
  unsigned long blk;		/* size in blks */

  for (midline = colno = 0, p = HEAD_dl(*lp); p; p = p->next) {

/* Nominal length of the name */
	tw = p->length;

/* Pretty printing adds an extra character */
	if (EVAL(dopretty, TEST('F') || TEST('p')) &&
	    (S_ISDIR(p->status->st_mode) ||
	     (TEST('F') && (
#ifdef	S_IFLNK
			   S_ISLNK(p->status->st_mode) ||
#endif
#ifdef	S_IFIFO
			   S_ISFIFO(p->status->st_mode) ||
#endif
#ifdef	S_IFSOCK
			   S_ISSOCK(p->status->st_mode) ||
#endif
			   (p->status->st_mode & 0111) != 0))))
		tw++;

/* Size will add to the length */
	if (TEST('s')) {
		blk = bytestoblk(p->status);
		do {
			tw++;
		} while ((blk /= 10) != 0);
		tw++;
	}

/* Inode number adds to the length (plus the separating space) */
	if (TEST('i')) {
		x = p->status->st_ino;
		tw++;
		do
			tw++;
		while ((x /= 10) != 0);
	}

/* There will be a separating comma */
	if (p->next) tw++;

/* There will be a separating space */
	if (midline) tw++;

/* Begin a new line if necessary in which case there is no separating space */
	if (colno + tw >= columns) {
		(void) putchar('\n');
		midline = 0;
		colno = 0;
		tw--;
	}

/* Separate entries if required */
	if (midline) (void) putchar(' ');

/* Now the entry proper */
	printentry(parent, p, 0, 0);

/* Now the separating comma */
	if (p->next) (void) putchar(',');

	midline = 1;
	colno += tw;
  }
}

/* Optimise the number of columns taken to print listing.
 * Attempt to squeeze as many columns across the page as
 * possible. Return a pointer to a width vector and also
 * the number of columns to print. If the -S option is
 * not in effect, the number of columns returned might
 * be overoptimistic.
 */

int optcols F4(DIRLIST *, lp, NAMESTATS *, np, DIRENTRY **, c, int **, cwp)
{
  static int *cw = NIL;		/* column widths */
  int ccw;			/* width of current column */
  int scw;			/* sum of column widths */
  int cols;			/* column count */
  int rows;			/* row count */
  register DIRENTRY *p;		/* list scanner */
  register int i, j;

  if (! TEST('S')) {

/* Nominal number of columns */
	if ((cols = columns / (np->maxwidth + extrawidth)) == 0)
		cols = 1;
	*cwp = NIL;

  } else {

/* Allocate space for column widths */
	if (cw == NIL) {
		cols = (columns+INTERSPACE-1) / INTERSPACE;
		cw = (int *) safemalloc(sizeof(*cw) * cols);
	}

/* Allocate maximum number of columns */
	if ((cols = columns / (np->minwidth + extrawidth)) == 0)
		cols = 1;
	*cwp = cw;
  }

  if (cols > np->n) cols = np->n;

/* Distribute the names and compute the column widths */
  if (c) {
	while (1) {
		rows = (np->n + cols - 1) / cols;
		cols = (np->n + rows - 1) / rows;

		scw = cols * extrawidth;
		for (i = 0, p = HEAD_dl(*lp); p; i++) {
			c[i] = p;
			ccw = p->length;
			for (j = rows; j-- && p; p = p->next)
				if (ccw < p->length) ccw = p->length;
			if ((scw += ccw) > columns)
				if (cols > 1) break;
			if (cw) cw[i] = ccw;
		}
		if (cols <= 1 || scw <= columns) break;
		cols--;
	}
  } else if (TEST('S')) {
	while (1) {
		for (i = cols; i--; )
			cw[i] = 0;
		scw = cols * extrawidth;
		for (i = 0, p = HEAD_dl(*lp); p; p = p->next) {
			if (cw[i] < p->length) {
				scw -= cw[i];
				cw[i] = p->length;
				if ((scw += cw[i]) > columns)
					if (cols > 1) break;
			}
			if (++i == cols) i = 0;
		}
		if (cols <= 1 || scw <= columns) break;
		cols--;
	}
  }

  return cols;
}

/* Print this entry on stdout. No newline is appended to the
 * output. This function localises all the transformations
 * which have to be carried out to make the output readable.
 * Columnar magic is done elsewhere.
 */

void printentry F4(DIRENTRY *, parent,
	    register DIRENTRY *, p,
	    int, w,
	    register int, padout)
{
  int pad;			/* pad count */
  BOOL(dolong);			/* long print */
  DIRENTRY *ep;			/* new entry for recursion */

  if (TEST('i')) {
	if (sizeof(p->status->st_ino) == sizeof(unsigned int))
	       printf("%*u ",  w ? INODEWIDTH : 0,
			         (unsigned int)  p->status->st_ino);
	else
	       printf("%*lu ", w ? INODEWIDTH : 0,
			         (unsigned long) p->status->st_ino);
  }
  if (TEST('s'))
	(void) printf("%*lu ", w ? BLOCKWIDTH : 0, bytestoblk(p->status));

  if (EVAL(dolong, TEST('o') || TEST('g'))) longprint(p->status);

/* Print the name of this file */
  (*putstring)(p->name);
  (*putstring)(p->suffix);

/* Only pad it if the caller requires it */
  if (padout && (pad = w - p->length + INTERSPACE) > 0) {
	do {
		padout = pad;
		if (padout > sizeof(blanks)-1) padout = sizeof(blanks)-1;
		(void) fputs(&blanks[(sizeof(blanks)-1)-padout], stdout);
	} while ((pad -= padout) != 0);
  }

/* If recursion is required, add to list if it's a directory */
  if (TEST('R') && S_ISDIR(p->status->st_mode) &&
      (p->name[0] != '.' ||
       (p->name[1] != 0 && (p->name[1] != '.' || p->name[2] != 0)))) {
	ep = newentry(p->name);
	ep->parent = parent;
	APPEND_dl(rlist, ep);
  }
}

/* Format and print out the information for a long listing.
 * This function handles the conversion of the mode bits
 * owner, etc. It assumes that the status has been obtained.
 */

void longprint F1(register struct stat *, sp)
{
  register unsigned int permbits;	/* file access permissions */
  char filecode;		/* code for type of file */
  static struct passwd *pwent = NIL;	/* password entry */
  static struct group *grent = NIL;	/* group entry */
  char *perm[PERMCLASSES];	/* permissions ogu */

  if (S_ISREG(sp->st_mode))       filecode = '-';
  else if (S_ISDIR(sp->st_mode))  filecode = 'd';
  else if (S_ISBLK(sp->st_mode))  filecode = 'b';
  else if (S_ISCHR(sp->st_mode))  filecode = 'c';
#ifdef		S_IFIFO
  else if (S_ISFIFO(sp->st_mode)) filecode = 'p';
#endif
#ifdef		S_IFLNK
  else if (S_ISLNK(sp->st_mode))  filecode = 'l';
#endif
#ifdef		S_IFSOCK
  else if (S_ISSOCK(sp->st_mode)) filecode = 's';
#endif
  else                            filecode = '?';

  permbits = USRGRPOTH(sp->st_mode);

  perm[PERMUSR] = permstrings[(permbits >> 6) & 7];
  perm[PERMGRP] = permstrings[(permbits >> 3) & 7];
  perm[PERMOTH] = permstrings[(permbits)      & 7];

  if (sp->st_mode & S_ISUID) perm[PERMUSR] += PERM_SUID;
  if (sp->st_mode & S_ISGID)
#ifdef	_SYSV
	perm[PERMGRP] += PERM_LOCK;
#else				/* _SYSV */
	perm[PERMGRP] += PERM_SUID;
#endif
#ifdef	S_ISVTX
  if (sp->st_mode & S_ISVTX) perm[PERMOTH] += PERM_STICKY;
#endif

  (void) printf("%c%s%s%s%c %2d ",
	      filecode,
	      perm[PERMUSR], perm[PERMGRP], perm[PERMOTH],
	      ' ', sp->st_nlink);

  if (TEST('o')) {
	if (!TEST('n') && ((pwent && pwent->pw_uid == sp->st_uid) ||
			   (pwent = getpwuid(sp->st_uid)) != NIL))
		(void) printf("%-8.8s\t", pwent->pw_name);
	else
		(void) printf("%-8d\t", (int) sp->st_uid);
  }
  if (TEST('g')) {
	if (!TEST('n') && ((grent && grent->gr_gid == sp->st_gid) ||
			   (grent = getgrgid(sp->st_gid)) != NIL))
		(void) printf("%-8.8s\t", grent->gr_name);
	else
		(void) printf("%-8d\t", (int) sp->st_gid);
  }

/* Now show how big the file is */
  if (!S_ISCHR(sp->st_mode) && !S_ISBLK(sp->st_mode))
	(void) printf("%7lu ", (unsigned long) sp->st_size);
  else
	(void) printf(" %2d,%3d ", (int) (sp->st_rdev >> 8) & 0377,
		      (int) sp->st_rdev & 0377);

/* Now the date */
  date(TEST('u') ? sp->st_atime : TEST('c') ? sp->st_ctime : sp->st_mtime);
}

/* Given the time convert it into human readable form. The month and
 * day are always printed. If the time is within about the last half year,
 * the hour and minute are printed, otherwise the year.
 */

void date F1(time_t, t)
{
  struct tm *tmbuf;		/* output time */

  tmbuf = localtime(&t);
  (void) printf("%.3s %2u ",
	 "JanFebMarAprMayJunJulAugSepOctNovDec" + tmbuf->tm_mon * 3,
	      (unsigned) tmbuf->tm_mday);
  if (t <= today && (today - t) <= HALFYEAR)
	(void) printf("%02u:%02u ", (unsigned) tmbuf->tm_hour,
		      (unsigned) tmbuf->tm_min);
  else
	(void) printf("%5d ", tmbuf->tm_year + BASEYEAR);
}

/* Chase the parent pointers and make a path. This path is
 * used to locate the current directory. The function returns
 * the absolute pathname to the caller. The pathname relative
 * to the directory specified by the user is available from
 * the global variable pathname.
 */

char *makepath F1(DIRENTRY *, p)
{
  char tmppath[PATH_MAX + 1];	/* build it here */
  register char *cp;		/* pointer to tmppath */
  register char *cq;		/* pointer into name */
  char *cr;			/* temporary pointer */
  char absolute;		/* / for absolute file names */

/* Work your way back up to the root */
  for (cp = tmppath, *cp++ = 0, absolute = 0; p; p = p->parent) {
	for (cq = p->name, absolute = *cq; *cq; *cp++ = *cq++);
	*cp++ = 0;
  }

/* Now flip the order */
  for (cq = pathname, --cp; ; cp = cr) {
	while (*--cp);
	for (cr = cp++; *cp; *cq++ = *cp++);
	if (cr == tmppath) break;
	if (cq[-1] != '/') *cq++ = '/';
  }
  *cq = 0;
  return absolute == '/' ? pathname : cwd;
}

/* Allocate memory for a new entry. Memory is allocated and
 * filled in. The next, parent and status pointers are set
 * to null. The function returns a pointer to the new entry.
 */

DIRENTRY *newentry F1(char *, name)
{
  register DIRENTRY *p;		/* pointer to entry */

  p = (DIRENTRY *) safemalloc(sizeof(*p));
  p->name = (char *) safemalloc(strlen(name) + 1);
  (void) strcpy(p->name, name);
  p->suffix = NIL;
  p->next = NIL;
  p->parent = NIL;
  p->status = NIL;
  return p;
}

/* Free the memory associated with list of elements. The function
 * assumes all memory has been allocated using malloc(), so that
 * free() will work without suffering a heart attack. The list
 * header is set to null before returning.
 */

void freelist F1(DIRLIST *, lp)
{
  register DIRENTRY *ep, *nep;

  for (ep = HEAD_dl(*lp); ep; ep = nep) {
	nep = ep->next;
	freeentry(ep);
  }
  EMPTY_dl(*lp);
}

/* Free the memory associated with a directory entry. Remember that
 * all the memory should have been allocated using malloc().
 */

void freeentry F1(register DIRENTRY *, p)
{
  if (p) {
	if (p->name) free((FREE_T) p->name);
	if (p->suffix && p->suffix[0] != 0 && p->suffix[1] != 0)
		free((FREE_T) p->suffix);
	if (p->status) free((FREE_T) p->status);
	free((FREE_T) p);
  }
}

/* Compare entries in the file list. Pointers to two entries are
 * expected as arguments (non null pointers). Compare the entries
 * and return -1, 0 or 1 depending on whether the first argument
 * is less than, equal to or greater than the second.
 */

int alphacmp F2(DIRENTRY *, p, DIRENTRY *, q)
{
  register int rv = strcmp(p->name, q->name);

  return TEST('r') ? -rv : rv;
}

/* Compare entries on the basis of access time. Pointers to
 * two entries are expected as arguments. It is assumed that the status
 * has been obtained. The routine will return -1, 0 or 1 depending
 * on whether the first argument is later than, equal to or earlier
 * than the second.
 */

int atimecmp F2(DIRENTRY *, p, DIRENTRY *, q)
{
  register int rv;		/* comparison result */
  long delta = p->status->st_atime - q->status->st_atime;

  rv = delta > 0 ? -1 : delta ? 1 : 0;

  return TEST('r') ? -rv : rv;
}

/* Compare entries on the basis of status change time. Pointers to
 * two entries are expected as arguments. It is assumed that the status
 * has been obtained. The routine will return -1, 0 or 1 depending
 * on whether the first argument is later than, equal to or earlier
 * than the second.
 */

int ctimecmp F2(DIRENTRY *, p, DIRENTRY *, q)
{
  register int rv;		/* comparison result */
  long delta = p->status->st_ctime - q->status->st_ctime;

  rv = delta > 0 ? -1 : delta ? 1 : 0;

  return TEST('r') ? -rv : rv;
}

/* Compare entries on the basis of modification time. Pointers to
 * two entries are expected as arguments. It is assumed that the status
 * has been obtained. The routine will return -1, 0 or 1 depending
 * on whether the first argument is later than, equal to or earlier
 * than the second.
 */

int mtimecmp F2(DIRENTRY *, p, DIRENTRY *, q)
{
  register int rv;		/* comparison result */
  long delta = p->status->st_mtime - q->status->st_mtime;

  rv = delta > 0 ? -1 : delta ? 1 : 0;

  return TEST('r') ? -rv : rv;
}

/* Append file name suffix. The suffix can be a simple file type
 * indicator or can be the full path name if it is a symbolic
 * link. If LF_SYMLINK is set, the entry is assumed to be a symbolic
 * link. In this case, no further stat is performed and the entry
 * in p->status is assumed to be that of the real file (not the link).
 * If LF_BADCWD is set, the current working directory may not be
 * correct for parsing the symbolic link. In this case, cwd[] and
 * pathname are used as a scratch area for computing the correct name.
 */

void suffix F2(register DIRENTRY *, p, int, linkflags)
{
  char *type;			/* file type */
  BOOL(showtype);		/* show file type */
#ifdef		S_IFLNK
  BOOL(dolink);			/* follow link */
  char *link;			/* link to */
  char *u, *v, *w;		/* link name processing pointers */
  int ltextlen;			/* length of link text */
  struct stat lsb;		/* link stat buffer */
#endif

  p->length = (*strlength)(p->name);

#ifdef		S_IFLNK
  link = NIL;

  if (EVAL(dolink, TEST('o') || TEST('g')) &&
      ((linkflags & LF_SYMLINK) != 0 || S_ISLNK(p->status->st_mode))) {

	if ((link = followlink(p->name, &ltextlen)) != NIL) {
	    p->length += ARROWLENGTH + (*strlength)(link);
	    p->suffix = (char *) safemalloc(ltextlen + ARROWLENGTH + 2);

	    u = link;

	    if ((linkflags & LF_BADCWD) != 0 && link[0] != '/') {
	    	for (u = w = cwd, v = p->name; (*u = *v) != 0; u++, v++)
		    if (*u == '/')
			w = u+1;
		for (u = w, v = link; (*u = *v) != 0; u++, v++)
		    continue;
		u = cwd;
	    }

	    if ((linkflags & LF_SYMLINK) == 0 && !TEST('L') &&
		!filestat(1, OPAQUESTAT, u, &lsb)) {
		linkflags |= LF_SYMLINK;
		*p->status = lsb;
	    }

	    if ((linkflags & LF_SYMLINK) != 0) {
		(void) strcpy(p->suffix, ARROW);
		(void) strcpy(p->suffix+ARROWLENGTH, link);
	    }
	}
  }
#endif

  type = "";
  if (EVAL(showtype, TEST('F') || TEST('p'))) {
	if (S_ISDIR(p->status->st_mode))
		type = "/";
	else if (TEST('F')) {
		if (p->status->st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) type = "*";
#ifdef		S_IFIFO
		if (S_ISFIFO(p->status->st_mode)) type = "|";
#endif
#ifdef		S_IFLNK
		if (S_ISLNK(p->status->st_mode)) type = "@";
#endif
#ifdef		S_IFSOCK
		if (S_ISSOCK(p->status->st_mode)) type = "=";
#endif
	}
  }
  p->length += strlen(type);

#ifdef	S_IFLNK
  if (link == NIL)
	p->suffix = type;
  else if ((linkflags & LF_SYMLINK) != 0)
	strcpy(p->suffix+ARROWLENGTH+ltextlen, type);
  else {
	(void) strcpy(p->suffix, type);
	(void) strcat(p->suffix+1, ARROW);
	(void) strcat(p->suffix+1+ARROWLENGTH, link);
  }
#else
  p->suffix = type;
#endif
}

/* Follow a symbolic link and return the name of the file
 * to which it points. The function will return a pointer
 * to a static area.
 */

#ifdef		S_IFLNK
char *followlink F2(char *, name, register int *, len)
{
  static char linkto[PATH_MAX + 1];	/* link to name */

  if ((*len = readlink(name, linkto, sizeof(linkto) - 1)) < 0) {
	error(name);
	return NIL;
  }
  linkto[*len] = 0;
  return linkto;
}

#endif

/* Get the status of a file prepending the path before calling
 * stat. The function pointer should indicate which status function
 * to call. Return 0 if successful, non-zero if the name cannot
 * be found.
 */

int filestat F4(int, silent, statfunc, status, char *, name,
	 struct stat *, statbuf)
{
  if ((*status) (name, statbuf)) {
	if (!silent) {
		if (errno == ENOENT)
			(void) fprintf(stderr, "ls: %s not found\n", name);
		else
			error(name);
	}
	return 1;
  }
  return 0;
}

/* Compute the length of the string taking into account the
 * form of output. String lengths will increase if the
 * visible output flag has been specified.
 */

int stringlength F1(register CONST char *, s)
{
  register int i;		/* length */

  for (i = 0; *s != 0; s++)
	if (TEST('b') && !isprint(*s))
		i += 4;
	else
		i++;
  return i;
}

/* Print a string without any conversion.
 */

void printstring F1(char *, s)
{
  (void) fputs(s, stdout);
}

/* Print a string converting invisible characters into question marks.
 */

void qprintstring F1(register char *, s)
{
  for (; *s; s++) {
	if (isprint(*s))
		(void) putchar(*s);
	else
		(void) putchar('?');
  }
}

/* Print a string converting invisible characters into visible sequences.
 */

void bprintstring F1(register char *, s)
{
  for (; *s; s++) {
	if (isprint(*s))
		(void) putchar(*s);
	else
		(void) printf("\\%03d", *s & ((1 << CHAR_BIT) - 1));
  }
}

/* This function does a malloc() with feeling. If malloc() returns
 * null, indicating no more memory, the function will fail with
 * an error message. The function will return a pointer to the
 * allocated memory.
 */

void *safemalloc F1(size_t, n)
{
  register void *p;		/* malloc'ed region */

  if ((p = (void *) malloc(n)) == NIL) {
	(void) fputs("ls: insufficient memory\n", stderr);
	exit(EXIT_FAILURE);
  }
  return p;
}

/* Wrapper for perror to prefix error codes with program name.
 */

void error F1(char *, e)
{
  int err;			/* saved error code */

  err = errno;
  (void) fprintf(stderr, "ls: %s: ", e);
  errno = err;
  perror("");
}

/* This is a two way merge sort, but non-recursive and using a binary
 * comb to combine the sublists. The problem with the straightforward
 * two way merge is that switching the output from one bin to another
 * is time consuming. In this approach, bin[i] contains either zero,
 * or (1<<i) sorted elements (except for the last bin which can hold
 * any number of elements). When a bin overflows (it will always double
 * in size), it is emptied and merged with the next bin (hence the
 * doubling effect). This cascading is forced to stop at the last bin.
 * When all elements have been placed in bins, all bins are merged
 * yielding a sorted list. If the number of bins is too small, the
 * sort collapses, in the limit, into an insertion sort.
 *
 * The first is a pointer to the first element of the list.
 * The second argument is the byte offset from the element pointer to
 * find the pointer to the next element in the list. The third
 * argument is a pointer to a comparison function that returns -ve, zero
 * and +ve respectively if the first element is less than, equal to
 * or greater that the second element.
 */

#define NEXT(p)		(* (void **) ((char *) p + offset))

void *lsort F3(void *, head, int, offset, cmpfunc, cmp)
{
  register void *rp, *rq;		/* merge pointers */
  register void **lp;			/* merge insertion pointer */
  register void **bp;			/* list header scanner */
  void **ep;				/* last header bin */
  void *p;				/* next element */
  void *q;				/* current element */
  void *bin[9+1];			/* list headers */

  ep = &bin[sizeof(bin)/sizeof(bin[0])-1];
  for (bp = &bin[0]; bp <= ep; bp++)
	*bp = 0;

  for (p = head; p != 0; *bp = p, p = q) {
	q = NEXT(p);
	NEXT(p) = 0;

	for (bp = &bin[0]; ;bp++) {
	      if ((rp = *bp) != 0) {
		    rq  = p;
		    lp  = &p;
		    *bp = 0;

		    for (;;) {
		      if ((*cmp)(rp, rq) < 0) {
			*lp = rp;
			lp = &NEXT(rp);
			if ((rp = NEXT(rp)) == 0) {
			  *lp = rq;
			  break;
			}
		      } else {
			*lp = rq;
			lp = &NEXT(rq);
			if ((rq = NEXT(rq)) == 0) {
			  *lp = rp;
			  break;
			}
		      }
		    }
	      } else if (q != 0 || bp == ep)
		    break;
	}
	if (bp == ep) bp--;
  }
  return ep[-1];
}

/***********************************************************************\
 *                             Portable Code                           *
\***********************************************************************/
#endif

/***********************************************************************\
 *                            Unportable Code                          *
\***********************************************************************/
#ifdef	PASS2
/* Convert the size of a file (in bytes) to the number of
 * kilobytes of storage used. This figure will include the
 * number of indirect blocks used to store the file.
 * The Minix code was lifted from the original Minix ls.
 */

#ifdef		_MINIX
static unsigned long v1_zone_group[] = {
  (unsigned long) V1_NR_DZONES,
  (unsigned long) V1_INDIRECTS,
  (unsigned long) 0};
#if V1_NR_TZONES - V1_NR_DZONES != 2
  << Wrong number of version 1 indirects >>
#endif

#ifdef V2_NR_DZONES
static unsigned long v2_zone_group[] = {
  (unsigned long) V2_NR_DZONES,
  (unsigned long) V2_INDIRECTS,
  (unsigned long) V2_INDIRECTS*V2_INDIRECTS,
  (unsigned long) 0};
#if V2_NR_TZONES - V2_NR_DZONES != 3
  << Wrong number of version 2 indirects >>
#endif
#endif

static unsigned long *filestructure[] = {	/* file structure */
  v1_zone_group,			/* default version 1 */
  v1_zone_group,			/* version 1 */
#ifdef V2_NR_DZONES
  v2_zone_group,			/* version 2 */
#endif
};
#endif

unsigned long bytestoblk F1(struct stat *, sp)
{
#ifdef		_BSD
  return (sp->st_blocks * STD_BLK + BYTESPERBLK - 1) / BYTESPERBLK;
#endif

#ifdef		_SYSV
  return (sp->st_blocks * STD_BLK + BYTESPERBLK - 1) / BYTESPERBLK;
#endif

#ifdef		_MINIX
  unsigned long blocks;		/* accumulated blocks */
  unsigned long fileb;		/* size of file in blocks */
  unsigned int filetype;	/* type of file */
  unsigned long *fsp;		/* structure of filesystem for this file */
  int i, j;			/* zone table scanner */

#ifndef		USEMTAB
  fsp = filestructure[0];
#else
  static dev_t baddev = NO_DEV;	/* previous bad file system */
  extern FILESYSTEM *cfs;	/* current file system */
  extern FILESYSTEM *filesystems; /* list of file systems */

/* Locate the file system */
  if (cfs->dev == sp->st_dev)
	fsp = cfs->fs;
  else if (sp->st_dev == baddev)
	fsp = filestructure[0];
  else {
	for (cfs = filesystems; cfs != NIL && cfs->dev != sp->st_dev; )
		cfs = cfs->next;
	if (cfs != NIL)
		fsp = cfs->fs;
	else {
		(void) fprintf(stderr, "ls: device %d/%d not in /etc/mtab\n",
				       (sp->st_dev >> MAJOR) & BYTE,
				       (sp->st_dev >> MINOR) & BYTE);
		baddev = sp->st_dev;
		cfs = filesystems;
		fsp = filestructure[0];
	}
  }
#endif

/* Compute raw file size */
  fileb = ((unsigned long) sp->st_size + STD_BLK - 1) / STD_BLK;
  blocks = fileb;
  filetype = sp->st_mode;

/* Compute indirect block overhead */
  if (fileb > fsp[0] && !S_ISBLK(filetype) && !S_ISCHR(filetype)) {
	fileb -= fsp[0];
	for (i = 1; fileb > fsp[i] && fsp[i] != 0; i++) {
		blocks += (fsp[i] - 1)/(fsp[1] - 1);
		fileb -= fsp[i];
	}
	blocks++;
	for (j = 1; j < i; j++)
		blocks += (fileb + fsp[j] - 1)/fsp[j];
  }
  return (blocks * STD_BLK + BYTESPERBLK - 1) / BYTESPERBLK;
#endif
}

/* Build a table of mounted files systems. For Minix, each mounted file
 * system is characterised by the number of zone numbers, the number of
 * directs and the number of indirects within the inodes. Return a
 * pointer to a list of file system entries. If /etc/mtab is empty or
 * doesn't exist, a dummy list is returned.
 */

FILESYSTEM *scanmtab F0()
{
#ifdef		_BSD
  return NIL;
#endif

#ifdef		_SYSV
  return NIL;
#endif

#ifdef		_MINIX
#ifndef		USEMTAB
  return NIL;
#else
  struct stat sb;			/* stat buffer */
  FILESYSTEM *fs;			/* list of file systems */
  FILESYSTEM *fsp;			/* pointer to file system entry */
  int version;				/* file system version */
  struct mntent *mp;			/* mounted file system */
  FILE *mf;				/* mtab scanner */
  static FILESYSTEM dummyfs = {NIL, NO_DEV, NIL};

  if ((mf = setmntent(MTAB, "r")) == NIL) {
    (void) fputs("ls: cannot access /etc/mtab\n", stderr);
    return &dummyfs;
  }

  fs = NIL;
  while ((mp = getmntent(mf)) != NIL) {
	if (stat(mp->mnt_fsname, &sb) < 0) {
		(void) fprintf(stderr,
		               "ls: cannot get status of %s\n", mp->mnt_fsname);
		continue;
	}
	if (S_ISBLK(sb.st_mode) == 0) {
		(void) fprintf(stderr,
		               "ls: %s not a block device\n", mp->mnt_fsname);
		continue;
	}

	version = atoi(mp->mnt_type);
	if (version < 1 ||
	    version > sizeof(filestructure)/sizeof(filestructure[0])-1) {
		(void) fprintf(stderr,
		               "%s has bad filesystem version\n", mp->mnt_fsname);
		continue;
	}
	fsp = (FILESYSTEM *) safemalloc(sizeof(*fsp));
	fsp->dev = sb.st_rdev;
	fsp->next = fs;
	fsp->fs = filestructure[version];
	fs = fsp;
  }
  endmntent(mf);
  return fs == NIL ? &dummyfs : fs;
#endif
#endif
}

/****************************************************\
 *                 mntent library                    *
 *                                                   *
 * Most Minix systems won't have the mntent library, *
 * so the code is included here. This library is     *
 * preferred to the mtab library because the data    *
 * structures are allocated dynamically and the      *
 * data space can be re-used later by ls.            *
\****************************************************/
#ifdef _MINIX
#ifndef USEMNTENTLIB

#include <string.h>

/* Read the next entry from the mtab file. The entry is parsed and returned
 * as a struct mntent. A static structure is returned. Return NULL on end
 * of file or error.
 */
struct mntent *getmntent(mf)
FILE *mf;
{
  int c;
  char *p;
  static char line[128];		/* local line buffer */
  static struct mntent mt;		/* local mtab entry */
  static char mde[] = " ";		/* delimiter */

  while (fgets(line, sizeof line, mf) != NIL) {
	p = strchr(line, 0);
	if (p[-1] == '\n') {
		*--p = 0;
	} else {
		while ((c = getc(mf)) != 'n' && c != EOF)
			continue;
	}
	if (line[0] == '#') continue;
	if ((mt.mnt_fsname = strtok(line, mde)) != NIL &&
	    (mt.mnt_dir    = strtok((char *) NIL, mde)) != NIL &&
	    (mt.mnt_type   = strtok((char *) NIL, mde)) != NIL &&
	    (mt.mnt_opts   = strtok((char *) NIL, mde)) != NIL)
		return &mt;
	break;
  }
  return NIL;
}
#endif
#endif
#endif
