/* dos{dir|read|write} - {list|read|write} MS-DOS disks	 Author: M. Huisjes */

/* Dosdir - list MS-DOS directories. doswrite - write stdin to DOS-file
 * dosread - read DOS-file to stdout
 *
 * Author: Michiel Huisjes.
 *
 * Usage: dos... [-lra] drive [file/dir]
 *	  l: Give long listing.
 *	  r: List recursively.
 *	  a: Set ASCII bit.
 */

#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define DRIVE		"/dev/dosX"
#define DRIVE_NR	8

#define MAX_CLUSTER_SIZE	4096
#define MAX_ROOT_ENTRIES	512
#define FAT_START		512L	/* After bootsector */
#define ROOTADDR		(FAT_START + 2L * fat_size)
#define clus_add(cl_no)		((long) (((long) cl_no - 2L) \
				* (long) cluster_size \
				+ data_start \
			       ))
struct dir_entry {
  unsigned char d_name[8];
  unsigned char d_ext[3];
  unsigned char d_attribute;
  unsigned char d_reserved[10];
  unsigned short d_time;
  unsigned short d_date;
  unsigned short d_cluster;
  unsigned long d_size;
};

typedef struct dir_entry DIRECTORY;

#define NOT_USED	0x00
#define ERASED		0xE5
#define DIR		0x2E
#define DIR_SIZE	(sizeof (struct dir_entry))
#define SUB_DIR		0x10
#define NIL_DIR		((DIRECTORY *) 0)

#define LAST_CLUSTER	0xFFFF
#define MASK		0xFF8
#define MASK16		0xFFF8
#define FREE		0x000
#define BAD		0xFF0
#define BAD16		0xFFF0

typedef int BOOL;

#define TRUE	1
#define FALSE	0
#define NIL_PTR	((char *) 0)

#define DOS_TIME	315532800L	/* 1970 - 1980 */

#define READ			0
#define WRITE			1
#define get_fat(f, b)		buf_read(FAT_START + f, b, 1)

#define put_fat(f, b)		{ disk_io(WRITE, FAT_START + f, b, 1); \
			disk_io(WRITE, FAT_START + f + fat_size, b, 1);}

#define put_fat16(f, b)		{ disk_io(WRITE, FAT_START + f, b, 2); \
			disk_io(WRITE, FAT_START + f + fat_size, b, 2);}

#define get_fat16(f, b)		buf_read(FAT_START + f, b, 2)

#define FIND	3
#define LABEL	4
#define ENTRY	5
#define find_entry(d, e, p)	directory(d, e, FIND, p)
#define list_dir(d, e, f)	(void) directory(d, e, f, NIL_PTR)
#define label()			directory(root, root_entries, LABEL, NIL_PTR)
#define new_entry(d, e)		directory(d, e, ENTRY, NIL_PTR)

#define is_dir(d)		((d)->d_attribute & SUB_DIR)

#define EOF			0400
#define EOF_MARK		'\032'
#define STD_OUT			1
#define flush()			print(STD_OUT, NIL_PTR, 0)

short disk;
unsigned char fat_info;
DIRECTORY root[MAX_ROOT_ENTRIES];
DIRECTORY save_entry;
char null[MAX_CLUSTER_SIZE], *device = DRIVE, path[128];
long data_start, mark;
unsigned short total_clusters, cluster_size, root_entries, sub_entries;
unsigned long fat_size;

BOOL Rflag, Lflag, Aflag, dos_read, dos_write, dos_dir, fat_16 = 0;

char disk_written = 1, buf_buf[1025];
long buf_addr = 0;

