
/* This file contains the main program of the File System.  It consists of
 * a loop that gets messages requesting work, carries out the work, and sends
 * replies.
 *
 * The entry points into this file are
 *   main:	main program of the File System
 *   reply:	send a reply to a process after the requested work is done
 */

struct super_block;		/* proto.h needs to know this */

#include "fs.h"
#include <fcntl.h>
#include <string.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/boot.h>
#include "buf.h"
#include "dev.h"
#include "file.h"
#include "fproc.h"
#include "inode.h"
#include "param.h"
#include "super.h"

#define M64K     0xFFFF0000L	/* 16 bit mask for DMA check */
#define MAX_RAM        16384	/* maximum RAM disk size in blocks */
#define RAM_IMAGE (dev_t)0x303	/* major-minor dev where root image is kept */

FORWARD _PROTOTYPE( void buf_pool, (void)				);
FORWARD _PROTOTYPE( void fs_init, (void)				);
FORWARD _PROTOTYPE( void get_boot_parameters, (void)			);
FORWARD _PROTOTYPE( void get_work, (void)				);
FORWARD _PROTOTYPE( dev_t load_ram, (void)				);
FORWARD _PROTOTYPE( void load_super, (Dev_t super_dev, block_t origin)	);

#if ASKDEV
FORWARD _PROTOTYPE( int askdev, (void)					);
#endif

#if FASTLOAD
FORWARD _PROTOTYPE( void fastload, (Dev_t boot_dev, char *address)	);
FORWARD _PROTOTYPE( int lastused, (Dev_t boot_dev)			);
#endif

#if (CHIP == INTEL)
FORWARD _PROTOTYPE( phys_bytes get_physbase, (void)			);
#endif

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
PUBLIC void main()
{
/* This is the main program of the file system.  The main loop consists of
 * three major activities: getting new work, processing the work, and sending
 * the reply.  This loop never terminates as long as the file system runs.
 */
  int error;

  fs_init();

  /* This is the main loop that gets work, processes it, and sends replies. */
  while (TRUE) {
	get_work();		/* sets who and fs_call */

	fp = &fproc[who];	/* pointer to proc table struct */
	super_user = (fp->fp_effuid == SU_UID ? TRUE : FALSE);   /* su? */
	dont_reply = FALSE;	/* in other words, do reply is default */

	/* Call the internal function that does the work. */
	if (fs_call < 0 || fs_call >= NCALLS)
		error = EBADCALL;
	else
		error = (*call_vector[fs_call])();

	/* Copy the results back to the user and send reply. */
	if (dont_reply) continue;
	reply(who, error);
	if (rdahed_inode != NIL_INODE) read_ahead(); /* do block read ahead */
  }
}


/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
PRIVATE void get_work()
{  
  /* Normally wait for new input.  However, if 'reviving' is
   * nonzero, a suspended process must be awakened.
   */

  register struct fproc *rp;

  if (reviving != 0) {
	/* Revive a suspended process. */
	for (rp = &fproc[0]; rp < &fproc[NR_PROCS]; rp++) 
		if (rp->fp_revived == REVIVING) {
			who = (int)(rp - fproc);
			fs_call = rp->fp_fd & BYTE;
			fd = (rp->fp_fd >>8) & BYTE;
			buffer = rp->fp_buffer;
			nbytes = rp->fp_nbytes;
			rp->fp_suspended = NOT_SUSPENDED; /*no longer hanging*/
			rp->fp_revived = NOT_REVIVING;
			reviving--;
			return;
		}
	panic("get_work couldn't revive anyone", NO_NUM);
  }

  /* Normal case.  No one to revive. */
  if (receive(ANY, &m) != OK) panic("fs receive error", NO_NUM);

  who = m.m_source;
  fs_call = m.m_type;
}


/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PUBLIC void reply(whom, result)
int whom;			/* process to reply to */
int result;			/* result of the call (usually OK or error #) */
{
/* Send a reply to a user process. It may fail (if the process has just
 * been killed by a signal), so don't check the return code.  If the send
 * fails, just ignore it.
 */

  reply_type = result;
  send(whom, &m1);
}

/*===========================================================================*
 *				fs_init					     *
 *===========================================================================*/
