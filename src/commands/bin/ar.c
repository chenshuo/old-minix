/* ar - archiver		Author: Michiel Huisjes */
/* V7 upgrade			Author:	Monty Walls */

/*
 * Usage: ar 'key' [posname] archive [file] ...
 *
 *	where 'key' is one of: qrqtpmx
 *
 *	  q: quickly append to the end of the archive file
 *	  m: move named files 
 *	  r: replace (append when not in archive)
 *	  d: delete
 *	  t: print contents of archive
 *	  p: print named files
 *	  x: extract
 *
 *	concatencated with one or more of: vuaibcl
 *  
 *	  l: local temporary file for work instead of /tmp/ar.$$$$$
 *	  v: verbose
 *	  a: after 'posname'
 *	  b: before 'posname'
 *	  i: before 'posname'
 *	  c: create  (suppresses creation message)
 *	  u: replace only if dated later than member in archive
 */

/* mods:
 *	1.2 upgrade.
 *	local directory support	(mrw).
 *	full V7 functionality + complete rewrite (mrw).
 *	changed verbose mode to give member name on print (mrw).
 *	fixed up error messages to give more info (mrw).
 *
 * notes:
 *	pdp11 long format & Intel long format are different.
 *
 * change log:
 *	forgot that ar_size is a long for printing - 2/14/88 - mrw
 *	got the mode bit maps mirrored - 2/19/88 - mrw
 *	print & extract member logic fixed - 2/19/88 - mrw
 */

/* include files */
#include <stdio.h>
#include <sys/stat.h>
#include <signal.h>

/* ar.c header file V7 */

#define ARMAG	0177545l
struct ar_hdr {
	char ar_name[14];
	long ar_date;		/* not Intel format */
	char ar_uid;
	char ar_gid;
	int ar_mode;
	long ar_size;		/* not Intel format */
};


/* macro functions */
#define FOREVER		(32766)
#define odd(nr)		(nr & 1)
#define even(nr)	(odd(nr) ? nr + 1 : nr)
#define quit(pid,sig)	(kill(pid,sig),sleep(FOREVER))
#ifndef tell 
#	define tell(f)	(lseek(f, 0l, 1))
#endif

/* option switches */
/* major options */
#define EXTRACT		0x01
#define REPLACE		0x02
#define PRINT		0x04
#define TABLE		0x08
#define DELETE		0x10
#define APPEND		0x20
#define MOVE		0x40

/* minor options */
#define BEFORE		0x01
#define AFTER		0x02
#define LOCAL		0x01
#define VERBOSE		0x01
#define CREATE		0x01
#define ONLY		0x01

/* mode bits maps */
#define EXEC_OWNER	00100
#define EXEC_GROUP	00010
#define EXEC_ALL	00001
#define READ_OWNER	00400
#define READ_GROUP	00040
#define READ_ALL	00004
#define WRITE_OWNER	00200
#define WRITE_GROUP	00020
#define WRITE_ALL	00002
#define SET_UID		04000
#define SET_GID		02000

/* global defines */
#define BUFFERSIZE	4096
#define WRITE		2		/* both read & write */
#define READ		0
#define MAGICSIZE	sizeof(short)	/* size of magic number in file */

/* option switches */
char verbose = 0;
char local = 0;
char create = 0;
char only = 0;
char major = 0;
char minor = 0;

/* global variables */
char *tmp1;
char *tmp2;
char *progname;
char *posname = NULL;
char *afile;
char buffer[BUFFERSIZE];
long pos_offset = -1;
int mypid;

/* keep track of member moves using this struct */
struct mov_list {
	long pos;
	struct mov_list *next;
} *moves = NULL;

/* forward declarations and external references */
extern char *malloc();
extern char *mktemp(), *rindex();
extern int strncmp();
extern print_date();
extern user_abort(), usage();
extern long lseek();
extern char *basename();

