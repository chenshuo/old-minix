/* This file contains the main program of the File System.  It consists of
 * a loop that gets messages requesting work, carries out the work, and sends
 * replies.
 *
 * The entry points into this file are
 *   main:	main program of the File System
 *   reply:	send a reply to a process after the requested work is done
 */

#ifdef ATARI_ST
#define ASKDEV		/* ask for boot device */
#define FASTLOAD	/* use multiple block transfers to init ram */
#endif

#include "../h/const.h"
#include "../h/type.h"
#include "../h/callnr.h"
#include "../h/com.h"
#include "../h/error.h"
#include "const.h"
#include "type.h"
#include "buf.h"
#include "file.h"
#include "fproc.h"
#include "glo.h"
#include "inode.h"
#include "param.h"
#include "super.h"

#ifdef FASTLOAD
#include "dev.h"
#endif FASTLOAD

#define M64K     0xFFFF0000L	/* 16 bit mask for DMA check */
#define INFO               2	/* where in data_org is info from build */
#define MAX_RAM        16384	/* maximum RAM disk size in blocks */
#define RAM_IMAGE (dev_nr)0x303	/* major-minor dev where root image is kept */

#ifdef i8088
#define EM_ORIGIN   0x100000	/* origin of extended memory RAM disk on AT */
#define MAX_CRD           255	/* if root fs > MAX_CRD, use extended mem */
#endif

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
PUBLIC main()
{
/* This is the main program of the file system.  The main loop consists of
 * three major activities: getting new work, processing the work, and sending
 * the reply.  This loop never terminates as long as the file system runs.
 */
  int error;
  extern int (*call_vector[NCALLS])();

  fs_init();

  /* This is the main loop that gets work, processes it, and sends replies. */
  while (TRUE) {
	get_work();		/* sets who and fs_call */

	fp = &fproc[who];	/* pointer to proc table struct */
	super_user = (fp->fp_effuid == SU_UID ? TRUE : FALSE);   /* su? */
	dont_reply = FALSE;	/* in other words, do reply is default */

	/* Call the internal function that does the work. */
	if (fs_call < 0 || fs_call >= NCALLS)
		error = E_BAD_CALL;
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
PRIVATE get_work()
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
PUBLIC reply(whom, result)
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
PRIVATE fs_init()
{
/* Initialize global variables, tables, etc. */

  register struct inode *rip;
  int i;
  extern struct inode *get_inode();

  buf_pool();			/* initialize buffer pool */
  load_ram();			/* Load RAM disk from root diskette. */
  load_super();			/* Load super block for root device */

  /* Initialize the 'fproc' fields for process 0 and process 2. */
  for (i = 0; i < 3; i+= 2) {
	fp = &fproc[i];
	rip = get_inode(ROOT_DEV, ROOT_INODE);
	fp->fp_rootdir = rip;
	dup_inode(rip);
	fp->fp_workdir = rip;
	fp->fp_realuid = (uid) SYS_UID;
	fp->fp_effuid = (uid) SYS_UID;
	fp->fp_realgid = (gid) SYS_GID;
	fp->fp_effgid = (gid) SYS_GID;
	fp->fp_umask = ~0;
  }

  /* Certain relations must hold for the file system to work at all. */
  if (ZONE_NUM_SIZE != 2) panic("ZONE_NUM_SIZE != 2", NO_NUM);
  if (SUPER_SIZE > BLOCK_SIZE) panic("SUPER_SIZE > BLOCK_SIZE", NO_NUM);
  if(BLOCK_SIZE % INODE_SIZE != 0)panic("BLOCK_SIZE % INODE_SIZE != 0", NO_NUM);
  if (NR_FDS > 127) panic("NR_FDS > 127", NO_NUM);
  if (NR_BUFS < 6) panic("NR_BUFS < 6", NO_NUM);
  if (sizeof(d_inode) != 32) panic("inode size != 32", NO_NUM);
}

/*===========================================================================*
 *				buf_pool				     *
 *===========================================================================*/
PRIVATE buf_pool()
{
/* Initialize the buffer pool.  On the IBM PC, the hardware DMA chip is
 * not able to cross 64K boundaries, so any buffer that happens to lie
 * across such a boundary is not used.  This is not very elegant, but all
 * the alternative solutions are as bad, if not worse.  The fault lies with
 * the PC hardware.
 */
  register struct buf *bp;
#ifdef i8088
  vir_bytes low_off, high_off;
  phys_bytes org;
  extern phys_clicks get_base();
#endif

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

  /* Delete any buffers that span a 64K boundary. */
#ifdef i8088
  for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++) {
	org = get_base() << CLICK_SHIFT;	/* phys addr where FS is */
	low_off = (vir_bytes) bp->b_data;
	high_off = low_off + BLOCK_SIZE - 1;
	if (((org + low_off) & M64K) != ((org + high_off) & M64K)) {
		if (bp == &buf[0]) {
			front = &buf[1];
			buf[1].b_prev = NIL_BUF;
		} else if (bp == &buf[NR_BUFS - 1]) {
			rear = &buf[NR_BUFS - 2];
			buf[NR_BUFS - 2].b_next = NIL_BUF;
		} else {
			/* Delete a buffer in the middle. */
			bp->b_prev->b_next = bp + 1;
			bp->b_next->b_prev = bp - 1;
		}
	}
  }
#endif

  for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++) bp->b_hash = bp->b_next;
  buf_hash[NO_BLOCK & (NR_BUF_HASH - 1)] = front;
}