PRIVATE void fs_init()
{
/* Initialize global variables, tables, etc. */

  register struct inode *rip;
  int i;
  dev_t d;			/* device to fetch the superblock from */
  
  /* The following 3 initializations are needed to let dev_open succeed .*/
  fp = (struct fproc *) NULL;
  who = FS_PROC_NR;

  buf_pool();			/* initialize buffer pool */
  get_boot_parameters();	/* get the parameters from the menu */
  d = load_ram();		/* init RAM disk, load if it is root */
  load_super(d, (block_t) 0);	/* load super block for root device */

  /* Initialize the 'fproc' fields for process 0 .. INIT. */
  for (i = 0; i <= LOW_USER; i+= 1) {
	if (i == FS_PROC_NR) continue;	/* do not initialize FS */
	fp = &fproc[i];
	rip = get_inode(ROOT_DEV, ROOT_INODE);
	fp->fp_rootdir = rip;
	dup_inode(rip);
	fp->fp_workdir = rip;
	fp->fp_realuid = (uid_t) SYS_UID;
	fp->fp_effuid = (uid_t) SYS_UID;
	fp->fp_realgid = (gid_t) SYS_GID;
	fp->fp_effgid = (gid_t) SYS_GID;
	fp->fp_umask = ~0;
  }

  /* Certain relations must hold for the file system to work at all. */
  if (SUPER_SIZE > BLOCK_SIZE) panic("SUPER_SIZE > BLOCK_SIZE", NO_NUM);
  if (BLOCK_SIZE % V2_INODE_SIZE != 0)	/* this checks V1_INODE_SIZE too */
	panic("BLOCK_SIZE % V2_INODE_SIZE != 0", NO_NUM);
  if (OPEN_MAX > 127) panic("OPEN_MAX > 127", NO_NUM);
  if (NR_BUFS < 6) panic("NR_BUFS < 6", NO_NUM);
  if (V1_INODE_SIZE != 32) panic("V1 inode size != 32", NO_NUM);
  if (V2_INODE_SIZE != 64) panic("V2 inode size != 64", NO_NUM);
  if (OPEN_MAX > 8 * sizeof(long)) panic("Too few bits in fp_cloexec", NO_NUM);
}

/*===========================================================================*
 *				buf_pool				     *
 *===========================================================================*/
PRIVATE void buf_pool()
{
/* Initialize the buffer pool.  On the IBM PC, the hardware DMA chip is
 * not able to cross 64K boundaries, so any buffer that happens to lie
 * across such a boundary is not used.  This is not very elegant, but all
 * the alternative solutions are as bad, if not worse.  The fault lies with
 * the PC hardware.
 */
  register struct buf *bp;

  vir_bytes low_off, high_off;		/* only used on INTEL chips */
  phys_bytes org;

  bufs_in_use = 0;
  front = &buf[0];
  rear = &buf[NR_BUFS - 1];

  for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++) {
	bp->b_blocknr = NO_BLOCK;
	bp->b_dev = NO_DEV;
	bp->b_next = bp + 1;
	bp->b_prev = bp - 1;
  }
  buf[0].b_prev = NIL_BUF;
  buf[NR_BUFS - 1].b_next = NIL_BUF;

  /* Delete any buffers that span a 64K boundary, by marking them as used. */
#if (CHIP == INTEL)
  for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++) {
	org = get_physbase();	/* phys addr where FS is */
	low_off = (vir_bytes) bp->b_data;
	high_off = low_off + BLOCK_SIZE - 1;
	if (((org + low_off) & M64K) != ((org + high_off) & M64K)) {
		++bp->b_count;	/* it was 0, by static initialization */
		rm_lru(bp);
	}
  }
#endif

  for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++) bp->b_hash = bp->b_next;
  buf_hash[0] = front;
}


/*===========================================================================*
 *				load_ram				     *
 *===========================================================================*/
