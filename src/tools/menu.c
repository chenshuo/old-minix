/* menu - print initial menu		Author: Bruce Evans */

#include <sys/types.h>
#include <limits.h>

#include <minix/config.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/boot.h>

#include "../fs/const.h"
#include "../fs/type.h"

/* Menu prints the initial menu and waits for directions.  To compile, use:
 *
 *    cc -c -D_MINIX -D_POSIX_SOURCE menu.c
 *    asld -o menu menu1.s menu.s /usr/lib/libc.a /usr/lib/end.s
 *
 */

#define MAXWIDTH	 32	/* max. width of an ``integer string'' */
#define SECT_SHIFT        9	/* sectors are 512 bytes */
#define SECTOR_SIZE (1<<SECT_SHIFT)	/* bytes in a sector */
#define PARB 6

struct bparam_s boot_parameters =
	{DROOTDEV, DRAMIMAGEDEV, DRAMSIZE, DSCANCODE, DPROCESSOR};

char *ramimname = "/dev/fd0";
char *rootname = "/dev/ram";

#define between(c,l,u)	((unsigned short) ((c) - (l)) <= ((u) - (l)))
#define isprint(c)	between(c, ' ', '~')
#define isdigit(c)	between(c, '0', '9')
#define islower(c)	between(c, 'a', 'z')
#define isupper(c)	between(c, 'A', 'Z')
#define toupper(c)	( (c) + 'A' - 'a' )
#define nextarg(t)	(*argp.t++)
#define prn(t,b,s)	{ printnum((long)nextarg(t),b,s,width,pad); width= 0; }
#define prc(c)		{ width -= printchar(c, mode); }

int drive, partition, cylsiz, tracksiz;
int virgin = 1;			/* MUST be initialized to put it in data seg */
int floptrk = 9;		/* MUST be initialized to put it in data seg */
int *brk;			/* the ``break'' (end of data space) */

char *rwbuf;			/* one block buffer cache */
char rwbuf1[BLOCK_SIZE];	/* in case of a DMA-overrun under DOS .. */

extern long lseek();
extern end;			/* last variable */

unsigned part_offset;		/* sector offset for this partition */

union types {
  int *u_char;			/* %c */
  int *u_int;			/* %d */
  unsigned *u_unsigned;		/* %u */
  long *u_long;			/* %ld */
  char **u_charp;		/* %s */
};

/* Print the given character. */
putchar(c)
{
  if (c == '\n') putc('\r');
  putc(c);
}

/* Get a character from the user and echo it. */
getchar()
{
  register c;

  if ((c = getc() & 0xFF) == '\r') c = '\n';
  putchar(c);
  return(c);
}

/* Print the number n. */
printnum(n, base, sign, width, pad)
long n;
int base, sign;
int width, pad;
{
  register short i, mod;
  char a[MAXWIDTH];
  register char *p = a;

  if (sign)
	if (n < 0) {
		n = -n;
		width--;
	} else
		sign = 0;
  do {				/* mod = n % base; n /= base */
	mod = 0;
	for (i = 0; i < 32; i++) {
		mod <<= 1;
		if (n < 0) mod++;
		n <<= 1;
		if (mod >= base) {
			mod -= base;
			n++;
		}
	}
	*p++ = "0123456789ABCDEF"[mod];
	width--;
  } while (n);
  while (width-- > 0) putchar(pad);
  if (sign) *p++ = '-';
  while (p > a) putchar(*--p);
}

/* Print the character c. */
printchar(c, mode)
{
  if (mode == 0 || (isprint(c) && c != '\\')) {
	putchar(c);
	return(1);
  } else {
	putchar('\\');
	switch (c) {
	    case '\0':	putchar('0');	break;
	    case '\b':	putchar('b');	break;
	    case '\n':	putchar('n');	break;
	    case '\r':	putchar('r');	break;
	    case '\t':	putchar('t');	break;
	    case '\f':	putchar('f');	break;
	    case '\\':	putchar('\\');	break;
	    default:
		printnum((long) (c & 0xFF), 8, 0, 3, '0');
		return(4);
	}
	return(2);
  }
}

/* Print the arguments pointer to by `arg' according to format. */
doprnt(format, argp)
char *format;
union types argp;
{
  register char *fmt, *s;
  register short width, pad, mode;

