/* format -  floppy disk format (PC-MINIX)          Author: D. Chapman */

/* Author: Donald E. Chapman.      Copyright 1991 by Donald E. Chapman
 *
 * Usage: format [-q][-a][special [kbsize]][-v [dosvollabel]]
 *
 * Flags: -q: quiet will skip the "are you sure" prompt.
 *	  -v: volume label added along with structures Dos needs.
 *	      If no label is given format will prompt for it.
 *	  -a: sort interactive list alphabetically rather than by size.
 *
 *	     Format allows super user to format disks.  If it is suid
 *	  then it allows others to format if they are at the console.
 *	     Format will format all seven of the non-automatic
 *	  disk/media combinations that PC-Minix supports.  It will
 *	  also try to format automatics (minor 0 through 3) if a device
 *	  size was given when the node was made.  Format usually expects
 *	  special devices to have sizes.
 *	     Format will optionally add the structures Dos needs,
 *	  including a "non-bootable" message, if either the device
 *	  name is a dos device, like dosA, or if the -v flag is used.
 *	     If a special device is specified in the command line then
 *	  a size may also be specified there.  If a size is specified
 *	  in the command line the size must agree with the capability
 *	  of the special device.
 *	     If no special device is specified in the command line
 *	  format will interactively help you to determine which of
 *	  your drives you wish to format on.  It actually allows
 *	  the automatic devices to be formatted that way even if
 *	  they were made with a size of zero.
 *
 * Warning:  Some disk drives are media sensitive and on them you
 *	  should use DD or HD media as appropriate for the density
 *	  you are formatting.  For some media sensitive drives if
 *	  you try to format media of the wrong density the disk
 *	  can not be formatted correctly.
 *
 * Examples:	format /dev/at0			format disk in /dev/at0.
 *		format /dev/at0 360		format disk to size 360.
 *		format /dev/dosA		format for Dos.
 *		format /dev/fd1 -v DOS_DISK	format labeled Dos disk.
 *		format				interactive mode formatting.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <minix/config.h>
#include <minix/const.h>
#ifndef _V15_
#include <minix/minlib.h>
#endif
#include <dirent.h>
#include <ctype.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>

/* Kludges */
#ifdef _V15_
#define mknod4 mknod
#endif

/* Macros */
#define maj(d)  ( ((d) >> MAJOR) & BYTE)
#define min(d)  ( ((d) >> MINOR) & 0x7F)	/* ignore format bit */
#define illegal(s) ((s)!=360 && (s)!=720 && (s)!=1200 && (s)!=1440)

/* For the list of existing drives */
struct drive {
  dev_t device;
  int dos_type;
  int is_auto;
  off_t msize;
  char *name;
  struct drive *next;
};

struct param_struct {		/* Must align with diskparm.h */
  unsigned char s1;
  unsigned char s2;
  unsigned char motor_off;
  unsigned char sec_size_code;	/* Needed */
  unsigned char sec_per_cyl;	/* Needed */
  unsigned char gap;
  unsigned char data_len;
  unsigned char gap_fmt;	/* Needed */
  unsigned char fill_byte;	/* Needed */
  unsigned char head_settle;
  unsigned char motor_start;
};

struct table_entry {
  unsigned char cyl;
  unsigned char head;
  unsigned char sector;
  unsigned char sec_size_code;
};

/* Some constants */
#define NULL_L		(struct drive *)NULL
#define NULL_D		(struct dirent *)NULL
#define SEC_SIZE	 512
#define NR_HEADS	   2
#define F_MAJOR		   2
#define F_DRIVE_BITS	0x03
#define F_BIT		0x80
#define UNLINK		TRUE
#define OMIT		FALSE
#define NAME_MAX	  14	/* was in limit.h strangely */
#define PATH_MAX	 255
#define FAIL_MOUNTED	TRUE
#define NOT_MOUNTED	FALSE

#define FAIL_OPEN	   1
#define FAIL_SEEK	   2
#define FAIL_WRITE	   4
#define MAX_TRIES	  10

#ifndef CONSOLE
#define CONSOLE         "/dev/tty0"
#endif