/*===========================================================================*
 *				load_ram				     *
 *===========================================================================*/
PRIVATE load_ram()
{
/* The root diskette contains a block-by-block image of the root file system
 * starting at 0.  Go get it and copy it to the RAM disk. 
 */

  register struct buf *bp, *bp1;
  int count;
  long k_loaded;
  struct super_block *sp;
  block_nr i;
  dev_nr root_device;
  phys_clicks ram_clicks, init_org, init_text_clicks, init_data_clicks;
  long base;
  extern phys_clicks data_org[INFO + 2];
  extern struct buf *get_block();

  /* Get size of INIT by reading block on diskette where 'build' put it. */
  init_org = data_org[INFO];
  init_text_clicks = data_org[INFO + 1];
  init_data_clicks = data_org[INFO + 2];
  base = (long) init_org + (long) init_text_clicks + (long) init_data_clicks;
  base = base << CLICK_SHIFT;

  /* Get size of RAM disk by reading root file system's super block.
   * First read block 0 from the floppy.  If this is a valid file system, use
   * it as the root image, otherwise try the hard disk (RAM_IMAGE).  
   */
#ifdef ATARI_ST
  printf("Booting MINIX-ST 1.1.  Copyright 1988 Prentice-Hall, Inc.\n");
#endif ATARI_ST
#ifdef ASKDEV
  root_device = (dev_nr)askdev();
  if (root_device == 0)
#endif ASKDEV
  root_device = BOOT_DEV;	/* try floppy disk first */
  bp = get_block(root_device, SUPER_BLOCK, NORMAL);  /* get RAM super block */
  copy(super_block, bp->b_data, sizeof(struct super_block));
  sp = &super_block[0];
  if (sp->s_magic != SUPER_MAGIC) {
	put_block(bp, FULL_DATA_BLOCK);
	root_device = RAM_IMAGE;
	bp = get_block(root_device, SUPER_BLOCK, NORMAL);  /* get RAM super block */
	copy(super_block, bp->b_data, sizeof(struct super_block));
	sp = &super_block[0];
	if (sp->s_magic != SUPER_MAGIC)
		panic("Invalid root file system", NO_NUM);
  }
  count = sp->s_nzones << sp->s_log_zone_size;	/* # blocks on root dev */
  if (count > MAX_RAM) panic("RAM disk is too big. # blocks = ", count);
  ram_clicks = count * (BLOCK_SIZE/CLICK_SIZE);
  put_block(bp, FULL_DATA_BLOCK);

#ifdef i8088
  /* There are two possibilities now (by convention):  
   *    count < MAX_CRD  ==> RAM disk is in core
   *    count >=MAX_CRD  ==> RAM disk is in extended memory (AT only)
   * In the latter case, tell MM that RAM disk size is 0 and tell the ram disk
   * driver than the device begins at 1MB.
   */
  if (count > MAX_CRD) {
	ram_clicks = 0;		/* MM does not have to allocate any core */
	base = EM_ORIGIN;	/* tell RAM disk driver RAM disk origin */
  }
#endif

  /* Tell MM the origin and size of INIT, and the amount of memory used for the
   * system plus RAM disk combined, so it can remove all of it from the map.
   */
  m1.m_type = BRK2;
  m1.m1_i1 = init_text_clicks;
  m1.m1_i2 = init_data_clicks;
  m1.m1_i3 = init_org + init_text_clicks + init_data_clicks + ram_clicks;
#ifdef ATARI_ST
  m1.m1_p1 = (char *) (int) init_org;	/* Bug in Alcyon 4.14 C */
#else
  m1.m1_p1 = (char *) init_org;
#endif
  if (sendrec(MM_PROC_NR, &m1) != OK) panic("FS Can't report to MM", NO_NUM);

  /* Tell RAM driver where RAM disk is and how big it is. */
  m1.m_type = DISK_IOCTL;
  m1.DEVICE = RAM_DEV;
  m1.POSITION = base;
  m1.COUNT = count;
  if (sendrec(MEM, &m1) != OK) panic("Can't report size to MEM", NO_NUM);

  /* Copy the blocks one at a time from the root diskette to the RAM */
#ifdef i8088
  if (ram_clicks == 0) 	
	printf("RAM disk of %d blocks is in extended memory\n\n", count);
#endif
#ifdef FASTLOAD
  fastload(root_device, (char *)base);
#else
  printf("Loading RAM disk.                          Loaded:   0K ");
  for (i = 0; i < count; i++) {
	bp = get_block(root_device, (block_nr) i, NORMAL);
	bp1 = get_block(ROOT_DEV, i, NO_READ);
	copy(bp1->b_data, bp->b_data, BLOCK_SIZE);
	bp1->b_dirt = DIRTY;
	put_block(bp, I_MAP_BLOCK);
	put_block(bp1, I_MAP_BLOCK);
	k_loaded = ( (long) i * BLOCK_SIZE)/1024L;	/* K loaded so far */
	if (k_loaded % 5 == 0) printf("\b\b\b\b\b\b%4DK %c", k_loaded, 0);
  }
#endif FASTLOAD

  if (root_device == BOOT_DEV)
	printf("\rRAM disk loaded.    Please remove root diskette.           \n\n");
  else
	printf("\rRAM disk loaded.                                           \n\n");
}


