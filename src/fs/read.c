/* This file contains the heart of the mechanism used to read (and write)
 * files.  Read and write requests are split up into chunks that do not cross
 * block boundaries.  Each chunk is then processed in turn.  Reads on special
 * files are also detected and handled.
 *
 * The entry points into this file are
 *   do_read:	 perform the READ system call by calling read_write
 *   read_write: actually do the work of READ and WRITE
 *   read_map:	 given an inode and file position, look up its zone number
 *   rd_indir:	 read an entry in an indirect block 
 *   rw_user:	 call the kernel to read and write user space
 *   read_ahead: manage the block read ahead business
 */

#include "fs.h"
#include <fcntl.h>
#include <minix/com.h>
#include "buf.h"
#include "file.h"
#include "fproc.h"
#include "inode.h"
#include "param.h"
#include "super.h"

#define FD_MASK          077	/* max file descriptor is 63 */

PRIVATE message umess;		/* message for asking SYSTASK for user copy */

FORWARD _PROTOTYPE( int rw_chunk, (struct inode *rip, off_t position,
			unsigned off, int chunk, unsigned left, int rw_flag,
			char *buff, int seg, int usr)			);

/*===========================================================================*
 *				do_read					     *
 *===========================================================================*/
PUBLIC int do_read()
{
  return(read_write(READING));
}


/*===========================================================================*
 *				read_write				     *
 *===========================================================================*/