/* Constants for DOS */
#define BOOT_SIZE	sizeof(non_boot)
#define VE_SIZE		sizeof(vol_entry)
#define VEL_SIZE	sizeof(vol_entry.label)
#define BLANKS		"           "	/* max 11 */
#define SMALL_DIR	0x70
#define LARGE_DIR	0xE0
#define DIRENT_SIZE	  32
#define VOL_ARC		0x28
#define Y_SHIFT		   9
#define MON_SHIFT	   5
#define H_SHIFT		  11
#define MIN_SHIFT	   5
#define SEC_DIV		   2
#define DOS_BASE_YEAR	  80
#define FAT_SIZE	   3
#define MAGIC_SIZE	   5
#define MAGIC_LOC	 510
#define NO_FAILURE	   0
#define FAIL_D0		   1
#define FAIL_DCLEAR	   2
#define FAIL_DFAT2	   4
#define FAIL_DVOL	   8
#define FAIL_DFAT1	  16

static char version[] = {"format V1.2, Copyright 1991 by Donald E. Chapman"};

/* Globals */
struct param_struct param;

unsigned char non_boot[113] = {	/* hand constructed with loving care */
	 0xEB, 0x50, 0x90, 0x4D, 0x49, 0x4E, 0x49, 0x58, 0x66, 0x31,
	 0x31, 0x00, 0x02, 0x01, 0x01, 0x00, 0x02, 0xE0, 0x00, 0x60,
	 0x09, 0xF9, 0x07, 0x00, 0x0F, 0x00, 0x02, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x0D, 0x0A, 0x4E, 0x6F, 0x74, 0x20, 0x61, 0x20,
	 0x73, 0x79, 0x73, 0x74, 0x65, 0x6D, 0x20, 0x64, 0x69, 0x73,
	 0x6B, 0x2E, 0x20, 0x52, 0x65, 0x70, 0x6C, 0x61, 0x63, 0x65,
	 0x20, 0x61, 0x6E, 0x64, 0x20, 0x70, 0x72, 0x65, 0x73, 0x73,
	 0x20, 0x61, 0x6E, 0x79, 0x20, 0x6B, 0x65, 0x79, 0x2E, 0x0D,
	 0x0A, 0x00, 0xFA, 0x33, 0xC0, 0x8E, 0xD0, 0x8E, 0xD8, 0xBC,
	 0x00, 0x7C, 0xBE, 0x20, 0x7C, 0xFC, 0xAC, 0x3C, 0x00, 0x74,
	 0x06, 0xB4, 0x0E, 0xCD, 0x10, 0xEB, 0xF5, 0x33, 0xC0, 0xCD,
		       0x16, 0xCD, 0x19
};

/* Magic number for DOS and fat bytes */
static unsigned char f_bytes[5] = {0x55, 0xAA, 0xF9, 0xFF, 0xFF};

/* Volume label and date */
struct vol_entry {
  char label[11];
  unsigned char attrib;
  char reserv[10];
  short time;
  short date;
} vol_entry;
static char vol_temp[20] = {'\0'};

/* Formatting work area */
static union u {
  char buffer[BLOCK_SIZE];
  struct p {
	struct table_entry	/* max 128 */
	 table[BLOCK_SIZE / 2 / sizeof(struct table_entry)];
	struct param_struct param;
  } p;
} u;

/* MINIX Drive/media combinations (automatics are mapped.)
 * minor 0-3 4-7  8-11 12-15  12-15  16-19  16-19 20-23  24-27  28-31
 * dtype  0   1    2     3      3      4      4     5      6      7
 * drv.  au. 360 1.2M   720   1.44M  720   1.44M   1.2M   1.2M  1.44M
 * med.  au. 360 1.2M   360    360   720    720    360    720   1.44M
 */

int dsize[] = {0, 360, 1200, 720, 720, 1200, 1200, 1440};
unsigned char cyl_table[] = {0, 39, 79, 39, 79, 39, 79, 79};
unsigned char gap_fmt_table[] = {0, 0x50, 0x54, 0x50, 0x50, 0x50, 0x50, 0x54};
unsigned char media_id[] = {0, 0xFD, 0xF9, 0xFD, 0xF9, 0xFD, 0xF9, 0xF0};
unsigned char sector_table[] = {0, 9, 15, 9, 9, 9, 9, 18};
unsigned char sec_cluster[] = {0, 2, 1, 2, 2, 2, 2, 1};
unsigned char sec_fat[] = {0, 2, 7, 2, 3, 2, 3, 9};
unsigned char dir_size[] = {0, SMALL_DIR, LARGE_DIR, SMALL_DIR,
		    SMALL_DIR, SMALL_DIR, SMALL_DIR, LARGE_DIR};

