/* This program takes the previously compiled and linked pieces of the
 * operating system, and puts them together to build a boot diskette.
 * The files are read and put on the boot diskette in this order:
 *
 *      bootblok:       the diskette boot program
 *      kernel:         the operating system kernel
 *      mm:             the memory manager
 *      fs:             the file system
 *      init:           the system initializer
 *      menu:           the file system checker
 *
 * The bootblok file goes in sector 0 of the boot diskette.  The operating system
 * begins directly after it.  The kernel, mm, fs, init, and menu are each
 * padded out to a multiple of clicksize bytes, and then concatenated into a
 * single file beginning 512 bytes into the file.  The first byte of sector 1
 * contains executable code for the kernel.  There is no header present.
 * Clicksize is the CLICK_SIZE which the kernel, mm and fs files were
 * compiled with, and is encoded in bytes 2-3 of the kernel code segment.
 *
 * After the boot image has been built, build goes back and makes several
 * patches to the image file or diskette:
 *
 *      1. The last 4 words of the boot block are set as follows:
 *	   Word at 504:	Number of sectors to load
 *	   Word at 506:	DS value for running menu
 *	   Word at 508:	PC value for starting menu
 *	   Word at 510:	CS value for running menu
 *
 *	2. Build writes a table into the first 8 words of the kernel's
 *	   data space.  It has 4 entries, the cs and ds values for each
 *	   program.  The kernel needs this information to run mm, fs, and
 *	   init.  Build also writes the kernel's DS value into address 4
 *	   of the kernel's TEXT segment, so the kernel can set itself up.
 *
 *      3. The origin and size of the init program are patched into bytes 4-9
 *         of the file system data space. The file system needs this
 *         information, and expects to find it here.
 *
 * Build is called by:
 *
 *      build bootblok kernel mm fs init menu image
 *
 * to get the resulting image onto the file "image".
 */

/* Modified by Bruce Evans, 21 Nov 88 to load symbol tables for debugger.
   For each piece of the kernel, the symbol table is loaded at the end of
   the bss. A pointer to it is placed in the spare word at location 2 in
   the code segment. The sizes array is adjusted so that the symbol table
   is effectively part of the data segment.
   It would be better for everything to load the exec headers and chain
   them together.

   BDE 8 Feb 89. CLICK_SIZE 256 instead of 16.
   BDE 2 Mar 89. CLICK_SIZE and CLICK_SHIFT variables clicksize, click_shift.
 */


#define PROGRAMS 5              /* kernel + mm + fs + init + menu = 5 */
#define PROG_ORG 1536           /* where does kernel begin in abs mem */
#define DS_OFFSET 4L            /* position of DS written in kernel text seg */
#define SYM_OFFSET 2L		/* position of syms writ in kernel text seg */
#define SECTOR_SIZE 512         /* size of buf */
#define READ_UNIT 512           /* how big a chunk to read in */
#define KERNEL_D_MAGIC 0x526F   /* identifies kernel data space */
#define FS_D_MAGIC 0xDADA	/* identifies fs data space */
#define DATA_ALIGNMENT 16	/* minimum alignment of separate I&D data */
#define HCLICK_SHIFT 4
#define KERN 0
#define MM   1
#define FS   2
#define INIT 3
#define FSCK 4

/* Information about the file header. */
#define HEADER1 32              /* short form header size */
#define HEADER2 48              /* long form header size */
#define SEP_POS 1               /* tells where sep I & D bit is */
#define HDR_LEN 2               /* tells where header length is */
#define TEXT_POS 0              /* where is text size in header */
#define DATA_POS 1              /* where is data size in header */
#define BSS_POS 2               /* where is bss size in header */
#define SYM_POS 5               /* where is sym size in header */
#define SEP_ID_BIT 0x20         /* bit that tells if file is separate I & D */

#ifdef MSDOS
# define BREAD 4                /* value 0 means ASCII read */
#else
# define BREAD 0
#endif

