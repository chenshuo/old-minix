/* mon2.c - Load an image and start it.		Author: Kees J. Bot */

#include "sys/types.h"
#include "sys/stat.h"
#include "stdlib.h"
#include "stddef.h"
#include "limits.h"
#include "string.h"
#include "errno.h"
#include "minix/config.h"
#include "minix/const.h"
#include "minix/partition.h"
#include "minix/boot.h"
#include "minix/syslib.h"
#include "minix/type.h"
#include "kernel/const.h"
#include "kernel/type.h"
#include "a.out.h"

#define _MONHEAD
#include "tools.h"

#ifndef _ANSI
#undef NULL
#define NULL 0
#endif

#define between(a, c, z)	((unsigned) ((c) - (a)) <= ((z) - (a)))
#define	printf	printk
#define click_shift	clck_shft	/* 7 char clash with click_size. */

/* 386 Kernels may require something extra: */
#define K_I386	0x0001	      /* Make the 386 transition before you call me */
#define K_CLAIM	0x0002	      /* I will acquire my own bss pages, thank you */
#define K_CHMEM 0x0004	      /* This kernel listens to chmem for stack size */
#define K_HDR	0x0010	      /* No need to patch sizes, kernel uses the hdrs*/

/* To force all unread sectors in: */
#define get_force()	get_sector((int) 0, (u32_t) 0)
#define align(i, n)	(((i) + ((n) - 1)) & ~((n) - 1))

char *minix_version = "1.6.25";
char *copyright = "Copyright 1993 Prentice-Hall, Inc.";

u16_t click_shift;
u16_t click_size;		/* click_size = Smallest kernel memory object*/
u16_t segclick;			/* Size of a click in paragraphs. */
u16_t segalign;			/* For align of a seg to max(sector, click) */
u16_t click2click;		/* To go from hardwr clicks to clicks and vv*/
u16_t k_flags;			/* Not all kernels are created equal. */
off_t image_off, image_size;
u16_t eqscancode = STANDARD_SCAN;	/* IBM scancode by default. */

/* Data about the different processes. */
#define PROCESS_MAX	16	/* Must match the space in kernel/startx.x */
#define KERNEL		0	/* The first process is the kernel. */
#define FS		2	/* The third must be fs. */

struct process {
  char name[IM_NAME_MAX + 1];	/* Nice to have a name for the thing. */
  u16_t cs;			/* Code segment. */
  u16_t ds;			/* Data segment. */
  u16_t bss;			/* Missing bss clicks. */
  u16_t stack;			/* Missing stack clicks. */
  u16_t dseg;			/* To access data segment. */
  u16_t doff;			/* dseg:doff = start of data. */
} process[PROCESS_MAX + 1];

/* Magic numbers in process' data space. */
#define MAGIC_OFF	0	/* Offset of magic # in data seg. */
#define CLICK_OFF	2	/* Offset in kernel text to click_shift. */
#define FLAGS_OFF	4	/* Offset in kernel text to flags. */
#define KERNEL_D_MAGIC	0x526F	/* In kernel data. */
#define OTHER_D_MAGIC	0xDADA	/* The other processes. */

/* Offsets of sizes to be patched into kernel and fs. */
#define P_SIZ_OFF	0	/* Process' sizes into kernel data. */
#define P_KDS_OFF	4	/* Kernel ds into kernel text. */
#define P_INIT_OFF	4	/* Init cs & sizes into fs data. */

_PROTOTYPE(void pretty_image, (char *image ));
_PROTOTYPE(int get_sector, (int seg, u32_t sec ));
_PROTOTYPE(int check_magic, (void));
_PROTOTYPE(void patch_sizes, (void));
_PROTOTYPE(int read_header, (struct image_header *hdr, u32_t sec ));
_PROTOTYPE(int selected, (char *name ));
_PROTOTYPE(u32_t proc_size, (struct image_header *hdrp ));
_PROTOTYPE(u32_t file_vir2sec, (u32_t vsec ));
_PROTOTYPE(u32_t flat_vir2sec, (u32_t vsec ));
_PROTOTYPE(int get_click, (u32_t sec, struct exec *hdrp ));
_PROTOTYPE(int read_code_segment, (u32_t *vsec, long *size, u16_t *seg ));
_PROTOTYPE(void exec_image, (char *image, char *params, size_t paramsize ));
_PROTOTYPE(char *params2params, (size_t *size ));
_PROTOTYPE(ino_t latest_version, (char *version, struct stat *stp ));
_PROTOTYPE(char *select_image, (char *image ));