/* Chart of dtype mapping. 3.5 inch 1440's serve as 720's too.
 *                   dsize
 * map[u][m]    360 720 1200 1440
 * usize  360    1   3    5    3
 * usize  720    .   4    6    4
 * usize 1200    .   .    2    .
 * usize 1440    .   .    .    7
 */
int dtype_map[4][4] = {1, 3, 5, 3, -1, 4, 6, 4, -1, -1, 2, -1, -1, -1, -1, 7};

char *progname, *nname;
int interrupt = FALSE;		/* If sigint or sigquit occurs	 */
int a_flag = FALSE;		/* Don't sort alphabetically */
int v_flag = 0;			/* Can be -1, 0 or +n */
int dos_type = FALSE;
int interactive = TRUE;
int max_cyl;
int max_len;			/* Length of longest special name */
int msize = 0;			/* usually holds media size in Kb */
int usize = 0;			/* User specified size in Kb */
off_t dir_start;
off_t dir_max;
off_t second_fat;

_PROTOTYPE(void usage, (void));
_PROTOTYPE(void quit, (char *mess, char *more, int linked));
_PROTOTYPE(void add_to_list, (struct drive ** h, struct drive * f));
_PROTOTYPE(void make_list, (struct drive ** drive_list));
_PROTOTYPE(struct drive * make_selection, (struct drive * list_head));
_PROTOTYPE(struct drive * command_line_check, (struct drive * dp));
_PROTOTYPE(void set_parameters, (int dtype));
_PROTOTYPE(void adjust_bootblock, (int dtype, int msize));
_PROTOTYPE(int mounted, (char *dname));
_PROTOTYPE(void Sigint, (int s));
_PROTOTYPE(void Sigquit, (int s));
_PROTOTYPE(void prepare_node, (Dev_t device));
_PROTOTYPE(int format_track, (int cyl, int head, int logical));
_PROTOTYPE(int full_format, (void));
_PROTOTYPE(int add_vol_label, (int fd));
_PROTOTYPE(int add_structures, (char *dname, Dev_t device));
_PROTOTYPE(int test_error, (char *dname));
_PROTOTYPE(int main, (int argc, char *argv[]));


void usage()
{
  fprintf(stderr,
	"\nUsage: %s [-q][-a][special [kbsize]][-v [dosvollabel]]\n",
	progname);
  exit(EXIT_FAILURE);
}

void quit(mess, more, linked)
char *mess;
char *more;
int linked;
{
  if (linked) unlink(nname);	/* remove temporary */
  fprintf(stderr, "\n%s: %s%s.\n", progname, mess, more);
  exit(EXIT_FAILURE);
}

void add_to_list(h, f)		/* by sizes or alphabetic */
struct drive **h, *f;
{
  /* At the tail */
  if ((*h) == NULL_L) {
	f->next = (*h);
	(*h) = f;
  } else if ((a_flag && strcmp((*h)->name, f->name) > 0) ||
	     (!a_flag && ((*h)->msize > f->msize) ||
		       ((*h)->msize == f->msize && (
					 (*h)->device > f->device ||
					((*h)->device == f->device &&
				       (*h)->dos_type > f->dos_type)))
		       )
	) {
	/* Amid the list */
	f->next = *h;
	(*h) = f;
  } else			/* Recursion */
	add_to_list(&((*h)->next), f);
}

/* Make a list of existing floppy drives found in /dev */
void make_list(drive_list)
struct drive **drive_list;
{
  struct drive *dp;
  DIR *dd1;
  struct stat dstat;
  struct dirent *dentry;
  char *dname;
  int sres, nlen;