int image;                      /* file descriptor used for output file */
int cur_sector;                 /* which 512-byte sector to be written next */
int buf_bytes;                  /* # bytes in buf at present */
char buf[SECTOR_SIZE];          /* buffer for output file */
char zero[SECTOR_SIZE];         /* zeros, for writing bss segment */

long cum_size;                  /* Size of kernel+mm+fs+init */
long all_size;                  /* Size of all 5 programs */

unsigned click_shift;		/* CLICK_SHIFT used to compile kernel/mm/fs */
unsigned clicksize;		/* CLICK_SIZE used to compile kernel/mm/fs */
				/* grrr, click_size would be ambiguous */

struct sizes {
  unsigned text_size;           /* size in bytes */
  unsigned data_size;           /* size in bytes */
  unsigned bss_size;            /* size in bytes */
  unsigned sym_size;            /* size in bytes */
  int sep_id;                   /* 1 if separate, 0 if not */
} sizes[PROGRAMS];

char *name[] = {"\nkernel", "mm    ", "fs    ", "init  ", "menu  "};

main(argc, argv)
int argc;
char *argv[];
{
/* Copy the boot block and the 5 programs to the output. */

  int i;

  if (argc != PROGRAMS+3) pexit("seven file names expected. ", "");

  IOinit();			/* check for DMAoverrun (DOS) */
  create_image(argv[7]);              /* create the output file */

  /* Go get the boot block and copy it to the output file or diskette. */
  copy1(argv[1]);

  /* Copy the 5 programs to the output file or diskette. */
  for (i = 0; i < PROGRAMS; i++) copy2(i, argv[i+2]);
  flush();
  printf("                                               -----      -----\n");
  printf("Operating system size  %29ld      %5lx\n", cum_size, cum_size);
  printf("\nTotal size including menu is %ld.\n", all_size);

  /* Make the three patches to the output file or diskette. */
  patch1(all_size);
  patch2();
  patch3();
  exit(0);
}



copy1(file_name)
char *file_name;
{
/* Copy the specified file to the output.  The file has no header.  All the
 * bytes are copied, until end-of-file is hit.
 */

  int fd, bytes_read;
  char inbuf[READ_UNIT];

  if ( (fd = open(file_name, BREAD)) < 0) pexit("can't open ",file_name);

  do {
        bytes_read = read(fd, inbuf, READ_UNIT);
        if (bytes_read < 0) pexit("read error on file ", file_name);
        if (bytes_read > 0) wr_out(inbuf, bytes_read);
  } while (bytes_read > 0);
  flush();
  close(fd);
}