/* Prototypes. */
void print_string();		/* HACK.  Should be varargs */
_PROTOTYPE(int leave, (int nr) );
_PROTOTYPE(int usage, (char *prog_name) );
_PROTOTYPE(unsigned c2u2, (unsigned char *ucarray) );
_PROTOTYPE(unsigned long c4u4, (unsigned char *ucarray) );
_PROTOTYPE(int determine, (void));
_PROTOTYPE(int main, (int argc, char *argv []));
_PROTOTYPE(DIRECTORY *directory, (DIRECTORY *dir, int entries, BOOL function, char *pathname) );
_PROTOTYPE(int extract, (DIRECTORY *entry) );
_PROTOTYPE(int print, (int fd, char *buffer, int bytes) );
_PROTOTYPE(int make_file, (DIRECTORY *dir_ptr, int entries, char *name) );
_PROTOTYPE(int fill_date, (DIRECTORY *entry) );
_PROTOTYPE(char *make_name, (DIRECTORY *dir_ptr, int dir_fl) );
_PROTOTYPE(int fill, (char *buffer) );
_PROTOTYPE(int get_char, (void));
_PROTOTYPE(int xmodes, (int mode) );
_PROTOTYPE(int show, (DIRECTORY *dir_ptr, char *name) );
_PROTOTYPE(int free_blocks, (void));
_PROTOTYPE(char *num_out, (long number) );
/* _PROTOTYPE(void print_string, (BOOL err_fl, char *fmt, int args) ); */
_PROTOTYPE(DIRECTORY *read_cluster, (unsigned int cluster) );
_PROTOTYPE(unsigned short free_cluster, (BOOL leave_fl) );
_PROTOTYPE(int link_fat, (unsigned int cl_1, unsigned int cl_2) );
_PROTOTYPE(unsigned short next_cluster, (unsigned int cl_no) );
_PROTOTYPE(char *slash, (char *str) );
_PROTOTYPE(void add_path, (char *file, BOOL slash_fl) );
_PROTOTYPE(int b_copy, (char *src, char *dest, int bytes) );
_PROTOTYPE(int disk_io, (BOOL op, unsigned long seek, void *address, unsigned bytes) );
_PROTOTYPE(int bad, (void));
_PROTOTYPE(int buf_read, (long seek, char *b, int c) );

_PROTOTYPE( void perror, (const char *_s)				);


leave(nr)
short nr;
{
  (void) umount(device);
  exit(nr);
}

usage(prog_name)
register char *prog_name;
{
  print_string(TRUE, "Usage: %s [%s\n", prog_name,
	     (int) (dos_dir ? "-lr] drive [dir]" : "-a] drive file"));
  exit(1);
}

unsigned c2u2(ucarray)
unsigned char *ucarray;
{
  return ucarray[0] + (ucarray[1] << 8);	/* parens vital */
}

unsigned long c4u4(ucarray)
unsigned char *ucarray;
{
  return ucarray[0] + ((unsigned long) ucarray[1] << 8) +
		      ((unsigned long) ucarray[2] << 16) +
		      ((unsigned long) ucarray[3] << 24);
}

determine()
{
  struct dosboot {
	unsigned char cjump[2];	/* unsigneds avoid bugs */
	unsigned char nop;
	unsigned char name[8];
	unsigned char cbytepers[2];	/* don't use shorts, etc */
	unsigned char secpclus;	/* to avoid struct member */
	unsigned char creservsec[2];	/* alignment and byte */
	unsigned char fats;	/* order bugs */
	unsigned char cdirents[2];
	unsigned char ctotsec[2];
	unsigned char media;
	unsigned char csecpfat[2];
	unsigned char csecptrack[2];
	unsigned char cheads[2];
	unsigned char chiddensec[2];
	unsigned char dos4hidd2[2];
	unsigned char dos4totsec[4];
	/* Char    fill[476]; */
  } boot;
  unsigned short boot_magic;	/* last of boot block */
  unsigned bytepers, reservsec, dirents;
  unsigned secpfat, secptrack, heads, hiddensec;
  unsigned long totsec;

  int errcount = 0;

  /* Read Bios-Parameterblock */
  disk_io(READ, 0L, &boot, sizeof boot);
  disk_io(READ, 0x1FEL, &boot_magic, sizeof boot_magic);

  /* Convert some arrays */
  bytepers = c2u2(boot.cbytepers);
  reservsec = c2u2(boot.creservsec);
  dirents = c2u2(boot.cdirents);
  totsec = c2u2(boot.ctotsec);
  if (totsec == 0) totsec = c4u4(boot.dos4totsec);
  secpfat = c2u2(boot.csecpfat);
  secptrack = c2u2(boot.csecptrack);
  heads = c2u2(boot.cheads);
  hiddensec = c2u2(boot.chiddensec);

  /* Calculate everything. */
  total_clusters = (unsigned short)
	(totsec / (boot.secpclus == 0 ? 1 : boot.secpclus));
  cluster_size = bytepers * boot.secpclus;
  fat_size = (unsigned long) secpfat * (unsigned long) bytepers;
  data_start = (long) bytepers + (long) boot.fats * fat_size
	+ (long) dirents *32L;
  root_entries = dirents;
  sub_entries = boot.secpclus * bytepers / 32;
  if (total_clusters > 4096) fat_16 = 1;

  /* Safety checking */
  if (boot_magic != 0xAA55) {
	print_string(TRUE, "magic != 0xAA55\n", 0);
	++errcount;
  }

  /* Check sectors per track instead of inadequate media byte */
  if (secptrack < 15 &&		/* assume > 15 hard disk & wini OK */
#ifdef SECT10			/* BIOS modified for 10 sec/track */
      secptrack != 10 &&
#endif
#ifdef SECT8			/* BIOS modified for 8 sec/track */
      secptrack != 8 &&
#endif
      secptrack != 9) {
	print_string(TRUE, "sectors per track not supported\n", 0);
	++errcount;
  }
  if (boot.secpclus == 0) {
	print_string(TRUE, "sectors per cluster == 0\n", 0);
	++errcount;
  }
  if (boot.fats != 2 && dos_write) {
	print_string(TRUE, "fats != 2\n", 0);
	++errcount;
  }
  if (reservsec != 1) {
	print_string(TRUE, "reserved != 1\n", 0);
	++errcount;
  }
  if (cluster_size > MAX_CLUSTER_SIZE) {
	print_string(TRUE, "cluster size too big\n", 0);
	++errcount;
  }
  if (errcount != 0) {
	print_string(TRUE, "Can't handle disk\n", 0);
	leave(2);
  }
}