PRIVATE dev_t load_ram()
{
/* If the root device is the RAM disk, copy the entire root image device
 * block-by-block to a RAM disk with the same size as the image.
 * Otherwise, just allocate a RAM disk with size given in the boot parameters.
 */

  register struct buf *bp, *bp1;
  int count;
  long k_loaded, lcount;
  struct super_block *sp;
  block_t ram_offset = 0;	/* block offset of RAM image on demo diskette*/
  block_t i, b;
  dev_t root_device;		/* really the root image device */
  dev_t super_dev;		/* device to get superblock from */
  phys_clicks ram_clicks, init_org, init_text_clicks, init_data_clicks;
  int major, task;
  message dev_mess;

  /* Get size of INIT by reading block on diskette where 'build' put it. */
  init_org = data_org[INFO];
  init_text_clicks = data_org[INFO + 1];
  init_data_clicks = data_org[INFO + 2];

  /* Print ATARI copyright message. */
#if (MACHINE == ATARI)
  printf("Booting MINIX 1.6.25.  Copyright 1993 Prentice-Hall, Inc.\n");
#endif

  /* If the root device is specified in the boot parameters, use it. */
  if (ROOT_DEV != DEV_RAM) {
	lcount = boot_parameters.bp_ramsize;
	super_dev = ROOT_DEV;	/* get superblock directly from root device */
	major = (super_dev >> MAJOR) & BYTE;	/* major device nr */
	task = dmap[major].dmap_task;		/* device task nr */
	dev_mess.m_type = DEV_OPEN;		/* distinguish from close */
	dev_mess.DEVICE = super_dev;
	dev_mess.TTY_FLAGS = O_RDWR;
	(*dmap[major].dmap_open)(task, &dev_mess);
	if (dev_mess.REP_STATUS != OK) panic("Cannot open root device",NO_NUM);
	goto got_root_dev;	/* kludge to avoid excessive indent/diffs */
  } else {
	super_dev = DEV_RAM;	/* get superblock from RAM disk */
  }

  sp = &super_block[0];
  /* Get size of RAM disk by reading root file system's super block.
   * First read block 0 from the floppy.  If this is a valid file system, use
   * it as the root image, otherwise try the hard disk (RAM_IMAGE).  
   */
#if ASKDEV
  sp->s_dev = (dev_t) askdev();
  if (sp->s_dev == 0)
#endif
  sp->s_dev = BOOT_DEV;	/* this is the 'then' clause if ASKDEV is defined */

  major = (sp->s_dev >> MAJOR) & BYTE;	/* major device nr */
  task = dmap[major].dmap_task;		/* device task nr */
  dev_mess.m_type = DEV_OPEN;		/* distinguish from close */
  dev_mess.DEVICE = sp->s_dev;
  dev_mess.TTY_FLAGS = O_RDWR;
  (*dmap[major].dmap_open)(task, &dev_mess);
  if (dev_mess.REP_STATUS != OK) panic("Cannot open root device", NO_NUM);

  read_super(sp, ram_offset);	/* read in default (root or image) super blk*/
  if (sp->s_version == 0) {
	dev_mess.m_type = DEV_CLOSE;		/* distinguish from open */
	(*dmap[major].dmap_close)(task, &dev_mess);
	sp->s_dev = RAM_IMAGE;
	major = (sp->s_dev >> MAJOR) & BYTE;	/* major device nr */
	task = dmap[major].dmap_task;		/* device task nr */
	dev_mess.m_type = DEV_OPEN;		/* distinguish from close */
	dev_mess.DEVICE = sp->s_dev;
	dev_mess.TTY_FLAGS = O_RDWR;
	(*dmap[major].dmap_open)(task, &dev_mess);
	if (dev_mess.REP_STATUS != OK) panic("Cannot open RAM image",NO_NUM);
	read_super(sp, ram_offset);	/* read in HD RAM image super block */
	if (sp->s_version == 0) panic("Bad root file system", NO_NUM);
  }

  root_device = sp->s_dev;
  lcount = sp->s_zones << sp->s_log_zone_size;	/* # blks on root dev*/

got_root_dev:
  if (lcount > MAX_RAM) panic("RAM disk is too big. # blocks > ", MAX_RAM);
  count = (int) lcount;		/* lcount is now known to be <= MAX_RAM */
  ram_clicks = (lcount * BLOCK_SIZE)/CLICK_SIZE;

  /* Tell MM the origin and size of INIT, and the amount of memory used for the
   * system plus RAM disk combined, so it can remove all of it from the map.
   */
  m1.m_type = BRK2;
  m1.m1_i1 = init_text_clicks;
  m1.m1_i2 = init_data_clicks;
  m1.m1_i3 = init_org + init_text_clicks + init_data_clicks + ram_clicks;
  m1.m1_p1 = (char *) (int) init_org;	/* bug in Alcyon 4.14 C needs 2 casts*/
  if (sendrec(MM_PROC_NR, &m1) != OK) panic("FS Can't report to MM", NO_NUM);

  /* Tell RAM driver where RAM disk is and how big it is. The BRK2 call has
   * filled in the m1.POSITION field.
   */
  m1.m_type = DEV_IOCTL;
  m1.DEVICE = RAM_DEV;
  m1.COUNT = count;
  if (sendrec(MEM, &m1) != OK) panic("Can't report size to MEM", NO_NUM);

#if (CHIP == INTEL)
  /* Say if we are running in real mode or protected mode.
   * 'bp_processor' is re-used to mean 'protected_mode'.
   */
  printf("Executing in %s mode\n\n",
	 boot_parameters.bp_processor ? "protected" : "real");
#endif

  /* If the root device is not the RAM disk, it doesn't need loading. */
  if (ROOT_DEV != DEV_RAM) return(super_dev);	/* ROOT_DEV is a macro */

#if FASTLOAD
  /* Copy the blocks one at a time from the root diskette to the RAM */
  fastload(root_device, (char *) m1.POSITION);	/* assumes 32 bit pointers */
#else

  printf("Loading RAM disk.                       Loaded:    0K ");

  inode[0].i_mode = I_BLOCK_SPECIAL;	/* temp inode for rahead */
  inode[0].i_size = LONG_MAX;
  inode[0].i_dev = root_device;
  inode[0].i_zone[0] = root_device;

  for (i = 0; i < count; i++) {
	b = i + ram_offset;		/* true block number */
	bp = rahead(&inode[0], (block_t) b, (off_t)BLOCK_SIZE*b, BLOCK_SIZE);
	bp1 = get_block(ROOT_DEV, i, NO_READ);
	memcpy(bp1->b_data, bp->b_data, (size_t) BLOCK_SIZE);
	bp1->b_dirt = DIRTY;
	put_block(bp, I_MAP_BLOCK);
	put_block(bp1, I_MAP_BLOCK);
	k_loaded = ( (long) i * BLOCK_SIZE)/1024L;	/* K loaded so far */
	if (k_loaded % 5 == 0) printf("\b\b\b\b\b\b%4DK %c", k_loaded, 0);
  }
#endif /* FASTLOAD */

  if ( ((root_device ^ DEV_FD0) & ~BYTE) == 0 )
	printf("\rRAM disk loaded.    Please remove root diskette.           \n\n");
  else
	printf("\rRAM disk loaded.                                           \n\n");
  return(super_dev);
}


