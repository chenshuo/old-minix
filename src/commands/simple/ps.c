/* ps - print status 			Author: Peter Valkenburg */

/* Ps.c, Peter Valkenburg (valke@psy.vu.nl), january 1990.
 *
 * This is a V7 ps(1) look-alike for MINIX >= 1.5.0.  It can use a database
 * with information on system addresses and terminal names as an extra, and
 * has some additional fields.
 * It does not support the 'k' option (i.e. cannot read memory from core file).
 * If you want to compile this for non-IBM PC architectures, the header files
 * require that you have your CHIP, MACHINE etc. defined.
 * Full syntax:
 *	ps [-][alxU] [kernel mm fs]
 * Option `a' gives all processes, `l' for detailed info, `x' includes even
 * processes without a terminal, `U' updates /etc/psdatabase where ps keeps
 * system information and terminal names.  Additional arguments are the paths
 * of system binaries from which system addresses are taken.
 * The database update with the `U' option uses paths kept in the old
 * /etc/psdatabase, unless the additional kernel/mm/fs arguments were given.
 * It defaults to /usr/src/{kernel/kernel,mm/mm,fs/fs} if the latter is not
 * the case and psdatabase is non-existent.  Ps warns if the database appears
 * older than the system executables.
 *
 * VERY IMPORTANT NOTE:
 *	To compile ps, the kernel/, fs/ and mm/ source directories must be in
 *	../ relative to the directory where ps is compiled (normally the
 *	command source directory).
 *	To run ps the kernel, mm and fs executables must contain symbol tables.
 *	This can be arranged using the -s flag and the ast program.  For
 *	example in fs, one would have
 *
 *	asld -s -i -o fs $l/head.s $(obj) $l/libc.a $l/end.s >symbol.out
 *	ast -X fs		# include symbol.out in fs
 *
 *	Ps does *NOT* need the boot image to contain symbol tables.  Nor does
 *	it need kernel, fs or mm binaries after a `ps -U'.  The paths of these
 *	executables can be changed by passing them in the command line, e.g.,
 *	a "ps /kernel /mm /fs" runs ps with namelists extracted from the
 *	system executables found in /.  This also works for `-U'.
 *	If you want your ps to be useable by anyone, yet disallow modification
 *	of the database by anyone other than root and bin, you can arrange the
 *	following access permissions (note the protected memory files and set
 *	*group* id on ps):
 *	-rwxr-sr-x  1 bin         11916 Jul  4 15:31 /bin/ps
 *	-rw-r--r--  1 bin           848 Jul  4 15:39 /etc/psdatabase
 *	crw-r-----  1 bin        1,   1 Jan  1  1970 /dev/mem
 *	crw-r-----  1 bin        1,   2 Jan  1  1970 /dev/kmem
 *
 *	Finally, this is what you have to do if you have amoeba in your kernel:
 *	  1) compile ps including -DAM_KERNEL as an argument to cc and
 *	  2) pass the amoeba kernel, mm and fs binaries to a ps U, as in:
 *		ps -U /amoeba/kernel/kernel /amoeba/mm/mm /amoeba/fs/fs
 */

/* Some technical comments on this implementation:
 *
 * Most fields are similar to V7 ps(1), except for CPU, NICE, PRI which are
 * absent, RECV which replaces WCHAN, and PGRP that is an extra.
 * The info is obtained from the following fields of proc, mproc and fproc:
 * F	- kernel status field, p_flags
 * S	- kernel status field, p_flags; mm status field, mp_flags (R if p_flags
 * 	  is 0; Z if mp_flags == HANGING; T if mp_flags == STOPPED; else W).
 * UID	- mm eff uid field, mp_effuid
 * PID	- mm pid field, mp_pid
 * PPID	- mm parent process index field, mp_parent (used as index in proc).
 * PGRP - mm process group field, mp_procgrp
 * ADDR	- kernel physical text address, p_map[T].mem_phys
 * SZ	- kernel text size + physical stack address - physical data address
 *			   + stack size
 * 	  p_map[T].mem_len + p_map[S].mem_phys - p_map[D].mem_phys
 * 	  		   + p_map[S].mem_len
 * RECV	- kernel process index field for message receiving, p_getfrom
 *	  If sleeping, mm's mp_flags, or fs's fp_task are used for more info.
 * TTY	- fs controlling tty device field, fs_tty.
 * TIME	- kernel user + system times fields, user_time + sys_time
 * CMD	- system process index (converted to mnemonic name obtained by reading
 *	  tasktab array from kmem), or user process argument list (obtained by
 *	  reading the stack frame; the resulting address is used to get
 *	  the argument vector from user space and converted into a concatenated
 *	  argument list).
 */

 /* #define NO_CLICKS  *//* new kernel doesn't use clicks */