main(argc, argv)
int argc;
register char *argv[];
{
  register char *arg_ptr = slash(argv[0]);
  DIRECTORY *entry;
  short index = 1;
  char dev_nr = '0';
  unsigned char fat_check;

  if (!strcmp(arg_ptr, "dosdir"))
	dos_dir = TRUE;
  else if (!strcmp(arg_ptr, "dosread"))
	dos_read = TRUE;
  else if (!strcmp(arg_ptr, "doswrite"))
	dos_write = TRUE;
  else {
	print_string(TRUE, "Program should be named dosread, doswrite or dosdir.\n", 0);
	exit(1);
  }

  if (argc == 1) usage(argv[0]);

  if (argv[1][0] == '-') {
	for (arg_ptr = &argv[1][1]; *arg_ptr; arg_ptr++) {
		if (*arg_ptr == 'l' && dos_dir)
			Lflag = TRUE;
		else if (*arg_ptr == 'r' && dos_dir)
			Rflag = TRUE;
		else if (*arg_ptr == 'a' && !dos_dir)
			Aflag = TRUE;
		else
			usage(argv[0]);
	}
	index++;
  }
  if (index == argc) usage(argv[0]);

  if (strncmp(argv[index], "/dev/", 5) == 0)
	device = argv[index++];
  else {
	if ((dev_nr = toupper (*argv[index++])) < 'A' || dev_nr > 'Z')
		usage(argv[0]);

	device[DRIVE_NR] = dev_nr;
  }

  if ((disk = open(device, dos_write ? O_RDWR : O_RDONLY)) < 0) {
	print_string(TRUE, "Cannot open %s\n", device);
	exit(1);
  }
  disk_io(READ, FAT_START, &fat_info, 1);
  determine();
  disk_io(READ, FAT_START + fat_size, &fat_check, sizeof(fat_check));

  if (fat_check != fat_info) {
	print_string(TRUE, "Disk type in FAT copy differs from disk type in FAT original.\n", 0);
	leave(1);
  }
  disk_io(READ, ROOTADDR, root, DIR_SIZE * root_entries);

  if (dos_dir && Lflag) {
	entry = label();
	print_string(FALSE, "Volume in drive %c ", dev_nr);
	if (entry == NIL_DIR)
		print(STD_OUT, "has no label.\n\n", 0);
	else
		print_string(FALSE, "is %S\n\n", entry->d_name);
  }
  if (argv[index] == NIL_PTR) {
	if (!dos_dir) usage(argv[0]);
	if (Lflag) print(STD_OUT, "Root directory:\n", 0);
	list_dir(root, root_entries, FALSE);
	if (Lflag) free_blocks();
	flush();
	leave(0);
  }
  for (arg_ptr = argv[index]; *arg_ptr; arg_ptr++)
	if (*arg_ptr == '\\')
		*arg_ptr = '/';
	else if (*arg_ptr >= 'a' && *arg_ptr <= 'z')
		*arg_ptr += ('A' - 'a');
  if (*--arg_ptr == '/') *arg_ptr = '\0';	/* skip trailing '/' */

  add_path(argv[index], FALSE);
  add_path("/", FALSE);

  if (dos_dir && Lflag) print_string(FALSE, "Directory %s:\n", path);

  entry = find_entry(root, root_entries, argv[index]);

  if (dos_dir) {
	list_dir(entry, sub_entries, FALSE);
	if (Lflag) free_blocks();
  } else if (dos_read)
	extract(entry);
  else {
	if (entry != NIL_DIR) {
		flush();
		if (is_dir(entry))
			print_string(TRUE, "%s is a directory.\n", path);
		else
			print_string(TRUE, "%s already exists.\n", argv[index]);
		leave(1);
	}
	add_path(NIL_PTR, TRUE);

	if (*path) make_file(find_entry(root, root_entries, path),
			  sub_entries, slash(argv[index]));
	else
		make_file(root, root_entries, argv[index]);
  }

  (void) close(disk);
  flush();
  leave(0);
}