_PROTOTYPE(u32_t (*vir2sec), (u32_t vsec )); /* Where is a sector on disk? */

void pretty_image(image)
char *image;
/* Pretty print the name of the image to load.  Translate '/' and '_' to
 * space, first letter goes uppercase.  An 'r' before a digit prints as
 * 'revision'.  E.g. 'minix/1.6.16r10' -> 'Minix 1.6.16 revision 10'.
 * The idea is that the part before the 'r' is the official Minix release
 * and after the 'r' you can put version numbers for your own changes.
 * The default version number 'minix_version' is printed if no digit was
 * seen at all.  Not everyone is a kernel hacker.
 */
{
  int up = 0, digit = 0, c;

  while ((c = *image++) != 0) {
	if (c == '/' || c == '_') {
		putchar(' ');
		continue;
	}
	if (c == 'r' && between('0', *image, '9')) {
		printf(" revision ");
		continue;
	}
	if (!up && between('a', c, 'z')) c = c - 'a' + 'A';

	if (between('A', c, 'Z')) up = 1;

	putchar(c);
	if (between('0', c, '9')) digit = 1;
  }
  if (!digit) printf(" %s", minix_version);
}

int get_sector(s, sec)
int s;
u32_t sec;
/* Read sector "sec" at segment "seg" (offset 0).  Seg must be at a 512-byte
 * boundary in memory!  It is awfully smart at reading many sectors at once.
 * It even knows that address == 0 means a hole in a file.
 * Use get_sector(0, 0) to read the last few sectors.  This is the first
 * function to feel the "can't read past a 64K boundary" limitation of the
 * DMA chip.  Return value is 1 for success, 0 if out of memory.
 */
{
  static u32_t address;		/* Sector to read. */
  static u16_t count;		/* How many to read. */
  static u16_t segment;		/* Where to put them. */
  static u16_t dma64k;		/* Can't read past this segment. */
  u16_t highseg = segment + count * HRATIO;
  int r;
  u16_t seg;

  seg = (u16_t) s;
  if (highseg == seg && address + count == sec && seg < dma64k)
	count++;
  else {
	if (segment != 0) {
		if (highseg > cseg) {
			/* Sector can't be read where this code sits. */
			errno = ENOMEM;
			return(0);
		}
		if (address == 0)	/* A hole, count must be 1. */
			raw_clear(0, segment, SECTOR_SIZE);
		else if ((r = readsectors(0, segment, address, count)) != 0) {
			readerr(address, r);
			errno = 0;
			return(0);
		}
	}
	address = sec;
	count = 1;
	segment = seg;
	dma64k = (seg + 0x0FFF) & 0xF000;
  }
  return(1);
}


int check_magic()
/* Check the magic numbers and clickshifts of the different processes. */
{
  struct process *procp;
  int ok = 1;

  for (procp = process; procp->ds != 0; procp++) {
	if ((procp == process + KERNEL ? KERNEL_D_MAGIC : OTHER_D_MAGIC)
			   != get_word(procp->doff + MAGIC_OFF, procp->dseg)) {
		printf("%s magic number is incorrect\n", procp->name);
		ok = 0;
	}
	if (get_word(procp->doff + CLICK_OFF, procp->dseg) != click_shift) {
		printf("%s click size doesn't agree with %s, ",
		       procp->name, process[KERNEL].name);
		printf("it may need to be recompiled\n");
		ok = 0;
	}
  }
  errno = 0;
  return(ok);
}

