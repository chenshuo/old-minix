/* construct 1.7 - Make a device bootable	Author: Kees J. Bot */

/* Construct is a program that enables MINIX to be booted in various ways.
 * It is the successor to 'build'.  It deals with four components:
 *	- boot sector		# a 512-byte program written in assembler
 *	- param sector		# sector following boot sector, holding params
 *	- monitor		# a C program that offers menus at startup
 *	- image			# a file containing kernel(s), mm, fs, init
 *
 * Booting can occur from any of these kinds of devices:
 *	- floppy containing boot sector, param sector, monitor, image
 *	- floppy containing boot sector, param sector, file sys, monitor, image
 *	- HD partition: boot sector, param sector, files /monitor and /minix
 *
 * Construct can be called in several ways, most commonly:
 *     construct -i image kernel mm fs init		   # make image on file
 *     construct -b /dev/fdx bootblk monitor [label:]image # make boot floppy
 *     construct -d /dev/fdx bootblk monitor [label:]image # make demo floppy
 *     construct -h /dev/hdx bootblk monitor		   # install HD monitor
 *
 * The -i flag combines the pieces to make an image file (like the old 'build'
 * program did).  It is also possible to put >= 2 kernels on a boot disk, e.g.:
 *     construct -i image at:at_ker xt:xt_ker mm fs init   # 2 kernels on disk
 *     construct -b /dev/fd0 bootblk monitor at,xt:image
 *
 * The -b flag takes such an image file, a bootblk, and a monitor file, and 
 * puts them on a floppy disk, along with a parameter sector so the book 
 * sector can locate the monitor to start.  
 *
 * The -d flag is like -b, except that expects the device already contains a
 * valid file system.  The monitor will be placed after the file system instead
 * of in sector 2 (directly after the param sector), as -b does.
 *
 * The -h flag installs the boot block on a hard disk partition.  It also 
 * looks up the block addresses on the disk where the monitor is located, and
 * patches them into the boot block. This means that when /monitor is changed, 
 * construct -h must be run again.  However, changing /minix does not require
 * running construct -h.
 *
 * The most common combinations are thus (to build a boot disk on floppy):
 *	construct -i image kernel mm fs init		# build the image file
 *	construct -b /dev/fdx bootblk monitor image	# create boot floppy
 *
 * or, to build a demo disk on a floppy already containing a file system:
 *	construct -i image kernel mm fs init		# build the image file
 *	construct -d /dev/fdx bootblk monitor image	# create boot floppy
 *
 * or, to install a new /minix on a HD partition, compiled monitor and copy
 * it to /monitor on /dev/hdx, then type:
 *
 *	construct -i /minix kernel mm fs init		# install new binary
 *      construct -h /dev/hdx bootblk monitor          # install HD monitor
 *
 * The construct -h call need only be repeated when /monitor is changed.
 * Booting from the hard disk requires the monitor partition to be active.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <a.out.h>
#include <minix/config.h>
#include <minix/const.h>	/* Gives us BLOCK_SIZE. */
#include <stdio.h>
#include "tools.h"

#define BOOTBLOCK	0	/* Of course */
#define SIGNATURE	0xAA55	/* Boot block signature. */
#define BOOT_MAX	64	/* Absolute maximum size of secondary boot */
#define SIGPOS		510	/* Where to put signature word. */
#define PARTPOS		446	/* Offset to the partition table in a master
				 * boot block. */
enum howto {FS, BOOT, DEMO};

#define align(n)	(((n) + ((SECTOR_SIZE) - 1)) & ~((SECTOR_SIZE) - 1))
#define intel()(memcmp((void*)little_endian,(void*)&some_endian,(size_t)4)==0)

off_t total_text = 0;
off_t total_data = 0;
off_t total_bss = 0;

int making_image = 0;
long some_endian = 0x12345678;
char little_endian[] = {0x78, 0x56, 0x34, 0x12};
int load_syms;		/* Flag that tells if the symbol table must be loaded*/