/*===========================================================================*
 *				load_super				     *
 *===========================================================================*/
PRIVATE load_super()
{
  register struct super_block *sp;
  register struct inode *rip;
  extern struct inode *get_inode();

/* Initialize the super_block table. */

  for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++)
  	sp->s_dev = NO_DEV;

  /* Read in super_block for the root file system. */
  sp = &super_block[0];
  sp->s_dev = ROOT_DEV;
  rw_super(sp,READING);
  rip = get_inode(ROOT_DEV, ROOT_INODE);	/* inode for root dir */

  /* Check super_block for consistency (is it the right diskette?). */
  if ( (rip->i_mode & I_TYPE) != I_DIRECTORY || rip->i_nlinks < 3 ||
						sp->s_magic != SUPER_MAGIC)
	panic("Root file system corrupted.  Possibly wrong diskette.", NO_NUM);

  sp->s_imount = rip;
  dup_inode(rip);
  sp->s_isup = rip;
  sp->s_rd_only = 0;
  if (load_bit_maps(ROOT_DEV) != OK)
	panic("init: can't load root bit maps", NO_NUM);
}

#ifdef ASKDEV
/*===========================================================================*
 *				askdev					     *
 *===========================================================================*/
PRIVATE askdev()
{
  char line[80];
  register char *p;
  register min, maj, c, n;

  printf("Insert ROOT diskette and hit RETURN (or specify bootdev) %c", 0);
  m.m_type = TTY_READ;
  m.TTY_LINE = 0;
  m.PROC_NR = FS_PROC_NR;
  m.ADDRESS = line;
  m.COUNT = sizeof(line);
  if (sendrec(TTY, &m) != OK)
	return(0);
  for (;;) {
	if (m.REP_PROC_NR != FS_PROC_NR)
		return(-1);
	if (m.REP_STATUS != SUSPEND)
		break;
	receive(TTY, &m);
  }
  if ((n = m.REP_STATUS) <= 0)
	return(0);
  p = line;
  for (maj = 0;;) {
	if (--n < 0)
		return(0);
	c = *p++;
	if (c == ',')
		break;
	if (c < '0' || c > '9')
		return(0);
	maj = maj * 10 + c - '0';
  }
  for (min = 0;;) {
	if (--n < 0)
		return(0);
	c = *p++;
	if (c == '\n')
		break;
	if (c < '0' || c > '9')
		return(0);
	min = min * 10 + c - '0';
  }
  if (n != 0)
	return(0);
  return((maj << 8) | min);
}
#endif ASKDEV

#ifdef FASTLOAD
/*===========================================================================*
 *				fastload				     *
 *===========================================================================*/
PRIVATE fastload(boot_dev, address)
dev_nr boot_dev;
char *address;
{
  register i, blocks;
  register long position;

  blocks = lastused(boot_dev);
  printf("Loading RAM disk. To load: %4DK           Loaded:   0K %c",
	((long)blocks * BLOCK_SIZE) / 1024, 0);
  position = 0;
  while (blocks) {
	i = blocks;
	if (i > (18*1024)/BLOCK_SIZE)
		i = (18*1024)/BLOCK_SIZE;
	blocks -= i;
	i *= BLOCK_SIZE;
	m1.m_type = DISK_READ;
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
PRIVATE lastused(boot_dev)
dev_nr boot_dev;
{
  register i, w, b, last, this, zbase;
  register struct super_block *sp = &super_block[0];
  register struct buf *bp;
  register short *wptr, *wlim;

  zbase = SUPER_BLOCK + 1 + sp->s_imap_blocks;
  this = sp->s_firstdatazone;
  last = this - 1;
  for (i = 0; i < sp->s_zmap_blocks; i++) {
	bp = get_block(boot_dev, (block_nr) zbase + i, NORMAL);
	wptr = (short *)&bp->b_data[0];
	wlim = (short *)&bp->b_data[BLOCK_SIZE];
	while (wptr != wlim) {
		w = *wptr++;
		for (b = 0; b < 8*sizeof(*wptr); b++) {
			if (this == sp->s_nzones) {
				put_block(bp, ZMAP_BLOCK);
				return(last << sp->s_log_zone_size);
			}
			if ((w>>b) & 1)
				last = this;
			this++;
		}
	}
	put_block(bp, ZMAP_BLOCK);
  }
  panic("lastused", NO_NUM);
}
#endif FASTLOAD
