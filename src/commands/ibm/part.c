/*	part 1.34 - Partition table editor		Author: Kees J. Bot
 *								13 Mar 1992
 * Needs about 20k heap+stack.
 */
#define nil 0
#include <sys/types.h>
#include <stdio.h>
#include <termcap.h>
#include <errno.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/partition.h>
#include <minix/boot.h>
#if __minix_vmd
#include <termios.h>
#else
#include <sgtty.h>
#endif

/* Template:
                      ----first----  --geom/last--  ------sectors-----
Num Sort   Type        Cyl Head Sec   Cyl Head Sec      Base      Size       Kb
         /dev/hd0                     977    5  17
         /dev/hd0:2      0    0   2   976    4  16         2     83043    41521
 1* hd1  81 MINIX        0    0   3    33    4   9         3      2880     1440
 2  hd2  81 MINIX       33    4  10   178    2   2      2883     12284     6142
 3  hd3  81 MINIX      178    2   3   976    4  16     15167     67878    33939
 4  hd4  00 None         0    0   0     0    0  -1         0         0        0

 */
#define MAXSIZE		(1024L * 255 * 63)	/* Max cyls*heads*sectors */
#define SECTOR_SIZE	512

#define arraysize(a)	(sizeof(a) / sizeof((a)[0]))
#define arraylimit(a)	((a) + arraysize(a))

void report(const char *label)
{
	fprintf(stderr, "part: %s: %s\n", label, strerror(errno));
}

void fatal(const char *label)
{
	report(label);
	exit(1);
}

#if __minix_vmd
struct termios termios;
#else
struct sgttyb ttyb;
#endif

void save_ttyflags(void)
/* Save tty attributes for later restoration. */
{
#if __minix_vmd
	if (tcgetattr(0, &termios) < 0) fatal("");
#else
	if (ioctl(0, TIOCGETP, &ttyb) < 0) fatal("");
#endif
}

void restore_ttyflags(void)
/* Reset the tty flags to how we got 'em. */
{
#if __minix_vmd
	if (tcsetattr(0, TCSANOW, &termios) < 0) fatal("");
#else
	if (ioctl(0, TIOCSETP, &ttyb) < 0) fatal("");
#endif
}

void tty_raw(void)
/* Set the terminal to raw mode, no signals, no echoing. */
{
#if __minix_vmd
	struct termios rawterm;

	rawterm= termios;
	rawterm.c_lflag &= ~(ICANON|ISIG|ECHO);
	rawterm.c_iflag &= ~(ICRNL);
	if (tcsetattr(0, TCSANOW, &rawterm) < 0) fatal("");
#else
	struct sgttyb rawttyb;

	rawttyb= ttyb;
	rawttyb.sg_flags= (rawttyb.sg_flags | RAW) & ~ECHO;
	if (ioctl(0, TIOCSETP, &rawttyb) < 0) fatal("");
#endif
}

#define ctrl(c)		((c) == '?' ? '\177' : ((c) & '\37'))

char t_cd[16], t_cm[32], t_so[16], t_se[16], t_md[16], t_me[16];
int t_li, t_co;
#define STATUSROW	9

void init_tty(void)
/* Get terminal capabilities and set the tty to "editor" mode. */
{
	char *term;
	static char termbuf[1024];
	char *tp;

	if ((term= getenv("TERM")) == nil || tgetent(termbuf, term) != 1) {
		fprintf(stderr, "part: Can't get terminal capabilities\n");
		exit(1);
	}
	if (tgetstr("cd", (tp= t_cd, &tp)) == nil
				|| tgetstr("cm", (tp= t_cm, &tp)) == nil) {
		fprintf(stderr, "part: This terminal is too dumb\n");
		exit(1);
	}
	t_li= tgetnum("li");
	t_co= tgetnum("co");
	(void) tgetstr("so", (tp= t_so, &tp));
	(void) tgetstr("se", (tp= t_se, &tp));
	(void) tgetstr("md", (tp= t_md, &tp));
	(void) tgetstr("me", (tp= t_me, &tp));

	save_ttyflags();
	tty_raw();
}

void putchr(int c)
{
	putchar(c);
}

void putstr(char *s)
{
	int c;

	while ((c= *s++) != 0) putchr(c);
}

void set_cursor(int row, int col)
{
	tputs(tgoto(t_cm, col, row), 1, putchr);
}

void clear_screen(void)
{
	set_cursor(0, 0);
	tputs(t_cd, 1, putchr);
}

int statusrow= STATUSROW;

void stat_start(int serious)
/* Prepare for printing on a fresh status line, possibly highlighted. */
{
	set_cursor(statusrow++, 0);
	tputs(t_cd, 1, putchr);
	if (serious) tputs(t_so, 1, putchr);
}

void stat_end(void)
/* Closing bracket for stat_start. */
{
	tputs(t_se, 1, putchr);
}

/* Reset the statusline pointer. */
#define stat_reset()	((void) (statusrow= STATUSROW))

void reset_tty(void)
/* Reset the tty to cooked mode. */
{
	restore_ttyflags();
	set_cursor(statusrow, 0);
	tputs(t_cd, 1, putchr);
}

void *alloc(size_t n)
{
	void *m;

	if ((m= malloc(n)) == nil) { reset_tty(); fatal(""); }

	return m;
}

#ifndef makedev		/* Missing in sys/types.h */
#define minor(dev)	(((dev) >> MINOR) & BYTE)
#define major(dev)	(((dev) >> MAJOR) & BYTE)
#define makedev(major, minor)	\
			((dev_t) (((major) << MAJOR) | ((minor) << MINOR)))
#endif

typedef enum parttype { DUNNO, SUBPART, PRIMARY } parttype_t;

typedef struct device {
	struct device *next, *prev;	/* Circular dequeue. */
	dev_t	rdev;			/* Device number (sorting only). */
	char	*name;			/* E.g. /dev/hd0 */
	char	*subname;			/* E.g. /dev/hd0:2 */
	char	part[6];		/* E.g. hd2a */
	parttype_t parttype;
} device_t;

device_t *firstdev= nil, *curdev;

void newdevice(char *name, int scanning)
/* Add a device to the device list.  If scanning is set then we are reading
 * /dev, so insert the device in device number order and make /dev/hd0 current.
 */
{
	device_t *new, *nextdev, *prevdev;
	char *base;
	int max, len;
	struct stat st;

	st.st_rdev= 0;
	if (scanning) {
		if (stat(name, &st) < 0 || !S_ISBLK(st.st_mode)) return;

		switch (major(st.st_rdev)) {
		case 0:
		case 1:
			return;
		case 2:
			if (minor(st.st_rdev) >= 4) return;
			break;
		default:
			if (minor(st.st_rdev) >= 0x80
					|| minor(st.st_rdev) % 5 != 0) return;
		}
		/* Interesting device found. */
	} else {
		(void) stat(name, &st);
	}

	new= alloc(sizeof(*new));
	new->rdev= st.st_rdev;
	new->name= alloc((strlen(name) + 1) * sizeof(new->name[0]));
	strcpy(new->name, name);
	new->subname= new->name;
	new->parttype= DUNNO;
	if (major(st.st_rdev) == major(DEV_FD0)) {
		new->parttype= SUBPART;
	} else
	if (st.st_rdev >= DEV_HD0 && minor(st.st_rdev) < 128
			&& minor(st.st_rdev) % 5 == 0) {
		new->parttype= PRIMARY;
	}
	if ((base= strrchr(name, '/')) == nil) base= name; else base++;
	max= new->parttype == SUBPART ? 4 : 5;
	len= strlen(base);
	strcpy(new->part, len < max ? base : base + len - max);

	if (firstdev == nil) {
		firstdev= new;
		new->next= new->prev= new;
		curdev= firstdev;
		return;
	}
	nextdev= firstdev;
	while (new->rdev >= nextdev->rdev
				&& (nextdev= nextdev->next) != firstdev) {}
	prevdev= nextdev->prev;
	new->next= nextdev;
	nextdev->prev= new;
	new->prev= prevdev;
	prevdev->next= new;

	if (new->rdev < firstdev->rdev) firstdev= new;
	if (new->rdev == DEV_HD0) curdev= new;
	if (curdev->rdev != DEV_HD0) curdev= firstdev;
}