PUBLIC int read_write(rw_flag)
int rw_flag;			/* READING or WRITING */
{
/* Perform read(fd, buffer, nbytes) or write(fd, buffer, nbytes) call. */

  register struct inode *rip;
  register struct filp *f;
  off_t bytes_left, f_size, position;
  unsigned int off, cum_io;
  int op, oflags, r, chunk, usr, seg, block_spec, char_spec;
  int regular, partial_pipe = 0, partial_cnt = 0;
  dev_t dev;
  mode_t mode_word;
  struct filp *wf;

  /* MM loads segments by putting funny things in upper 10 bits of 'fd'. */
  if (who == MM_PROC_NR && (fd & (~BYTE)) ) {
	usr = (fd >> 8) & BYTE;
	seg = (fd >> 6) & 03;
	fd &= FD_MASK;		/* get rid of user and segment bits */
  } else {
	usr = who;		/* normal case */
	seg = D;
  }

  /* If the file descriptor is valid, get the inode, size and mode. */
  if (nbytes < 0) return(EINVAL);
  if ( (f = get_filp(fd)) == NIL_FILP) return(err_code);
  if ( ((f->filp_mode) & (rw_flag == READING ? R_BIT : W_BIT)) == 0)
	return(EBADF);
  if (nbytes == 0) return(0);	/* so char special files need not check for 0*/
  position = f->filp_pos;
  if (position > MAX_FILE_POS) return(EINVAL);
  if (position + nbytes < position) return(EINVAL); /* unsigned overflow */
  oflags = f->filp_flags;
  rip = f->filp_ino;
  f_size = rip->i_size;
  r = OK;
  if (rip->i_pipe == I_PIPE) {
	/* fp->fp_cum_io_partial is only nonzero when doing partial writes */
	cum_io = fp->fp_cum_io_partial; 
  } else {
	cum_io = 0;
  }
  op = (rw_flag == READING ? DEV_READ : DEV_WRITE);
  mode_word = rip->i_mode & I_TYPE;
  regular = mode_word == I_REGULAR || mode_word == I_NAMED_PIPE;

  char_spec = (mode_word == I_CHAR_SPECIAL ? 1 : 0);
  block_spec = (mode_word == I_BLOCK_SPECIAL ? 1 : 0);
  if (block_spec && f_size == 0) f_size = LONG_MAX;
  rdwt_err = OK;		/* set to EIO if disk error occurs */

  /* Check for character special files. */
  if (char_spec) {
	dev = (dev_t) rip->i_zone[0];
	r = dev_io(op, oflags & O_NONBLOCK, dev, position, nbytes, who,buffer);
	if (r >= 0) {
		cum_io = r;
		position += r;
		r = OK;
	}
  } else {
	if (rw_flag == WRITING && block_spec == 0) {
		/* Check in advance to see if file will grow too big. */
		if (position > rip->i_sp->s_max_size - nbytes) return(EFBIG);

		/* Check for O_APPEND flag. */
		if (oflags & O_APPEND) position = f_size;

		/* Clear the zone containing present EOF if hole about
		 * to be created.  This is necessary because all unwritten
		 * blocks prior to the EOF must read as zeros.
		 */
		if (position > f_size) clear_zone(rip, f_size, 0);
	}

	/* Pipes are a little different.  Check. */
	if (rip->i_pipe == I_PIPE) {
	       r = pipe_check(rip,rw_flag,oflags,nbytes,position,&partial_cnt);
	       if (r <= 0) return(r);
	}

	if (partial_cnt > 0) partial_pipe = 1;

	/* Split the transfer into chunks that don't span two blocks. */
	while (nbytes != 0) {
		off = (unsigned int) (position % BLOCK_SIZE);/* offset in blk*/
		if (partial_pipe) {  /* pipes only */
			chunk = MIN(partial_cnt, BLOCK_SIZE - off);
		} else
			chunk = MIN(nbytes, BLOCK_SIZE - off);
		if (chunk < 0) chunk = BLOCK_SIZE - off;

		if (rw_flag == READING || (block_spec && rw_flag == WRITING)) {
			bytes_left = f_size - position;
			if (position >= f_size) break;	/* we are beyond EOF */
			if (chunk > bytes_left) chunk = (int) bytes_left;
		}

		/* Read or write 'chunk' bytes. */
		r = rw_chunk(rip, position, off, chunk, (unsigned) nbytes,
			     rw_flag, buffer, seg, usr);
		if (r != OK) break;	/* EOF reached */
		if (rdwt_err < 0) break;

		/* Update counters and pointers. */
		buffer += chunk;	/* user buffer address */
		nbytes -= chunk;	/* bytes yet to be read */
		cum_io += chunk;	/* bytes read so far */
		position += chunk;	/* position within the file */

		if (partial_pipe) {
			partial_cnt -= chunk;
			if (partial_cnt <= 0)  break;
		}
	}
  }

  /* On write, update file size and access time. */
  /* DEBUG FIXME.  Don't we have to set i_dirty after changing i_size?
   * Probably write_map will already have changed it for the WRITING case
   * and it doesn't matter for the case of reading from pipes.  However,
   * it would be cleaner to set it for all cases at the start of this
   * routine, and not in write_map, etc., now that the inode is almost
   * always dirtied by changing some of its times.
   *
   * Does POSIX really say not to change i_ctime after an unsuccesful write?
   * It is possible for part of a failed write to work and change i_size.
   */
  if (rw_flag == WRITING) {
	if (regular || mode_word == I_DIRECTORY) {
		if (position > f_size) rip->i_size = position;
	}
  } else {
	if (rip->i_pipe == I_PIPE && position >= rip->i_size) {
		/* Reset pipe pointers. */
		rip->i_size = 0;	/* no data left */
		position = 0;		/* reset reader(s) */
		if ( (wf = find_filp(rip, W_BIT)) != NIL_FILP) wf->filp_pos =0;
	}
  }
  f->filp_pos = position;

  /* Check to see if read-ahead is called for, and if so, set it up. */
  if (rw_flag == READING && rip->i_seek == NO_SEEK && position % BLOCK_SIZE== 0
		&& (regular || mode_word == I_DIRECTORY)) {
	rdahed_inode = rip;
	rdahedpos = position;
  }
  rip->i_seek = NO_SEEK;

  if (rdwt_err != OK) r = rdwt_err;	/* check for disk error */
  if (rdwt_err == END_OF_FILE) r = OK;
  if (r == OK) {
	if (rw_flag == READING) rip->i_update |= ATIME;
	if (rw_flag == WRITING) rip->i_update |= CTIME | MTIME;
	rip->i_dirt = DIRTY;		/* inode is thus now dirty */
	if (partial_pipe) {
		partial_pipe = 0;
			/* partial write on pipe with */
			/* O_NOBLOCK, return write count */
		if (!(oflags & O_NONBLOCK)) {
			fp->fp_cum_io_partial += cum_io;
			suspend(XPIPE); /* partial write on pipe with */
			return(0);	/* nbyte > PIPE_SIZE - non-atomic */
		}
	}
	fp->fp_cum_io_partial = 0;
	return(cum_io);
  } else {
	return(r);
  }
}


/*===========================================================================*
 *				rw_chunk				     *
 *===========================================================================*/