copy2(num, file_name)
int num;                        /* which program is this (0 - 4) */
char *file_name;                /* file to open */
{
/* Open and read a file, copying it to output.  First read the header,
 * to get the text, data, bss and symbol sizes.  Also see if it is separate
 * I & D.  Write the text, data, bss and symbols to output.  The sum of these
 * four pieces must be padded upwards to a multiple of clicksize, if need
 * be.  The individual pieces need not be multiples of clicksize bytes,
 * except for the text size when separate I & D is in use.
 */

  int fd, sepid, bytes_read, count;
  unsigned text_bytes, data_bytes, bss_bytes, rest, filler;
  unsigned file_text_bytes, sym_bytes;
  long tot_bytes;
  
  if ( (fd = open(file_name, BREAD)) < 0) pexit("can't open ", file_name);

  /* Read the header to see how big the segments are. */
  read_header(fd, &sepid, &text_bytes, &data_bytes, &bss_bytes, &sym_bytes,
              file_name);

  /* If the kernel, determine click_shift and clicksize. */
  if (num == 0) {
	long lseek();
	long offset;
	unsigned char click_buf[4];

	offset = sizeof click_buf;
	if (read(fd, click_buf, sizeof click_buf) != sizeof click_buf)
		pexit("can't read click_shift in ", file_name);
	if (lseek(fd,  -offset, 1) < 0)
		pexit("can't seek before click_shift in ", file_name);
	click_shift = click_buf[2] + (click_buf[3] << 8);
	if (click_shift == 0)
		click_shift = HCLICK_SHIFT;	/* old kernel */
	else if (click_shift < HCLICK_SHIFT)
		pexit("kernel click_shift must be >= 4", "");
	clicksize = 1 << click_shift;
  }

  /* Pad the total size to a clicksize-byte multiple, if needed. */
  if (sepid && ((text_bytes % DATA_ALIGNMENT) != 0) ) {
        pexit("separate I & D but text size not multiple of 16 bytes.  File: ", 
                                                                file_name);
  }
  file_text_bytes = text_bytes;
  if (sepid)
	text_bytes = (text_bytes + clicksize - 1) & ~(clicksize - 1);
  tot_bytes = (long) text_bytes + (data_bytes + bss_bytes) + sym_bytes;
  rest = tot_bytes % clicksize;
  filler = (rest > 0 ? clicksize - rest : 0);
  bss_bytes += filler;
  tot_bytes += filler;
  if (num < FSCK) cum_size += tot_bytes;
  all_size += tot_bytes;

  /* Record the size information in the table. */
  sizes[num].text_size = text_bytes;
  sizes[num].data_size = data_bytes;
  sizes[num].bss_size  = bss_bytes;
  sizes[num].sym_size  = sym_bytes;
  sizes[num].sep_id    = sepid;

  /* Print a message giving the program name and size, except for menu. */
  if (num < FSCK) { 
        printf("%s  text=%5u  data=%5u  bss=%5u  tot=%5ld  hex=%5lx  %s\n",
                name[num], text_bytes, data_bytes, bss_bytes, tot_bytes,
                tot_bytes, (sizes[num].sep_id ? "Separate I & D" : ""));
  }


  /* Read in the text and data segments, and copy them to output. */
  copy3(fd, file_text_bytes, file_name);

  /* Oops, pad the text segment in the middle of this. */
  text_bytes -= file_text_bytes;	/* remainder now */
  while (text_bytes != 0) {
        count = (text_bytes < SECTOR_SIZE ? text_bytes : SECTOR_SIZE);
        wr_out(zero, count);
        text_bytes -= count;
  }
  copy3(fd, data_bytes, file_name);

  /* Write the bss to output. */
  while (bss_bytes > 0) {
        count = (bss_bytes < SECTOR_SIZE ? bss_bytes : SECTOR_SIZE);
        wr_out(zero, count);
        bss_bytes -= count;
  }

  /* Copy symbol table to output. */
  copy3(fd, sym_bytes, file_name);

  close(fd);
}


copy3(fd, left_to_read, file_name)
int fd;
unsigned left_to_read;
char *file_name;
{
  int bytes_read;
  int count;
  char inbuf[READ_UNIT];

  while (left_to_read != 0)
  {
    if ( (unsigned) (count = left_to_read) > READ_UNIT)
      count = READ_UNIT;
    if ( (bytes_read = read(fd, inbuf, count)) <= 0) {
      pexit("read error on file ", file_name);
    }
    wr_out(inbuf, bytes_read);
    left_to_read -= count;
  }
}


#ifdef XENIX_HEADER
# include </usr/include/sys/a.out.h>
#endif