_PROTOTYPE(int main, (int argc, char **argv ));
_PROTOTYPE(void report, (char *label ));
_PROTOTYPE(void fatal, (char *label ));
_PROTOTYPE(char *basename, (char *name ));
_PROTOTYPE(void hdr2hdr, (struct exec *hdr, struct exec *rawhdr ));
_PROTOTYPE(void read_header, (char *proc, FILE *procf, struct image_header *ih,
					struct exec *hdr ));
_PROTOTYPE(void padimage, (char *image, FILE *imagef, int n ));
_PROTOTYPE(void copyexec, (char *proc, FILE *procf, char *image, FILE *imagef,
					long n ));
_PROTOTYPE(void make_image, (char *image, char **procv ));
_PROTOTYPE(void readblock, (off_t blk, char *buf ));
_PROTOTYPE(void writeblock, (off_t blk, char *buf ));
_PROTOTYPE(void make_bootable, (enum howto how, char *device, char *bootblock,
			char *bootcode, off_t *bootoff, off_t *bootlen ));
_PROTOTYPE(void raw_install, (char *file, off_t start, off_t *len ));
_PROTOTYPE(void boot_disk, (enum howto how, char *device, char *bootblock, 
					char *bootcode, char **imagev ));
_PROTOTYPE(void install_master, (char *device, char *masterboot ));
_PROTOTYPE(void usage, (void));

#define between(a, c, z)	((unsigned) ((c) - (a)) <= ((z) - (a)))


int main(argc, argv)
int argc;
char **argv;
{
  int n;

  if (argc < 2) usage();
  if ((n = strlen(argv[1])) < 2) usage();

  if (argc >= 4 && n <= 6 && strncmp(argv[1], "-image", n) == 0)
	make_image(argv[2], argv + 3);
  else if (!intel())
	usage();
  else if (argc == 5 && n <= 5 && strncmp(argv[1], "-hard", n) == 0)
	make_bootable(FS, argv[2], argv[3], argv[4], NULL, NULL);
  else if (argc >= 6 && n <= 5 && strncmp(argv[1], "-boot", n) == 0)
	boot_disk(BOOT, argv[2], argv[3], argv[4], argv + 5);
  else if (argc >= 6 && n <= 5 && strncmp(argv[1], "-demo", n) == 0)
	boot_disk(DEMO, argv[2], argv[3], argv[4], argv + 5);
  else if (argc == 4 && n <= 7 && strncmp(argv[1], "-master", n) == 0)
	install_master(argv[2], argv[3]);
  else
	usage();
  exit(0);
}

void report(label)
char *label;
/* Label: No such file or directory */
{
  int e = errno;
  fprintf(stderr, "construct: ");
  fflush(stderr);
  errno = e;
  perror(label);
}

void fatal(label)
char *label;
{
  report(label);
  exit(1);
}

char *basename(name)
char *name;
/* Return the last component of name, stripping trailing slashes from name.
 * Precondition: name != "/".  If name is prefixed by a label, then the
 * label is copied to the basename too.
 */
{
  static char base[IM_NAME_MAX];
  char *p, *bp = base;

  if ((p = strchr(name, ':')) != NULL) {
	while (name <= p && bp < base + IM_NAME_MAX - 1) *bp++ = *name++;
  }
  for (;;) {
	if ((p = strrchr(name, '/')) == NULL) {
		p = name;
		break;
	}
	if (*++p != 0) break;
	*--p = 0;
  }
  while (*p != 0 && bp < base + IM_NAME_MAX - 1) *bp++ = *p++;
  *bp = 0;
  return(base);
}


void hdr2hdr(hdr, rawhdr)
struct exec *hdr, *rawhdr;
/* Transform an Intel little-endian header to a header for internal use. */
{
  int n = sizeof(*hdr) / sizeof(long) - 2;
  long *h = (long *) hdr + 2;
  unsigned char *r;

  /* Copy the character fields. */
  *hdr = *rawhdr;

  /* A_version is a short. */
  r = (unsigned char *) &rawhdr->a_version;
  hdr->a_version = (short) *r++ << 0;
  hdr->a_version |= (short) *r++ << 8;

  /* The rest are longs. */
  do {
	*h = (long) *r++ << 0;
	*h |= (long) *r++ << 8;
	*h |= (long) *r++ << 16;
	*h++ |= (long) *r++ << 24;
  } while (--n > 0);
}

