/* This file manages the super block table and the related data structures,
 * namely, the bit maps that keep track of which zones and which inodes are
 * allocated and which are free.  When a new inode or zone is needed, the
 * appropriate bit map is searched for a free entry.
 *
 * The entry points into this file are
 *   load_bit_maps:   get the bit maps for the root or a newly mounted device
 *   unload_bit_maps: write the bit maps back to disk after an UMOUNT
 *   alloc_bit:       somebody wants to allocate a zone or inode; find one
 *   free_bit:        indicate that a zone or inode is available for allocation
 *   get_super:       search the 'superblock' table for a device
 *   mounted:         tells if file inode is on mounted (or ROOT) file system
 *   read_super:      read a superblock
 */

#include "fs.h"
#include <string.h>
#include <minix/boot.h>
#include "buf.h"
#include "inode.h"
#include "super.h"

#define BITCHUNK_BITS (usizeof(bitchunk_t) * CHAR_BIT)
#define BIT_MAP_SHIFT     13	/* (log2 of BLOCK_SIZE) + 3; 13 for 1k blocks*/

/*===========================================================================*
 *				load_bit_maps				     *
 *===========================================================================*/
PUBLIC int load_bit_maps(dev)
dev_t dev;			/* which device? */
{
/* Load the bit map for some device into the cache and set up superblock. */

  register int i;
  register struct super_block *sp;
  block_t zbase;

  sp = get_super(dev);		/* get the superblock pointer */
  if (bufs_in_use + sp->s_imap_blocks + sp->s_zmap_blocks >= NR_BUFS - 3)
	return(ERROR);		/* insufficient buffers left for bit maps */
  if (sp->s_imap_blocks > I_MAP_SLOTS || sp->s_zmap_blocks > Z_MAP_SLOTS)
	return(ERROR);

  /* Load the inode map from the disk. */
  for (i = 0; i < sp->s_imap_blocks; i++)
	sp->s_imap[i] = get_block(dev, SUPER_BLOCK + 1 + i, NORMAL);

  /* Load the zone map from the disk. */
  zbase = SUPER_BLOCK + 1 + sp->s_imap_blocks;
  for (i = 0; i < sp->s_zmap_blocks; i++)
	sp->s_zmap[i] = get_block(dev, zbase + i, NORMAL);

  /* Inodes 0 and 1, and zone 0 are never allocated.  Mark them as busy. */
  sp->s_imap[0]->b_bitmap[0] |= conv2(sp->s_native, 3); /* inodes 0, 1 busy */
  sp->s_zmap[0]->b_bitmap[0] |= conv2(sp->s_native, 1); /* zone 0 busy */
  return(OK);
}



/*===========================================================================*
 *				unload_bit_maps				     *
 *===========================================================================*/
PUBLIC int unload_bit_maps(dev)
dev_t dev;			/* which device is being unmounted? */
{
/* Unload the bit maps so a device can be unmounted. */

  register int i;
  register struct super_block *sp;

  sp = get_super(dev);		/* get the superblock pointer */

  for (i = 0; i < sp->s_imap_blocks; i++)
	put_block(sp->s_imap[i],I_MAP_BLOCK);

  for (i = 0; i < sp->s_zmap_blocks; i++)
	put_block(sp->s_zmap[i], ZMAP_BLOCK);

  return(OK);
}


/*===========================================================================*
 *				alloc_bit				     *
 *===========================================================================*/