PRIVATE int rw_chunk(rip, position, off, chunk, left, rw_flag, buff, seg, usr)
register struct inode *rip;	/* pointer to inode for file to be rd/wr */
off_t position;			/* position within file to read or write */
unsigned off;			/* off within the current block */
int chunk;			/* number of bytes to read or write */
unsigned left;			/* max number of bytes wanted after position */
int rw_flag;			/* READING or WRITING */
char *buff;			/* virtual address of the user buffer */
int seg;			/* T or D segment in user space */
int usr;			/* which user process */
{
/* Read or write (part of) a block. */

  register struct buf *bp;
  register int r;
  int dir, n, block_spec;
  vir_bytes vbuff, vchunk;
  block_t b;
  dev_t dev;

  block_spec = (rip->i_mode & I_TYPE) == I_BLOCK_SPECIAL;
  if (block_spec) {
	b = position/BLOCK_SIZE;
	dev = (dev_t) rip->i_zone[0];
  } else {
	b = read_map(rip, position);
	dev = rip->i_dev;
  }

  if (!block_spec && b == NO_BLOCK) {
	if (rw_flag == READING) {
		/* Reading from a nonexistent block.  Must read as all zeros.*/
		bp = get_block(NO_DEV, NO_BLOCK, NORMAL);    /* get a buffer */
		zero_block(bp);
	} else {
		/* Writing to a nonexistent block. Create and enter in inode.*/
		if ((bp= new_block(rip, position)) == NIL_BUF)return(err_code);
	}
  } else if (rw_flag == READING) {
	/* Read and read ahead if convenient. */
	bp = rahead(rip, b, position, left);
  } else {
	/* Normally an existing block to be partially overwritten is first read
	 * in.  However, a full block need not be read in.  If it is already in
	 * the cache, acquire it, otherwise just acquire a free buffer.
	 */
	n = (chunk == BLOCK_SIZE ? NO_READ : NORMAL);
	if (!block_spec && off == 0 && position >= rip->i_size) n = NO_READ;
	bp = get_block(dev, b, n);
  }

  /* In all cases, bp now points to a valid buffer. */
  if (rw_flag == WRITING && chunk != BLOCK_SIZE && !block_spec &&
					position >= rip->i_size && off == 0)
	zero_block(bp);
  dir = (rw_flag == READING ? TO_USER : FROM_USER);
  vbuff = (vir_bytes) buff;
  vchunk = (vir_bytes) chunk;
  r = rw_user(seg, usr, vbuff, vchunk, bp->b_data+off,dir);
  if (rw_flag == WRITING) bp->b_dirt = DIRTY;
  n = (off + chunk == BLOCK_SIZE ? FULL_DATA_BLOCK : PARTIAL_DATA_BLOCK);
  put_block(bp, n);
  return(r);
}


/*===========================================================================*
 *				read_map				     *
 *===========================================================================*/
PUBLIC block_t read_map(rip, position)
register struct inode *rip;	/* ptr to inode to map from */
off_t position;			/* position in file whose blk wanted */
{
/* Given an inode and a position within the corresponding file, locate the
 * block (not zone) number in which that position is to be found and return it.
 */

  register struct buf *bp;
  register zone_t z;
  int scale, boff, dzones, nr_indirects, index, zind, ex;
  block_t b;
  long excess, zone, block_pos;
  
  scale = rip->i_sp->s_log_zone_size;	/* for block-zone conversion */
  block_pos = position/BLOCK_SIZE;	/* relative blk # in file */
  zone = block_pos >> scale;	/* position's zone */
  boff = (int) (block_pos - (zone << scale) ); /* relative blk # within zone */
  dzones = rip->i_ndzones;
  nr_indirects = rip->i_nindirs;

  /* Is 'position' to be found in the inode itself? */
  if (zone < dzones) {
	zind = (int) zone;	/* index should be an int */
	z = rip->i_zone[zind];
	if (z == NO_ZONE) return(NO_BLOCK);
	b = ((block_t) z << scale) + boff;
	return(b);
  }

  /* It is not in the inode, so it must be single or double indirect. */
  excess = zone - dzones;	/* first Vx_NR_DZONES don't count */

  if (excess < nr_indirects) {
	/* 'position' can be located via the single indirect block. */
	z = rip->i_zone[dzones];
  } else {
	/* 'position' can be located via the double indirect block. */
	if ( (z = rip->i_zone[dzones+1]) == NO_ZONE) return(NO_BLOCK);
	excess -= nr_indirects;			/* single indir doesn't count*/
	b = (block_t) z << scale;
	bp = get_block(rip->i_dev, b, NORMAL);	/* get double indirect block */
	index = (int) (excess/nr_indirects);
	z = rd_indir(bp, index);		/* z= zone for single*/
	put_block(bp, INDIRECT_BLOCK);		/* release double ind block */
	excess = excess % nr_indirects;		/* index into single ind blk */
  }

  /* 'z' is zone num for single indirect block; 'excess' is index into it. */
  if (z == NO_ZONE) return(NO_BLOCK);
  b = (block_t) z << scale;			/* b is blk # for single ind */
  bp = get_block(rip->i_dev, b, NORMAL);	/* get single indirect block */
  ex = (int) excess;				/* need an integer */
  z = rd_indir(bp, ex);				/* get block pointed to */
  put_block(bp, INDIRECT_BLOCK);		/* release single indir blk */
  if (z == NO_ZONE) return(NO_BLOCK);
  b = ((block_t) z << scale) + boff;
  return(b);
}