#include <minix/config.h>
#include <limits.h>
#include <sys/types.h>

#include <minix/const.h>
#undef EXTERN			/* <minix/const.h> defined this */
#define EXTERN			/* so we get proc, mproc and fproc */
#include <minix/type.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <minix/com.h>
#include <fcntl.h>
#include <a.out.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdio.h>

#include "../../kernel/const.h"
#include "../../kernel/type.h"
#include "../../kernel/proc.h"
#undef printf			/* kernel's const.h defined this */

#include "../../mm/mproc.h"
#include "../../fs/fproc.h"
#include "../../fs/const.h"
#undef printf			/* fs's const.h defined this */

#ifdef NO_CLICKS
#undef CLICK_SHIFT
#define CLICK_SHIFT 0
#define phys_clicks phys_bytes

/* Fix: many off_t's and one long should be phys_bytes' and a couple of longs
 * should be off_t's. Also, phys_bytes should be unsigned as well as long.
 * There will be a problem lseeking if the 386 kernel is every mapped high.
 */
#endif

/*----- ps's local stuff below this line ------*/


#define mindev(dev)	(((dev)>>MINOR) & 0377)	/* yield minor device */
#define majdev(dev)	(((dev)>>MAJOR) & 0377)	/* yield major device */

#define	TTY_MAJ		4	/* major device of console */

/* Macro to convert memory offsets to rounded kilo-units */
#define	off_to_k(off)	((unsigned) (((off) + 512) / 1024))

/* What we think the relevant identifiers in the namelists are */
#define	ID_PROC		"_proc"	/* from kernel namelist */
#define	ID_MPROC	"_mproc"/* from mm namelist */
#define	ID_FPROC	"_fproc"/* from fs namelist */
#define	ID_TASKTAB	"_tasktab"	/* from kernel namelist */

/* Structure for system address info (also part of ps's database). */
typedef struct {
  struct nlist ke_proc[2], ke_tasktab[2];
  struct nlist mm_mproc[2];
  struct nlist fs_fproc[2];
} sysinfo_t;

sysinfo_t sysinfo;		/* sysinfo holds actual system info */

/* Structure for tty name info (also part of ps's database). */
typedef struct {
  char tty_name[NAME_MAX + 1];	/* file name in /dev */
  dev_t tty_dev;		/* major/minor pair */
} ttyinfo_t;

/* Structure for system path info (part of ps's database). */
typedef struct {
  char ke_path[PATH_MAX + 1];	/* paths of kernel, */
  char mm_path[PATH_MAX + 1];	/* mm, */
  char fs_path[PATH_MAX + 1];	/* and fs used in last ps -U */
} pathinfo_t;

pathinfo_t pathinfo;		/* pathinfo holds actual path info */

/* N_TTYINFO is # of ttyinfo_t structs - should be >= # of entries in /dev. */
#define	N_TTYINFO	100

ttyinfo_t ttyinfo[N_TTYINFO];	/* ttyinfo holds actual tty info */

#define	NAME_SIZ	(sizeof(sysinfo.ke_proc[0].n_name))	/* 8 chars */

/* What we think the identfiers of the imported variables in this program are */
#define	PROC	proc
#define	MPROC	mproc
#define	FPROC	fproc
#define	TASKTAB	tasktab

/* Default paths for system binaries */
#define KERNEL_PATH	"/usr/src/kernel/kernel"
#define MM_PATH		"/usr/src/mm/mm"
#define FS_PATH		"/usr/src/fs/fs"