read_header(fd, sepid, text_bytes, data_bytes, bss_bytes, sym_bytes,file_name)
int fd, *sepid;
unsigned *text_bytes, *data_bytes, *bss_bytes, *sym_bytes;
char *file_name;
{
/* Read the header and check the magic number.  The standard Monix header 
 * consists of 8 longs, as follows:
 *      0: 0x04100301L (combined I & D space) or 0x04200301L (separate I & D)
 *      1: 0x00000020L (stripped file) or 0x00000030L (unstripped file)
 *      2: size of text segments in bytes
 *      3: size of initialized data segment in bytes
 *      4: size of bss in bytes
 *      5: 0x00000000L
 *      6: total memory allocated to program (text, data and stack, combined)
 *      7: 0x00000000L
 * The longs are represented low-order byte first and high-order byte last.
 * The first byte of the header is always 0x01, followed by 0x03.
 * The header is followed directly by the text and data segments, whose sizes
 * are given in the header.
 */

#ifdef XENIX_HEADER
  struct  aexec a_header;
#else
  long head[12];
  unsigned short hd[4];
#endif
  int n, header_len;

#ifdef XENIX_HEADER
  /*
    Do it right, read header *structure* to get header length.
    Fortunately header has no longs so we don't have to worry about
    swapped words, not to mention swapped bytes.
  */
  if ((n = read(fd, &a_header, sizeof a_header)) != sizeof a_header)
  {
    printf("expected %d, got %d\n", sizeof a_header, n);
    pexit("file header too short: ", file_name);
  }
  if (a_header.xa_magic == FMAGIC)
    *sepid = 0;
  else if (a_header.xa_magic == IMAGIC)
    *sepid = 1;
  else
    pexit("not Xenix a.out FMAGIC or IMAGIC. FIle: ", file_name);
  if (a_header.xa_entry != 0)
    pexit("nonzero entry point. FIle: ", file_name);
  *text_bytes = a_header.xa_text;
  *data_bytes = a_header.xa_data;
  *bss_bytes  = a_header.xa_bss;
  *sym_bytes  = a_header.xa_syms;
#else
  /* Read first 8 bytes of header to get header length. */
  if ((n = read(fd, hd, 8)) != 8) pexit("file header too short: ", file_name);
  header_len = hd[HDR_LEN];
  if (header_len != HEADER1 && header_len != HEADER2) 
        pexit("bad header length. File: ", file_name);

  /* Extract separate I & D bit. */
  *sepid = hd[SEP_POS] & SEP_ID_BIT;

  /* Read the rest of the header and extract the sizes. */
  if ((n = read(fd, head, header_len - 8)) != header_len - 8)
        pexit("header too short: ", file_name);

  *text_bytes = (unsigned) head[TEXT_POS];
  *data_bytes = (unsigned) head[DATA_POS];
  *bss_bytes  = (unsigned) head[BSS_POS];
  *sym_bytes  = (unsigned) head[SYM_POS];
#endif
}


wr_out(buffer, bytes)
char buffer[READ_UNIT];
int bytes;
{
/* Write some bytes to the output file.  This procedure must avoid writes
 * that are not entire 512-byte blocks, because when this program runs on
 * MS-DOS, the only way it can write the raw diskette is by using the system
 * calls for raw block I/O.
 */

  int room, count, count1;
  register char *p, *q;

  /* Copy the data to the output buffer. */
  room = SECTOR_SIZE - buf_bytes;
  count = (bytes <= room ? bytes : room);
  count1 = count;
  p = &buf[buf_bytes];
  q = buffer;
  while (count--) *p++ = *q++;
  
  /* See if the buffer is full. */
  buf_bytes += count1;
  if (buf_bytes == SECTOR_SIZE) {
        /* Write the whole block to the disk. */
        write_block(cur_sector, buf);
        clear_buf();
  }

  /* Is there any more data to copy. */
  if (count1 == bytes) return;
  bytes -= count1;
  buf_bytes = bytes;
  p = buf;
  while (bytes--) *p++ = *q++;
}


flush()
{
  if (buf_bytes == 0) return;
  write_block(cur_sector, buf);
  clear_buf();
}


clear_buf()
{
  register char *p;

  for (p = buf; p < &buf[SECTOR_SIZE]; p++) *p = 0;
  buf_bytes = 0;
  cur_sector++;
}