void getdevices(void)
/* Get all block devices from /dev that look interesting. */
{
	DIR *d;
	struct dirent *e;
	char name[5 + NAME_MAX + 1];

	if ((d= opendir("/dev")) == nil) fatal("/dev");

	while ((e= readdir(d)) != nil) {
		strcpy(name, "/dev/");
		strcpy(name + 5, e->d_name);
		newdevice(name, 1);
	}
	(void) closedir(d);
}

/* One nice hand made featureful master bootstrap. */
char bootstrap[] = {
0353,0001,0000,0061,0300,0216,0330,0216,0300,0372,0216,0320,0274,0000,0174,0373,
0275,0276,0007,0211,0346,0126,0277,0000,0006,0271,0000,0001,0374,0362,0245,0352,
0044,0006,0000,0000,0264,0002,0315,0026,0250,0010,0164,0033,0350,0062,0001,0166,
0007,0060,0344,0315,0026,0242,0177,0007,0054,0060,0074,0012,0163,0363,0120,0350,
0037,0001,0177,0007,0130,0353,0007,0240,0002,0006,0204,0300,0164,0144,0230,0262,
0005,0366,0362,0262,0200,0000,0302,0210,0340,0120,0350,0240,0000,0163,0003,0351,
0144,0000,0130,0054,0001,0174,0141,0276,0276,0175,0211,0357,0271,0040,0000,0362,
0245,0200,0301,0004,0211,0356,0215,0174,0020,0070,0154,0004,0164,0016,0213,0135,
0010,0053,0134,0010,0213,0135,0012,0033,0134,0012,0163,0014,0212,0044,0206,0144,
0020,0210,0044,0106,0071,0376,0162,0364,0211,0376,0201,0376,0356,0007,0162,0326,
0342,0322,0211,0356,0264,0020,0366,0344,0001,0306,0200,0174,0004,0001,0162,0026,
0353,0021,0204,0322,0175,0041,0211,0356,0200,0174,0004,0000,0164,0013,0366,0004,
0200,0164,0006,0350,0077,0000,0162,0055,0303,0203,0306,0020,0201,0376,0376,0007,
0162,0346,0350,0214,0000,0203,0007,0350,0207,0000,0221,0007,0376,0302,0174,0017,
0315,0021,0321,0340,0321,0340,0200,0344,0003,0070,0342,0166,0012,0262,0200,0264,
0010,0122,0315,0023,0132,0162,0143,0350,0003,0000,0162,0333,0303,0211,0356,0214,
0134,0010,0214,0134,0012,0277,0003,0000,0122,0264,0010,0315,0023,0200,0341,0077,
0376,0306,0210,0310,0366,0346,0211,0303,0213,0104,0010,0213,0124,0012,0367,0363,
0222,0210,0325,0366,0361,0060,0322,0321,0352,0321,0352,0010,0342,0210,0321,0376,
0301,0132,0210,0306,0273,0000,0174,0270,0001,0002,0315,0023,0163,0014,0117,0164,
0006,0060,0344,0315,0023,0163,0301,0071,0347,0303,0201,0076,0376,0175,0125,0252,
0165,0001,0303,0350,0013,0000,0251,0007,0353,0005,0350,0004,0000,0235,0007,0353,
0376,0136,0255,0126,0211,0306,0254,0204,0300,0164,0011,0264,0016,0273,0001,0000,
0315,0020,0353,0362,0303,0000,0057,0144,0145,0166,0057,0150,0144,0077,0010,0000,
0015,0012,0000,0116,0157,0156,0145,0040,0141,0143,0164,0151,0166,0145,0015,0012,
0000,0116,0145,0170,0164,0040,0144,0151,0163,0153,0015,0012,0000,0122,0145,0141,
0144,0040,0145,0162,0162,0157,0162,0040,0000,0116,0157,0164,0040,0142,0157,0157,
0164,0141,0142,0154,0145,0040,0000,0000,
};

int dirty= 0;
unsigned char bootblock[SECTOR_SIZE];
struct part_entry table[1 + NR_PARTITIONS];
int existing[1 + NR_PARTITIONS];
unsigned long offset= 0, extbase= 0, extsize;
int submerged= 0;
char sort_index[1 + NR_PARTITIONS];
unsigned cylinders= 1, heads= 1, sectors= 1, secpcyl= 1;
int precise= 0;
int device= -1;

unsigned long sortbase(struct part_entry *pe)
{
	return pe->sysind == NO_PART ? -1 : pe->lowsec;
}

void sort(void)
/* Let the sort_index array show the order partitions are sorted in. */
{
	int i, j;
	int idx[1 + NR_PARTITIONS];

	for (i= 1; i <= NR_PARTITIONS; i++) idx[i]= i;

	for (i= 1; i <= NR_PARTITIONS; i++) {
		for (j= 1; j <= NR_PARTITIONS-1; j++) {
			int sj= idx[j], sj1= idx[j+1];

			if (sortbase(&table[sj]) > sortbase(&table[sj1])) {
				idx[j]= sj1;
				idx[j+1]= sj;
			}
		}
	}
	for (i= 1; i <= NR_PARTITIONS; i++) sort_index[idx[i]]= i;
}

#ifdef DIOCGETP
/* Hard disk driver supports an ioctl to report the base and size of a
 * device as a partition table entry.
 */
#define diocntl(device, request, entry) \
			ioctl((device), (request), (void *) (entry))
#else
#define diocntl(d, r, e)		(errno= ENOTTY, -1)
#endif

void dos2chs(unsigned char *dos, unsigned *ac, unsigned *ah, unsigned *as)
/* Extract cylinder, head and sector from the three bytes DOS uses to address
 * a sector.  Note that bits 8 & 9 of the cylinder number come from bit 6 & 7
 * of the sector byte.  The caller must know that sector numbers start at 1.
 */
{
	*ac= ((dos[1] & 0xC0) << 2) | dos[2];
	*ah= dos[0];
	*as= dos[1] & 0x3F;
}

void abs2dos(unsigned char *dos, unsigned long pos)
/* Translate a sector offset to three DOS bytes. */
{
	unsigned h, c, s;

	c= pos / secpcyl;
	h= (pos % secpcyl) / sectors;
	s= pos % sectors + 1;

	dos[0]= h;
	dos[1]= s | ((c >> 2) & 0xC0);
	dos[2]= c & 0xFF;
}

void recompute0(void)
/* Recompute the partition size for the device after a geometry change. */
{
	if (device < 0) {
		cylinders= heads= sectors= 1;
		memset(table, 0, sizeof(table));
	} else
	if (!precise && offset == 0) {
		table[0].lowsec= 0;
		table[0].size= cylinders * heads * sectors;
	}
	table[0].sysind= MINIX_PART;
	secpcyl= heads * sectors;
}