int
main(argc, argv)
int argc;
char **argv;
{
	int ac, opts_seen = 0, rc;
	char *av;
	
	progname = argv[0];		
	if (argc < 3) 
		usage();

	for (av = argv[1]; *av; ++av) {	
		switch (*av) {		/* major option */
			case 'q':
				major |= APPEND;
				++opts_seen;
				break;
			case 'r':
				major |= REPLACE;
				++opts_seen;
				break;
			case 'x':
				major |= EXTRACT;
				++opts_seen;
				break;
			case 'p':
				major |= PRINT;
				++opts_seen;
				break;
			case 'm':
				major |= MOVE;
				++opts_seen;
				break;
			case 'd':
				major |= DELETE;
				++opts_seen;
				break;
			case 't':
				major |= TABLE;
				++opts_seen;
				break;
			case 'l':
				local |= LOCAL;
				break;
			case 'a':
				minor |= AFTER;
				break;
			case 'i':
			case 'b':
				minor |= BEFORE;
				break;
			case 'v':
				verbose |= VERBOSE;
				break;
			case 'c':
				create |= CREATE;
				break;
			case 'u':
				only |= ONLY;
				break;
			default:
				usage();
		}
	}

	if (opts_seen != 1) 
		usage();

	/* now do edits on options */
	if (!(major & (REPLACE | MOVE))) {
		if (minor & (AFTER | BEFORE))
			usage();
	}
	else if (major & MOVE) {
		if (!(minor & (AFTER | BEFORE)))
			usage();
	}
	else if (only & ONLY)
		if (!(major & REPLACE))
			usage();
	
	if (local) 
		tmp1 = mktemp("./ar.1.XXXXXX");
	else
		tmp1 = mktemp("/tmp/ar.1.XXXXXX");
			
	/* now if minor says AFTER or BEFORE - then get posname */
	if (minor & (AFTER | BEFORE) && argc >= 4) {
		posname = argv[2];
		afile = argv[3];
		ac = 4;
	}
	else {
		posname = (char *)NULL;
		afile = argv[2];
		ac = 3;
	}
	
	/* exit logic consists of doing a kill on my pid to insure that we */
	/* get the current clean up and exit logic */
	mypid = getpid();	
	signal(SIGINT, user_abort);
	
	switch (major) {
		case REPLACE:
		case DELETE:
		case MOVE:
			ar_members(ac, argc, argv);
			break;
		case EXTRACT:
		case TABLE:
		case PRINT:
			ar_common(ac, argc, argv);
			break;
		case APPEND:
			append_members(ac, argc, argv);
			break;
		default:
			usage();
	}

	for (rc = 0; ac < argc; ++ac) {
		if (*argv[ac] != '\0') {
			/* no processing done on this name */
			fprintf(stderr,"Error %s: %s not found in %s\n", progname, argv[ac], afile);
			rc = 1;
		}
	}
	fflush(stdout);
	exit(rc);
}

usage()
{
	fprintf(stderr,"Usage: %s [qrxdpmt][abivulc] [posname] afile name ... \n",progname);
	exit(1);
}

user_abort()
{
	unlink(tmp1);
	exit(1);
}

insert_abort()
{
	unlink(tmp1);
	unlink(tmp2);
	exit(1);
}

mwrite(fd, address, bytes)
int fd;
register char *address;
register int bytes;
{
  if (write(fd, address, bytes) != bytes) {
	fprintf(stderr," Error: %s - Write error\n",progname);
	quit(mypid, SIGINT);
  }
}

long
swap(l)
long l;
{
	union {
		struct {
			int word1, word2;
		} words;
		long n;
	} u_in, u_out;
	
	u_in.n = l;
	u_out.words.word1 = u_in.words.word2;
	u_out.words.word2 = u_in.words.word1;
	return (u_out.n);
	
}

addmove(pos)
long pos;
{
	struct mov_list *newmove;

	newmove = (struct mov_list *)malloc(sizeof(struct mov_list));
	newmove->pos = pos;
	newmove->next = moves;
	moves = newmove;		
}

struct ar_hdr *
get_member(fd)
int fd;
{
	int ret;
	static struct ar_hdr member;
	
	if ((ret = read(fd,  &member, sizeof(struct ar_hdr))) <= 0) 
		return ((struct ar_hdr *)NULL);
	if (ret != sizeof(struct ar_hdr)) {
		fprintf(stderr,"Error: ar corrupted archive %s\n",afile);
		quit(mypid,SIGINT);
	}

	/* the archive long format is pdp11 not intel
	 * therefore we must reformat them for our internal use
	 */

	member.ar_date = swap(member.ar_date);
	member.ar_size = swap(member.ar_size);
	return (&member);
}