void read_header(proc, procf, image_hdr, hdr)
char *proc;
FILE *procf;
struct image_header *image_hdr;
struct exec *hdr;
/* Read the a.out header of a program and check it.  If procf happens to be
 * NULL then the header is already in *image_hdr and need only be checked.
 */
{
  int n, big = 0;
  static int banner = 0;
  long a_text, a_bss;

  /* Put the basename of proc in the header. */
  strncpy(image_hdr->name, basename(proc), IM_NAME_MAX);
  image_hdr->name[IM_NAME_MAX] = 0;
  if (procf == NULL) {
	n = image_hdr->process.a_hdrlen;	/* header already present. */
  } else {
	/* Read the raw header. */
	n = fread((char *) &image_hdr->process, (int) sizeof(char),
		  A_MINHDR, procf);

	if (ferror(procf)) fatal(proc);
  }

  if (n < A_MINHDR || BADMAG(image_hdr->process)) {
	fprintf(stderr, "construct: %s is not an executable\n", proc);
	exit(1);
  }

  /* Get the rest of the exec header (usually nothing). */
  if (procf != NULL) {
	(void) fread((char *) &image_hdr->process + A_MINHDR,
		     (int) sizeof(char),
		     image_hdr->process.a_hdrlen - A_MINHDR, procf);

	if (ferror(procf)) fatal(proc);
  }

  /* No symbol table loaded by default. */
  if (!load_syms) image_hdr->process.a_syms = 0;

  /* Cook the header from Intel to internal format. */
  hdr2hdr(hdr, &image_hdr->process);

  if (!banner) {
	printf("\n    text    data     bss     size\n");
	banner = 1;
  }
  a_text = (making_image && hdr->a_flags & A_SEP) ? align(hdr->a_text)
	: hdr->a_text;
  a_bss = making_image ? align(hdr->a_bss) : hdr->a_bss;

  printf("%8ld%8ld%8ld%9ld  %s\n", a_text, hdr->a_data, a_bss,
         a_text + hdr->a_data + a_bss, proc);
  total_text += a_text;
  total_data += hdr->a_data;
  total_bss += a_bss;

  if (hdr->a_cpu == A_I8086) {
	long data = hdr->a_data + hdr->a_bss;

	if (!(hdr->a_flags & A_SEP)) data += hdr->a_text;
	if (hdr->a_text >= 65536) big |= 1;
	if (data >= 65536) big |= 2;
  }
  if (big) {
	fprintf(stderr,
		"%s will crash, %s%s%s segment%s larger then 64K\n",
		proc,
		big & 1 ? "text" : "",
		big == 2 ? " and " : "",
		big & 2 ? "data" : "",
		big == 2 ? "s are" : " is"
		);
  }
}

void padimage(image, imagef, n)
char *image;
FILE *imagef;
int n;
/* Add n zeros to image to pad it to a sector boundary. */
{
  while (n > 0) {
	if (putc(0, imagef) == EOF) fatal(image);
	n--;
  }
}

void copyexec(proc, procf, image, imagef, n)
char *proc;
FILE *procf;
char *image;
FILE *imagef;
long n;
/* Copy n bytes from proc to image padded to fill a sector. */
{
  int pad, c;

  /* Compute number of padding bytes. */
  pad = align(n) - n;

  while (n > 0) {
	if ((c = getc(procf)) == EOF) {
		if (ferror(procf)) fatal(proc);
		fprintf(stderr, "construct: premature EOF on %s\n",
			proc);
		exit(1);
	}
	if (putc(c, imagef) == EOF) fatal(image);
	n--;
  }
  padimage(image, imagef, pad);
}