#define	KMEM_PATH	"/dev/kmem"	/* opened for kernel proc table */
#define	MEM_PATH	"/dev/mem"	/* opened for mm/fs + user processes */

int kmemfd, memfd;		/* file descriptors of [k]mem */

#define DBASE_PATH	"/etc/psdatabase"	/* path of ps's database */
#define DBASE_MODE	0644	/* mode for ps's database */

struct tasktab tasktab[NR_TASKS + INIT_PROC_NR + 1];	/* task table */

/* Short and long listing formats:
 *
 *   PID TTY  TIME CMD
 * ppppp tttmmm:ss ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc
 *
 *   F S UID   PID  PPID  PGRP ADDR  SZ       RECV TTY  TIME CMD
 * fff s uuu ppppp ppppp ppppp aaaa sss rrrrrrrrrr tttmmm:ss ccccccccccccccccccccc
 */
#define S_HEADER "  PID TTY  TIME CMD\n"
#define S_FORMAT "%5d %3s%3ld:%02ld %.63s\n"
#define L_HEADER "  F S UID   PID  PPID  PGRP ADDR  SZ       RECV TTY  TIME CMD\n"
#define L_FORMAT "%3o %c %3d %5d %5d %5d %4d %3d %10s %3s%3ld:%02ld %.21s\n"

struct pstat {			/* structure filled by pstat() */
  dev_t ps_dev;			/* major/minor of controlling tty */
  uid_t ps_ruid;		/* real uid */
  uid_t ps_euid;		/* effective uid */
  pid_t ps_pid;			/* process id */
  pid_t ps_ppid;		/* parent process id */
  int ps_pgrp;			/* process group id */
  int ps_flags;			/* kernel flags */
  int ps_mflags;		/* mm flags */
  int ps_ftask;			/* (possibly pseudo) fs suspend task */
  char ps_state;		/* process state */
  size_t ps_tsize;		/* text size (in bytes) */
  size_t ps_dsize;		/* data size (in bytes) */
  size_t ps_ssize;		/* stack size (in bytes) */
  off_t ps_vtext;		/* physical text offset */
  off_t ps_vdata;		/* physical data offset */
  off_t ps_vstack;		/* physical stack offset */
  off_t ps_text;		/* physical text offset */
  off_t ps_data;		/* physical data offset */
  off_t ps_stack;		/* physical stack offset */
  int ps_recv;			/* process number to receive from */
  time_t ps_utime;		/* accumulated user time */
  time_t ps_stime;		/* accumulated system time */
  char *ps_args;		/* concatenated argument string */
  char *ps_istackframe;          /* initial stack frame from MM */
};

/* Ps_state field values in pstat struct above */
#define	Z_STATE		'Z'	/* Zombie */
#define	W_STATE		'W'	/* Waiting */
#define	S_STATE		'S'	/* Sleeping */
#define	R_STATE		'R'	/* Runnable */
#define	T_STATE		'T'	/* stopped (Trace) */

_PROTOTYPE(char *tname, (Dev_t dev_nr ));
_PROTOTYPE(char *taskname, (int p_nr ));
_PROTOTYPE(char *prrecv, (struct pstat *bufp ));
_PROTOTYPE(void disaster, (int sig ));
_PROTOTYPE(int main, (int argc, char *argv []));
_PROTOTYPE(char *get_args, (struct pstat *bufp ));
_PROTOTYPE(int pstat, (int p_nr, struct pstat *bufp ));
_PROTOTYPE(int addrread, (int fd, phys_clicks base, vir_bytes addr, 
						    char *buf, int nbytes ));
_PROTOTYPE(void usage, (char *pname ));
_PROTOTYPE(void err, (char *s ));
_PROTOTYPE(int gettynames, (ttyinfo_t ttyinfo []));
_PROTOTYPE(int outdates, (char *file, time_t tm ));


/*
 * Tname returns mnemonic string for dev_nr. This is "?" for maj/min pairs that
 * are not found.  It uses the ttyinfo array (prepared by gettynames).
 * Tname assumes that the first three letters of the tty's name can be omitted
 * and returns the rest (except for the console, which yields "co").
 * Thus, tname normally returns "co" for tty0, "1" for tty1, "01" for tty01,
 * "p0" for ptyp0, "i00" for ttyi00, etc.
 * One snag: If some non-standard name appears in /dev before the regular one,
 * ps will print the non-standard's suffix.
 */