patch1(all_size)
long all_size;
{
/* Put the ip and cs values for menu in the last two words of the boot blk.
 * If menu is sep I&D we must also provide the ds-value (addr. 506).
 * Put in bootblok-offset 504 the number of sectors to load.
 */

  long menu_org;
  unsigned short ip, cs, ds, ubuf[SECTOR_SIZE/2], sectrs;

  if (cum_size % clicksize != 0)
	pexit("MINIX is not multiple of clicksize bytes", "");
  menu_org = PROG_ORG + cum_size;       /* where does menu begin */
  ip = 0;
  cs = menu_org >> HCLICK_SHIFT;
  if (sizes[FSCK].sep_id)
     ds = cs + (sizes[FSCK].text_size >> HCLICK_SHIFT);
  else
     ds = cs;

  /* calc nr of sectors to load (starting at 0) */
  sectrs = (unsigned) (all_size / 512L);
  if (all_size % 512 != 0)
     ++sectrs;

  read_block(0, ubuf);          /* read in boot block */
  ubuf[(SECTOR_SIZE/2) - 4] = sectrs;
  ubuf[(SECTOR_SIZE/2) - 3] = ds;
  ubuf[(SECTOR_SIZE/2) - 2] = ip;
  ubuf[(SECTOR_SIZE/2) - 1] = cs;
  write_block(0, ubuf);
}

patch2()
{
/* This program now has information about the sizes of the kernel, mm, fs, and
 * init.  This information is patched into the kernel as follows. The first 8
 * words of the kernel data space are reserved for a table filled in by build.
 * The first 2 words are for kernel, then 2 words for mm, then 2 for fs, and
 * finally 2 for init.  The first word of each set is the text size in clicks;
 * the second is the data+bss size in clicks.  If separate I & D is NOT in
 * use, the text size is 0, i.e., the whole thing is data.
 *
 * In addition, the DS value the kernel is to use is computed here, and loaded
 * at location 4 in the kernel's text space.  It must go in text space because
 * when the kernel starts up, only CS is correct.  It does not know DS, so it
 * can't load DS from data space, but it can load DS from text space.
 * Write the offset of the symbol table for each progam into location 2 of
 * its code space, for the debugger. No one was expecting this, but is is
 * the only available unused space.
 */

  int i, j;
  unsigned short t, d, b, s, text_clicks, data_clicks, ds;
  long text_offset, data_offset;

  /* See if the magic number is where it should be in the kernel. */
  text_offset = 512L;
  data_offset = 512L + (long)sizes[KERN].text_size;    /* start of kernel data */
  i = (get_byte(data_offset+1L) << 8) + get_byte(data_offset);
  if (i != KERNEL_D_MAGIC)  {
	pexit("kernel data space: no magic #","");
  }
  
  for (i = 0; i < PROGRAMS - 1; i++) {
        t = sizes[i].text_size;
        d = sizes[i].data_size;
        b = sizes[i].bss_size;
	s = sizes[i].sym_size;
        if (sizes[i].sep_id) {
                text_clicks = t >> click_shift;
                data_clicks = ((unsigned long) d + b + s) >> click_shift;
		put_word(text_offset + SYM_OFFSET, d + b);
        } else {
                text_clicks = 0;
                data_clicks = ((unsigned long) t + d + b + s) >> click_shift;
		put_word(text_offset + SYM_OFFSET, t + d + b);
        }
        put_byte(data_offset + 4*i + 0L, (text_clicks>>0) & 0377);
        put_byte(data_offset + 4*i + 1L, (text_clicks>>8) & 0377);
        put_byte(data_offset + 4*i + 2L, (data_clicks>>0) & 0377);
        put_byte(data_offset + 4*i + 3L, (data_clicks>>8) & 0377);
        text_offset += (unsigned long) t + d + b + s;
  }

  /* Now write the DS value into word 4 of the kernel text space. */
  if (sizes[KERN].sep_id == 0)
        ds = PROG_ORG >> HCLICK_SHIFT;	/* combined I & D space */
  else
        ds = (PROG_ORG + sizes[KERN].text_size) >> HCLICK_SHIFT; /* separate */
  put_byte(512L + DS_OFFSET, ds & 0377);
  put_byte(512L + DS_OFFSET + 1L, (ds>>8) & 0377);
}