/*===========================================================================*
 *				load_super				     *
 *===========================================================================*/
PRIVATE void load_super(super_dev, origin)
dev_t super_dev;			/* place to get superblock from */
block_t origin;				/* offset to give to read_super() */
{
  int bad;
  register struct super_block *sp;
  register struct inode *rip;

  /* Initialize the super_block table. */
  for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++)
  	sp->s_dev = NO_DEV;

  /* Read in super_block for the root file system. */
  sp = &super_block[0];
  sp->s_dev = super_dev;
  read_super(sp, origin);

  /* Check super_block for consistency (is it the right diskette?). */
  bad = (sp->s_version == 0); /* version is zero if bad magic in super block.*/
  if (!bad) {
	rip = get_inode(super_dev, ROOT_INODE);	/* inode for root dir */
	if ( (rip->i_mode & I_TYPE) != I_DIRECTORY || rip->i_nlinks < 3) bad++;
  }
  if (bad)panic("Invalid root file system.  Possibly wrong diskette.",NO_NUM);

  sp->s_imount = rip;
  dup_inode(rip);
  sp->s_isup = rip;
  sp->s_rd_only = 0;
  if (load_bit_maps(super_dev) != OK)
	panic("init: can't load root bit maps", NO_NUM);
  return;
}

#if ASKDEV
/*===========================================================================*
 *				askdev					     *
 *===========================================================================*/
PRIVATE int askdev()
{
  char line[80];
  register char *p;
  register min, maj, c, n;

  printf("Insert ROOT diskette and hit RETURN (or specify bootdev) %c", 0);
  m.m_type = DEV_READ;
  m.TTY_LINE = 0;
  m.PROC_NR = FS_PROC_NR;
  m.ADDRESS = line;
  m.COUNT = sizeof(line);
  if (sendrec(TTY, &m) != OK) return(0);
  for (;;) {
	if (m.REP_PROC_NR != FS_PROC_NR) return(-1);
	if (m.REP_STATUS != SUSPEND) break;
	receive(TTY, &m);
  }
  if ((n = m.REP_STATUS) <= 0) return(0);
  p = line;
  for (maj = 0;;) {
	if (--n < 0) return(0);
	c = *p++;
	if (c == ',') break;
	if (c < '0' || c > '9') return(0);
	maj = maj * 10 + c - '0';
  }
  for (min = 0; ;) {
	if (--n < 0) return(0);
	c = *p++;
	if (c == '\n') break;
	if (c < '0' || c > '9') return(0);
	min = min * 10 + c - '0';
  }
  if (n != 0) return(0);
  return((maj << 8) | min);
}
#endif /* ASKDEV */