char *tname(dev_nr)
Dev_t dev_nr;
{
  int i;

  if (majdev(dev_nr) == TTY_MAJ && mindev(dev_nr) == 0) return "co";

  for (i = 0; i < N_TTYINFO && ttyinfo[i].tty_name[0] != '\0'; i++)
	if (ttyinfo[i].tty_dev == dev_nr)
		if (strlen(ttyinfo[i].tty_name) <= 3)
			return ttyinfo[i].tty_name;
		else
			return &(ttyinfo[i].tty_name[3]);

  return "?";
}

/* Return canonical task name of task p_nr; overwritten on each call (yucch) */
char *taskname(p_nr)
int p_nr;
{
  char *cp;

  if (p_nr < -NR_TASKS || p_nr > INIT_PROC_NR) return "?";

  /* Strip trailing blanks for right-adjusted output */
  for (cp = tasktab[p_nr + NR_TASKS].name; *cp != '\0'; cp++)
	if (*cp == ' ') break;
  *cp = '\0';

  return tasktab[p_nr + NR_TASKS].name;
}

/* Prrecv prints the RECV field for process with pstat buffer pointer bufp.
 * This is either "ANY", "taskname", or "(blockreason) taskname".
 */
char *prrecv(bufp)
struct pstat *bufp;
{
  char *blkstr, *task;		/* reason for blocking and task */
  static char recvstr[20];

  if (bufp->ps_recv == ANY) return "ANY";

  task = taskname(bufp->ps_recv);
  if (bufp->ps_state != S_STATE) return task;

  blkstr = "?";
  if (bufp->ps_recv == MM_PROC_NR) {
	if (bufp->ps_mflags & PAUSED)
		blkstr = "pause";
	else if (bufp->ps_mflags & WAITING)
		blkstr = "wait";
  } else if (bufp->ps_recv == FS_PROC_NR) {
	if (-bufp->ps_ftask == XOPEN)
		blkstr = "xopen";
	else if (-bufp->ps_ftask == XPIPE)
		blkstr = "xpipe";
	else
		blkstr = taskname(-bufp->ps_ftask);
  }
  (void) sprintf(recvstr, "(%s) %s", blkstr, task);
  return recvstr;
}

/* If disaster is called some of the system parameters imported into ps are
 * probably wrong.  This tends to result in memory faults.
 */
void disaster(sig)
int sig;
{
  fprintf(stderr, "Ooops, got signal %d: ", sig);
  fprintf(stderr, "Make sure up-to-date system executables are used and\n");
  fprintf(stderr, "correct system parameters are compiled into ps.\n");
  exit(3);
}

/* Main interprets arguments, gets system addresses, opens [k]mem, reads in
 * process tables from kernel/mm/fs and calls pstat() for relevant entries.
 */