patch3()
{
/* Write the origin and text and data sizes of the init program in FS's data
 * space.  The file system expects to find these 3 words there.
 */

  unsigned short init_text_size, init_data_size, init_buf[SECTOR_SIZE/2], i;
  unsigned short w0, w1, w2;
  int b0, b1, b2, b3, b4, b5, mag;
  long init_org, fs_org, fbase, mm_data;

  init_org  = PROG_ORG;
  init_org += (long)sizes[KERN].text_size+sizes[KERN].data_size+sizes[KERN].bss_size + sizes[KERN].sym_size;
  /* this code was awful and is worse after adding sym_sizes */
  mm_data = init_org - PROG_ORG +512L;	/* offset of mm in file */
  mm_data += (long) sizes[MM].text_size;
  init_org += (long)sizes[MM].text_size + sizes[MM].data_size + sizes[MM].bss_size + sizes[MM].sym_size;
  fs_org = init_org - PROG_ORG + 512L;	/* offset of fs-text into file */
  fs_org +=  (long) sizes[FS].text_size;
  init_org += (long)sizes[FS].text_size + sizes[FS].data_size + sizes[FS].bss_size + sizes[FS].sym_size;
  init_text_size = sizes[INIT].text_size;
  init_data_size = sizes[INIT].data_size + sizes[INIT].bss_size
                 + sizes[INIT].sym_size;
  init_org  = init_org >> click_shift;	/* convert to clicks */
  if (sizes[INIT].sep_id == 0) {
        init_data_size += init_text_size;
        init_text_size = 0;
  }
  init_text_size = init_text_size >> click_shift;
  init_data_size = init_data_size >> click_shift;

  w0 = (unsigned short) init_org;
  w1 = init_text_size;
  w2 = init_data_size;
  b0 =  w0 & 0377;
  b1 = (w0 >> 8) & 0377;
  b2 = w1 & 0377;
  b3 = (w1 >> 8) & 0377;
  b4 = w2 & 0377;
  b5 = (w2 >> 8) & 0377;

  /* Check for appropriate magic numbers. */
  fbase = fs_org;
  mag = (get_byte(mm_data+1L) << 8) + get_byte(mm_data+0L);
  if (mag != FS_D_MAGIC) pexit("mm data space: no magic #","");
  mag = (get_byte(mm_data+3L) << 8) + get_byte(mm_data+2L);
  if (mag == 0) mag = HCLICK_SHIFT;	/* old mm */
  if (mag != click_shift) pexit("mm click_shift does not match kernel's", "");
  mag = (get_byte(fbase+1L) << 8) + get_byte(fbase+0L);
  if (mag != FS_D_MAGIC) pexit("fs data space: no magic #","");
  mag = (get_byte(fbase+3L) << 8) + get_byte(fbase+2L);
  if (mag == 0) mag = HCLICK_SHIFT;	/* old fs */
  if (mag != click_shift) pexit("fs click_shift does not match kernel's", "");

  put_byte(fbase+4L, b0);
  put_byte(fbase+5L, b1);
  put_byte(fbase+6L, b2);
  put_byte(fbase+7L, b3);
  put_byte(fbase+8L ,b4);
  put_byte(fbase+9L, b5);
}


int get_byte(offset)
long offset;
{
/* Fetch one byte from the output file. */

  char buff[SECTOR_SIZE];

  read_block( (unsigned) (offset / SECTOR_SIZE), buff);
  return(buff[(unsigned) (offset % SECTOR_SIZE)] & 0377);
}