PUBLIC bit_t alloc_bit(map_ptr, map_bits, bit_blocks, origin)
struct buf *map_ptr[];		/* pointer to array of bit block pointers */
bit_t map_bits;			/* how many bits are there in the bit map? */
int bit_blocks;			/* how many blocks are there in the bit map? */
bit_t origin;			/* number of bit to start searching at */
{
/* Allocate a bit from a bit map and return its bit number. */

  int i, o, w, w_off, b, count;
  bit_t a;
  register bitchunk_t *wptr, *wlim;
  bitchunk_t k;
  short block_count;
  struct buf *bp;
  struct super_block *sp;

  sp = get_super(map_ptr[0]->b_dev);	/* get the superblock pointer */
  if (sp->s_rd_only) panic("can't allocate bit on read-only filesys.", NO_NUM);
  
  /* Figure out where to start the bit search (depends on 'origin'). */
  if (origin < 0 || origin >= map_bits) origin = 0;	/* for robustness */

  /* Truncation of the next expression from a bit_t to an int is safe because
   * it it is inconceivable that the number of blocks in the bit map > 32K.
   */  
  b = (int) (origin >> BIT_MAP_SHIFT);	/* relevant bit map block. */

  /* Truncation of the next expression from a bit_t to an int is safe because
   * its value is smaller than BITS_PER_BLOCK and easily fits in an int.
   * The expression is better written as (int) origin % BITS_PER_BLOCK or
   * even (int) origin & ~(BITS_PER_BLOCK - 1).
   */  
  o = (int) (origin - ((bit_t) b << BIT_MAP_SHIFT) );
  w = o/BITCHUNK_BITS;
  block_count = (w == 0 ? bit_blocks : bit_blocks + 1);

  /* The outer while loop iterates on the blocks of the map.  The inner
   * while loop iterates on the words of a block.  The for loop iterates
   * on the bits of a word.
   */
  while (block_count--) {
	/* If need be, loop on all the blocks in the bit map. */
	bp = map_ptr[b];
	wptr = &bp->b_bitmap[w];
	wlim = &bp->b_bitmap[BITMAP_CHUNKS];
	count = 0;
	while (count < BITMAP_CHUNKS) {
		/* Loop on all the words of one of the bit map blocks. */
		if (*wptr != 0xFFFF) {
			/* This word contains a free bit.  Allocate it. */
			k = conv2(sp->s_native, (int) *wptr);
			for (i = 0; i < BITCHUNK_BITS; i++)
				if (((k >> i) & 1) == 0) {
					w_off = (int)(wptr - &bp->b_bitmap[0]);
					w_off = w_off * BITCHUNK_BITS;
					a = i + w_off
					    + ((bit_t) b << BIT_MAP_SHIFT);
					/* If 'a' beyond map check other blks*/
					if (a >= map_bits) {
						wptr = wlim - 1;
						break;
					}
					k |= 1 << i;
					*wptr = conv2(sp->s_native, (int) k);
					bp->b_dirt = DIRTY;
					return(a);
				}
		}
		if (++wptr == wlim) wptr = &bp->b_bitmap[0];	/* wrap */
		count++;
	}
	if (++b == bit_blocks) b = 0;	/* we have wrapped around */
	w = 0;
  }
  return(NO_BIT);		/* no bit could be allocated */
}


/*===========================================================================*
 *				free_bit				     *
 *===========================================================================*/
PUBLIC void free_bit(map_ptr, bit_returned)
struct buf *map_ptr[];		/* pointer to array of bit block pointers */
bit_t bit_returned;		/* number of bit to insert into the map */
{
/* Return a zone or inode by turning off its bitmap bit. */

  int b, r, w, bit;
  struct buf *bp;
  bitchunk_t k;
  struct super_block *sp;

  sp = get_super(map_ptr[0]->b_dev);	/* get the superblock pointer */
  if (sp->s_rd_only) panic("can't free bit on read-only file system", NO_NUM);

  /* The truncations in the next two assignments are valid by the same
   * reasoning as in alloc_bit.
   */
  b = (int) (bit_returned >> BIT_MAP_SHIFT);	/* which block it is in */
  r = (int) (bit_returned - ((bit_t) b << BIT_MAP_SHIFT) );
  w = r/BITCHUNK_BITS;		/* 'w' tells which word it is in */
  bit = r % BITCHUNK_BITS;
  bp = map_ptr[b];
  if (bp == NIL_BUF) return;

  k = conv2(sp->s_native, (int) bp->b_bitmap[w]);
  if (((k >> bit) & 1) == 0) {
	printf("Cannot free bit %ld\n", bit_returned);
	panic("freeing unused block or inode--check file sys", NO_NUM);
  }
  k &= ~(1 << bit);	/* turn the bit off */
  bp->b_bitmap[w] = conv2(sp->s_native, (int) k);
  bp->b_dirt = DIRTY;
}


/*===========================================================================*
 *				get_super				     *
 *===========================================================================*/
PUBLIC struct super_block *get_super(dev)
dev_t dev;			/* device number whose super_block is sought */
{
/* Search the superblock table for this device.  It is supposed to be there. */

  register struct super_block *sp;

  for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++)
	if (sp->s_dev == dev) return(sp);

  /* Search failed.  Something wrong. */
  panic("can't find superblock for device (in decimal)", (int) dev);

  return(NIL_SUPER);		/* to keep the compiler and lint quiet */
}