/*===========================================================================*
 *				rd_indir				     *
 *===========================================================================*/
PUBLIC zone_t rd_indir(bp, index)
struct buf *bp;			/* pointer to indirect block */
int index;			/* index into *bp */
{
/* Given a pointer to an indirect block, read one entry.  The reason for
 * making a separate routine out of this is that there are four cases:
 * V1 (IBM and 68000), and V2 (IBM and 68000).
 */

  struct super_block *sp;
  zone_t zone;			/* V2 zones are longs (shorts in V1) */

  sp = get_super(bp->b_dev);	/* need super block to find file sys type */

  /* read a zone from an indirect block */
  if (sp->s_version == V1)
	zone = (zone_t) conv2(sp->s_native, (int)  bp->b_v1_ind[index]);
  else
	zone = (zone_t) conv4(sp->s_native, (long) bp->b_v2_ind[index]);

  if (zone != NO_ZONE &&
		(zone < (zone_t) sp->s_firstdatazone || zone >= sp->s_zones)) {
	printf("Illegal zone number %ld in indirect block, index %d\n",
	       (long) zone, index);
	panic("check file system", NO_NUM);
  }
  return(zone);
}


/*===========================================================================*
 *				rw_user					     *
 *===========================================================================*/
PUBLIC int rw_user(s, u, vir, bytes, buff, direction)
int s;				/* D or T space (stack is also D) */
int u;				/* process number to r/w (usually = 'who') */
vir_bytes vir;			/* virtual address to move to/from */
vir_bytes bytes;		/* how many bytes to move */
char *buff;			/* pointer to FS space */
int direction;			/* TO_USER or FROM_USER */
{
/* Transfer a block of data.  Two options exist, depending on 'direction':
 *     TO_USER:     Move from FS space to user virtual space
 *     FROM_USER:   Move from user virtual space to FS space
 */

  if (direction == TO_USER ) {
	/* Write from FS space to user space. */
	umess.SRC_SPACE  = D;
	umess.SRC_PROC_NR = FS_PROC_NR;
	umess.SRC_BUFFER = (long) (vir_bytes) buff;
	umess.DST_SPACE  = s;
	umess.DST_PROC_NR = u;
	umess.DST_BUFFER = (long) vir;
  } else {
	/* Read from user space to FS space. */
	umess.SRC_SPACE  = s;
	umess.SRC_PROC_NR = u;
	umess.SRC_BUFFER = (long) vir;
	umess.DST_SPACE  = D;
	umess.DST_PROC_NR = FS_PROC_NR;
	umess.DST_BUFFER = (long) (vir_bytes) buff;
  }

  umess.COPY_BYTES = (long) bytes;
  sys_copy(&umess);
  return(umess.m_type);
}


/*===========================================================================*
 *				read_ahead				     *
 *===========================================================================*/
PUBLIC void read_ahead()
{
/* Read a block into the cache before it is needed. */

  register struct inode *rip;
  struct buf *bp;
  block_t b;

  rip = rdahed_inode;		/* pointer to inode to read ahead from */
  rdahed_inode = NIL_INODE;	/* turn off read ahead */
  if ( (b = read_map(rip, rdahedpos)) == NO_BLOCK) return;	/* at EOF */
  bp = rahead(rip, b, rdahedpos, BLOCK_SIZE);
  put_block(bp, PARTIAL_DATA_BLOCK);
}


/*===========================================================================*
 *				rahead					     *
 *===========================================================================*/