int
open_archive(filename, opt, to_create)
char *filename;
int opt;
{
	static unsigned short magic;
	int fd;
	
	/* to_create can have values of 0,1,2 */
	/* 0 - don't create a file. */
	/* 1 - create file but use create switch message mode */
	/* 2 - create file but don't talk about it */
	
	if (to_create) {
		if ((fd = creat(filename, 0644)) < 0) {
			fprintf(stderr, "Error: %s can not create %s\n",progname, filename);
			quit(mypid,SIGINT);
		}
		if (!create && to_create == 1) fprintf(stderr, "%s:%s created\n", progname, filename);
		magic = ARMAG;
		mwrite(fd, &magic, MAGICSIZE);
		return (fd);
	}
	else {
		if ((fd = open(filename, opt)) < 0) {
			if (opt == WRITE) 
				return (open_archive(filename, opt, 1));
			else {
				fprintf(stderr, "Error: %s can not open %s\n",progname, filename);
				quit(mypid,SIGINT);
			}
		}
		/* now check the magic number for ar V7 file */
		lseek(fd, 0l, 0);
		read(fd, &magic, MAGICSIZE);
		if (magic != ARMAG) {
			fprintf(stderr, "Error: not %s V7 format - %s\n",progname, filename);
			quit(mypid,SIGINT);
		}
		if (major & APPEND)
			lseek(fd, 0l, 2);	/* seek eof position */
			
		return (fd);
	}		
}


int
rebuild(fd, tempfd)
register int fd, tempfd;
{
	register int n;

	/* after we have built the archive to a temporary file and */
	/* everything has worked out- we copy the archive back to */
	/* original file */
		
	signal(SIGINT, SIG_IGN);
	close(fd);
	close(tempfd);
	fd = open_archive(afile, WRITE, 2);
	tempfd = open_archive(tmp1, WRITE, 0);
	while ((n = read(tempfd, buffer, BUFFERSIZE)) > 0) 
		mwrite(fd, buffer, n);
	close(tempfd);
	unlink(tmp1);
	return (fd);
}

print_mode(mode)
short mode;
{
	char g_ex, o_ex, all_ex;
	char g_rd, o_rd, all_rd;
	char g_wr, o_wr, all_wr;
	
	g_ex = EXEC_GROUP & mode ? 'x' : '-';
	o_ex = EXEC_OWNER & mode ? 'x' : '-';
	all_ex = EXEC_ALL & mode ? 'x' : '-';
	
	g_ex = SET_GID & mode ? 's' : g_ex;
	o_ex = SET_UID & mode ? 's' : o_ex;
	
	g_rd = READ_GROUP & mode ? 'r' : '-';
	o_rd = READ_OWNER & mode ? 'r' : '-';
	all_rd = READ_ALL & mode ? 'r' : '-';
	
	g_wr = WRITE_GROUP & mode ? 'w' : '-';
	o_wr = WRITE_OWNER & mode ? 'w' : '-';
	all_wr = WRITE_ALL & mode ? 'w' : '-';

	fprintf(stdout,"%c%c%c",o_rd, o_wr, o_ex);
	fprintf(stdout,"%c%c%c",g_rd, g_wr, g_ex);
	fprintf(stdout,"%c%c%c",all_rd, all_wr, all_ex);
}

print_header(member)
struct ar_hdr *member;
{
	if (verbose) {
		print_mode(member->ar_mode);
		fprintf(stdout,"%3.3d",member->ar_uid);
		fprintf(stdout,"/%-3.3d ",member->ar_gid);
		fprintf(stdout,"%5.5D",member->ar_size);	/* oops is long - mrw */
		print_date(member->ar_date);
	}
	fprintf(stdout,"%-14.14s\n",member->ar_name);
}

print(fd,member)
int fd;
struct ar_hdr *member;
{
	int outfd;
	long size;
	register int cnt, ret;
	int do_align;