  for (fmt = format; *fmt != 0; fmt++) switch (*fmt) {
	    case '\n':
		putchar('\r');
	    default:	putchar(*fmt);	break;
	    case '%':
		if (*++fmt == '-') fmt++;
		pad = *fmt == '0' ? '0' : ' ';
		width = 0;
		while (isdigit(*fmt)) {
			width *= 10;
			width += *fmt++ - '0';
		}
		if (*fmt == 'l' && islower(*++fmt)) *fmt = toupper(*fmt);
		mode = isupper(*fmt);
		switch (*fmt) {
		    case 'c':
		    case 'C':
			prc(nextarg(u_char));
			break;
		    case 'b':
			prn(u_unsigned, 2, 0);
			break;
		    case 'B':	prn(u_long, 2, 0);	break;
		    case 'o':
			prn(u_unsigned, 8, 0);
			break;
		    case 'O':	prn(u_long, 8, 0);	break;
		    case 'd':	prn(u_int, 10, 1);	break;
		    case 'D':	prn(u_long, 10, 1);	break;
		    case 'u':
			prn(u_unsigned, 10, 0);
			break;
		    case 'U':	prn(u_long, 10, 0);	break;
		    case 'x':
			prn(u_unsigned, 16, 0);
			break;
		    case 'X':	prn(u_long, 16, 0);	break;
		    case 's':
		    case 'S':
			s = nextarg(u_charp);
			while (*s) prc(*s++);
			break;
		    case '\0':
			break;
		    default:	putchar(*fmt);
		}
		while (width-- > 0) putchar(pad);
	}
}


/* Print the arguments according to fmt. */
void printf(fmt, args)
char *fmt;
{
  doprnt(fmt, &args);
}


/* Initialize the variables used by this program. */
initvars()
{
  brk = &end;
}



/* Copy n bytes. */
copy(p, q, n)
register char *p, *q;
register int n;
{
  do
	*q++ = *p++;
  while (--n);
}

/* Print a string with either a singular or a plural pronoun. */
pr(fmt, cnt, s, p)
char *fmt, *s, *p;
{
  printf(fmt, cnt, cnt == 1 ? s : p);
}


main(argc, argv)
char **argv;
{
  register char **clist = 0, **ilist = 0, **zlist = 0;

  register c, command;
  int proc_lim;

  if (virgin) floptrk = tracksiz;	/* save 9 or 15 in floptrk */
  virgin = 0;			/* only on first pass thru */
  if (tracksiz < 9 || cylsiz < 18) printf("Bootblok gave bad tracksiz\n");
  rwbuf = rwbuf1;
  printf("\n\n\n\n");
  for (;;) {
	printf("\nHit key as follows:\n\n");
	printf("    =  start MINIX, standard keyboard\n");
	printf("    u  start MINIX, U.S. extended keyboard\n");
	printf("    d  start MINIX, Dutch keyboard for PS/2\n\n");
	printf("    r  select root device (now %s)\n", rootname);
	printf("    i  select RAM image device (now %s)%s\n",
	       ramimname,
	       boot_parameters.bp_rootdev == DEV_RAM ?
	       "" : " (not used - root is not RAM disk)");
	printf("    s  set RAM disk size (now %u)%s\n",
	       boot_parameters.bp_ramsize,
	       boot_parameters.bp_rootdev == DEV_RAM ?
	       " (real size is from RAM image)" : "");
	proc_lim = boot_parameters.bp_processor;
	if (proc_lim > 0)
		printf("    p  set processor type to 88, 286, or 386 (now %u)\n\n", proc_lim);
	printf("\n# ");
	c = getc();
	command = c & 0xFF;
	printf("%c\n", command);
	part_offset = 0;
	partition = 0;
	drive = 0;

	switch (command) {

	    case '=':
	    case 'u':
	    case 'd':
		return(boot_parameters.bp_scancode = (c >> 8) & 0xFF);

	    case 'i':
		boot_parameters.bp_ramimagedev =
			get_device(&ramimname, "ram image", 0);
		printf("\n\n");
		continue;

	    case 'p':
		boot_parameters.bp_processor = get_processor();
		printf("\n\n");
		continue;

	    case 'r':
		boot_parameters.bp_rootdev =
			get_device(&rootname, "root", 1);
		printf("\n\n");
		continue;

	    case 's':
		boot_parameters.bp_ramsize = get_ramsize();
		printf("\n\n");
		continue;

	    default:
		printf("Illegal command\n");
		continue;
	}
  }

}

get_partition()
{
/* Ask for a partition number and wait for it. */
  char chr;
  while (1) {
	printf("\n\nPlease enter partition number. Drive 0: 1-4, drive 1: 6-9, then hit RETURN: ");
	while (1) {
		chr = getc();
		printf("%c", chr);
		if (chr == '\r') {
			printf("\n");
			if (partition > 0)
				return;
			else
				break;
		} else {
			if (partition > 0) break;
		}
		if (chr < '1' || chr > '9' || chr == '5') break;
		partition = chr - '0';
	}
	partition = 0;
  }
}