void geometry(void)
/* Find out the geometry of the device.  This is no problem for the non-auto
 * floppy drives, except that you need a patch to kernel/floppy.c to use the
 * partitions.  The hard disk geometry may be obtained from the driver if
 * you've installed a patch so that it reports its geometry.  If all fails
 * then the number of cylinders, heads, and sectors per track is guessed by
 * using the maxima found in the partition table.
 */
{
				/* pc  at  qd  ps pat  qh  PS */
	static char fl_cyls[]=   { 40, 80, 40, 80, 40, 80, 80 };
	static char fl_secs[]=   {  9, 15,  9,  9,  9,  9, 18 };
	struct stat dst;
	int err= 0;

	if (submerged) {
		/* Geometry already known. */
		sort();
		return;
	}
	precise= 0;
	cylinders= 0;
	recompute0();
	if (device < 0) return;

	(void) fstat(device, &dst);

	if (S_ISBLK(dst.st_mode)
		&& DEV_FD0 + 4 <= dst.st_rdev
		&& dst.st_rdev <= DEV_FD0 + 4 + (7<<2)
	) {
		/* Non-auto floppy device, geometry is well known. */
		int density= (minor(dst.st_rdev) >> 2) - 1;

		cylinders= fl_cyls[density];
		heads= 2;
		sectors= fl_secs[density];
	} else
	if (S_ISBLK(dst.st_mode) || S_ISCHR(dst.st_mode)) {
		/* Try to get the drive's geometry from the driver. */

		if (diocntl(device, DIOCGETP, &table[0]) < 0)
			err= errno;
		else {
			dos2chs(&table[0].last_head,
						&cylinders, &heads, &sectors);
			cylinders++;
			heads++;
			precise= 1;
		}
	} else
		err= ENODEV;

	if (err != 0) {
		/* Use the maximum cylinder, head and sector numbers in the
		 * partition table as the geometry of the device.
		 */
		unsigned c, h, s;
		int i;

		cylinders= 1;

		for (i= 1; i <= NR_PARTITIONS; i++) {
			if (table[i].sysind == NO_PART) continue;
			dos2chs(&table[i].last_head, &c, &h, &s);
			c++; h++;

			if (c > cylinders) cylinders= c;
			if (h > heads) heads= h;
			if (s > sectors) sectors= s;
		}
		stat_start(1);
		printf("Failure to get the geometry of %s: %s", curdev->name,
			errno == ENOTTY ? "No driver support" : strerror(err));
		stat_end();
		stat_start(0);
		printf("The geometry has been guessed as %ux%ux%u",
						cylinders, heads, sectors);
		stat_end();
	}

	/* Show the base and size of the device instead of the whole drive.
	 * This makes sense for subpartitioning primary partitions.
	 */
	if (precise && diocntl(device, DIOCGETP, &table[0]) < 0)
		precise= 0;
	recompute0();
	sort();
}

typedef struct indicators {	/* Partition type to partition name. */
	unsigned char	ind;
	char		*name;
} indicators_t;

indicators_t ind_table[]= {
	{ NO_PART,	"None"		},
	{ 0x01,		"DOS-12"	},
	{ 0x02,		"XENIX /"	},
	{ 0x03,		"XENIX usr"	},
	{ 0x04,		"DOS-16"	},
	{ 0x05,		"DOS-EXT"	},
	{ 0x06,		"DOS-BIG"	},
	{ 0x07,		"HPFS"		},
	{ 0x08,		"AIX"		},
	{ 0x09,		"COHERENT"	},
	{ 0x0A,		"OS/2"		},
	{ 0x10,		"OPUS"		},
	{ 0x40,		"VENIX286"	},
	{ 0x52,		"MICROPORT"	},
	{ 0x63,		"386/IX"	},
	{ 0x64,		"NOVELL286"	},
	{ 0x65,		"NOVELL386"	},
	{ 0x75,		"PC/IX"		},
	{ 0x80,		"MINIX-OLD"	},
	{ 0x81,		"MINIX"		},
	{ 0x82,		"LINUXswap"	},
	{ 0x83,		"LINUX"		},
	{ 0x93,		"AMOEBA"	},
	{ 0x94,		"AMOEBAbad"	},
	{ 0xA5,		"386BSD"	},
	{ 0xB7,		"BSDI"		},
	{ 0xB8,		"BSDI swap"	},
	{ 0xC7,		"SYRINX"	},
	{ 0xDB,		"CPM"		},
	{ 0xFF,		"BADBLOCKS"	},
};

char *typ2txt(int ind)
/* Translate a numeric partition indicator for human eyes. */
{
	indicators_t *pind;

	for (pind= ind_table; pind < arraylimit(ind_table); pind++) {
		if (pind->ind == ind) return pind->name;
	}
	return "";
}

int round_sysind(int ind, int delta)
/* Find the next known partition type starting with ind in direction delta. */
{
	indicators_t *pind;

	ind= (ind + delta) & 0xFF;

	if (delta < 0) {
		for (pind= arraylimit(ind_table)-1; pind->ind > ind; pind--) {}
	} else {
		for (pind= ind_table; pind->ind < ind; pind++) {}
	}
	return pind->ind;
}

/* Objects on the screen, either simple pieces of the text or the cylinder
 * number of the start of partition three.
 */
typedef enum objtype {
	O_TEXT, O_DEV, O_SUB, O_CYL, O_HEAD, O_SEC,
	O_NUM, O_SORT, O_TYPHEX, O_TYPTXT,
	O_SCYL, O_SHEAD, O_SSEC, O_LCYL, O_LHEAD, O_LSEC, O_BASE, O_SIZE, O_KB
} objtype_t;

typedef struct object {
	struct object	*next;
	objtype_t	type;		/* Text field, cylinder number, etc. */
	char		flags;		/* Modifiable? */
	char		row;
	char		col;
	char		len;
	struct part_entry *entry;	/* What does the object refer to? */
	char		  *text;
	char		value[20];	/* Value when printed. */
} object_t;

#define OF_MOD		0x01	/* Object value is modifiable. */
#define OF_ODD		0x02	/* It has a somewhat odd value. */
#define OF_BAD		0x04	/* Its value is no good at all. */

/* Events: (Keypress events are the value of the key pressed.) */
#define E_ENTER		(-1)	/* Cursor moves onto object. */
#define E_LEAVE		(-2)	/* Cursor leaves object. */
#define E_WRITE		(-3)	/* Write, but not by typing 'w'. */

/* The O_SIZE objects have a dual identity. */
enum howend { SIZE, LAST } howend= SIZE;

object_t *world= nil;
object_t *curobj= nil;

object_t *newobject(objtype_t type, int flags, int row, int col, int len)
/* Make a new object given a type, flags, position and length on the screen. */
{
	object_t *new;
	object_t **aop= &world;

	new= alloc(sizeof(*new));

	new->type= type;
	new->flags= flags;
	new->row= row;
	new->col= col;
	new->len= len;
	new->entry= nil;
	new->text= "";
	new->value[0]= 0;

	new->next= *aop;
	*aop= new;

	return new;
}

unsigned long entry2base(struct part_entry *pe)
/* Return the base sector of the partition if defined. */
{
	return pe->sysind == NO_PART ? 0 : pe->lowsec;
}

unsigned long entry2last(struct part_entry *pe)
{
	return pe->sysind == NO_PART ? -1 : pe->lowsec + pe->size - 1;
}

unsigned long entry2size(struct part_entry *pe)
{
	return pe->sysind == NO_PART ? 0 : pe->size;
}

int overlap(unsigned long sec)
/* See if sec is part of another partition. */
{
	struct part_entry *pe;

	for (pe= table + 1; pe <= table + NR_PARTITIONS; pe++) {
		if (pe->sysind == NO_PART) continue;

		if (pe->lowsec < sec && sec < pe->lowsec + pe->size)
			return 1;
	}
	return 0;
}

int aligned(unsigned long sec, unsigned unit)
/* True if sec is aligned to unit or if it is no problem if it is unaligned. */
{
	return (offset != 0 && extbase == 0) || (sec % unit == 0);
}