  max_len = 0;
  dd1 = opendir("/dev");
  while ((dentry = readdir(dd1)) != NULL_D) {
	if (dentry->d_name[0] != '.') {
		dname = (char *) malloc(strlen(dentry->d_name) + 6);
		strcpy(dname, "/dev/");
		strcat(dname, dentry->d_name);
		sres = stat(dname, &dstat);
		if (sres >= 0
		    && S_ISBLK(dstat.st_mode)
		    && maj(dstat.st_rdev) == F_MAJOR) {
			dp = (struct drive *) malloc(sizeof(struct drive));
			dp->device = dstat.st_rdev;
			dp->msize = dstat.st_size / BLOCK_SIZE;
			dp->name = dname;
			nlen = strlen(dp->name);
			if (nlen > max_len) max_len = nlen;
			dp->dos_type = !strncmp(dentry->d_name, "dos", 3);
			if (!((dstat.st_rdev >> 2) & 0x1F))
				dp->is_auto = TRUE;
			else
				dp->is_auto = FALSE;
			add_to_list(drive_list, dp);
		} else
			free(dname);	/* no leaks */
	}
  }
  closedir(dd1);
}				/* make list */

/* Interactive query routine */
struct drive
*make_selection(list_head)
struct drive *list_head;
{
  static char insize[10] = {'\0'};
  static char choice[5] = {'\0'};
  struct drive *dp;
  int i, m, n, minor, drnr, dtype, fbit;
  dev_t device;

  if (list_head == NULL_L)
	quit("Use mknod with specific minor devices first", "", OMIT);
  printf("\nYour floppy drives:\n\n");
  for (dp = list_head, i = 1;
       dp != NULL_L;
       dp = dp->next, i++) {
	device = dp->device;
	fbit = device & F_BIT;
	minor = min(device);
	drnr = minor & F_DRIVE_BITS;
	dtype = minor >> 2;
	msize = dp->msize;
	if (dp->is_auto) printf(
		       "Number %2d  %-*s     ? Kb on %4s KB drive %d%s Auto.%s\n",
		    i, max_len, dp->name, msize ? itoa(msize) : "?",
		       drnr, dp->dos_type ? " Dos" : "    ",
		       fbit ? " Fbit" : "");
	else
		printf("Number %2d  %-*s  %4d Kb on %4d KB drive %d%s      %s\n",
		       i, max_len, dp->name, msize, dsize[dtype],
		       drnr, dp->dos_type ? " Dos" : "    ",
		       fbit ? " Fbit" : "");
  }
  m = i - 1;
  printf("\nWhich do you wish to format (number)? ");
  fflush(stdout);
  fgets(choice, 4, stdin);
  n = atoi(choice);
  if (n < 1 || n > m) exit(EXIT_FAILURE);
  for (dp = list_head, i = 1; i != n && dp != NULL_L; dp = dp->next, i++);
  msize = dp->msize;
  if (dp->is_auto) {
	/* Try to determine the drive type */
	printf("This is an automatic drive.\n");
	printf("Format how many Kb (360 720 1200 1440) ? ");
	fflush(stdout);
	fgets(insize, 7, stdin);
	usize = atoi(insize);
	if (illegal(msize)) {
		printf("What is the drive size (360 720 1200 1440) ? ");
		fflush(stdout);
		fgets(insize, 7, stdin);
		msize = atoi(insize);
	}
  }
  return(dp);
}				/* make selection */

/* Check the name and size from the command line */
struct drive
*command_line_check(dp)
struct drive *dp;
{
  struct stat dstat;
  char *ptr;
  int fd;
  int dtype;

  if (dp == NULL_L) usage();	/* at least special */

  /* Check spelling of name for DOS */
  ptr = (char *) (dp->name + strlen(dp->name));
  while (*ptr != '/' && ptr > dp->name) ptr--;
  if (*ptr == '/') ptr++;

  /* Also perhaps there was -v after special name */
  dp->dos_type = dos_type || !strncmp(ptr, "dos", 3);
  fd = open(dp->name, O_RDONLY);
  if (fd < 0) quit("Can't open ", dp->name, OMIT);
  if (fstat(fd, &dstat) != 0) quit("Can't stat ", dp->name, OMIT);
  close(fd);
  if (!S_ISBLK(dstat.st_mode) || maj(dstat.st_rdev) != F_MAJOR)
	quit("Not a floppy ", dp->name, OMIT);
  dp->device = dstat.st_rdev;
  dtype = min(dp->device) >> 2;
  msize = dp->msize = dstat.st_size / BLOCK_SIZE;
  if (!usize) usize = msize;