int main(argc, argv)
int argc;
char *argv[];
{
  int i;
  struct pstat buf;
  int db_fd;
  int uid = getuid();		/* real uid of caller */
  int opt_all = FALSE;		/* -a */
  int opt_long = FALSE;		/* -l */
  int opt_notty = FALSE;	/* -x */
  int opt_update = FALSE;	/* -U */
  int opt_path = FALSE;		/* alternative system executables */

  (void) signal(SIGSEGV, disaster);	/* catch a common crash */

  /* Parse arguments; a '-' need not be present (V7/BSD compatability) */
  switch (argc) {
      case 1:			/* plain ps */
	break;
      case 2:			/* ps <[-][alxU]> */
      case 5:			/* ps <[-][alxU]> <kernel mm fs> */
	for (i = (argv[1][0] == '-' ? 1 : 0); argv[1][i] != '\0'; i++)
		switch (argv[1][i]) {
		    case 'a':	opt_all = TRUE;	break;
		    case 'l':	opt_long = TRUE;	break;
		    case 'x':	opt_notty = TRUE;	break;
		    case 'U':	opt_update = TRUE;	break;
		    default:	usage(argv[0]);
		}
	break;
      case 4:			/* ps <kernel mm fs> */
	if (argv[1][0] != '-') break;
      default:	usage(argv[0]);
}

  if (argc >= 4) {		/* ps [-][alxU] <kernel mm fs> */
	opt_path = TRUE;
	strncpy(pathinfo.ke_path, argv[argc - 3], (size_t)PATH_MAX);
	strncpy(pathinfo.mm_path, argv[argc - 2], (size_t)PATH_MAX);
	strncpy(pathinfo.fs_path, argv[argc - 1], (size_t)PATH_MAX);
  } else {
	strcpy(pathinfo.ke_path, KERNEL_PATH);
	strcpy(pathinfo.mm_path, MM_PATH);
	strcpy(pathinfo.fs_path, FS_PATH);
  }

  /* Fill the sysinfo and ttyinfo structs */
  if (opt_update || opt_path ||
      (db_fd = open(DBASE_PATH, O_RDONLY)) == -1) {

	/* Try to use old database's paths for this update */
	if (opt_update && !opt_path &&
	    (db_fd = open(DBASE_PATH, O_RDONLY)) != -1) {
		if (read(db_fd, (char *) &sysinfo,
			 sizeof(sysinfo_t)) == sizeof(sysinfo_t) &&
		    read(db_fd, (char *) ttyinfo,
			 sizeof(ttyinfo)) == sizeof(ttyinfo) &&
		    read(db_fd, (char *) &pathinfo,
			 sizeof(pathinfo_t)) == sizeof(pathinfo_t))
			fprintf(stderr, "Using old pathnames found in %s.\n",
				DBASE_PATH);
		(void) close(db_fd);
	}
	strncpy(sysinfo.ke_proc[0].n_name, ID_PROC, (size_t)NAME_SIZ);
	strncpy(sysinfo.ke_tasktab[0].n_name, ID_TASKTAB, (size_t)NAME_SIZ);
	if (nlist(pathinfo.ke_path, sysinfo.ke_proc) != 0 ||
	    nlist(pathinfo.ke_path, sysinfo.ke_tasktab) != 0)
		err("Can't read kernel namelist");
	strncpy(sysinfo.mm_mproc[0].n_name, ID_MPROC, (size_t)NAME_SIZ);
	if (nlist(pathinfo.mm_path, sysinfo.mm_mproc) != 0)
		err("Can't read mm namelist");
	strncpy(sysinfo.fs_fproc[0].n_name, ID_FPROC, (size_t)NAME_SIZ);
	if (nlist(pathinfo.fs_path, sysinfo.fs_fproc) != 0)
		err("Can't read fs namelist");

	if (gettynames(ttyinfo) == -1) err("Can't get tty names");

	if (opt_update) {
		if ((db_fd = creat(DBASE_PATH, DBASE_MODE)) == -1)
			err("Can't creat psdatabase");
		if (write(db_fd, (char *) &sysinfo,
			  sizeof(sysinfo_t)) != sizeof(sysinfo_t) ||
		    write(db_fd, (char *) ttyinfo,
			  sizeof(ttyinfo)) != sizeof(ttyinfo) ||
		    write(db_fd, (char *) &pathinfo,
			  sizeof(pathinfo_t)) != sizeof(pathinfo_t))
			err("Can't write psdatabase");
		fprintf(stderr, "Updated terminal names and system addresses in %s using:\n",
			DBASE_PATH);
		fprintf(stderr, "%s, %s, %s.\n",
			pathinfo.ke_path, pathinfo.mm_path,
			pathinfo.fs_path);
		exit(0);	/* don't attempt to do further output */
	}
  } else {			/* get info from database */
	struct stat db_stat;

	if (read(db_fd, (char *) &sysinfo,
		 sizeof(sysinfo_t)) != sizeof(sysinfo_t) ||
	    read(db_fd, (char *) ttyinfo,
		 sizeof(ttyinfo)) != sizeof(ttyinfo) ||
	    read(db_fd, (char *) &pathinfo,
		 sizeof(pathinfo_t)) != sizeof(pathinfo_t))
		err("Can't read info from psdatabase");

	/* Compare modification time of database with kernel/mm/fs
	 * and warn user if the database appears to be outdated.
	 * Times are ignored if the system executables have no
	 * namelist.
	 */
	(void) fstat(db_fd, &db_stat);
	if (outdates(pathinfo.ke_path, db_stat.st_mtime))
		fprintf(stderr, "Warning: %s is older than %s\n",
			DBASE_PATH, pathinfo.ke_path);
	if (outdates(pathinfo.mm_path, db_stat.st_mtime))
		fprintf(stderr, "Warning: %s is older than %s\n",
			DBASE_PATH, pathinfo.mm_path);
	if (outdates(pathinfo.fs_path, db_stat.st_mtime))
		fprintf(stderr, "Warning: %s is older than %s\n",
			DBASE_PATH, pathinfo.fs_path);
  }
  (void) close(db_fd);

  /* Get kernel tables */
  if ((kmemfd = open(KMEM_PATH, O_RDONLY)) == -1) err(KMEM_PATH);
  if (addrread(kmemfd, (phys_clicks) 0,
	     (vir_bytes) sysinfo.ke_proc[0].n_value,
	     (char *) PROC, sizeof(PROC)) != sizeof(PROC))
	err("Can't get kernel proc table from /dev/kmem");
  if (addrread(kmemfd, (phys_clicks) 0,
	     (vir_bytes) sysinfo.ke_tasktab[0].n_value,
	     (char *) TASKTAB, sizeof(TASKTAB)) != sizeof(TASKTAB))
	err("Can't get kernel task table from /dev/kmem");

  /* Get mm/fs tables */
  if ((memfd = open(MEM_PATH, O_RDONLY)) == -1) err(MEM_PATH);
  if (addrread(memfd, PROC[NR_TASKS + MM_PROC_NR].p_map[D].mem_phys,
	     (vir_bytes) sysinfo.mm_mproc[0].n_value,
	     (char *) MPROC, sizeof(MPROC)) != sizeof(MPROC))
	err("Can't get mm proc table from /dev/mem");
  if (addrread(memfd, PROC[NR_TASKS + FS_PROC_NR].p_map[D].mem_phys,
	     (vir_bytes) sysinfo.fs_fproc[0].n_value,
	     (char *) FPROC, sizeof(FPROC)) != sizeof(FPROC))
	err("Can't get fs proc table from /dev/mem");

  /* Now loop through process table and handle each entry */
  printf("%s", opt_long ? L_HEADER : S_HEADER);
  for (i = -NR_TASKS; i < NR_PROCS; i++) {
	if (pstat(i, &buf) != -1 &&
	    (opt_all || buf.ps_euid == uid || buf.ps_ruid == uid) &&
	    (opt_notty || majdev(buf.ps_dev) == TTY_MAJ))
		if (opt_long) printf(L_FORMAT,
			       buf.ps_flags, buf.ps_state,
			       buf.ps_euid, buf.ps_pid, buf.ps_ppid,
			       buf.ps_pgrp,
			       off_to_k(buf.ps_text),
			       off_to_k((buf.ps_tsize
					 + buf.ps_stack - buf.ps_data
					 + buf.ps_ssize)),
			       (buf.ps_flags & RECEIVING ?
				prrecv(&buf) :
				""),
			       tname((Dev_t) buf.ps_dev),
			    (buf.ps_utime + buf.ps_stime) / HZ / 60,
			    (buf.ps_utime + buf.ps_stime) / HZ % 60,
			       i <= INIT_PROC_NR ? taskname(i) :
			       (buf.ps_args == NULL ? "" :
				buf.ps_args));
		else
			printf(S_FORMAT,
			       buf.ps_pid, tname((Dev_t) buf.ps_dev),
			    (buf.ps_utime + buf.ps_stime) / HZ / 60,
			    (buf.ps_utime + buf.ps_stime) / HZ % 60,
			       i <= INIT_PROC_NR ? taskname(i) :
			       (buf.ps_args == NULL ? "" :
				buf.ps_args));
  }
  return(0);
}

  union {
#if (CHIP == M68000)
	long stk_i;
#else
	int stk_i;
#endif
	char *stk_cp;
	char stk_c;
  } stk[ARG_MAX / sizeof(char *)], *sp;

