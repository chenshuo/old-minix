/* This is the filp table.  It is an intermediary between file descriptors and
 * inodes.  A slot is free if filp_count == 0.
 */

EXTERN struct filp {
  mode_t filp_mode;		/* RW bits, telling how file is opened */
  int filp_flags;		/* flags from open and fcntl */
  int filp_count;		/* how many file descriptors share this slot?*/
  struct inode *filp_ino;	/* pointer to the inode */
  off_t filp_pos;		/* file position */
} filp[NR_FILPS];

#define NIL_FILP (struct filp *) 0	/* indicates absence of a filp slot */



/* This is the file locking table.  Like the filp table, it points to the
 * inode table, however, in this case to achieve advisory locking.
 */
EXTERN struct file_lock {
  short lock_type;		/* F_RDLOCK or F_WRLOCK; 0 means unused slot */
  pid_t lock_pid;		/* pid of the process holding the lock */
  struct inode *lock_inode;	/* pointer to the inode locked */
  off_t lock_first;		/* offset of first byte locked */
  off_t lock_last;		/* offset of last byte locked */
} file_lock[NR_LOCKS];