  /* Auto will set device as determined by usize and msize */
  if (!dtype) {
	if (!msize) quit("Device size unknown. Mknod with size, ",
		     dp->name, OMIT);
	dp->is_auto = TRUE;
  } else {
	dp->is_auto = FALSE;
	if (!msize || msize != usize)
		quit("Size error, drive is ", itoa(msize), OMIT);
  }
  return(dp);
}				/* command_line_check */

/* Set the values for formatting parameters */
void set_parameters(dtype)
int dtype;
{
  param.sec_size_code = (unsigned char) 2;
  param.sec_per_cyl = sector_table[dtype];	/* sec/cyl */
  param.gap_fmt = gap_fmt_table[dtype];
  param.fill_byte = (unsigned char) 0xF6;	/* traditional */
}

/* Put proper values into the boot block header */
void adjust_bootblock(dtype, msize)
int dtype;
int msize;
{
  non_boot[0x0D] = sec_cluster[dtype];	/* sec/cluster */
  non_boot[0x11] = dir_size[dtype];
  non_boot[0x13] =
	(unsigned char) ((msize * (BLOCK_SIZE / SEC_SIZE)) % 256);
  non_boot[0x14] =
	(unsigned char) ((msize * (BLOCK_SIZE / SEC_SIZE)) / 256);
  non_boot[0x15] = media_id[dtype];	/* media id. */
  non_boot[0x16] = sec_fat[dtype];	/* sec/fat */
  non_boot[0x18] = sector_table[dtype];	/* sec/cyl */
}

/* See if drive is already mounted by any name. */
int mounted(dname)
char *dname;
{
  int n, ddrive, mdrive;
  struct stat dstat, mstat;
#ifdef _V15_
  FILE *fp;
  char line[60],mname[20];
#else
  char mdevice[PATH_MAX + 1], mounted[PATH_MAX + 1], vers[5], flag[15];
#endif
  /* Device may be mounted under a pseudonym */
  stat(dname, &dstat);
  ddrive = dstat.st_rdev & F_DRIVE_BITS;
#ifdef _V15_
  if((fp = fopen("/etc/mtab","r")) == (FILE *) NULL) return(FAIL_MOUNTED);
  while (TRUE) {
      fgets(line,59,fp);
      if(feof(fp)) break;
      sscanf(line,"%s",mname);
      stat(mname,&mstat);
	if (maj(mstat.st_rdev) != F_MAJOR) continue;
	mdrive = mstat.st_rdev & F_DRIVE_BITS;
	if (ddrive == mdrive)
		quit("Won't format, drive is mounted as ", mname, OMIT);
  }
  fclose(fp);
  return(NOT_MOUNTED);
#else
  if (load_mtab(progname) < 0) return(FAIL_MOUNTED);
  while (TRUE) {
	n = get_mtab_entry(mdevice, mounted, vers, flag);
	if (n < 0) return(NOT_MOUNTED);
	stat(mdevice, &mstat);
	if (maj(mstat.st_rdev) != F_MAJOR) continue;
	mdrive = mstat.st_rdev & F_DRIVE_BITS;
	if (ddrive == mdrive)
		quit("Won't format, drive is mounted as ", mdevice, OMIT);
  }
#endif
}

void Sigint(s)
int s;
{
  interrupt = SIGINT;
}


void Sigquit(s)
int s;
{
  interrupt = SIGQUIT;
}


/* Make a temporary formatting node.  It will be deleted after use. */
void prepare_node(device)
dev_t device;
{
  int fd, mres;
  struct stat dstat;
  char *ptr;

  /* Make a temporary name. Chop progname of excessive renaming. */
  ptr = (char *) progname + strlen(progname);
  while (*ptr != '/' && ptr > progname) ptr--;
  if (*ptr == '/') ptr++;	/* Elide path */
  nname = (char *) malloc(strlen(ptr) < 8 ? strlen(ptr) + 12 : 20);
  strcpy(nname, "/tmp/");
  strncat(nname, ptr, 8);
  strcat(nname, "XXXXXX");
  mktemp(nname);
  mres = mknod4(nname, S_IFBLK|S_IWUSR|S_IRUSR, device | F_BIT, (long) msize);
  if (mres) quit("Can't make temporary node ", nname, OMIT);

  /* As a pre-check open and stat the node */
  fd = open(nname, O_WRONLY | O_EXCL);
  if (fd < 0) quit("Can't open temporary node ", nname, UNLINK);
  if (fstat(fd, &dstat) != 0) {
	close(fd);
	quit("Can't stat temporary node ", nname, UNLINK);
  }
  close(fd);
}				/* prepare_node */