/* This define tells where to find things in partition table. */
#define P1 0x1C6
int read_partition()
{
  /* Read the partition table to find out where the requested partition
   * begins.  Put the sector offset in 'part_offset'. */
  int error, p, retries = 0;
  long val[4];
  long b0, b1, b2, b3;
  while (1) {
	retries++;
	if (retries > 5) {
		printf("Disk errors.  Can't read partition table\n");
		return(-1);
	}
	error = diskio(READING, 0, rwbuf, 1);
	if ((error & 0xFF00) == 0) break;
  }

  /* Find start of the requested partition and set 'part_offset'. */
  for (p = 0; p < 4; p++) {
	b0 = rwbuf[P1 + 16 * p + 0] & 0xFF;
	b1 = rwbuf[P1 + 16 * p + 1] & 0xFF;
	b2 = rwbuf[P1 + 16 * p + 2] & 0xFF;
	b3 = rwbuf[P1 + 16 * p + 3] & 0xFF;
	val[p] = (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
	if (val[p] > 65535) {
		printf("Fsck can't handle partitions above sector 65535\n");
		exit(1);
	}
  }
  p = (partition >= PARB ? partition - PARB + 1 : partition);
  sort(val);
  part_offset = (unsigned) val[p - 1];
  if ((part_offset % (BLOCK_SIZE / SECTOR_SIZE)) != 0)
	part_offset = (part_offset / (BLOCK_SIZE / SECTOR_SIZE) + 1) * (BLOCK_SIZE / SECTOR_SIZE);
  return(0);
}


int get_device(pname, description, ram_allowed)
char **pname;
char *description;
int ram_allowed;
{
  char chr;
  static char *devname[] = {
			  "/dev/fd0",
			  "/dev/fd1",
			  "/dev/hd1",
			  "/dev/hd2",
			  "/dev/hd3",
			  "/dev/hd4",
			  "",
			  "/dev/hd6",
			  "/dev/hd7",
			  "/dev/hd8",
			  "/dev/hd9",
  };
  printf("\nPlease enter (abbreviated) name of %s device.\n", description);
  printf("Floppy f0, f1, hard h1 to h4, h6 to h9");
  if (ram_allowed) printf(", RAM r");
  printf(".\nThen hit RETURN: ");
  while (1) {
	switch (chr = getc()) {
	    case 'f':
		putc(chr);
		while ((chr = getc()) < '0' || chr > '1');
		putc(chr);
		getnewline();
		*pname = devname[chr - '0'];
		return DEV_FD0 + chr - '0';
	    case 'h':
		putc(chr);
		while (((chr = getc()) < '1' || chr > '4') &&
		       (chr < '6' || chr > '9'));
		putc(chr);
		getnewline();
		*pname = devname[chr + 1 - '0'];
		return DEV_HD0 + chr - '0';
	    case 'r':
		if (ram_allowed) {
			putc(chr);
			getnewline();
			*pname = "/dev/ram";
			return DEV_RAM;
		}
	}
  }
}

getnewline()
{
  while ((char) getc() != '\r');
  putc('\n');
}

int get_processor()
{
  printf("\nPlease enter limit on processor type. Then hit RETURN: ");
  return get_size();
}

int get_ramsize()
{
  printf("\nPlease enter size of RAM disk. Then hit RETURN: ");
  return get_size();
}

int get_size()
{
  char chr;
  long size;

  while ((chr = getc()) < '0' || chr > '9');
  size = chr - '0';
  putc(chr);
  while ((chr = getc()) != '\r') {
	if (chr >= '0' && chr <= '9' && 10 * size + (chr - '0') < 0x10000) {
		putc(chr);
		size = 10 * size + (chr - '0');
	}
  }
  putc('\n');
  return size;
}


sort(val)
register long *val;
{
  register int i, j;

  for (i = 0; i < 4; i++) for (j = 0; j < 3; j++)
		if ((val[j] == 0) && (val[j + 1] != 0))
			swap(&val[j], &val[j + 1]);
		else if (val[j] > val[j + 1] && val[j + 1] != 0)
			swap(&val[j], &val[j + 1]);
}

swap(first, second)
register long *first, *second;
{
  register long tmp;

  tmp = *first;
  *first = *second;
  *second = tmp;
}