PUBLIC struct buf *rahead(rip, baseblock, position, bytes_ahead)
register struct inode *rip;	/* pointer to inode for file to be read */
block_t baseblock;		/* block at current position */
off_t position;			/* position within file */
unsigned bytes_ahead;		/* bytes beyond position for immediate use */
{
/* Fetch a block from the cache or the device.  If a physical read is
 * required, prefetch as many more blocks as convenient into the cache.
 * This usually covers bytes_ahead plus any more blocks on the last "track".
 * The device driver may decide it knows better about the track geometry
 * and stop reading at any track boundary (or after an error).
 * Rw_scattered() puts an optional flag on all reads to allow this.
 */

  int block_spec, reading_ahead, read_q_size;
  unsigned int blocks_ahead, blocks_per_track;
  unsigned int fragment, limit_bufs_in_use, track, max_track;
  block_t block;
  dev_t dev;
  off_t dev_size, file_size;
  register struct buf *bp;
  static struct buf *read_q[NR_BUFS];	/* static so it isn't on stack */

  block_spec = (rip->i_mode & I_TYPE) == I_BLOCK_SPECIAL;
  if (block_spec)
	dev = (dev_t) rip->i_zone[0];
  else
	dev = rip->i_dev;
  bp = get_block(dev, baseblock, PREFETCH);
  if (bp->b_dev != NO_DEV) return(bp);

  /* Guesstimate blocks_per_track.  A bad guess will work but be sub-optimal.
   * Dev_open may eventually do it properly.
   */
  if (block_spec)
	dev_size = rip->i_size;
  else
#if (MACHINE == ATARI)
	dev_size =  80L * 2 * 9 * 512;	/* can be 80L*1*9*512 as well */
#else
	dev_size =  80L * 2 * 15 * 512;	/* change to your usual floppy size */
#endif
  if (dev_size == 0)
	blocks_per_track = 17;	/* hard disk (17 * nr_heads / 2 is too many) */
  if (dev_size < 80L * 2 * 15 * 512)
	blocks_per_track = 9;	/* low-density floppy */
  else if (dev_size < 80L * 2 * 18 * 512)
	blocks_per_track = 15;	/* high-density floppy */
  else
	blocks_per_track = 18;	/* higher-density floppy */

  file_size = rip->i_size;
  if (block_spec && file_size == 0) file_size = LONG_MAX;
  fragment = (unsigned) (position % BLOCK_SIZE);
  position = position - fragment + BLOCK_SIZE;
  blocks_ahead = (fragment + bytes_ahead + BLOCK_SIZE - 1) / BLOCK_SIZE - 1;

  /* Set the limit (max + 1) on buffers used. Avoid taking the last 2 buffers
   * for ordinary files, because the cache will thrash if these are needed
   * for indirect blocks.  There is no point in stopping earlier for the
   * immediately-needed part of the read.  Large reads will evict from the
   * cache all blocks except those for the read and the indirect blocks, no
   * matter what is done here.
   */
  limit_bufs_in_use = block_spec ? NR_BUFS : NR_BUFS - 2;

  max_track = (unsigned) (bp->b_blocknr / blocks_per_track);
  reading_ahead = FALSE;
  read_q[0] = bp;		/* first buffer must be read */
  read_q_size = 1;

  /* The next loop has 2 phases, controlled by 'reading_ahead'. */
  while (TRUE) {
	if (position >= file_size || bufs_in_use >= limit_bufs_in_use) break;
  	if (blocks_ahead != 0)
		--blocks_ahead;
	else {
		/* All the immediately-needed blocks have been read.  Give
		 * up after seeks and partial reads.
		 */
		if (reading_ahead || rip->i_seek == ISEEK ||
		    (fragment + bytes_ahead) % BLOCK_SIZE != 0) break;

		/* Try for more blocks on the last "track".  Try a few more
		 * than 'blocks_per_track' to allow for blocks out of order.
		 * Reducing 'limit_bufs_in_use' here might reduce thrashing.
		 */
		blocks_ahead = blocks_per_track + 6;
		reading_ahead = TRUE;
  	}
	if (block_spec)
		block = position / BLOCK_SIZE;
	else
		block = read_map(rip, position);
	position += BLOCK_SIZE;
	track = (unsigned) (block / blocks_per_track);
	if (reading_ahead) {
		if (track != max_track) continue;
	} else {
		if (track > max_track) max_track = track;
	}
	if (block_spec || block != NO_BLOCK) {
		bp = get_block(dev, block, PREFETCH);
		if (bp->b_dev == NO_DEV)
			read_q[read_q_size++] = bp;
		else
			put_block(bp, FULL_DATA_BLOCK);
	}
  }
  rw_scattered(dev, read_q, read_q_size, READING);
  return(get_block(dev, baseblock, NORMAL));
}