char *get_args(bufp)
struct pstat *bufp;
{
  int nargv;
  int cnt;			/* # of bytes read from stack frame */
  int neos;			/* # of '\0's seen in argv string space */
  long l;
  char *cp, *args;

  /* Calculate the number of bytes to read from user stack */
  cnt = bufp->ps_ssize - ( (long)bufp->ps_istackframe - (long)bufp->ps_stack );
  if (cnt > ARG_MAX) cnt = ARG_MAX;

  /* Get cnt bytes from user initial stack to local stack buffer */
  if (lseek(memfd, ((long) bufp->ps_istackframe), 0) < 0)
	return NULL; 

  if ( read(memfd, (char *)stk, cnt) != cnt ) 
	return NULL;

  sp = stk;
  nargv = (int) sp[0].stk_i;  /* number of argv arguments */

  /* See if argv[0] is with the bytes we read in */
  l = (long) sp[1].stk_cp - (long) bufp->ps_istackframe + (long)bufp->ps_stack -
	(long)bufp->ps_vstack;
  if ( ( l < 0 ) || ( l > cnt ) )  
	return NULL;

  /* l is the offset of the argv[0] argument */
  /* change for concatenation the '\0' to space, for nargv elements */

  args = &((char *) stk)[(int)l]; 
  neos = 0;
  for (cp = args; cp < &((char *) stk)[cnt]; cp++)
	if (*cp == '\0')
		if (++neos >= nargv)
			break;
		else
			*cp = ' ';
  if (neos != nargv) return NULL;