/* Format one track */
int format_track(cyl, head, logical)
int cyl, head, logical;
{
  int errnum = 0;
  int physical;
  off_t cyl_start;
  int fd;

  /* Setup to do one head */
  memset(u.buffer, 0, BLOCK_SIZE);
  u.p.param = param;
  for (physical = 0; physical <= param.sec_per_cyl; ++physical) {
	u.p.table[physical].cyl = (unsigned char) cyl;
	u.p.table[physical].head = (unsigned char) head;
	u.p.table[physical].sector = (unsigned char) (physical + 1);
	u.p.table[physical].sec_size_code = param.sec_size_code;
  }

  /* Need to set only the cylinder and head for the drive with a seek.
   * Close enough: */
  cyl_start = (((off_t) logical) / 2) * 2 * SEC_SIZE;
  if ((fd = open(nname, O_WRONLY)) < 0) errnum |= FAIL_OPEN;
  if (lseek(fd, cyl_start, SEEK_SET) != cyl_start) {
	printf("Seek error: cyl %d head %d.\n", cyl, head);
	errnum |= FAIL_SEEK;
  }
  if (write(fd, u.buffer, (unsigned) BLOCK_SIZE) != BLOCK_SIZE) {
	printf("Write error: cyl %d head %d.\n", cyl, head);
	errnum |= FAIL_WRITE;
  }
  close(fd);			/* Don't need sync. This will do. */
  return(errnum);
}				/* format_track */

/* Format all of the tracks on the disk */
int full_format()
{
  int cyl;
  int head;
  int logical;
  int errnum = 0;
  int tries;

  for (cyl = 0, logical = 1; cyl <= max_cyl && !errnum; cyl++) {
	/* Check for interrupt */
	if (interrupt) quit("Interrupted..", "", UNLINK);

	/* Go ahead and do both heads  */
	for (head = 0; head < NR_HEADS && !errnum; ++head) {
		printf("\r%s: Cyl. %2d, Head %1d.  ", progname, cyl, head);
		fflush(stdout);
		tries = 0;
		errnum = format_track(cyl, head, logical);

		/* Were FS slots busy for other processes? */
		while (errnum & FAIL_OPEN && tries++ < MAX_TRIES) {
			printf("Retry: cyl %d head %d.\n", cyl, head);
			fflush(stdout);
			if (interrupt) quit("Interrupted..", "", UNLINK);
			sleep(1);
			errnum = format_track(cyl, head, logical);
		}
		logical += param.sec_per_cyl;
	}			/* for head */
  }				/* for  cyl */
  return(errnum);
}				/* full_format */

/* Put label and date in directory for Dos */
int add_vol_label(fd)
int fd;
{
  char *c;
  time_t now;
  struct tm *local;

  /* Use a different label next time */
  if (v_flag) v_flag = -1;

  /* Massage and pad the volume label */
  vol_temp[11] = '\0';		/* Guard */
  c = vol_temp;
  while (*c != '\0') {
	if (*c == '\n' || *c == '\r') {
		*c = '\0';
		break;
	} else if (!isalpha(*c) && !isdigit(*c) && *c != '.')
		*c = '_';
	else if (islower(*c))
		*c = toupper(*c);
	c++;
  }
  if (strlen(vol_temp) < VEL_SIZE)
	strncat(vol_temp, BLANKS, VEL_SIZE - strlen(vol_temp));
  strncpy(vol_entry.label, vol_temp, sizeof(vol_entry.label));
  vol_entry.attrib = VOL_ARC;
  time(&now);
  local = localtime(&now);
  vol_entry.date = (local->tm_year - DOS_BASE_YEAR) << Y_SHIFT;
  vol_entry.date += (local->tm_mon + 1) << MON_SHIFT;
  vol_entry.date += local->tm_mday;
  vol_entry.time = (local->tm_hour) << H_SHIFT;
  vol_entry.time += (local->tm_min) << MIN_SHIFT;
  vol_entry.time += local->tm_sec / SEC_DIV;
  if (lseek(fd, dir_start, SEEK_SET) != dir_start) return(FAIL_DVOL);
  if (write(fd, (char *) &vol_entry, VE_SIZE) != VE_SIZE) return (FAIL_DVOL);
  return(0);			/* No errors, errnum == 0  */
}				/* add_vol_label */