void print(object_t *op)
/* Print an object's value if it changed. */
{
	struct part_entry *pe= op->entry;
	int n;
	unsigned long t;
	char *name;
	int oldflags;
	char oldvalue[20];

	/* Remember the old flags and value. */
	oldflags= op->flags;
	strcpy(oldvalue, op->value);

	op->flags&= ~(OF_ODD | OF_BAD);

	switch (op->type) {
	case O_TEXT:
				/* Simple text field. */
		strcpy(op->value, op->text);
		break;
	case O_DEV:
	case O_SUB:
				/* Name of currently edited device. */
		name= op->type == O_DEV ? curdev->name
				: offset == 0 ? "" : curdev->subname;
		if ((n= strlen(name)) > 10)
			strcpy(op->value, name + n - 10);
		else
			sprintf(op->value, "%-10s", name);
		if (device < 0 && op->type == O_DEV) op->flags|= OF_BAD;
		break;
	case O_NUM:
				/* Position and active flag. */
		sprintf(op->value, "%d%c", (int) (pe - table),
					pe->bootind & ACTIVE_FLAG ? '*' : ' ');
		break;
	case O_SORT:
				/* Position if the driver sorts the table. */
		strcpy(op->value, "     ");
		n= strlen(curdev->part);
		switch (device < 0 ? DUNNO : curdev->parttype) {
		case DUNNO:
			sprintf(op->value + 2, "%d", sort_index[pe - table]);
			n= 3;
			break;
		case PRIMARY:
			strcpy(op->value + 3 - (n+1)/2, curdev->part);
			n= strlen(op->value);
			op->value[n-1] += sort_index[pe - table];
			break;
		case SUBPART:
			strcpy(op->value + 2 - n/2, curdev->part);
			n= strlen(op->value);
			op->value[n++] = 'a' + pe - table - 1;
			break;
		}
		while (n < 5) op->value[n++]= ' ';
		op->value[n]= 0;
		break;
	case O_TYPHEX:
				/* Hex partition type indicator. */
		sprintf(op->value, "%02X", pe->sysind);
		break;
	case O_TYPTXT:
				/* Ascii partition type indicator. */
		sprintf(op->value, "%-9s", typ2txt(pe->sysind));
		if (pe->sysind == OLD_MINIX_PART && pe->lowsec & 1)
			op->flags|= OF_BAD;
		break;
	case O_SCYL:
				/* Partition's start cylinder. */
		sprintf(op->value, "%4lu", entry2base(pe) / secpcyl);
		break;
	case O_SHEAD:
				/* Start head. */
		t= entry2base(pe);
		sprintf(op->value, "%3lu", t % secpcyl / sectors);
		if (!aligned(t, secpcyl) && t != table[0].lowsec + sectors)
			op->flags|= OF_ODD;
		break;
	case O_SSEC:
				/* Start sector. */
		t= entry2base(pe);
		sprintf(op->value, "%2lu", t % sectors);
		if (!aligned(t, sectors)) op->flags|= OF_ODD;
		break;
	case O_CYL:
				/* Number of cylinders. */
		sprintf(op->value, "%4u", cylinders);
		break;
	case O_HEAD:
				/* Number of heads. */
		sprintf(op->value, "%3u", heads);
		break;
	case O_SEC:
				/* Number of sectors per track. */
		sprintf(op->value, "%2u", sectors);
		break;
	case O_LCYL:
				/* Partition's last cylinder. */
		t= entry2last(pe);
		sprintf(op->value, "%4lu", t == -1 ? 0 : t / secpcyl);
		break;
	case O_LHEAD:
				/* Partition's last head. */
		t= entry2last(pe);
		sprintf(op->value, "%3lu", t == -1 ? 0 : t % secpcyl / sectors);
		if (!aligned(t + 1, secpcyl)) op->flags|= OF_ODD;
		break;
	case O_LSEC:
				/* Partition's last sector. */
		t= entry2last(pe);
		sprintf(op->value, t == -1 ? "-1" : "%2lu", t % sectors);
		if (!aligned(t + 1, sectors)) op->flags|= OF_ODD;
		break;
	case O_BASE:
				/* Partition's base sector. */
		sprintf(op->value, "%8lu", entry2base(pe));
		if (pe->sysind != NO_PART && pe != &table[0]
		   && (pe->lowsec <= table[0].lowsec || overlap(pe->lowsec)))
			op->flags|= OF_BAD;
		break;
	case O_SIZE:
				/* Size of partitition in sectors. */
		t= howend == SIZE ? entry2size(pe) : entry2last(pe);
		sprintf(op->value, "%8lu", pe->sysind == NO_PART ? 0 : t);
		if (pe->sysind != NO_PART && (pe->size == 0
		    || pe->lowsec + pe->size > table[0].lowsec + table[0].size
		    || overlap(pe->lowsec + pe->size)))
			op->flags|= OF_BAD;
		break;
	case O_KB:
				/* Size of partitition in kilobytes. */
		sprintf(op->value, "%7lu", entry2size(pe) / 2);
		break;
	default:
		sprintf(op->value, "?? %d ??", op->type);
	}

	/* If a value overflows the print field then show a blank
	 * reverse video field.
	 */
	if (strlen(op->value) > op->len) {
		memset(op->value, ' ', op->len);
		op->value[op->len]= 0;
		op->flags|= OF_BAD;
	}

	if ((op->flags & (OF_ODD | OF_BAD)) == (oldflags & (OF_ODD | OF_BAD))
				&& strcmp(op->value, oldvalue) == 0) {
		/* The value did not change. */
		return;
	}

	switch (op->type) {
	case O_TEXT:
	case O_DEV:
	case O_SUB:
	case O_TYPTXT:		/* Cursor at the left. */
		set_cursor(op->row, op->col);
		break;
	default:		/* Others have cursor at the right. */
		set_cursor(op->row, op->col - strlen(op->value) + 1);
	}
	if (op->flags & OF_BAD) tputs(t_so, 1, putchr);
	else
	if (op->flags & OF_ODD) tputs(t_md, 1, putchr);
	putstr(op->value);
	if (op->flags & OF_BAD) tputs(t_se, 1, putchr);
	else
	if (op->flags & OF_ODD) tputs(t_me, 1, putchr);
}

void display(void)
/* Repaint all objects that changed. */
{
	object_t *op;

	for (op= world; op != nil; op= op->next) print(op);
}

int typing;	/* Set if a digit has been typed to set a value. */
int magic;	/* Changes when using the magic key. */

void event(int ev, object_t *op);

void m_refresh(int ev, object_t *op)
/* Repaint the screen. */
{
	object_t *op2;

	if (ev != ctrl('L')) return;

	clear_screen();
	for (op2= world; op2 != nil; op2= op2->next) op2->value[0]= 0;
}

char size_last[]= "Size";

void m_orientation(int ev, object_t *op)
{
	if (ev != ' ') return;

	switch (howend) {
	case SIZE:
		howend= LAST;
		strcpy(size_last, "Last");
		break;
	case LAST:
		howend= SIZE;
		strcpy(size_last, "Size");
	}
}

void m_activate(int ev, object_t *op)
/* Toggle the active bit on a partition, only one partition is left active. */
{
	object_t *oldop;

	if ((ev != '-' && ev != '+') || device < 0 || op->type != O_NUM) return;

	for (oldop= world; oldop != nil; oldop= oldop->next) {
		if (oldop != op
			&& oldop->type == O_NUM
			&& oldop->entry->bootind != 0
		) {
			oldop->entry->bootind= 0;
		}
	}
	op->entry->bootind^= ACTIVE_FLAG;
	dirty= 1;
}

void m_move(int ev, object_t *op)
/* Move to the nearest modifiably object in the intended direction.  Objects
 * on the same row or column are really near.
 */
{
	object_t *near, *op2;
	unsigned dist, d2, dr, dc;

	if (ev != 'h' && ev != 'j' && ev != 'k' && ev != 'l' && ev != 'H')
		return;

	near= op;
	dist= -1;

	for (op2= world; op2 != nil; op2= op2->next) {
		if (op2 == op || !(op2->flags & OF_MOD)) continue;

		dr= abs(op2->row - op->row);
		dc= abs(op2->col - op->col);

		d2= 25*dr*dr + dc*dc;
		if (op2->row != op->row && op2->col != op->col) d2+= 1000;

		switch (ev) {
		case 'h':	/* Left */
			if (op2->col >= op->col) d2= -1;
			break;
		case 'j':	/* Down */
			if (op2->row <= op->row) d2= -1;
			break;
		case 'k':	/* Up */
			if (op2->row >= op->row) d2= -1;
			break;
		case 'l':	/* Right */
			if (op2->col <= op->col) d2= -1;
			break;
		case 'H':	/* Home */
			if (op2->type == O_DEV) d2= 0;
		}
		if (d2 < dist) { near= op2; dist= d2; }
	}
	if (near != op) event(E_LEAVE, op);
	event(E_ENTER, near);
}