void make_image(image, procv)
char *image, **procv;
/* Collect a set of files in an image, each "segment" is nicely padded out
 * to SECTOR_SIZE, so it may be read from disk into memory without trickery.
 */
{
  FILE *imagef, *procf;
  char *proc, *file;
  int procn;
  struct image_header proc_hdr;
  struct exec hdr;
  struct stat st;
  int k_flags;

  making_image = 1;
  if ((imagef = fopen(image, "w")) == NULL) fatal(image);
  for (procn = 0; (proc = *procv++) != NULL; procn++) {
	/* Check for the nostrip flag. */
	if (proc[0] == '-' && proc[1] == 's') {
		if (*(proc += 2) == 0 && (proc = *procv++) == NULL) break;
		load_syms = 1;
	} else {
		load_syms = 0;
	}

	/* Remove the label from the file name. */
	if ((file = strchr(proc, ':')) != NULL)
		file++;
	else
		file = proc;

	/* Real files please, may need to seek. */
	if (stat(file, &st) < 0 || (errno = EISDIR, !S_ISREG(st.st_mode))
				    || (procf = fopen(file, "r")) == NULL)
		fatal(proc);

	/* Read a.out header. */
	read_header(proc, procf, &proc_hdr, &hdr);

	/* Write header padded to fill a sector */
	(void) fwrite((char *) &proc_hdr, (int) sizeof(proc_hdr), 1, imagef);
	if (ferror(imagef)) fatal(image);
	padimage(image, imagef, (int) (SECTOR_SIZE - sizeof(proc_hdr)));

	/* Copy text and data of proc to image. */
	if (hdr.a_flags & A_SEP) {
		/* Separate I&D: pad text & data separately. */

		copyexec(proc, procf, image, imagef, hdr.a_text);
		copyexec(proc, procf, image, imagef, hdr.a_data);
	} else {
		/* Common I&D: keep text and data together. */

		copyexec(proc, procf, image, imagef,
			 hdr.a_text + hdr.a_data);
	}

	/* Load the symbol table. */
	copyexec(proc, procf, image, imagef, hdr.a_syms);

	/* Done with proc. */
	(void) fclose(procf);
  }

  /* Done with image. */
  if (fclose(imagef) == EOF) fatal(image);
  printf("  ------  ------  ------  -------\n");
  printf("%8ld%8ld%8ld%9ld  total\n\n\n", total_text, total_data, total_bss,
			         total_text + total_data + total_bss, proc);
}

int rawfd;			/* File descriptor to open device. */
char *rawdev;			/* Name of device. */

void readblock(blk, buf)
off_t blk;
char *buf;
/* For rawfs, so that it can read blocks. */
{
  int n;

  if (lseek(rawfd, blk * BLOCK_SIZE, SEEK_SET) < 0
				|| (n = read(rawfd, buf, BLOCK_SIZE)) < 0)
	fatal(rawdev);

  if (n < BLOCK_SIZE) {
	fprintf(stderr, "construct: unexpected EOF on %s\n", rawdev);
	exit(1);
  }
}

void writeblock(blk, buf)
off_t blk;
char *buf;
/* Add a function to write blocks for local use. */
{
  if (lseek(rawfd, blk * BLOCK_SIZE, SEEK_SET) < 0
				      || write(rawfd, buf, BLOCK_SIZE) < 0)
	fatal(rawdev);
}