DIRECTORY *directory(dir, entries, function, pathname)
DIRECTORY *dir;
int entries;
BOOL function;
register char *pathname;
{
  register DIRECTORY *dir_ptr = dir;
  DIRECTORY *mem = NIL_DIR;
  unsigned short cl_no = dir->d_cluster;
  unsigned short type, last;
  char file_name[14];
  char *name;
  int i = 0;

  if (function == FIND) {
	while (*pathname != '/' && *pathname && i < 12)
		file_name[i++] = *pathname++;
	while (*pathname != '/' && *pathname) pathname++;
	file_name[i] = '\0';
  }
  do {
	if (entries != root_entries) {
		mem = dir_ptr = read_cluster(cl_no);
		last = cl_no;
		cl_no = next_cluster(cl_no);
	}
	for (i = 0; i < entries; i++, dir_ptr++) {
		type = dir_ptr->d_name[0] & 0x0FF;
		if (function == ENTRY) {
			if (type == NOT_USED || type == ERASED) {
				if (!mem)
					mark = ROOTADDR + (long) i *(long) DIR_SIZE;
				else
					mark = clus_add(last) + (long) i *(long) DIR_SIZE;
				return dir_ptr;
			}
			continue;
		}
		if (type == NOT_USED) break;
		if (dir_ptr->d_attribute & 0x08) {
			if (function == LABEL) return dir_ptr;
			continue;
		}
		if (type == DIR || type == ERASED || function == LABEL)
			continue;
		type = is_dir(dir_ptr);
		name = make_name(dir_ptr, (function == FIND) ?
				 FALSE : type);
		if (function == FIND) {
			if (strcmp(file_name, name) != 0) continue;
			if (!type) {
				if (dos_dir || *pathname) {
					flush();
					print_string(TRUE, "Not a directory: %s\n", file_name);
					leave(1);
				}
			} else if (*pathname == '\0' && dos_read) {
				flush();
				print_string(TRUE, "%s is a directory.\n", path);
				leave(1);
			}
			if (*pathname) {
				dir_ptr = find_entry(dir_ptr,
					 sub_entries, pathname + 1);
			}
			if (mem) {
				if (dir_ptr) {
					b_copy((char *)dir_ptr, (char *)&save_entry, DIR_SIZE);
					dir_ptr = &save_entry;
				}
				(void) brk( (char *) mem);
			}
			return dir_ptr;
		} else {
			if (function == FALSE)
				show(dir_ptr, name);
			else if (type) {	/* Recursive */
				print_string(FALSE, "Directory %s%s:\n", path, name);
				add_path(name, FALSE);
				list_dir(dir_ptr, sub_entries, FALSE);
				add_path(NIL_PTR, FALSE);
			}
		}
	}
	if (mem) (void) brk( (char *) mem);
  } while (cl_no != LAST_CLUSTER && mem);

  switch (function) {
      case FIND:
	if (dos_write && *pathname == '\0') return NIL_DIR;
	flush();
	print_string(TRUE, "Cannot find `%s'.\n", file_name);
	leave(1);
      case LABEL:
	return NIL_DIR;
      case ENTRY:
	if (!mem) {
		flush();
		print_string(TRUE, "No entries left in root directory.\n", 0);
		leave(1);
	}
	cl_no = free_cluster(TRUE);
	link_fat(last, cl_no);
	link_fat(cl_no, LAST_CLUSTER);
	disk_io(WRITE, clus_add(cl_no), null, cluster_size);

	return new_entry(dir, entries);
      case FALSE:
	if (Rflag) {
		print(STD_OUT, "\n", 0);
		list_dir(dir, entries, TRUE);
	}
  }
}