  return args;

}

/* Pstat collects info on process number p_nr and returns it in buf.
 * It is assumed that tasks do not have entries in fproc/mproc.
 */
int pstat(p_nr, bufp)
int p_nr;
struct pstat *bufp;
{
  int p_ki = p_nr + NR_TASKS;	/* kernel proc index */

  if (p_nr < -NR_TASKS || p_nr >= NR_PROCS) return -1;

  if ((PROC[p_ki].p_flags & P_SLOT_FREE) && !(MPROC[p_nr].mp_flags & IN_USE))
	return -1;

  bufp->ps_flags = PROC[p_ki].p_flags;

  if (p_nr >= LOW_USER) {
	bufp->ps_dev = FPROC[p_nr].fs_tty;
	bufp->ps_ftask = FPROC[p_nr].fp_task;
  } else {
	bufp->ps_dev = 0;
	bufp->ps_ftask = 0;
  }

  if (p_nr >= LOW_USER) {
	bufp->ps_ruid = MPROC[p_nr].mp_realuid;
	bufp->ps_euid = MPROC[p_nr].mp_effuid;
	bufp->ps_pid = MPROC[p_nr].mp_pid;
	bufp->ps_ppid = MPROC[MPROC[p_nr].mp_parent].mp_pid;
	bufp->ps_pgrp = MPROC[p_nr].mp_procgrp;
	bufp->ps_mflags = MPROC[p_nr].mp_flags;
  } else {
	bufp->ps_pid = bufp->ps_ppid = 0;
	bufp->ps_ruid = bufp->ps_euid = 0;
	bufp->ps_pgrp = 0;
	bufp->ps_mflags = 0;
  }

  /* State is interpretation of combined kernel/mm flags for non-tasks */
  if (p_nr >= LOW_USER) {		/* non-tasks */
	if (MPROC[p_nr].mp_flags & HANGING)
		bufp->ps_state = Z_STATE;	/* zombie */
	else if (MPROC[p_nr].mp_flags & STOPPED)
		bufp->ps_state = T_STATE;	/* stopped (traced) */
	else if (PROC[p_ki].p_flags == 0)
		bufp->ps_state = R_STATE;	/* in run-queue */
	else if (MPROC[p_nr].mp_flags & (WAITING | PAUSED) ||
		 FPROC[p_nr].fp_suspended == SUSPENDED)
		bufp->ps_state = S_STATE;	/* sleeping */
	else
		bufp->ps_state = W_STATE;	/* a short wait */
  } else {			/* tasks are simple */
	if (PROC[p_ki].p_flags == 0)
		bufp->ps_state = R_STATE;	/* in run-queue */
	else
		bufp->ps_state = W_STATE;	/* other i.e. waiting */
  }