	if (major & EXTRACT) {
		if ((outfd = creat(member->ar_name,0666)) < 0) {
			fprintf(stderr,"Error: %s could not creat %-14.14s\n",progname, member->ar_name);
			quit(mypid,SIGINT);
		}
		if (verbose)
			fprintf(stdout,"x - %-14.14s\n",member->ar_name);
	}
	else {
		if (verbose) {
			fprintf(stdout,"p - %-14.14s\n",member->ar_name);
			fflush(stdout);
		}
		outfd = fileno(stdout);
	}
	/* changed loop to use long size for correct extracts */	
	for (size = member->ar_size; size > 0; size -= ret) {
		cnt = (size < BUFFERSIZE ? size : BUFFERSIZE);
		ret = read(fd, buffer, cnt);
		if (ret > 0)
			write(outfd,buffer, ret);
	}
	if (odd(member->ar_size))
		lseek(fd,1l,1);		/* realign ourselves */
		
	if (major & EXTRACT) {
		close(outfd);
		chmod(member->ar_name, member->ar_mode);
	}		
}

/* copy a given member from fd1 to fd2 */
copy_member(infd, outfd, member)
int infd, outfd;
struct ar_hdr *member;
{
	int n, cnt;
	long m, size;

	/* save copies for our use */
	m = size = member->ar_size;

	/* format for disk usage */
	member->ar_size = swap(member->ar_size);
	member->ar_date = swap(member->ar_date);

	mwrite(outfd, member, sizeof(struct ar_hdr));
	for (; m > 0; m -= n) {
		cnt = (m < BUFFERSIZE ? m : BUFFERSIZE);
		if ((n = read(infd, buffer, cnt)) != cnt) {
			fprintf(stderr,"Error: %s - read error on %-14.14s\n",progname, member->ar_name);
			quit(mypid, SIGINT);
		}
		mwrite(outfd, buffer, n);
	}	
	if (odd(size)) {		/* pad to word boundary */
		mwrite(outfd, buffer, 1);
		lseek(infd,1l,1);		/* realign reading fd */ 
	}
}

/* insert at current offset - name file */
insert(fd, name, mess, oldmember)
int fd;
char *name, *mess;
struct ar_hdr *oldmember;
{
	static struct ar_hdr member;
	static struct stat status;
	int in_fd;

	if (stat(name, &status) < 0) {
		fprintf(stderr,"Error: %s cannot find file %s\n",progname,name);
		quit(mypid,SIGINT);
	}
	else if ((in_fd = open(name, READ)) < 0) {
		fprintf(stderr,"Error: %s cannot open file %s\n",progname,name);
		quit(mypid,SIGINT);
	}
	strncpy(member.ar_name, basename(name),14);
	member.ar_uid = status.st_uid;
	member.ar_gid = status.st_gid;
	member.ar_mode = status.st_mode & 07777;
	member.ar_date = status.st_mtime;
	member.ar_size = status.st_size;
	if (only & ONLY)
		if (oldmember != (struct ar_hdr *)NULL)  
			if (member.ar_date <= oldmember->ar_date) {
				close(in_fd);
				if (verbose) fprintf(stdout, "not %-14.14s - %-14.14s\n",mess, name);
				return (-1);
			}
		
	copy_member(in_fd, fd, &member); 
	if (verbose) 
		fprintf(stdout, "%s - %-14.14s\n",mess, name);
	close(in_fd);
	return (1);
}

int
ar_move(oldfd, arfd,mov)
int oldfd, arfd;
struct mov_list *mov;
{
	long pos;
	int cnt, want, a, newfd;
	struct ar_hdr *member;

	if (local) 
		tmp2 = mktemp("./ar.2.XXXXXX");
	else
		tmp2 = mktemp("/tmp/ar.2.XXXXXX");

	close(oldfd);				/* close old temp file */
	signal(SIGINT, insert_abort);		/* set new signal handler */
	newfd = open_archive(tmp2, WRITE, 2);	/* open new tmp file */
	oldfd = open_archive(tmp1, WRITE, 0);	/* reopen old tmp file */

	/* copy archive till we get to pos_offset */
	for (pos = pos_offset; pos > 0; pos -= cnt) {
		want = (pos < BUFFERSIZE ? pos : BUFFERSIZE);
		if ((cnt = read(oldfd, buffer, want)) > 0)
			mwrite(newfd, buffer, cnt);
	}
	/* if minor = 'a' then skip over posname */
	if (minor & AFTER) {
		if ((member = get_member(oldfd)) != NULL)
			copy_member(oldfd, newfd, member);
	}
	/* move members in the library */
	while (mov != NULL) {
		lseek(arfd, mov->pos, 0);
		if ((member = get_member(arfd)) != NULL)
			copy_member(arfd, newfd, member);
		mov = mov->next;
		if (verbose) fprintf(stdout, "m - %-14.14s\n", member->ar_name);
	}