extract(entry)
register DIRECTORY *entry;
{
  register unsigned short cl_no = entry->d_cluster;
  char buffer[MAX_CLUSTER_SIZE];
  short rest;

  if (entry->d_size == 0)	/* Empty file */
	return;

  do {
	disk_io(READ, clus_add(cl_no), buffer, cluster_size);
	rest = (entry->d_size > (long) cluster_size) ? cluster_size : (short) entry->d_size;
	print(STD_OUT, buffer, rest);
	entry->d_size -= (long) rest;
	cl_no = next_cluster(cl_no);
	if (cl_no == (fat_16 ? BAD16 : BAD)) {
		flush();
		print_string(TRUE, "Reserved cluster value encountered.\n", 0);
		leave(1);
	}
  } while (entry->d_size && cl_no != LAST_CLUSTER);

  if (cl_no != LAST_CLUSTER)
	print_string(TRUE, "Too many clusters allocated for file.\n", 0);
  else if (entry->d_size != 0)
	print_string(TRUE, "Premature EOF: %L bytes left.\n",
		     entry->d_size);
}

print(fd, buffer, bytes)
short fd;
register char *buffer;
register short bytes;
{
  static short index;
  static BOOL lf_pending = FALSE;
  static char output[MAX_CLUSTER_SIZE + 1];

  if (buffer == NIL_PTR) {
	if (dos_read && Aflag && lf_pending) {
		output[index++] = '\r';
		lf_pending = FALSE;
	}
	if (write(fd, output, index) != index) bad();
	index = 0;
	return;
  }
  if (bytes == 0) bytes = strlen(buffer);

  while (bytes--) {
	if (index >= MAX_CLUSTER_SIZE) {
		if (write(fd, output, index) != index) bad();
		index = 0;
	}
	if (dos_read && Aflag) {
		if (*buffer == '\r') {
			if (lf_pending)
				output[index++] = *buffer++;
			else {
				lf_pending = TRUE;
				buffer++;
			}
		} else if (*buffer == '\n') {
			output[index++] = *buffer++;
			lf_pending = FALSE;
		} else if (lf_pending) {
			output[index++] = '\r';
			output[index++] = *buffer++;
		} else if ((output[index++] = *buffer++) == EOF_MARK) {
			if (lf_pending) {
				output[index - 1] = '\r';
				index++;
				lf_pending = FALSE;
			}
			index--;
			return;
		}
	} else
		output[index++] = *buffer++;
  }
}

make_file(dir_ptr, entries, name)
DIRECTORY *dir_ptr;
int entries;
char *name;
{
  register DIRECTORY *entry = new_entry(dir_ptr, entries);
  register char *ptr;
  char buffer[MAX_CLUSTER_SIZE];
  unsigned short cl_no, next;
  short i, r;
  long size = 0L;

  b_copy("           ", (char *) &entry->d_name[0], 11);    /* clear entry */
  for (i = 0, ptr = name; i < 8 && *ptr != '.' && *ptr; i++)
	entry->d_name[i] = *ptr++;
  while (*ptr != '.' && *ptr) ptr++;
  if (*ptr == '.') ptr++;
  for (i = 0; i < 3 && *ptr; i++) entry->d_ext[i] = *ptr++;

  for (i = 0; i < 10; i++) entry->d_reserved[i] = '\0';
  entry->d_attribute = '\0';

  entry->d_cluster = 0;

  while ((r = fill(buffer)) > 0) {
	if ((next = free_cluster(FALSE)) > total_clusters) {
		print_string(TRUE, "Diskette full. File truncated.\n", 0);
		break;
	}
	disk_io(WRITE, clus_add(next), buffer, r);

	if (entry->d_cluster == 0)
		cl_no = entry->d_cluster = next;
	else {
		link_fat(cl_no, next);
		cl_no = next;
	}

	size += r;
  }

  if (entry->d_cluster != 0) link_fat(cl_no, LAST_CLUSTER);

  entry->d_size = Aflag ? (size - 1) : size;	/* Strip added ^Z */
  fill_date(entry);
  disk_io(WRITE, mark, entry, DIR_SIZE);
}