  bufp->ps_tsize = (size_t) PROC[p_ki].p_map[T].mem_len << CLICK_SHIFT;
  bufp->ps_dsize = (size_t) PROC[p_ki].p_map[D].mem_len << CLICK_SHIFT;
  bufp->ps_ssize = (size_t) PROC[p_ki].p_map[S].mem_len << CLICK_SHIFT;
  bufp->ps_vtext = (off_t) PROC[p_ki].p_map[T].mem_vir << CLICK_SHIFT;
  bufp->ps_vdata = (off_t) PROC[p_ki].p_map[D].mem_vir << CLICK_SHIFT;
  bufp->ps_vstack = (off_t) PROC[p_ki].p_map[S].mem_vir << CLICK_SHIFT;
  bufp->ps_text = (off_t) PROC[p_ki].p_map[T].mem_phys << CLICK_SHIFT;
  bufp->ps_data = (off_t) PROC[p_ki].p_map[D].mem_phys << CLICK_SHIFT;
  bufp->ps_stack = (off_t) PROC[p_ki].p_map[S].mem_phys << CLICK_SHIFT;

  bufp->ps_recv = PROC[p_ki].p_getfrom;

  bufp->ps_utime = PROC[p_ki].user_time;
  bufp->ps_stime = PROC[p_ki].sys_time;

  bufp->ps_istackframe = (char *)(MPROC[p_nr].mp_procargs 
			    - (PROC[p_ki].p_map[S].mem_vir << CLICK_SHIFT)
			    + (PROC[p_ki].p_map[S].mem_phys << CLICK_SHIFT));

  if (bufp->ps_state == Z_STATE)
	bufp->ps_args = "<defunct>";
  else if (p_nr > INIT_PROC_NR)
	bufp->ps_args = get_args(bufp);

  return 0;
}

/* Addrread reads nbytes from offset addr to click base of fd into buf. */
int addrread(fd, base, addr, buf, nbytes)
int fd;
phys_clicks base;
vir_bytes addr;
char *buf;
int nbytes;
{
  if (lseek(fd, ((long) base << CLICK_SHIFT) + (long) addr, 0) < 0)
	return -1;

  return read(fd, buf, nbytes);
}

void usage(pname)
char *pname;
{
  fprintf(stderr, "Usage: %s [-][alxU] [kernel mm fs]\n", pname);
  exit(1);
}

void err(s)
char *s;
{
  extern int errno;

  if (errno == 0)
	fprintf(stderr, "%s\n", s);
  else
	perror(s);

  exit(2);
}

/* Fill ttyinfo by fstatting character specials in /dev. */
int gettynames(ttyinfo)
ttyinfo_t ttyinfo[];
{
  static char dev_path[] = "/dev/";
  DIR *dev_dir;
  struct dirent *ent;
  struct stat statbuf;
  static char path[sizeof(dev_path) + NAME_MAX];
  int index;

  if ((dev_dir = opendir(dev_path)) == NULL) return -1;

  index = 0;
  while ((ent = readdir(dev_dir)) != NULL) {
	strcpy(path, dev_path);
	strcat(path, ent->d_name);
	if (stat(path, &statbuf) == -1 || !S_ISCHR(statbuf.st_mode))
		continue;
	if (index >= N_TTYINFO) err("Can't keep info on all ttys in /dev");
	ttyinfo[index].tty_dev = statbuf.st_rdev;
	strcpy(ttyinfo[index].tty_name, ent->d_name);
	index++;
  }

  return 0;
}

/* Outdates returns true iff non-stripped file was modified later than time. */
int outdates(file, tm)
char *file;
time_t tm;
{
  int fd;
  struct exec hd;
  struct stat buf;

  if ((fd = open(file, O_RDONLY)) == -1) return 0;

  if (read(fd, (char *) &hd,
	 sizeof(struct exec)) != sizeof(struct exec) ||
      fstat(fd, &buf) == -1) {
	(void) close(fd);
	return 0;
  }
  (void) close(fd);

  return !BADMAG(hd) && hd.a_syms != 0 && buf.st_mtime > tm;
}