void patch_sizes()
/* Patch sizes of each process into kernel data space, kernel ds into kernel
 * text space, and sizes of init into data space of fs.  All the patched
 * numbers are based on the kernel click_shift, not hardware segments.
 */
{
  u16_t text_size, data_size;
  struct process *procp, *initp;
  u16_t doff;

  /* Patch kernel ds into the code segment. */
  put_word(P_KDS_OFF, process[KERNEL].cs, process[KERNEL].ds);

  /* Patch text and data sizes of the processes into kernel data space.
   * Kernels that want to claim their own bss pages and/or stack will
   * get that information too. 
   */
  doff = process[KERNEL].doff + P_SIZ_OFF;

  for (procp = process; procp->ds != 0; procp++) {
	text_size = (procp->ds - procp->cs) >> click2click;
	data_size = ((procp + 1)->cs - procp->ds) >> click2click;

	/* Standard two words, text and data size: */
	put_word(doff, process[KERNEL].dseg, text_size);
	doff += 2;
	put_word(doff, process[KERNEL].dseg, data_size);
	doff += 2;

	if (k_flags & K_CLAIM) {
		/* Size of missing bss: */
		put_word(doff, process[KERNEL].dseg, procp->bss);
		doff += 2;
	}
	if ((k_flags & (K_CLAIM | K_CHMEM)) == (K_CLAIM | K_CHMEM)) {
		/* Size of missing stack: */
		put_word(doff, process[KERNEL].dseg, procp->stack);
		doff += 2;
	}
	initp = procp;		/* The last process must be init. */
  }

  if (k_flags & K_CLAIM) return;	/* This kernel tells fs about init. */

  /* Patch cs and sizes of init into fs data. */
  put_word(process[FS].doff + P_INIT_OFF + 0, process[FS].dseg,
	 initp->cs >> click2click);
  put_word(process[FS].doff + P_INIT_OFF + 2, process[FS].dseg, text_size);
  put_word(process[FS].doff + P_INIT_OFF + 4, process[FS].dseg, data_size);
}

int read_header(hdr, sec)
struct image_header *hdr;
u32_t sec;
/* Read and check the a.out header at sector sec. */
{
  int r;
  char buf[SECTOR_SIZE];

  if (sec == 0) return(0);

  if ((r = readsectors((u16_t) buf, dseg, sec, 1)) != 0) {
	readerr(sec, r);
	errno = 0;
	return(0);
  }
  memcpy((void *) hdr, (void *) buf, sizeof(*hdr));

  if (BADMAG(hdr->process)) {
	errno = ENOEXEC;
	return(0);
  }
  return(1);
}

int selected(name)
char *name;
/* True iff name has no label or the proper label. */
{
  char *colon, *label;
  int cmp;

  if ((colon = strchr(name, ':')) == NULL) return(1);
  if ((label = b_value("label")) == NULL) return(1);
  *colon = 0;
  cmp = strcmp(label, name);
  *colon = ':';
  return(cmp == 0);
}

u32_t proc_size(hdrp)
struct image_header *hdrp;
/* Return the size of a process in sectors as found in an image. */
{
  u32_t len = hdrp->process.a_text;

  if (hdrp->process.a_flags & A_SEP) len = align(len, SECTOR_SIZE);
  len = align(len + hdrp->process.a_data, SECTOR_SIZE);
  len = align(len + hdrp->process.a_syms, SECTOR_SIZE);

  return(len >> SECTOR_SHIFT);
}

u32_t file_vir2sec(vsec)
u32_t vsec;
/* Translate a virtual sector number to an absolute disk sector. */
{
  off_t blk;

  if ((blk = r_vir2abs((off_t) (vsec / RATIO))) == 0) {
	errno = EIO;
	return(0);
  }
  return(lowsec + blk * RATIO + vsec % RATIO);
}

u32_t flat_vir2sec(vsec)
u32_t vsec;
/* Simply add an absolute sector offset to vsec. */
{
  if (vsec >= image_size) {
	errno = EIO;
	return(0);
  }
  return(lowsec + image_off + vsec);
}

int get_click(sec, hdrp)
u32_t sec;
struct exec *hdrp;
/* Get the click shift and special flags from the start of kernel text. */
{
  int r;
  char buf[SECTOR_SIZE];
  char *textp = buf;

  if (sec == 0) return(0);

  if ((r = readsectors((u16_t) buf, dseg, sec, 1)) != 0) {
	readerr(sec, r);
	errno = 0;
	return(0);
  }

  click_shift = *(u16_t *) (textp + CLICK_OFF);
  if (click_shift < HCLICK_SHIFT || click_shift > 16)
	click_shift = HCLICK_SHIFT;
  click_size = 1 << click_shift;
  click2click = click_shift - HCLICK_SHIFT;
  segclick = 1 << click2click;
  segalign = click_size > SECTOR_SIZE ? segclick : HRATIO;

  k_flags = *(u16_t *) (textp + FLAGS_OFF);

  return(1);
}