void m_updown(int ev, object_t *op)
/* Move a partition table entry up or down. */
{
	int i, j;
	struct part_entry tmp;
	int tmpx;
	object_t *op2;

	if (ev != ctrl('K') && ev != ctrl('J')) return;
	if (op->entry == nil) return;

	i= op->entry - table;
	if (ev == ctrl('K')) {
		if (i <= 1) return;
		j= i-1;
	} else {
		if (i >= NR_PARTITIONS) return;
		j= i+1;
	}

	tmp= table[i]; table[i]= table[j]; table[j]= tmp;
	tmpx= existing[i]; existing[i]= existing[j]; existing[j]= tmpx;
	sort();
	dirty= 1;
	event(ev == ctrl('K') ? 'k' : 'j', op);
}

void m_enter(int ev, object_t *op)
/* We've moved onto this object. */
{
	if (ev != E_ENTER && ev != ' ' && ev != '<' && ev != '>') return;
	curobj= op;
	typing= 0;
	magic= 0;
}

void m_leave(int ev, object_t *op)
/* About to leave this object. */
{
	if (ev != E_LEAVE) return;
	event('r', op);
}

int within(unsigned *var, unsigned low, unsigned value, unsigned high)
/* Only set *var to value if it looks reasonable. */
{
	if (low <= value && value <= high) {
		*var= value;
		return 1;
	} else
		return 0;
}

int lwithin(unsigned long *var, unsigned long low, unsigned long value,
							unsigned long high)
{
	if (low <= value && value <= high) {
		*var= value;
		return 1;
	} else
		return 0;
}

int nextdevice(object_t *op, int delta)
/* Select the next or previous device from the device list. */
{
	dev_t rdev;

	if (offset != 0) return 0;
	if (dirty) event(E_WRITE, op);
	if (dirty) return 0;

	if (device >= 0) {
		(void) close(device);
		device= -1;
	}
	recompute0();

	rdev= curdev->rdev;
	if (delta < 0) {
		do
			curdev= curdev->prev;
		while (delta < -1 && major(curdev->rdev) == major(rdev)
			&& curdev->rdev < rdev);
	} else {
		do
			curdev= curdev->next;
		while (delta > 1 && major(curdev->rdev) == major(rdev)
			&& curdev->rdev > rdev);
	}
	return 1;
}

void check_ind(struct part_entry *pe)
/* If there are no other partitions then make this new one active. */
{
	struct part_entry *pe2;

	if (pe->sysind != NO_PART) return;

	for (pe2= table + 1; pe2 < table + 1 + NR_PARTITIONS; pe2++)
		if (pe2->sysind != NO_PART || pe2->bootind & ACTIVE_FLAG) break;

	if (pe2 == table + 1 + NR_PARTITIONS) pe->bootind= ACTIVE_FLAG;
}

int check_existing(struct part_entry *pe)
/* Check and if not ask if an existing partition may be modified. */
{
	static int expert= 0;
	int c;

	if (expert || pe == nil || !existing[pe - table]) return 1;

	stat_start(1);
	putstr("Do you wish to modify existing partitions? (y/n) ");
	fflush(stdout);
	while ((c= getchar()) != 'y' && c != 'n') {}
	putchr(c);
	stat_end();
	return (expert= (c == 'y'));
}

void m_modify(int ev, object_t *op)
/* Increment, decrement, set, or toggle the value of an object, using
 * arithmetic tricks the author doesn't understand either.
 */
{
	struct part_entry *pe= op->entry;
	int mul, delta;
	unsigned level= 1;
	unsigned long surplus;
	int radix= op->type == O_TYPHEX ? 0x10 : 10;
	unsigned long t;

	if (device < 0 && op->type != O_DEV) return;

	switch (ev) {
	case '-':
		mul= radix; delta= -1; typing= 0;
		break;
	case '+':
		mul= radix; delta= 1; typing= 0;
		break;
	case '\b':
		if (!typing) return;
		mul= 1; delta= 0;
		break;
	case '\r':
		typing= 0;
		return;
	default:
		if ('0' <= ev && ev <= '9')
			delta= ev - '0';
		else
		if (radix == 0x10 && 'a' <= ev && ev <= 'f')
			delta= ev - 'a' + 10;
		else
		if (radix == 0x10 && 'A' <= ev && ev <= 'F')
			delta= ev - 'A' + 10;
		else
			return;

		mul= typing ? radix*radix : 0;
		typing= 1;
	}
	magic= 0;

	if (!check_existing(pe)) return;

	switch (op->type) {
	case O_DEV:
		if (ev != '-' && ev != '+') return;
		if (!nextdevice(op, delta)) return;
		break;
	case O_CYL:
		if (!within(&cylinders, 1,
			cylinders * mul / radix + delta, 1024)) return;
		recompute0();
		break;
	case O_HEAD:
		if (!within(&heads, 1, heads * mul / radix + delta, 255))
			return;
		recompute0();
		break;
	case O_SEC:
		if (!within(&sectors, 1, sectors * mul / radix + delta, 63))
			return;
		recompute0();
		break;
	case O_TYPHEX:
		check_ind(pe);
		pe->sysind= pe->sysind * mul / radix + delta;
		break;
	case O_TYPTXT:
		if (ev != '-' && ev != '+') return;
		check_ind(pe);
		pe->sysind= round_sysind(pe->sysind, delta);
		break;
	case O_SCYL:
		level= heads;
	case O_SHEAD:
		level*= sectors;
	case O_SSEC:
		if (op->type != O_SCYL && ev != '-' && ev != '+') return;
	case O_BASE:
		if (pe->sysind == NO_PART) memset(pe, 0, sizeof(*pe));
		t= pe->lowsec;
		surplus= t % level;
		if (!lwithin(&t, 0L,
			(t / level * mul / radix + delta) * level + surplus,
			MAXSIZE)) return;
		if (howend == LAST) pe->size-= t - pe->lowsec;
		pe->lowsec= t;
		check_ind(pe);
		if (pe->sysind == NO_PART) pe->sysind= MINIX_PART;
		break;
	case O_LCYL:
		level= heads;
	case O_LHEAD:
		level*= sectors;
	case O_LSEC:
		if (op->type != O_LCYL && ev != '-' && ev != '+') return;

		if (pe->sysind == NO_PART) memset(pe, 0, sizeof(*pe));
		t= pe->lowsec + pe->size - 1 + level;
		surplus= t % level - mul / radix * level;
		if (!lwithin(&t, 0L,
			(t / level * mul / radix + delta) * level + surplus,
			MAXSIZE)) return;
		if (howend == SIZE) {
			pe->lowsec= t - pe->size + 1;
		} else {
			pe->size= t - pe->lowsec + 1;
		}
		check_ind(pe);
		if (pe->sysind == NO_PART) pe->sysind= MINIX_PART;
		break;
	case O_KB:
		level= 2;
		if (mul == 0) pe->size= 0;	/* new value, no surplus */
	case O_SIZE:
		if (pe->sysind == NO_PART) {
			if (op->type == O_KB || howend == SIZE) {
				/* First let loose magic to set the base. */
				event('m', op);
				magic= 0;
				pe->size= 0;
				event(ev, op);
				return;
			}
			memset(pe, 0, sizeof(*pe));
		}
		t= (op->type == O_KB || howend == SIZE) ? pe->size
						: pe->lowsec + pe->size - 1;
		surplus= t % level;
		if (!lwithin(&t, 0L,
			(t / level * mul / radix + delta) * level + surplus,
			MAXSIZE)) return;
		pe->size= (op->type == O_KB || howend == SIZE) ? t :
							t - pe->lowsec + 1;
		check_ind(pe);
		if (pe->sysind == NO_PART) pe->sysind= MINIX_PART;
		break;
	default:
		return;
	}

	/* The order among the entries may have changed. */
	sort();
	dirty= 1;
}