void make_bootable(how, device, bootblock, bootcode, bootoff, bootlen)
enum howto how;
char *device, *bootblock, *bootcode;
off_t *bootoff, *bootlen;
/* Install bootblock on the bootsector of device with the disk addresses to
 * bootcode patched into the data segment of bootblock.  "How" tells if the
 * boot code is present inside a file system, simply behind the boot block,
 * or behind the file system for a demo disk.
 */
{
  char buf[BLOCK_SIZE], *adrp;
  struct fileaddr {
	off_t address;
	int count;
  } bootaddr[BOOT_MAX + 1], *bap = bootaddr;
  struct exec boothdr;
  struct image_header dummy;
  struct stat st;
  ino_t ino;
  off_t sector, max_sector;
  FILE *bootf;
  off_t addr, fssize;

  /* Open device and set variables for readblock. */
  if ((rawfd = open(rawdev = device, O_RDWR)) < 0) fatal(device);

  /* Read and check the superblock. */
  fssize = r_super();

  switch (how) {
      case FS:
      case DEMO:
	if (fssize == 0) {
		fprintf(stderr,
		     "construct: %s is not a Minix file system\n", device);
		exit(1);
	}
	break;
      case BOOT:
	if (fssize != 0) {
		int s;
		printf("%s contains a file system!\n", device);
		printf("Scribbling in 10 seconds");
		for (s = 0; s < 10; s++) {
			fputc('.', stdout);
			fflush(stdout);
			sleep(1);
		}
		fputc('\n', stdout);
	}
	fssize = 1;		/* Just a boot block. */
  }

  if (how != FS) {
	/* For a raw installation, we need to copy the boot code onto
	 * the device, so we need to look at the file to be copied. 
	 */
	if (stat(bootcode, &st) < 0) fatal(bootcode);

	if ((bootf = fopen(bootcode, "r")) == NULL) fatal(bootcode);
  } else {
	/* For a normal installation, the boot code is already
	 * present on the device, so we find and check that. */
	if ((ino = r_lookup(ROOT_INO, bootcode)) == 0) fatal(bootcode);

	r_stat(ino, &st);

	/* Get the header from the first block. */
	if ((addr = r_vir2abs((off_t) 0)) == 0) {
		dummy.process.a_magic[0] = (unsigned char) (~A_MAGIC0 & BYTE);
	} else {
		readblock(addr, buf);
		memcpy((void*) &dummy.process, (char*)buf,sizeof(struct exec));
	}
	bootf = NULL;
  }

  /* See if it is an executable (read_header does the check). */
  read_header(bootcode, bootf, &dummy, &boothdr);
  if (bootf != NULL) fclose(bootf);

  /* Get all the sector addresses of the secondary boot code. */
  max_sector = (boothdr.a_hdrlen + boothdr.a_text
	      + boothdr.a_data + SECTOR_SIZE - 1) / SECTOR_SIZE;

  if (max_sector > BOOT_MAX * RATIO) {
	fprintf(stderr, "construct: %s is way too big\n", bootcode);
	exit(0);
  }
  if (how != FS) {
	/* Pass offset and lenght of boot code to caller. */
	*bootoff = fssize * RATIO;
	*bootlen = max_sector;
  }
  bap->count = 0;		/* Trick to get the address recording going. */

  for (sector = 0; sector < max_sector; sector++) {
	if (how != FS)
		addr = fssize + (sector / RATIO);
	else if ((addr = r_vir2abs(sector / RATIO)) == 0) {
		fprintf(stderr, "construct: %s has holes!\n");
		exit(1);
	}
	addr = (addr * RATIO) + (sector % RATIO);

	/* First address of the addresses array? */
	if (bap->count == 0) bap->address = addr;

	/* Paste sectors together in a multisector read. */
	if (bap->address + bap->count == addr)
		bap->count++;
	else {
		/* New address. */
		bap++;
		bap->address = addr;
		bap->count = 1;
	}
  }
  (++bap)->count = 0;		/* No more. */

  /* Get the boot block and patch the pieces in. */
  readblock((off_t) BOOTBLOCK, buf);
  if ((bootf = fopen(bootblock, "r")) == NULL) fatal(bootblock);
  read_header(bootblock, bootf, &dummy, &boothdr);

  if (boothdr.a_text + boothdr.a_data + 4 * (bap - bootaddr) + 1 > SIGPOS) {
	fprintf(stderr,
		"construct: %s + addresses to %s don't fit in boot sector\n",
		bootblock, bootcode);
	fprintf(stderr,
	   "You can try copying/reinstalling %s to defragment it\n", bootcode);
	exit(1);
  }

  /* All checks out right.  Read bootblock into the boot block! */
  (void) fread(buf, 1, (int) (boothdr.a_text + boothdr.a_data), bootf);
  if (ferror(bootf)) fatal(bootblock);
  (void) fclose(bootf);

  /* Patch the addresses in. */
  adrp = buf + (int) (boothdr.a_text + boothdr.a_data);
  for (bap = bootaddr; bap->count != 0; bap++) {
	*adrp++ = bap->count;
	*adrp++ = (bap->address >> 0) & 0xFF;
	*adrp++ = (bap->address >> 8) & 0xFF;
	*adrp++ = (bap->address >> 16) & 0xFF;
  }

  /* Zero count stops bootblock's reading loop. */
  *adrp++ = 0;

/* In case somebody is some day interested.
  printf("%d address%s (%d bytes) patched into the boot block\n",
         bap - bootaddr, bap != bootaddr + 1 ? "es" : "",
         4 * (bap - bootaddr) + 1);
*/

  /* Boot block signature. */
  adrp = buf + SIGPOS;
  *adrp++ = (SIGNATURE >> 0) & 0xFF;
  *adrp++ = (SIGNATURE >> 8) & 0xFF;

  /* Sector 2 of the boot block is used for boot parameters, initially
   * filled with null commands (newlines).  Initialize it only if there
   * is a control character there other than newline.
   */
  for (adrp = buf + SECTOR_SIZE; adrp < buf + 2 * SECTOR_SIZE; adrp++) {
	if (*adrp != '\n' && (unsigned) *adrp < ' ') {
		/* Assume param sector is junk, initialize it. */
		memset((void *) (buf + SECTOR_SIZE), '\n',
		       (size_t) SECTOR_SIZE);
		break;
	}
  }

  /* Install boot block. */
  writeblock((off_t) BOOTBLOCK, buf);
}