/*===========================================================================*
 *				mounted					     *
 *===========================================================================*/
PUBLIC int mounted(rip)
register struct inode *rip;	/* pointer to inode */
{
/* Report on whether the given inode is on a mounted (or ROOT) file system. */

  register struct super_block *sp;
  register dev_t dev;

  dev = (dev_t) rip->i_zone[0];
  if (dev == ROOT_DEV) return(TRUE);	/* inode is on root file system */

  for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++)
	if (sp->s_dev == dev) return(TRUE);

  return(FALSE);
}


/*===========================================================================*
 *				read_super				     *
 *===========================================================================*/
PUBLIC int read_super(sp)
register struct super_block *sp; /* pointer to a superblock */
{
/* Read a superblock. */

  register struct buf *bp;
  dev_t dev;
  int magic;
  int version, native;

  dev = sp->s_dev;		/* save device (will be overwritten by copy) */
  bp = get_block(sp->s_dev, SUPER_BLOCK, NORMAL);
  memcpy( (char *) sp, bp->b_data, (size_t) SUPER_SIZE);
  put_block(bp, ZUPER_BLOCK);
  sp->s_dev = NO_DEV;		/* restore later */
  magic = sp->s_magic;		/* determines file system type */

  /* Get file system version and type. */
  if (magic == SUPER_MAGIC || magic == conv2(BYTE_SWAP, SUPER_MAGIC)) {
	version = V1;
	native  = (magic == SUPER_MAGIC);
  } else if (magic == SUPER_V2 || magic == conv2(BYTE_SWAP, SUPER_V2)) {
	version = V2;
	native  = (magic == SUPER_V2);
  } else {
	return(EINVAL);
  }

  /* If the super block has the wrong byte order, swap the fields; the magic
   * number doesn't need conversion. */
  sp->s_ninodes =       conv2(native, (int) sp->s_ninodes);
  sp->s_nzones =        conv2(native, (int) sp->s_nzones);
  sp->s_imap_blocks =   conv2(native, (int) sp->s_imap_blocks);
  sp->s_zmap_blocks =   conv2(native, (int) sp->s_zmap_blocks);
  sp->s_firstdatazone = conv2(native, (int) sp->s_firstdatazone);
  sp->s_log_zone_size = conv2(native, (int) sp->s_log_zone_size);
  sp->s_max_size =      conv4(native, sp->s_max_size);
  sp->s_zones =         conv4(native, sp->s_zones);

  /* In V1, the device size was kept in a short, s_nzones, which limited
   * devices to 32K zones.  For V2, it was decided to keep the size as a
   * long.  However, just changing s_nzones to a long would not work, since
   * then the position of s_magic in the super block would not be the same
   * in V1 and V2 file systems, and there would be no way to tell whether
   * a newly mounted file system was V1 or V2.  The solution was to introduce
   * a new variable, s_zones, and copy the size there.
   *
   * Calculate some other numbers that depend on the version here too, to
   * hide some of the differences.
   */
  if (version == V1) {
	sp->s_zones = sp->s_nzones;	/* only V1 needs this copy */
	sp->s_inodes_per_block = V1_INODES_PER_BLOCK;
	sp->s_ndzones = V1_NR_DZONES;
	sp->s_nindirs = V1_INDIRECTS;
  } else {
	sp->s_inodes_per_block = V2_INODES_PER_BLOCK;
	sp->s_ndzones = V2_NR_DZONES;
	sp->s_nindirs = V2_INDIRECTS;
  }

  sp->s_isearch = 0;		/* inode searches initially start at 0 */
  sp->s_zsearch = 0;		/* zone searches initially start at 0 */
  sp->s_version = version;
  sp->s_native  = native;

  /* Make a few basic checks to see if super block looks reasonable. */
  if (sp->s_imap_blocks < 1 || sp->s_zmap_blocks < 1
				|| sp->s_ninodes < 1 || sp->s_zones < 1
				|| (unsigned) sp->s_log_zone_size > 4) {
	return(EINVAL);
  }
  sp->s_dev = dev;		/* restore device number */
  return(OK);
}