#define SEC_MIN	60L
#define SEC_HOUR	(60L * SEC_MIN)
#define SEC_DAY	(24L * SEC_HOUR)
#define SEC_YEAR	(365L * SEC_DAY)
#define SEC_LYEAR	(366L * SEC_DAY)

short mon_len[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

fill_date(entry)
DIRECTORY *entry;
{
  register long cur_time = time((long *) 0) - DOS_TIME;
  unsigned short year = 0, month = 1, day, hour, minutes, seconds;
  int i;
  long tmp;

  if (cur_time < 0)		/* Date not set on booting ... */
	cur_time = 0;
  for (;;) {
	tmp = (year % 4 == 0) ? SEC_LYEAR : SEC_YEAR;
	if (cur_time < tmp) break;
	cur_time -= tmp;
	year++;
  }

  day = (unsigned short) (cur_time / SEC_DAY);
  cur_time -= (long) day *SEC_DAY;

  hour = (unsigned short) (cur_time / SEC_HOUR);
  cur_time -= (long) hour *SEC_HOUR;

  minutes = (unsigned short) (cur_time / SEC_MIN);
  cur_time -= (long) minutes *SEC_MIN;

  seconds = (unsigned short) cur_time;

  mon_len[1] = (year % 4 == 0) ? 29 : 28;
  i = 0;
  while (day >= mon_len[i]) {
	month++;
	day -= mon_len[i++];
  }
  day++;

  entry->d_date = (year << 9) | (month << 5) | day;
  entry->d_time = (hour << 11) | (minutes << 5) | seconds;
}

char *make_name(dir_ptr, dir_fl)
register DIRECTORY *dir_ptr;
short dir_fl;
{
  static char name_buf[14];
  register char *ptr = name_buf;
  short i;

  for (i = 0; i < 8; i++) *ptr++ = dir_ptr->d_name[i];

  while (*--ptr == ' ');

  ptr++;
  if (dir_ptr->d_ext[0] != ' ') {
	*ptr++ = '.';
	for (i = 0; i < 3; i++) *ptr++ = dir_ptr->d_ext[i];
	while (*--ptr == ' ');
	ptr++;
  }
  if (dir_fl) *ptr++ = '/';
  *ptr = '\0';

  return name_buf;
}

fill(buffer)
register char *buffer;
{
  static BOOL eof_mark = FALSE;
  char *last = &buffer[cluster_size];
  char *begin = buffer;
  register short c;

  if (eof_mark) return 0;

  while (buffer < last) {
	if ((c = get_char()) == EOF) {
		eof_mark = TRUE;
		if (Aflag) *buffer++ = EOF_MARK;
		break;
	}
	*buffer++ = c;
  }

  return(int) (buffer - begin);
}

get_char()
{
  static short read_chars, index;
  static char input[MAX_CLUSTER_SIZE];
  static BOOL new_line = FALSE;

  if (new_line == TRUE) {
	new_line = FALSE;
	return '\n';
  }
  if (index == read_chars) {
	if ((read_chars = read(0, input, cluster_size)) == 0) return EOF;
	index = 0;
  }
  if (Aflag && input[index] == '\n') {
	new_line = TRUE;
	index++;
	return '\r';
  }
  return input[index++];
}

#define HOUR	0xF800		/* Upper 5 bits */
#define MIN	0x07E0		/* Middle 6 bits */
#define YEAR	0xFE00		/* Upper 7 bits */
#define MONTH	0x01E0		/* Mid 4 bits */
#define DAY	0x01F		/* Lowest 5 bits */

char *month[] = {
	 "Jan", "Feb", "Mar", "Apr", "May", "Jun",
	 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

xmodes(mode)
int mode;
{
  print_string(FALSE, "\t%c%c%c%c%c", (mode & SUB_DIR) ? 'd' : '-',
	     (mode & 02) ? 'h' : '-', (mode & 04) ? 's' : '-',
	     (mode & 01) ? '-' : 'w', (mode & 0x20) ? 'a' : '-');
}

show(dir_ptr, name)
DIRECTORY *dir_ptr;
char *name;
{
  register unsigned short e_date = dir_ptr->d_date;
  register unsigned short e_time = dir_ptr->d_time;
  unsigned short next;
  char bname[20];
  short i = 0;

  while (*name && *name != '/') bname[i++] = *name++;
  bname[i] = '\0';
  if (!Lflag) {
	print_string(FALSE, "%s\n", bname);
	return;
  }
  xmodes( (int) dir_ptr->d_attribute);
  print_string(FALSE, "\t%s%s", bname, strlen(bname) < 8 ? "\t\t" : "\t");
  i = 1;
  if (is_dir(dir_ptr)) {
	next = dir_ptr->d_cluster;
	while ((next = next_cluster(next)) != LAST_CLUSTER) i++;
	print_string(FALSE, "%L", (long) i * (long) cluster_size);
  } else
	print_string(FALSE, "%L", dir_ptr->d_size);
  print_string(FALSE, "\t%N:%N %P %s %d\n", ((e_time & HOUR) >> 11),
	     ((e_time & MIN) >> 5), (e_date & DAY),
   month[((e_date & MONTH) >> 5) - 1], ((e_date & YEAR) >> 9) + 1980);
}

free_blocks()
{
  register unsigned short cl_no;
  long free = 0;
  long bad = 0;

  for (cl_no = 2; cl_no <= total_clusters; cl_no++) {
	switch (next_cluster(cl_no)) {
	    case FREE:	free++;	break;
	    case BAD16:	bad++;	break;
	    case BAD:
		if (!fat_16) bad++;
	}
  }

  print_string(FALSE, "Free space: %L bytes.\n", free * (long) cluster_size);
  if (bad)
	print_string(FALSE, "Bad sectors: %L bytes.\n", bad * (long) cluster_size);
}

char *num_out(number)
register long number;
{
  static char num_buf[13];
  char temp[13];
  register short i = 0;
  short j;

  if (number == 0) temp[i++] = '0';

  while (number) {
	temp[i++] = (char) (number % 10L + '0');
	number /= 10L;
  }

  for (j = 0; j < 11; j++) num_buf[j] = temp[i - j - 1];

  num_buf[i] = '\0';
  return num_buf;
}

/* VARARGS */
void print_string(err_fl, fmt, args)
BOOL err_fl;
char *fmt;
int args;
{
  char buf[200];
  register char *buf_ptr = buf;
  char *scan_ptr;
  register int *arg_ptr = &args;
  short i;

  while (*fmt) {
	if (*fmt == '%') {
		fmt++;
		if (*fmt == 'c') {
			*buf_ptr++ = (char) *arg_ptr++;
			fmt++;
			continue;
		}
		if (*fmt == 'S') {
			scan_ptr = (char *) *arg_ptr;
			for (i = 0; i < 11; i++) *buf_ptr++ = *scan_ptr++;
			fmt++;
			continue;
		}
		if (*fmt == 's')
			scan_ptr = (char *) *arg_ptr;
		else if (*fmt == 'L') {
			scan_ptr = num_out(*((long *) arg_ptr));
			arg_ptr++;
		} else {
			scan_ptr = num_out((long) *arg_ptr);
			if (*fmt == 'P' && *arg_ptr < 10)
				*buf_ptr++ = ' ';
			else if (*fmt == 'N' && *arg_ptr < 10)
				*buf_ptr++ = '0';
		}
		while (*buf_ptr++ = *scan_ptr++);
		buf_ptr--;
		arg_ptr++;
		fmt++;
	} else
		*buf_ptr++ = *fmt++;
  }

  *buf_ptr = '\0';

  if (err_fl) {
	flush();
	write(2, buf, (int) (buf_ptr - buf));
  } else
	print(STD_OUT, buf, 0);
}

DIRECTORY *read_cluster(cluster)
register unsigned short cluster;
{
  register DIRECTORY *sub_dir;

  if ((sub_dir = (DIRECTORY *) sbrk(cluster_size)) == (DIRECTORY *) -1) {
	print_string(TRUE, "Cannot set break!\n", 0);
	leave(1);
  }
  disk_io(READ, clus_add(cluster), sub_dir, cluster_size);

  return sub_dir;
}

unsigned short free_cluster(leave_fl)
BOOL leave_fl;
{
  static unsigned short cl_index = 2;

  while (cl_index <= total_clusters && next_cluster(cl_index) != FREE)
	cl_index++;

  if (leave_fl && cl_index > total_clusters) {
	flush();
	print_string(TRUE, "Diskette full. File not added.\n", 0);
	leave(1);
  }
  return cl_index++;
}

link_fat(cl_1, cl_2)
unsigned short cl_1;
register unsigned short cl_2;
{
  register unsigned fat_index;
  unsigned char pad;
  unsigned pad16;

  if (fat_16) {
	pad16 = cl_2;
	put_fat16((long) (cl_1 << 1), &pad16);
	return;
  }
  fat_index = (cl_1 >> 1) * 3 + 1;
  if (cl_1 & 0x01) {
	pad = cl_2 >> 4;
	put_fat((long) (fat_index + 1), &pad);
	get_fat((long) fat_index, (char *) &pad);
	pad = (pad & 0x0F) | ((cl_2 & 0x0F) << 4);
	put_fat((long) fat_index, &pad);
  } else {
	pad = cl_2;
	put_fat((long) (fat_index - 1), &pad);
	get_fat((long) fat_index, (char *) &pad);
	pad = (pad & 0xF0) | (0xf & (cl_2 >> 8));
	put_fat((long) fat_index, &pad);
  }
}

unsigned short next_cluster(cl_no)
register unsigned short cl_no;
{
  register unsigned fat_index;
  unsigned char pad;
  unsigned pad16;
  unsigned mask = MASK16;
  unsigned bad = BAD16;

  if (!fat_16) {
	fat_index = (cl_no >> 1) * 3 + 1;
	get_fat((long) fat_index, (char *) &pad);
	if (cl_no & 0x01) {
		pad16 = 0x0f & (pad >> 4);
		get_fat((long) (fat_index + 1), (char *) &pad);
		cl_no = (((short) pad) << 4) | pad16;
	} else {
		pad16 = (0x0f & pad) << 8;
		get_fat((long) (fat_index - 1), (char *) &pad);
		cl_no = (short) pad | pad16;
	}
	mask = MASK;
	bad = BAD;
  } else {
	get_fat16((long) (cl_no << 1), (char *) &pad16);
	cl_no = pad16;
  }

  if ((cl_no & mask) == mask)
	cl_no = LAST_CLUSTER;
  else if ((cl_no & bad) == bad)
	cl_no = bad;

  return cl_no;
}

char *slash(str)
register char *str;
{
  register char *result = str;

  while (*str)
	if (*str++ == '/') result = str;

  return result;
}

void add_path(file, slash_fl)
char *file;
BOOL slash_fl;
{
  register char *ptr = path;

  while (*ptr) ptr++;

  if (file == NIL_PTR) {
	if (ptr != path) ptr--;
	if (ptr != path) do {
			ptr--;
		} while (*ptr != '/' && ptr != path);
	if (ptr != path && !slash_fl) *ptr++ = '/';
	*ptr = '\0';
  } else
	while (*ptr++ = *file++);
}

b_copy(src, dest, bytes)
register char *src, *dest;
short bytes;
{
  while (bytes--) *dest++ = *src++;
}

disk_io(op, seek, address, bytes)
register BOOL op;
unsigned long seek;
void *address;
register unsigned bytes;
{
  unsigned int r;

  if (lseek(disk, seek, SEEK_SET) < 0L) {
	flush();
	print_string(TRUE, "Bad lseek\n", 0);
	leave(1);
  }
  if (op == READ)
	r = read(disk, (char *) address, bytes);
  else {
	disk_written = 1;
	r = write(disk, (char *) address, bytes);
  }

  if (r != bytes) bad();
}

bad()
{
  flush();
  perror("I/O error");
  leave(1);
}

buf_read(seek, b, c)
long seek;
register char *b;
int c;
{
  if (disk_written || (seek & (~0x3ffL)) != buf_addr) {
	disk_written = 0;
	disk_io(READ, buf_addr = seek & (~0x3ffL), buf_buf, 1025);
  }
  seek &= 0x3ffL;
  *b++ = buf_buf[(int) seek++];
  if (c == 2) *b = buf_buf[(int) seek];
}