/* Add the structures Dos needs to the disk. */
int add_structures(dname, device)
char *dname;
dev_t device;
{
  int fd;
  int errnum;
  off_t dos_position;
  off_t to_do;
  int written;
  int do_label;

  /* If special has format bit set it can't write normally */
  if (device & F_BIT)
	quit("Format Bit Set, Can't add structures Dos needs, rm ",
	     dname, UNLINK);
  printf("\nAdding structures Dos needs.  ");
  if (interactive || v_flag < 0) {
	printf("\nEnter Volume Label (11 or less) : ");
	fflush(stdout);
	fgets(vol_temp, 19, stdin);
	if (vol_temp[0] == '\n')
		do_label = FALSE;
	else
		do_label = TRUE;
	printf("\n");
  }
  if ((fd = open(dname, O_WRONLY)) < 0)
	quit("Can't open to write structures Dos needs ",
	     dname, UNLINK);

  /* Write the boot block */
  if (lseek(fd, (off_t) 0, SEEK_SET) != (off_t) 0) return(FAIL_D0);
  dos_position = write(fd, (char *) non_boot, BOOT_SIZE);
  if (dos_position != (off_t) BOOT_SIZE) return(FAIL_D0);

  /* Round out the first block, then zero enough of them */
  memset(u.buffer, 0, BLOCK_SIZE);
  to_do = (off_t) BLOCK_SIZE - dos_position;
  while (to_do > (off_t) 0) {
	if (interrupt) quit("Interrupted..", "", UNLINK);
	written = write(fd, u.buffer, (int) to_do);
	if (written != (int) to_do) return(FAIL_DCLEAR);
	dos_position += written;
	/* Do whole blocks */
	to_do = dir_max - dos_position;
	if (to_do > (off_t) BLOCK_SIZE) to_do = (off_t) BLOCK_SIZE;
  }

  /* Add volume label and date */
  if (do_label || v_flag > 0) {
	errnum = add_vol_label(fd);
	if (errnum) return(errnum);
  }
  if (second_fat) {
	dos_position = lseek(fd, second_fat, SEEK_SET);
	if (dos_position != second_fat) return(FAIL_DFAT2);
	written = write(fd, (char *) &f_bytes[2], FAT_SIZE);
	if (written != FAT_SIZE) return(FAIL_DFAT2);
  }

  /* Do DOS magic number and first fat */
  dos_position = lseek(fd, (off_t) MAGIC_LOC, SEEK_SET);
  if (dos_position != (off_t) MAGIC_LOC) return(FAIL_DFAT1);
  written = write(fd, (char *) f_bytes, MAGIC_SIZE);
  if (written != MAGIC_SIZE) return(FAIL_DFAT1);
  close(fd);			/* close dos structure device */
  return(NO_FAILURE);
}				/* add_structures */

/* Try to read block 0 from the disk */
int test_error(dname)
char *dname;
{
  int fd;
  if ((fd = open(dname, O_RDONLY)) < 0) return(TRUE);
  if (lseek(fd, (off_t) 0, SEEK_SET) != (off_t) 0) {
	close(fd);
	return(TRUE);
  }
  if (read(fd, u.buffer, BLOCK_SIZE) != BLOCK_SIZE) {
	close(fd);
	return(TRUE);
  }
  close(fd);
  return(FALSE);
}