void raw_install(file, start, len)
char *file;
off_t start, *len;
/* Copy bootcode or an image to the boot device at the given absolute disk
 * block number.  This "raw" installation is used to place bootcode and
 * image on a disk without a filesystem to make a simple boot disk.  Useful
 * in automated scripts for J. Random User.
 * Note: *len == 0 when an image is read.  It is set right afterwards.
 */
{
  static char buf[BLOCK_SIZE];	/* Nonvolatile block buffer. */
  FILE *f;
  off_t sec = start;
  static int banner = 0;

  if ((f = fopen(file, "r")) == NULL) fatal(file);

  /* Copy sectors from file onto the boot device. */
  do {
	int off = sec % RATIO;

	if (fread(buf + off * SECTOR_SIZE, 1, SECTOR_SIZE, f) == 0) break;
	if (off == RATIO - 1) writeblock(sec / RATIO, buf);
  } while (++sec != start + *len);

  if (ferror(f)) fatal(file);
  (void) fclose(f);

  /* Write a partial block, this may be the last image. */
  if (sec % RATIO != 0) writeblock(sec / RATIO, buf);

  if (!banner) {
	printf("\n                    start sectors\n");
	banner = 1;
  }
  *len = sec - start;
  printf("%25ld%8ld  %s\n", start, *len, file);
}