put_byte(offset, byte_value)
long offset;
int byte_value;
{
/* Write one byte into the output file. This is not very efficient, but
 * since it is only called to write a few words it is just simpler.
 */

  char buff[SECTOR_SIZE];

  read_block( (unsigned) (offset/SECTOR_SIZE), buff);
  buff[(unsigned) (offset % SECTOR_SIZE)] = byte_value;
  write_block( (unsigned)(offset/SECTOR_SIZE), buff);
}


pexit(s1, s2)
char *s1, *s2;
{
  printf("Build: %s%s\n", s1, s2);
  exit(1);
}


/* this should be used instead of paired put_byte()'s */
put_word(offset, word_value)
long offset;
unsigned word_value;
{
  put_byte(offset, word_value % 256);
  put_byte(offset + 1, word_value / 256);
}


/*===========================================================================
 * The following code is only used in the UNIX version of this program.
 *===========================================================================*/
#ifndef MSDOS
create_image(f)
char *f;
{
/* Create the output file. */
  image = creat(f, 0666);
  close(image);
  image = open(f, 2);
}

read_block(blk, buff)
int blk;
char buff[SECTOR_SIZE];
{
  lseek(image, (long)SECTOR_SIZE * (long) blk, 0);
  if (read(image, buff, SECTOR_SIZE) != SECTOR_SIZE) pexit("block read error", "");
}

write_block(blk, buff)
int blk;
char buff[SECTOR_SIZE];
{
  lseek(image, (long)SECTOR_SIZE * (long) blk, 0);
  if (write(image, buff, SECTOR_SIZE) != SECTOR_SIZE) pexit("block write error", "");
}

IOinit() {}	/* dummy */

#else /*MSDOS*/
/*===========================================================================
 *   This is the raw diskette I/O for MSDOS. It uses diskio.asm or biosio.asm
 *==========================================================================*/

#define MAX_RETRIES     5

char *buff;
char buff1[SECTOR_SIZE];
char buff2[SECTOR_SIZE];
int drive;

IOinit()			/* check if no DMAoverrun & assign the buffer */
{
  if (DMAoverrun(buff1))
     buff = buff2;
  else
     buff = buff1;
}


read_block (blocknr,user)
int blocknr;
char user[SECTOR_SIZE];
{
  /* read the requested MINIX-block in core */
  int retries,err,i;
  char *p;

  retries = MAX_RETRIES;
  do
      err = absread (drive, blocknr, buff);
  while (err && --retries);

  if (!retries)
    dexit ("reading",drive,blocknr,err);

  p=buff; i=SECTOR_SIZE;
  while (i--) *(user++) = *(p++);
}



write_block (blocknr,user)
int blocknr;
char user[SECTOR_SIZE];
{
  /* write the requested MINIX-block to disk */
  int retries,err,i;
  char *p;

  p=buff; i=SECTOR_SIZE;
  while (i--) *(p++) = *(user++);

  retries = MAX_RETRIES;
  do
      err = abswrite (drive, blocknr, buff);
  while (err && --retries);

  if (!retries)
    dexit ("writing",drive,blocknr,err);
}



dexit (s,drive,sectnum,err)
int sectnum, err,drive;
char *s;
{ extern char *derrtab[];
  printf ("Error %s drive %c, sector: %d, code: %d, %s\n",
           s, drive+'A',sectnum, err, derrtab[err] );
  exit (2);
}


create_image (s)
char *s;
{
  char kbstr[10];
  if (s[1] != ':') pexit ("wrong drive name (dos): ",s);
  drive = (s[0] & ~32) - 'A';
  if (drive<0 || drive>32) pexit ("no such drive: ",s);
  printf("Put a blank, formatted diskette in drive %s\nHit return when ready",s);
  gets (kbstr,10);
  puts("");
}

char *derrtab[14] = {
        "no error",
        "disk is read-only",
        "unknown unit",
        "device not ready",
        "bad command",
        "data error",
        "internal error: bad request structure length",
        "seek error",
        "unknown media type",
        "sector not found",
        "printer out of paper (??)",
        "write fault",
        "read error",
        "general error"
};


#endif /*MSDOS*/