	/* copy rest of library into new tmp file */
	while ((member = get_member(oldfd)) != NULL) 
		copy_member(oldfd, newfd, member);

	/* detach old temp file */
	close(oldfd);
	unlink(tmp1);

	/* change context temp file */
	tmp1 = tmp2;
	return (newfd);
}

int
ar_insert(oldfd, ac, argc, argv)
int oldfd;
int ac, argc;
char **argv;
{
	long pos;
	int cnt, want, a, newfd;
	struct ar_hdr *member;

	if (local) 
		tmp2 = mktemp("./ar.2.XXXXXX");
	else
		tmp2 = mktemp("/tmp/ar.2.XXXXXX");

	close(oldfd);				/* close old temp file */
	signal(SIGINT, insert_abort);		/* set new signal handler */
	newfd = open_archive(tmp2, WRITE, 2);	/* open new tmp file */
	oldfd = open_archive(tmp1, WRITE, 0);	/* reopen old tmp file */

	/* copy archive till we get to pos_offset */
	for (pos = pos_offset; pos > 0; pos -= cnt) {
		want = (pos < BUFFERSIZE ? pos : BUFFERSIZE);
		if ((cnt = read(oldfd, buffer, want)) > 0)
			mwrite(newfd, buffer, cnt);
	}
	/* if minor = 'a' then skip over posname */
	if (minor & AFTER) {
		if ((member = get_member(oldfd)) != NULL)
			copy_member(oldfd, newfd, member);
	}
	
	/* copy new members into the library */
	for (a = ac+1; a <= argc; ++a)
		if (argv[a-1] && *argv[a-1] != '\0') {
			insert(newfd, argv[a-1], "a", (struct ar_hdr *)NULL);
			*argv[a-1] = '\0';
		}

	/* copy rest of library into new tmp file */
	while ((member = get_member(oldfd)) != NULL) 
		copy_member(oldfd, newfd, member);

	/* detach old temp file */
	close(oldfd);
	unlink(tmp1);

	/* change context temp file */
	tmp1 = tmp2;
	return (newfd);
}

ar_common(ac, argc, argv)
int ac, argc;
char **argv;
{
	int a, fd, did_print;
	struct ar_hdr *member;

	fd = open_archive(afile, READ, 0);
	while ((member = get_member(fd)) != NULL) {
		did_print = 0;
		if (ac < argc) {
			for (a = ac+1; a <= argc; ++a) {
				if (strncmp(basename(argv[a-1]),member->ar_name,14) == 0) {
					if (major & TABLE)
						print_header(member);
					else if (major & (PRINT | EXTRACT)) {
						print(fd, member);
						did_print = 1;
					}
					*argv[a-1] = '\0';
					break;
				}
			}
		}
		else {
			if (major & TABLE)
				print_header(member);
			else if (major & (PRINT | EXTRACT)) {
				print(fd, member);
				did_print = 1;
			}
		}
		/* if we didn't print the member or didn't use it we will
		 * have to seek over its contents
		 */
		if (!did_print)
			lseek(fd, (long)even(member->ar_size), 1);
	}
}