unsigned long spell[3 + 4 * (1+NR_PARTITIONS)];
int nspells;
objtype_t touching;

void newspell(unsigned long charm)
/* Add a new spell, descending order for the base, ascending for the size. */
{
	int i, j;

	if (charm - table[0].lowsec > table[0].size) return;

	for (i= 0; i < nspells; i++) {
		if (charm == spell[i]) return;	/* duplicate */

		if (touching == O_BASE) {
			if (charm == table[0].lowsec + table[0].size) return;
			if ((spell[0] - charm) < (spell[0] - spell[i])) break;
		} else {
			if (charm == table[0].lowsec) return;
			if ((charm - spell[0]) < (spell[i] - spell[0])) break;
		}
	}
	for (j= ++nspells; j > i; j--) spell[j]= spell[j-1];
	spell[i]= charm;
}

void m_magic(int ev, object_t *op)
/* Apply magic onto a base or size number. */
{
	struct part_entry *pe= op->entry, *pe2;
	int rough= (offset != 0 && extbase == 0);

	if (ev != 'm' || device < 0) return;
	typing= 0;

	if (!check_existing(pe)) return;

	if (magic == 0) {
		/* See what magic we can let loose on this value. */
		nspells= 1;

		/* First spell, the current value. */
		switch (op->type) {
		case O_SCYL:
		case O_SHEAD:	/* Start of partition. */
		case O_SSEC:
		case O_BASE:
			touching= O_BASE;
			spell[0]= pe->lowsec;
			break;
		case O_LCYL:
		case O_LHEAD:
		case O_LSEC:	/* End of partition. */
		case O_KB:
		case O_SIZE:
			touching= O_SIZE;
			spell[0]= pe->lowsec + pe->size;
			break;
		default:
			return;
		}
		if (pe->sysind == NO_PART) {
			memset(pe, 0, sizeof(*pe));
			check_ind(pe);
			pe->sysind= MINIX_PART;
			spell[0]= 0;
			if (touching == O_SIZE) {
				/* First let loose magic on the base. */
				object_t *op2;

				for (op2= world; op2 != nil; op2= op2->next) {
					if (op2->row == op->row &&
							op2->type == O_BASE) {
						event('m', op2);
					}
				}
				magic= 0;
				event('m', op);
				return;
			}
		}
		/* Avoid the first sector on the device. */
		if (spell[0] == table[0].lowsec) newspell(spell[0] + 1);

		/* Further interesting values are the the bases of other
		 * partitions or their ends.
		 */
		for (pe2= table; pe2 < table + 1 + NR_PARTITIONS; pe2++) {
			if (pe2 == pe || pe2->sysind == NO_PART) continue;
			if (pe2->lowsec == table[0].lowsec)
				newspell(table[0].lowsec + 1);
			else
				newspell(pe2->lowsec);
			newspell(pe2->lowsec + pe2->size);
			if (touching == O_BASE && howend == SIZE) {
				newspell(pe2->lowsec - pe->size);
				newspell(pe2->lowsec + pe2->size - pe->size);
			}
			if (pe2->lowsec % sectors != 0) rough= 1;
		}
		/* Present values rounded up to the next cylinder unless
		 * the table is already a mess.  Use "start + 1 track" instead
		 * of "start + 1 cylinder".  Also add the end of the last
		 * cylinder.
		 */
		if (!rough) {
			unsigned long n= spell[0];
			if (n == table[0].lowsec) n++;
			n= (n + sectors - 1) / sectors * sectors;
			if (n != table[0].lowsec + sectors)
				n= (n + secpcyl - 1) / secpcyl * secpcyl;
			newspell(n);
			if (touching == O_SIZE)
				newspell(table[0].size / secpcyl * secpcyl);
		}
	}
	/* Magic has been applied, a spell needs to be chosen. */

	if (++magic == nspells) magic= 0;

	if (touching == O_BASE) {
		if (howend == LAST) pe->size-= spell[magic] - pe->lowsec;
		pe->lowsec= spell[magic];
	} else
		pe->size= spell[magic] - pe->lowsec;

	/* The order among the entries may have changed. */
	sort();
	dirty= 1;
}

typedef struct diving {
	struct diving	*up;
	struct part_entry  old0;
	char		*oldsubname;
	char		oldpart[6];
	parttype_t	oldparttype;
	unsigned long	oldoffset;
	unsigned long	oldextbase;
} diving_t;

diving_t *diving= nil;

void m_in(int ev, object_t *op)
/* Go down into a primary or extended partition. */
{
	diving_t *newdiv;
	struct part_entry *pe= op->entry, ext;
	int n;

	if (ev != '>' || device < 0 || pe == nil || pe == &table[0]
		|| (pe->sysind != MINIX_PART && pe->sysind != EXT_PART)
		|| pe->size == 0) return;

	ext= *pe;
	if (extbase != 0) ext.size= extbase + extsize - ext.lowsec;

	if (dirty) event(E_WRITE, op);
	if (dirty) return;
	if (device >= 0) { close(device); device= -1; }

	newdiv= alloc(sizeof(*newdiv));
	newdiv->old0= table[0];
	newdiv->oldsubname= curdev->subname;
	strcpy(newdiv->oldpart, curdev->part);
	newdiv->oldparttype= curdev->parttype;
	newdiv->oldoffset= offset;
	newdiv->oldextbase= extbase;
	newdiv->up= diving;
	diving= newdiv;

	table[0]= ext;

	n= strlen(diving->oldsubname);
	curdev->subname= alloc((n + 3) * sizeof(curdev->subname[0]));
	strcpy(curdev->subname, diving->oldsubname);
	curdev->subname[n++]= ':';
	curdev->subname[n++]= '0' + (pe - table);
	curdev->subname[n]= 0;

	if (curdev->parttype != DUNNO) curdev->parttype--;
	offset= ext.lowsec;
	if (ext.sysind == EXT_PART && extbase == 0) {
		extbase= ext.lowsec;
		extsize= ext.size;
		curdev->parttype= DUNNO;
	}
	if (curdev->parttype == SUBPART) {
		n= strlen(curdev->part);
		if (n == 5) { strcpy(curdev->part, curdev->part + 1); n--; }
		curdev->part[n-1] += sort_index[pe - table];
	}

	submerged= 1;
	event('r', op);
}

void m_out(int ev, object_t *op)
/* Go up from an extended or subpartition table to its enclosing. */
{
	diving_t *olddiv;

	if (ev != '<' || diving == nil) return;

	if (dirty) event(E_WRITE, op);
	if (dirty) return;
	if (device >= 0) { close(device); device= -1; }

	olddiv= diving;
	diving= olddiv->up;

	table[0]= olddiv->old0;

	free(curdev->subname);
	curdev->subname= olddiv->oldsubname;

	strcpy(curdev->part, olddiv->oldpart);
	curdev->parttype= olddiv->oldparttype;
	offset= olddiv->oldoffset;
	extbase= olddiv->oldextbase;

	free(olddiv);

	event('r', op);
	if (diving == nil) submerged= 0;	/* We surfaced. */
}