int read_code_segment(vsec, size, seg)
u32_t *vsec;			/* Virtual sector number to read. */
long *size;			/* Bytes to read. */
u16_t *seg;			/* Segment to put it in. */
{
  while (*size > 0) {
	if (!get_sector(*seg, (*vir2sec) (*vsec))) return(0);
	*vsec += 1;
	*seg += HRATIO;
	*size -= SECTOR_SIZE;
  }
  return(1);
}

void exec_image(image, params, paramsize)
char *image, *params;
size_t paramsize;
/* Get a named Minix image into core, patch it up and execute. */
{
  struct image_header hdr;
  u32_t vsec = 0;		/* Load this sector from image next. */
  u16_t seg = MINIXSEG;		/* Put it at this segment. */
  u16_t n, p_cs, p_ds;
  struct process *procp;	/* Process under construction. */
  int procn;			/* Counts them. */
  long a_text, a_data, a_bss, a_stack, a_syms;
  char *msec;
  int banner = 0;
  long processor = a2l(b_value("processor"));

  /* Clear the area where the headers will be placed. */
  raw_clear(0, HEADERSEG, PROCESS_MAX * A_MINHDR);

  /* Read the many different processes: */
  for (procn = 0, procp = process; vsec < image_size; procn++, procp++) {
	if (procn == PROCESS_MAX) {
		printf("There are more then %d programs in %s\n",
		       PROCESS_MAX, image);
		errno = 0;
		return;
	}

	/* Read header. */
	for (;;) {
		if (!read_header(&hdr, (*vir2sec) (vsec++))) return;

		/* Check the optional label on the process. */
		if (selected(hdr.name)) break;

		/* Bad label, skip this process. */
		vsec += proc_size(&hdr);
	}

	/* Place a copy of the header where the kernel can get it. */
	raw_copy(procn * A_MINHDR, HEADERSEG,
		 (u16_t) & hdr.process, dseg, A_MINHDR);

	/* Sanity check: an 8086 can't run a 386 kernel. */
	if (hdr.process.a_cpu == A_I80386 && processor < 386) {
		printf("You can't run a 386 kernel on this 80%ld\n",
		       processor);
		errno = 0;
		return;
	}

	/* Get the click shift from the first kernel sector. */
	if (procn == KERNEL) {
		if (!get_click((*vir2sec) (vsec), &hdr.process)) return;
	}
	if (!banner) {
		printf("   cs     ds    text    data     bss");
		if (k_flags & K_CHMEM) printf("   stack");
		putchar('\n');
		banner = 1;
	}

	/* Segment sizes. */
	a_text = hdr.process.a_text;
	a_data = hdr.process.a_data;
	a_bss = hdr.process.a_bss;
	if (k_flags & K_CHMEM) {
		a_stack = hdr.process.a_total - a_data - a_bss;
		if (!(hdr.process.a_flags & A_SEP)) a_stack -= a_text;
	} else
		a_stack = 0;
	a_syms = hdr.process.a_syms;

	/* Collect info about the process to be. */
	strcpy(procp->name, hdr.name);
	procp->cs = seg = align(seg, segalign);

	/* Read the text segment. */
	if (!read_code_segment(&vsec, &a_text, &seg)) return;

	/* Note that some of the data of a common I&D program has
	 * been read at this point to fill a sector.  This can be
	 * corrected by adding the now negative a_text to a_data.
	 */

	if (hdr.process.a_flags & A_SEP) {
		/* Align the data segment to a click. */
		seg = align(seg, segclick);
		procp->ds = seg;
		procp->dseg = seg;
		procp->doff = 0;
	} else {
		/* Compute precise start of data and correct a_data. */
		procp->ds = procp->cs;
		procp->dseg = seg - HRATIO;
		procp->doff = SECTOR_SIZE + a_text;
		a_data += a_text;
	}

	printf("%04x0  %04x0%8ld%8ld%8ld", procp->cs, procp->ds,
	            hdr.process.a_text, hdr.process.a_data, hdr.process.a_bss);
	if (k_flags & K_CHMEM) printf("%8ld", a_stack);
	printf("  %s\n", hdr.name);

	/* Read the data segment. */
	if (!read_code_segment(&vsec, &a_data, &seg)) return;

	/* Force the last sectors into memory. */
	if (!get_force()) return;

	/* Zero extend data to a click. */
	n = align(seg, segclick) - seg;
	if (seg + n > cseg) {
		errno = ENOMEM;
		return;
	}
	raw_clear(0, seg, n << HCLICK_SHIFT);
	a_data -= n << HCLICK_SHIFT;
	seg += n;

	/* Compute the number of bss clicks left. */
	a_bss += a_data;
	procp->bss = (a_bss + click_size - 1) >> click_shift;
	a_bss -= (long) procp->bss << click_shift;

	/* And the number of stack clicks. */
	a_stack += a_bss;
	procp->stack = (a_stack + click_size - 1) >> click_shift;

	/* Make space for bss and stack unless... */
	if (procn == KERNEL || !(k_flags & K_CLAIM)) {
		/* Zero out bss a click at a time. */
		while (procp->bss > 0) {
			if (seg + segclick > cseg) {
				errno = ENOMEM;
				return;
			}
			raw_clear(0, seg, click_size);
			seg += segclick;
			procp->bss--;
		}

		/* Add space for the stack. */
		seg += procp->stack << click2click;
		procp->stack = 0;
	}

	/* Skip symbol table. */
	vsec += (a_syms + SECTOR_SIZE - 1) >> SECTOR_SHIFT;
  }
  if (procn == 0) {
	printf("There are no programs in %s\n", image);
	errno = 0;
	return;
  }
  procp->cs = seg;		/* Record first free segment. */
  procp->ds = 0;		/* Mark end. */

  /* Check magic numbers and clickshifts in each process. */
  if (!check_magic()) return;

  /* Patch sizes, etc. into kernel data unless it uses the headers. */
  if (!(k_flags & K_HDR)) patch_sizes();

  /* Wait a while if delay is set, bail out if ESC typed. */
  msec = b_value("delay");
  if (!delay(msec != NULL ? msec : "0")) {
	errno = 0;
	return;
  }

  /* Minix. */
  p_cs = process[KERNEL].cs;
  p_ds = process[KERNEL].ds;

  if (k_flags & K_I386)
	minix386(p_cs, p_ds, params, paramsize);
  else
	minix86(p_cs, p_ds, params, paramsize);
}