int main(argc, argv)
int argc;
char *argv[]; {

  int q_flag = FALSE;
  char *dname;
  static char answer[4] = {'n'};
  struct drive *dp = NULL_L;
  struct drive *drive_list = NULL_L;
  int is_auto = FALSE;
  dev_t device;
  int minor, dtype, drnr;
  int i, errnum;

  progname = argv[0];		/* this became a variable */

/* Su should decide who is allowed to format. Normally only su should format.
 * Plucky su's could suid it however...
 */
  if (geteuid() != 0) quit("Must be su to format", "", OMIT);

  /* Normally only from the console. */
  if (strcmp(ttyname(0), CONSOLE)) {
	/* Suid'ed user must be at console. */
	if (getuid() != 0) quit("Only from the console", "", OMIT);
	/* Su can make big mistakes. :-) */
  }

  /* Get command line parameters if there are some. */
  for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
		if (argv[i][1] == 'v') {
			dos_type = TRUE;
			if (argc - 1 - i > 0) {
				v_flag = i + 1;
				strncpy(vol_temp, argv[v_flag], 11);
				i++;
			} else
				v_flag = -1;
		}
		if (argv[i][1] == 'q') q_flag = TRUE;
		if (argv[i][1] == 'a') a_flag = TRUE;
	} else if (isdigit(argv[i][0])) {
		interactive = FALSE;
		msize = usize = atoi(argv[i]);
	} else if (isalpha(argv[i][0]) ||
		   argv[i][0] == '/' ||
		   argv[i][0] == '.') {
		/* Name or path */
		interactive = FALSE;
		if (dp != NULL_L) free(dp);
		dp = (struct drive *) malloc(sizeof(struct drive));
		dp->name = (char *) malloc(strlen(argv[i]) + 1);
		strcpy(dp->name, argv[i]);
	} else
		usage();
  }

  if (interactive) {
	make_list(&drive_list);
	dp = make_selection(drive_list);
  } else
	dp = command_line_check(dp);

  if (illegal(msize)) quit("Can't do drive size ", itoa(msize), OMIT);

  if (dp->is_auto) {
	is_auto = TRUE;
	if (illegal(usize)) quit("Can't do size ", itoa(usize), OMIT);
	dtype = dtype_map[usize / 360 - 1][msize / 360 - 1];
	if (dtype == -1) quit("Invalid size for drive ", dp->name, OMIT);
	dp->device |= (dtype << 2);
	msize = dp->msize = usize;
  }
  device = dp->device;
  minor = min(device);
  drnr = minor & F_DRIVE_BITS;
  dtype = minor >> 2;
  dos_type = dp->dos_type || v_flag;
  dname = dp->name;

  set_parameters(dtype);

  max_cyl = cyl_table[dtype];

  if (dos_type) {
	adjust_bootblock(dtype, msize);
	f_bytes[2] = non_boot[0x15];	/* media id. for fat */
	second_fat = SEC_SIZE *
		(unsigned int) non_boot[0x16] + SEC_SIZE;
	dir_start = (off_t) non_boot[0x16] * SEC_SIZE + second_fat;
	dir_max = (off_t) non_boot[0x11] * DIRENT_SIZE + dir_start;
  }

  /* Check /etc/mtab */
  if (mounted(dname)) exit(EXIT_FAILURE);

  /* Trap interrupts to allow cleanup of temporary node */
  if (signal(SIGINT, SIG_IGN) != SIG_IGN) {
	signal(SIGINT, Sigint);
	signal(SIGQUIT, Sigquit);
  }
  prepare_node(device);

  if (interactive)
	printf("%s  %s%4d Kb on %4d Kb %sdrive %d (cyl 0-%d, sec 1-%d)\n",
	       dname, dos_type ? "Dos " : "", msize,
	       dsize[dtype], is_auto ? "Auto. " : "", drnr,
	       max_cyl, param.sec_per_cyl);

  if (interactive || !q_flag) {
	printf("Are you sure (y/n) ? ");
	fflush(stdout);
	fgets(answer, 3, stdin);
	if (answer[0] != 'y') {
		unlink(nname);
		exit(EXIT_FAILURE);
	}
	printf("\n");
  }
  do {				/* While answer == 'y' */

	errnum = full_format();
	if (errnum) quit("Format errors on ", dname, UNLINK);

	if (test_error(dname))
		quit("Wrong media? Format failed on ", dname, UNLINK);

	if (dos_type) {
		errnum = add_structures(dname, device);
		if (errnum) quit("Failed to add structures Dos needs, ",
			     itoa(errnum), UNLINK);
	}
	if (interactive || !q_flag) {
		printf("\nAnother %s%d Kb on drive %d (y/n) ? ",
		       dos_type ? "Dos Disk " : "", msize, drnr);
		fflush(stdout);
		fgets(answer, 3, stdin);
	} else {
		printf("\n");
		answer[0] = 'n';
	}
  } while (answer[0] == 'y');
  unlink(nname);
  exit(EXIT_SUCCESS);
}