void m_read(int ev, object_t *op)
/* Read the partition table from the current device. */
{
	int i, mode, n;
	struct part_entry *pe;

	if (ev != 'r' || device >= 0) return;

	/* Open() may cause kernel messages: */
	stat_start(0);
	fflush(stdout);

	if (((device= open(curdev->name, mode= O_RDWR|O_CREAT, 0666)) < 0
		    && (errno != EACCES
			|| (device= open(curdev->name, mode= O_RDONLY)) < 0))
		|| lseek(device, (off_t) offset * SECTOR_SIZE, SEEK_SET) == -1
	) {
		stat_start(1);
		printf("%s: %s", curdev->name, strerror(errno));
		stat_end();
		return;
	}

	/* Assume up to five lines of kernel messages. */
	statusrow+= 5-1;
	stat_end();

	if (mode == O_RDONLY) {
		stat_start(1);
		printf("%s: Readonly", curdev->name);
		stat_end();
	}
	memset(bootblock, 0, sizeof(bootblock));

	n= read(device, bootblock, SECTOR_SIZE);

	if (n <= 0) stat_start(1);
	if (n < 0) {
		printf("%s: %s", curdev->name, strerror(errno));
		close(device);
		device= -1;
	} else
	if (n < SECTOR_SIZE) printf("%s: Unexpected EOF", curdev->subname);
	if (n <= 0) stat_end();

	if (n < SECTOR_SIZE) n= SECTOR_SIZE;

	memcpy(table+1, bootblock+PART_TABLE_OFF,
					NR_PARTITIONS * sizeof(table[1]));
	for (i= 1; i <= NR_PARTITIONS; i++) {
		if (table[i].lowsec > MAXSIZE) break;
	}
	if (i <= NR_PARTITIONS || bootblock[510] != 0x55
				|| bootblock[511] != 0xAA) {
		/* Invalid boot block, install bootstrap, wipe partition table.
		 */
		memset(bootblock, 0, sizeof(bootblock));
		memcpy(bootblock, (void *) bootstrap, sizeof(bootstrap));
		memset(table+1, 0, NR_PARTITIONS * sizeof(table[1]));
		stat_start(1);
		printf("%s: Invalid partition table (reset)", curdev->subname);
		stat_end();
	}

	/* Fix an extended partition table up to something mere mortals can
	 * understand.  Record already defined partitions.
	 */
	for (i= 1; i <= NR_PARTITIONS; i++) {
		pe= &table[i];
		if (extbase != 0 && pe->sysind != NO_PART)
			pe->lowsec+= pe->sysind == EXT_PART
						? extbase : offset;
		existing[i]= pe->sysind != NO_PART;
	}
	geometry();
	dirty= 0;

	/* Warn about grave dangers ahead. */
	if (extbase != 0) {
		stat_start(1);
		printf("Warning: You are in a DOS extended partition.");
		stat_end();
	}
}

void m_write(int ev, object_t *op)
/* Write the partition table back if modified. */
{
	int c;
	struct part_entry new_table[NR_PARTITIONS], *pe;

	if ((ev != 'w' && ev != E_WRITE) || !dirty) return;
	if (device < 0) { dirty= 0; return; }

	if (bootblock[510] != 0x55 || bootblock[511] != 0xAA) {
		/* Invalid boot block, warn user. */
		stat_start(1);
		printf("Warning: About to write a new table on %s",
							curdev->subname);
		stat_end();
	}
	if (extbase != 0) {
		/* Will this stop the luser?  Probably not... */
		stat_start(1);
		printf("You have changed a DOS extended partition.  Bad Idea.");
		stat_end();
	}
	stat_start(1);
	putstr("Save partition table? (y/n) ");
	fflush(stdout);

	while ((c= getchar()) != 'y' && c != 'n' && c != ctrl('?')) {}

	if (c == ctrl('?')) putstr("DEL"); else putchr(c);
	stat_end();
	if (c == 'n' && ev == E_WRITE) dirty= 0;
	if (c != 'y') return;

	memcpy(new_table, table+1, NR_PARTITIONS * sizeof(table[1]));
	for (pe= new_table; pe < new_table + NR_PARTITIONS; pe++) {
		if (pe->sysind == NO_PART) {
			memset(pe, 0, sizeof(*pe));
		} else {
			abs2dos(&pe->start_head, pe->lowsec);
			abs2dos(&pe->last_head, pe->lowsec + pe->size - 1);

			/* Fear and loathing time: */
			if (extbase != 0)
				pe->lowsec-= pe->sysind == EXT_PART
						? extbase : offset;
		}
	}
	memcpy(bootblock+PART_TABLE_OFF, new_table, sizeof(new_table));
	bootblock[510]= 0x55;
	bootblock[511]= 0xAA;

	if (lseek(device, (off_t) offset * SECTOR_SIZE, SEEK_SET) == -1
		|| write(device, bootblock, SECTOR_SIZE) < 0
	) {
		stat_start(1);
		printf("%s: %s", curdev->name, strerror(errno));
		stat_end();
		return;
	}
	dirty= 0;
}

void m_shell(int ev, object_t *op)
/* Shell escape, to do calculations for instance. */
{
	int r, pid, status;
	void (*sigint)(int), (*sigquit)(int), (*sigterm)(int);

	if (ev != 's') return;

	reset_tty();
	fflush(stdout);

	switch (pid= fork()) {
	case -1:
		stat_start(1);
		printf("can't fork: %s\n", strerror(errno));
		stat_end();
		break;
	case 0:
		if (device >= 0) (void) close(device);
		execl("/bin/sh", "sh", (char *) nil);
		r= errno;
		stat_start(1);
		printf("/bin/sh: %s\n", strerror(errno));
		stat_end();
		exit(127);
	}
	sigint= signal(SIGINT, SIG_IGN);
	sigquit= signal(SIGQUIT, SIG_IGN);
	sigterm= signal(SIGTERM, SIG_IGN);
	while (pid >= 0 && (r= wait(&status)) >= 0 && r != pid) {}
	(void) signal(SIGINT, sigint);
	(void) signal(SIGQUIT, sigquit);
	(void) signal(SIGTERM, sigterm);
	tty_raw();
	if (pid < 0)
		;
	else
	if (WIFEXITED(status) && WEXITSTATUS(status) == 127)
		stat_start(0);	/* Match the stat_start in the child. */
	else
		event(ctrl('L'), op);
}

void m_dump(int ev, object_t *op)
/* Raw dump of the partition table. */
{
	struct part_entry table[NR_PARTITIONS], *pe;
	int i;
	unsigned c, h, s;

	if (ev != 'p' || device < 0) return;

	memcpy(table, bootblock+PART_TABLE_OFF,
					NR_PARTITIONS * sizeof(table[0]));
	for (i= 0; i < NR_PARTITIONS; i++) {
		pe= &table[i];
		stat_start(0);
		dos2chs(&pe->start_head, &c, &h, &s);
		printf("%2d%c      %02X%15u%5u%4u",
			i+1,
			pe->bootind & ACTIVE_FLAG ? '*' : ' ',
			pe->sysind,
			c, h, s);
		dos2chs(&pe->last_head, &c, &h, &s);
		printf("%6u%5u%4u%10lu%10ld%9lu",
			c, h, s,
			pe->lowsec,
			howend == SIZE ? pe->size : pe->size + pe->lowsec - 1,
			pe->size / 2);
		stat_end();
	}
}

int quitting= 0;

void m_quit(int ev, object_t *op)
/* Write the partition table if modified and exit. */
{
	if (ev != 'q') return;

	quitting= 1;

	if (dirty) event(E_WRITE, op);
	if (dirty) quitting= 0;
}