char *params2params(size)
size_t *size;
/* Package the environment settings for the kernel. */
{
  char *parms;
  size_t i, z;
  environment *e;
  int sc;

  parms = (char *) malloc((z = 64) * sizeof(char *));
  i = 0;

  for (e = env; e != NULL; e = e->next) {
	char *name = e->name, *value = e->value;
	size_t n;
	dev_t dev;

	if (!(e->flags & E_VAR)) continue;

	if (strcmp(name, "keyboard") == 0) {
		name = "scancode";

		if (!numeric(value)) {
			sc = eqscancode;	/* default */
			if (strcmp(value, "olivetti") == 0) sc = OLIVETTI_SCAN;
			if (strcmp(value, "dutch") == 0) sc = DUTCH_EXT_SCAN;
			if (strcmp(value, "us") == 0) sc = US_EXT_SCAN;
			value = u2a(sc);
		}
	} else if (e->flags & E_DEV) {
		if ((dev = name2dev(value)) == -1) {
			free(parms);
			errno = 0;
			return(NULL);
		}
		value = u2a((u16_t) dev);
	}
	n = i + strlen(name) + 1 + strlen(value) + 1;
	if (n > z) {
		parms = (char *) realloc((void *) parms,
					 (z += n) * sizeof(char));
	}
	strcpy(parms + i, name);
	strcat(parms + i, "=");
	strcat(parms + i, value);
	i = n;
  }
  parms[i++] = 0;		/* end marked with empty string. */
  *size = i;
  return(parms);
}