ar_members(ac, argc, argv)
int ac, argc;
char **argv;
{
	int a, fd, tempfd, rc;
	struct ar_hdr *member;
	long *lpos;

	fd = open_archive(afile, WRITE, 0);
	tempfd = open_archive(tmp1, WRITE, 2);
	while ((member = get_member(fd)) != NULL) {

		/* if posname specified check for our member */
		/* if our member save his starting pos in our working file*/
		if (posname && strncmp(posname, member->ar_name, 14) == 0)
			pos_offset = tell(tempfd) - MAGICSIZE;

		if (ac < argc) { /* we have a list of members to check */
			for (a = ac+1; a <= argc; ++a)
				if (strncmp(basename(argv[a-1]),member->ar_name,14) == 0) {
					if (major & REPLACE) {
						if (insert(tempfd,argv[a-1],"r", member) < 0)
							copy_member(fd, tempfd, member);
						else 
							lseek(fd, (long)even(member->ar_size), 1);
					}
					else if (major & MOVE) {
						/* cheat by saving pos in archive */
						addmove((tell(fd) - sizeof(struct ar_hdr)));
						lseek(fd, (long)even(member->ar_size), 1);
					}
					*argv[a-1] = '\0';
					break;
				}
		}
		if (ac >= argc || a > argc) 	/*nomatch on a member name */
			copy_member(fd, tempfd, member);
		else if (major & DELETE) {
			if (verbose) fprintf(stdout,"d - %-14.14s\n",member->ar_name);
			lseek(fd, (long)even(member->ar_size), 1);
		}
	}
	if (major & MOVE) {
		if (posname == NULL) 
			pos_offset = lseek(fd, 0l, 2);
		else if (pos_offset == (-1)) {
			fprintf(stderr,"Error: %s cannot find file %-14.14s\n",progname,posname);
			quit(mypid,SIGINT);
		}
		tempfd = ar_move(tempfd, fd, moves);
	}
	else if (major & REPLACE) {
		/* take care to add left overs */
		/* if the posname is not found we just add to end of ar */
		if (posname && pos_offset != (-1)) {
			tempfd = ar_insert(tempfd, ac, argc, argv);
		}
		else {
			for (a = ac+1; a <= argc; ++a)
				if (*argv[a-1]) {
					insert(tempfd, argv[a-1], "a", (struct ar_hdr *)NULL);
					*argv[a-1] = '\0';
				}
		}
	}
	fd = rebuild(fd, tempfd);
	close(fd);
}

append_members(ac, argc, argv)
int ac, argc;
char **argv;
{
	int a, fd;
	struct ar_hdr *member;
	
	/* quickly append members don't worry about dups in ar */
	fd = open_archive(afile, WRITE, 0);
	if (ac < argc) {
		if (odd(lseek(fd, 0l, 2)))
			mwrite(fd, buffer, 1);
		/* while not end of member list insert member at end */
		for (a = ac+1; a <= argc; ++a) {
			insert(fd, argv[a-1], "a", (struct ar_hdr *)NULL);
			*argv[a-1] = '\0';
		}
	}
	close(fd);
}


char *basename(path)
char *path;
{
  register char *ptr = path;
  register char *last = (char *)NULL;

  while (*ptr != '\0') {
	if (*ptr == '/')
		last = ptr;
	ptr++;
  }
  if (last == (char *)NULL)
	return path;
  if (*(last + 1) == '\0') {
	*last = '\0';
	return basename(path);
  }
  return last + 1;
}


#define MINUTE	60L
#define HOUR	(60L * MINUTE)
#define DAY	(24L * HOUR)
#define YEAR	(365L * DAY)
#define LYEAR	(366L * DAY)

int mo[] = {
  31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

char *moname[] = {
  " Jan ", " Feb ", " Mar ", " Apr ", " May ", " Jun ",
  " Jul ", " Aug ", " Sep ", " Oct ", " Nov ", " Dec "
};

/* Print the date.  This only works from 1970 to 2099. */
print_date(t)
long t;
{
  int i, year, day, month, hour, minute;
  long length, time(), original;

  year = 1970;
  original = t;
  while (t > 0) {
	length = (year % 4 == 0 ? LYEAR : YEAR);
	if (t < length)
		break;
	t -= length;
	year++;
  }

 /* Year has now been determined.  Now the rest. */
  day = (int) (t / DAY);
  t -= (long) day * DAY;
  hour = (int) (t / HOUR);
  t -= (long) hour * HOUR;
  minute = (int) (t / MINUTE);

 /* Determine the month and day of the month. */
  mo[1] = (year % 4 == 0 ? 29 : 28);
  month = 0;
  i = 0;
  while (day >= mo[i]) {
	month++;
	day -= mo[i];
	i++;
  }

  /* At this point, 'year', 'month', 'day', 'hour', 'minute'  ok */
  fprintf(stdout, "%s%2.2d ",moname[month],++day);
  if (time((long *)NULL) - original >= YEAR / 2L)
	fprintf(stdout,"%4.4D ",(long)year);
  else 
	fprintf(stdout,"%02.2d:%02.2d ",hour, minute);
}