void m_help(int ev, object_t *op)
/* For people without a clue; let's hope they can find the '?' key. */
{
	static struct help {
		char	*keys;
		char	*what;
	} help[]= {
	  { "? !",			"This help / more advice!" },
	  { "CTRL-L",			"Refresh screen" },
	  { "hjkl (arrow keys)",	"Move around" },
	  { "+ - (= _ PgUp PgDn)",	"Increment/decrement/make active" },
	  { "0-9 (a-f)",		"Enter value" },
	  { "CTRL-K CTRL-J",		"Move entry up/down" },
	  { ">",			"Start a subpartition table" },
	  { "<",			"Back to the primary partition table" },
	  { "m",			"Cycle through magic values" },
	  { "spacebar",			"Keep \"Size\" or \"Last\" constant" },
	  { "r w",			"Read/write partition table" },
	  { "p s q",			"Raw dump / Shell escape / Quit" },
	};
	static char *advice[] = {
	  "Walk through a device list with - and +, then hit 'r'.",
	  "",
	  "To make a new partition:  Go over to the Size or Kb field of an",
	  "unused partition and type the size.  Hit the 'm' key to pad the",
	  "partition out to a cylinder boundary.  Hit 'm' again to pad it out",
	  "to the end of the disk.  You can hit 'm' more than once on a base",
	  "or size field to see several interesting values go by.",
	  "Note: Other Operating Systems can be picky about partitions that",
	  "are not padded to cylinder boundaries.  Look for highlighted head",
	  "or sector numbers.",
	  "",
	  "To delete a partition:  Type a zero in the hex Type field.",
	  "",
	  "To make a partition active: Type - or + in the Num field.",
	};

	if (ev == '?') {
		struct help *hp;

		for (hp= help; hp < arraylimit(help); hp++) {
			stat_start(0);
			printf("%-25s - %s", hp->keys, hp->what);
			stat_end();
		}
		stat_start(0);
		putstr("Things like ");
		putstr(t_so); putstr("this"); putstr(t_se);
		putstr(" must be checked, but ");
		putstr(t_md); putstr("this"); putstr(t_me);
		putstr(" is not really a problem");
		stat_end();
	} else
	if (ev == '!') {
		char **ap;

		for (ap= advice; ap < arraylimit(advice); ap++) {
			stat_start(0);
			putstr(*ap);
			stat_end();
		}
	}
}

void event(int ev, object_t *op)
/* Simply call all modifiers for an event, each one knows when to act. */
{
	m_help(ev, op);
	m_refresh(ev, op);
	m_orientation(ev, op);
	m_activate(ev, op);
	m_move(ev, op);
	m_updown(ev, op);
	m_enter(ev, op);
	m_leave(ev, op);
	m_modify(ev, op);
	m_magic(ev, op);
	m_in(ev, op);
	m_out(ev, op);
	m_read(ev, op);
	m_write(ev, op);
	m_shell(ev, op);
	m_dump(ev, op);
	m_quit(ev, op);
}

int keypress(void)
/* Get a single keypress.  Translate compound keypresses (arrow keys) to
 * their simpler equivalents.
 */
{
	char ch;
	int c;
	int esc= 0;

	set_cursor(curobj->row, curobj->col);
	fflush(stdout);

	do {
		if (read(0, &ch, sizeof(ch)) < 0) fatal("stdin");
		c= (unsigned char) ch;
		switch (esc) {
		case 0:
			switch (c) {
			case ctrl('['):	esc= 1; break;
			case '_':	c= '-'; break;
			case '=':	c= '+'; break;
			}
			break;
		case 1:
			esc= c == '[' ? 2 : 0;
			break;
		case 2:
			switch (c) {
			case 'D':	c= 'h';	break;
			case 'B':	c= 'j';	break;
			case 'A':	c= 'k';	break;
			case 'C':	c= 'l';	break;
			case 'H':	c= 'H';	break;
			case 'U':
			case 'S':	c= '-';	break;
			case 'V':
			case 'T':	c= '+';	break;
			}
			/*FALL THROUGH*/
		default:
			esc= 0;
		}
	} while (esc > 0);

	switch (c) {
	case ctrl('B'):	c= 'h';	break;
	case ctrl('N'):	c= 'j';	break;
	case ctrl('P'):	c= 'k';	break;
	case ctrl('F'):	c= 'l';	break;
	}

	return c;
}

void mainloop(void)
/* Get keypress, handle event, display results, reset screen, ad infinitum. */
{
	int key;

	while (!quitting) {
		key= keypress();

		stat_reset();

		event(key, curobj);

		display();
	}
}

int main(int argc, char **argv)
{
	object_t *op;
	int i, r, key;
	struct part_entry *pe;

	/* Define a few objects to show on the screen.  First text: */
	op= newobject(O_TEXT, 0, 0, 22, 13); op->text= "----first----";
	op= newobject(O_TEXT, 0, 0, 37, 13); op->text= "--geom/last--";
	op= newobject(O_TEXT, 0, 0, 52, 18); op->text= "------sectors-----";
	op= newobject(O_TEXT, 0, 1,  0, 15); op->text= "Num Sort   Type";
	op= newobject(O_TEXT, 0, 1, 23, 12); op->text= "Cyl Head Sec";
	op= newobject(O_TEXT, 0, 1, 38, 12); op->text= "Cyl Head Sec";
	op= newobject(O_TEXT, 0, 1, 56,  4); op->text= "Base";
	op= newobject(O_TEXT, 0, 1, 66,  4); op->text= size_last;
	op= newobject(O_TEXT, 0, 1, 77,  2); op->text= "Kb";

	/* The device is the current object: */
    curobj= newobject(O_DEV,  OF_MOD, 2,  9, 10);
	op= newobject(O_SUB,       0, 3,  9, 10);

	/* Geometry: */
	op= newobject(O_CYL,  OF_MOD, 2, 40,  4); op->entry= &table[0];
	op= newobject(O_HEAD, OF_MOD, 2, 45,  3); op->entry= &table[0];
	op= newobject(O_SEC,  OF_MOD, 2, 49,  2); op->entry= &table[0];

	/* Objects for the device: */
	op= newobject(O_SCYL,  0, 3, 25,  4); op->entry= &table[0];
	op= newobject(O_SHEAD, 0, 3, 30,  3); op->entry= &table[0];
	op= newobject(O_SSEC,  0, 3, 34,  2); op->entry= &table[0];
	op= newobject(O_LCYL,  0, 3, 40,  4); op->entry= &table[0];
	op= newobject(O_LHEAD, 0, 3, 45,  3); op->entry= &table[0];
	op= newobject(O_LSEC,  0, 3, 49,  2); op->entry= &table[0];
	op= newobject(O_BASE,  0, 3, 59,  8); op->entry= &table[0];
	op= newobject(O_SIZE,  0, 3, 69,  8); op->entry= &table[0];
	op= newobject(O_KB,    0, 3, 78,  7); op->entry= &table[0];

	/* Objects for each partition: */
	for (r= 4, pe= table+1; r <= 7; r++, pe++) {
		op= newobject(O_NUM,    OF_MOD, r,  2,  2); op->entry= pe;
		op= newobject(O_SORT,        0, r,  7,  5); op->entry= pe;
		op= newobject(O_TYPHEX, OF_MOD, r, 10,  2); op->entry= pe;
		op= newobject(O_TYPTXT, OF_MOD, r, 12,  9); op->entry= pe;
		op= newobject(O_SCYL,   OF_MOD, r, 25,  4); op->entry= pe;
		op= newobject(O_SHEAD,  OF_MOD, r, 30,  3); op->entry= pe;
		op= newobject(O_SSEC,   OF_MOD, r, 34,  2); op->entry= pe;
		op= newobject(O_LCYL,   OF_MOD, r, 40,  4); op->entry= pe;
		op= newobject(O_LHEAD,  OF_MOD, r, 45,  3); op->entry= pe;
		op= newobject(O_LSEC,   OF_MOD, r, 49,  2); op->entry= pe;
		op= newobject(O_BASE,   OF_MOD, r, 59,  8); op->entry= pe;
		op= newobject(O_SIZE,   OF_MOD, r, 69,  8); op->entry= pe;
		op= newobject(O_KB,     OF_MOD, r, 78,  7); op->entry= pe;
	}

	for (i= 1; i < argc; i++) newdevice(argv[i], 0);

	if (firstdev == nil) {
		getdevices();
		key= ctrl('L');
	} else {
		key= 'r';
	}

	if (firstdev != nil) {
		init_tty();
		clear_screen();
		event(key, curobj);
		display();
		stat_start(0);
		putstr("Type '?' for help");
		stat_end();
		mainloop();
		reset_tty();
	}
	exit(0);
}