ino_t latest_version(version, stp)
char *version;
struct stat *stp;
/* Recursively read the current directory, selecting the newest image on
 * the way up.  (One can't use r_stat while reading a directory.)
 */
{
  char name[NAME_MAX + 1];
  ino_t ino, newest;
  time_t mtime;

  if ((ino = r_readdir(name)) == 0) {
	stp->st_mtime = 0;
	return(0);
  }
  newest = latest_version(version, stp);
  mtime = stp->st_mtime;
  r_stat(ino, stp);

  if (S_ISREG(stp->st_mode) && stp->st_mtime > mtime) {
	newest = ino;
	strcpy(version, name);
  } else
	stp->st_mtime = mtime;
  return(newest);
}

char *select_image(image)
char *image;
/* Look image up on the filesystem, if it is a file then we're done, but
 * if its a directory then we want the newest file in that directory.  If
 * it doesn't exist at all, then see if it is 'number:number' and get the
 * image from that absolute offset off the disk.
 */
{
  ino_t image_ino;
  struct stat st;

  image = strcpy((char *) malloc((strlen(image) + 1 + NAME_MAX + 1)
			       * sizeof(char)), image);

  if (!fsok || (image_ino = r_lookup(ROOT_INO, image)) == 0) {
	char *size;

	if (numprefix(image, &size) && *size++ == ':'
	    && numeric(size)) {
		vir2sec = flat_vir2sec;
		image_off = a2l(image);
		image_size = a2l(size);
		strcpy(image, "Minix");
		return(image);
	}
	if (!fsok)
		printf("No image selected\n");
	else
		printf("Can't load %s: %s\n", image, unix_err(errno));
	goto bail_out;
  }
  r_stat(image_ino, &st);
  if (!S_ISREG(st.st_mode)) {
	char *version = image + strlen(image);
	char dots[NAME_MAX + 1];

	if (!S_ISDIR(st.st_mode)) {
		printf("%s: %s\n", *image, unix_err(ENOTDIR));
		goto bail_out;
	}
	(void) r_readdir(dots);
	(void) r_readdir(dots);	/* "." & ".." */
	*version++ = '/';
	*version = 0;
	if ((image_ino = latest_version(version, &st)) == 0) {
		printf("There are no images in %s\n", image);
		goto bail_out;
	}
	r_stat(image_ino, &st);
  }
  vir2sec = file_vir2sec;
  image_size = (st.st_size + SECTOR_SIZE - 1) >> SECTOR_SHIFT;
  return(image);

bail_out:
  free(image);
  return(NULL);
}

void minix()
/* Load Minix and run it.  (Given the size of this program it is surprising
 * that it ever gets to that.)
 */
{
  char *image;
  char *minixparams;
  size_t paramsize;
  char *chrome;
  int color;

  /* Translate the bootparameters to what Minix likes best. */
  if ((minixparams = params2params(&paramsize)) == NULL) return;

  if ((image = select_image(b_value("image"))) == NULL) return;

  /* Things are getting serious, kill the cache! */
  invalidate_cache();

  /* Reset and clear the screen setting the proper video mode.  This is more
   * important than it seems, Minix depends on the mode set right.
   */
  chrome = b_value("chrome");		/* display type: "mono" or "color" */
  color = strcmp(chrome, "color") == 0;	/* true if color */
  reset_video(color);			/* reset to the proper video mode */

  /* Display copyright message and load the image. */
  printf("Loading ");
  pretty_image(image);
  printf(".  %s\n\n", copyright);

  exec_image(image, minixparams, paramsize);
  /* Not supposed to return, if it does however, errno tells why. */

  switch (errno) {
     case ENOEXEC:
	printf("%s contains a bad program header\n", image);
	break;

     case ENOMEM:
	printf("%s does not fit in %d KB memory\n",
	       image, (cseg - MINIXSEG) / (1024 / HCLICK_SIZE));
	break;

     case EIO:
	printf("Unsuspected EOF on %s\n", image);

     case 0:		/* Error already reported. */ ;
}

  /* Put all that free memory to use again. */
  init_cache();
  free((void *) minixparams);
  free((void *) image);
}