void boot_disk(how, device, bootblock, bootcode, imagev)
enum howto how;
char *device, *bootblock, *bootcode, **imagev;
/* Make a boot or demo disk placing code in the params sector to select
 * among several kernels, each containing a different winchester driver.
 */
{
  off_t pos;			/* Sector to write next file. */
  off_t len;			/* Lenght of file in sectors. */
  char buf[BLOCK_SIZE + 256];	/* Boot and params sector. */
  char *parmp = buf + SECTOR_SIZE;
  char *labels, *label, *image;
  int nolabel = 0, choice;

  /* Fill the boot block with code & addresses.  Ask for position and
   * length of boot code. 
   */
  make_bootable(how, device, bootblock, bootcode, &pos, &len);
  readblock((off_t) BOOTBLOCK, buf);

  /* Place the boot code onto the boot device. */
  raw_install(bootcode, pos, &len);

  if (how == BOOT) {
	/* A boot only disk needs a floppy swap. */
	strcpy(parmp, "delay=swap\n");
	parmp += strlen(parmp);
  }
  while ((labels = *imagev++) != NULL) {
	/* Place each kernel image on the boot device. */

	if ((image = strchr(labels, ':')) != NULL) {
		*image++ = 0;
	} else {
		if (nolabel) {
			fprintf(stderr,
			     "construct: Only one image can be the default\n");
			exit(1);
		}
		nolabel = 1;
		image = labels;
		labels = NULL;
	}
	pos += len;
	len = 0;
	raw_install(image, pos, &len);

	if (labels == NULL) {
		/* Let this image be the default. */
		sprintf(parmp, "image=%ld:%ld\n", pos, len);
		parmp += strlen(parmp);
	}
	while (labels != NULL) {
		/* Image is prefixed by a comma separated list of
		 * labels.  Define functions to select label and
		 * image. 
		 */
		label = labels;
		if ((labels = strchr(labels, ',')) != NULL) *labels++ = 0;

		choice = label[0];
		if (between('A', choice, 'Z')) choice = choice - 'A' + 'a';

		sprintf(parmp,
		"%s(%c){label=%s;image=%ld:%ld;echo %s kernel selected;menu}\n",
			label, choice, label, pos, len, label);
		parmp += strlen(parmp);
	}

	if (parmp > buf + BLOCK_SIZE) {
		fprintf(stderr, "construct: Out of param space\n");
		exit(1);
	}
  }
  memset((void *) parmp, '\n', buf + BLOCK_SIZE - parmp);
  writeblock((off_t) BOOTBLOCK, buf);

  /* Tell the total size of the data on the device, nice for dd(1). */
  pos += len;
  printf("\nNumber of disk blocks used = %ld\n", (pos + RATIO - 1) / RATIO);
}

void install_master(device, masterboot)
char *device, *masterboot;
/* Booting a hard disk is a two stage process:  The master bootstrap in sector
 * 0 loads the bootstrap from sector 0 of the active partition which in turn
 * starts the operating system.  This code installs such a master bootstrap
 * on a hard disk.
 */
{
  FILE *masf;
  unsigned long size;
  struct stat st;
  char buf[BLOCK_SIZE];

  /* Open device. */
  if ((rawfd = open(rawdev = device, O_RDWR)) < 0) fatal(device);

  /* Open the master boot code. */
  if ((masf = fopen(masterboot, "r")) == NULL) fatal(masterboot);

  /* See if the user is cloning a device. */
  if (fstat(fileno(masf), &st) >= 0 && S_ISBLK(st.st_mode))
	size = PARTPOS;
  else {
	/* Read and check header otherwise. */
	struct image_header dummy;
	struct exec hdr;

	read_header(masterboot, masf, &dummy, &hdr);
	size = hdr.a_text + hdr.a_data;
  }
  if (size > PARTPOS) {
	fprintf(stderr, "construct: %s is too big\n", masterboot);
	exit(1);
  }

  /* Read the master boot block, patch it, write. */
  readblock((off_t) BOOTBLOCK, buf);

  (void) fread(buf, 1, (int) size, masf);
  if (ferror(masf)) fatal(masterboot);

  /* Install signature. */
  buf[SIGPOS + 0] = (SIGNATURE >> 0) & 0xFF;
  buf[SIGPOS + 1] = (SIGNATURE >> 8) & 0xFF;

  writeblock((off_t) BOOTBLOCK, buf);
}

void usage()
{
  fprintf(stderr, "Usage:\n\tconstruct -i image kernel mm fs init		(To make image file)\n");
  if (!intel()) {
	fprintf(stderr,	"\nThese don't work on this non-Intel machine:\n");
  }
  fprintf(stderr, "\tconstruct -b device bootblk monitor image	(To make a boot disk)\n");
  fprintf(stderr, "\tconstruct -d device bootblk monitor image	(To make a demo disk)\n");
  fprintf(stderr, "\tconstruct -h device bootblk monitor		(To install HD monitor)\n");
  exit(1);
}