#if FASTLOAD
/*===========================================================================*
 *				fastload				     *
 *===========================================================================*/
PRIVATE void fastload(boot_dev, address)
dev_t boot_dev;
char *address;
{
  register i, blocks;
  register long position;

  blocks = lastused(boot_dev);
  printf("Loading RAM disk. To load: %4DK        Loaded:   0K %c",
	((long)blocks * BLOCK_SIZE) / 1024, 0);
  position = 0;
  while (blocks) {
	i = blocks;
	if (i > (18*1024)/BLOCK_SIZE) i = (18*1024)/BLOCK_SIZE;
	blocks -= i;
	i *= BLOCK_SIZE;
	m1.m_type = DEV_READ;
	m1.DEVICE = (boot_dev >> MINOR) & BYTE;
	m1.POSITION = position;
	m1.PROC_NR = HARDWARE;
	m1.ADDRESS = address;
	m1.COUNT = i;
	(*dmap[(boot_dev >> MAJOR) & BYTE].dmap_rw)(
		dmap[(boot_dev >> MAJOR) & BYTE].dmap_task,
		&m1
	);
	if (m1.REP_STATUS < 0)
		panic("Disk error loading BOOT disk", m1.REP_STATUS);
	position += i;
	address += i;
	printf("\b\b\b\b\b\b%4DK %c", position / 1024L, 0);
  }
}

/*===========================================================================*
 *				lastused				     *
 *===========================================================================*/
PRIVATE int lastused(boot_dev)
dev_t boot_dev;
{
  register i, w, b, last, this, zbase;
  register struct super_block *sp = &super_block[0];
  register struct buf *bp;
  register bitchunk_t *wptr, *wlim;

  zbase = SUPER_BLOCK + 1 + sp->s_imap_blocks;
  this = sp->s_firstdatazone;
  last = this - 1;
  for (i = 0; i < sp->s_zmap_blocks; i++) {
	bp = get_block(boot_dev, (block_t) zbase + i, NORMAL);
	wptr = &bp->b_bitmap[0];
	wlim = &bp->b_bitmap[BITMAP_CHUNKS];
	while (wptr != wlim) {
		w = *wptr++;
		for (b = 0; b < 8*sizeof(*wptr); b++) {
			if (this == sp->s_zones) {
				put_block(bp, ZMAP_BLOCK);
				return(last << sp->s_log_zone_size);
			}
			if ((w>>b) & 1) last = this;
			this++;
		}
	}
	put_block(bp, ZMAP_BLOCK);
  }
  panic("lastused", NO_NUM);
}
#endif /* FASTLOAD */

/*===========================================================================*
 *				get_boot_parameters			     *
 *===========================================================================*/
PUBLIC struct bparam_s boot_parameters =  /* overwritten if new kernel */
{
  DROOTDEV, DRAMIMAGEDEV, DRAMSIZE, DSCANCODE,
};

PRIVATE void get_boot_parameters()
{
/* Ask kernel for boot parameters. */

  struct bparam_s temp_parameters;

  m1.m_type = SYS_GBOOT;
  m1.PROC1 = FS_PROC_NR;
  m1.MEM_PTR = (char *) &temp_parameters;
  if (sendrec(SYSTASK, &m1) == OK && m1.m_type == OK)
	boot_parameters = temp_parameters;
}

#if (CHIP == INTEL)
/*===========================================================================*
 *				get_physbase				     *
 *===========================================================================*/
PRIVATE phys_bytes get_physbase()
{
/* Ask kernel for base of fs data space. */

  m1.m_type = SYS_UMAP;
  m1.SRC_PROC_NR = FS_PROC_NR;
  m1.SRC_SPACE = D;
  m1.SRC_BUFFER = 0;
  m1.COPY_BYTES = 1;
  if (sendrec(SYSTASK, &m1) != OK || m1.SRC_BUFFER == 0)
	panic("Can't get fs base", NO_NUM);
  return(m1.SRC_BUFFER);
}
#endif /* INTEL */

