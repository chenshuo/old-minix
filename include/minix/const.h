/* Copyright (C) 1992 by Prentice-Hall, Inc.  Permission is hereby granted
 * to redistribute the binary and source programs of this system for
 * educational or research purposes.  For other use, written permission from
 * Prentice-Hall is required.  
 */

#define EXTERN        extern	/* used in *.h files */
#define PRIVATE       static	/* PRIVATE x limits the scope of x */
#define PUBLIC			/* PUBLIC is the opposite of PRIVATE */
#define FORWARD       static	/* some compilers require this to be 'static'*/

#define TRUE               1	/* used for turning integers into Booleans */
#define FALSE              0	/* used for turning integers into Booleans */

#define HZ	          60	/* clock freq (software settable on IBM-PC) */
#define BLOCK_SIZE      1024	/* # bytes in a disk block */
#define SUPER_USER (uid_t) 0	/* uid_t of superuser */

#define MAJOR	           8	/* major device = (dev>>MAJOR) & 0377 */
#define MINOR	           0	/* minor device = (dev>>MINOR) & 0377 */

#define NULL     ((void *)0)	/* null pointer */
#define CPVEC_NR          16	/* max # of entries in a SYS_VCOPY request */
#if (MACHINE == SUN_4)
#define LOAD_ADDR     0x4000	/* addr at which the file is loaded (SPARC) */
				/* DEBUG.  What file?  Is this the same as
				 * KERNEL_LOAD_ADDRESS for the 8088/386?
				 * It is 0x600 for the 8088 and for old 386
				 * versions, 0x700 for the current 386 version,
				 * and may have to be page-aligned for later
				 * 386 versions.  It surely belongs in the
				 * machine-dependent section.
				 */
#endif

#define NR_PROCS          32	/* number of slots in proc table */
#define NR_SEGS            3	/* # segments per process */
#define T                  0	/* proc[i].mem_map[T] is for text */
#define D                  1	/* proc[i].mem_map[D] is for data */
#define S                  2	/* proc[i].mem_map[S] is for stack */

/* Process numbers of some important processes. */
#define MM_PROC_NR         0	/* process number of memory manager */
#define FS_PROC_NR         1	/* process number of file system */

/* Miscellaneous */
#define BYTE            0377	/* mask for 8 bits */
#define TO_USER            0	/* flag telling to copy from fs to user */
#define FROM_USER          1	/* flag telling to copy from user to fs */
#define READING            0	/* copy data to user */
#define WRITING            1	/* copy data from user */
#define NO_NUM        0x8000	/* used as numerical argument to panic() */
#define NIL_PTR   (char *) 0	/* generally useful expression */
#define HAVE_SCATTERED_IO  1	/* scattered I/O is now standard */

/* How many bytes are pushed by a signal. */
#define SIG_PUSH_BYTES (sizeof(int)+sizeof(u16_t)+sizeof(u32_t)) 

/* Macros. */
#define MAX(a, b)   ((a) > (b) ? (a) : (b))
#define MIN(a, b)   ((a) < (b) ? (a) : (b))

/* Machine dependent stuff. */
#if INET_KERNEL
#define INET_TASKS         1	/* Ethernet task */
#else
#define INET_TASKS         0
#endif

#if (MACHINE == ATARI)
#define NR_TASKS (11 + INET_TASKS)	/* number of tasks */
#else
#define NR_TASKS (10 + INET_TASKS)	/* number of tasks */
#endif

/* Memory is allocated in clicks. */
#if (CHIP == INTEL)
#define CLICK_SIZE       256	/* unit in which memory is allocated */
#define CLICK_SHIFT        8	/* log2 of CLICK_SIZE */
#endif

#if (CHIP == SPARC) || (CHIP == M68000)
#define CLICK_SIZE	4096	/* unit in which memory is alocated */
#define CLICK_SHIFT	  12	/* 2log of CLICK_SIZE */
#endif

#define click_to_round_k(n) \
	((unsigned) ((((unsigned long) (n) << CLICK_SHIFT) + 512) / 1024))
#if CLICK_SIZE < 1024
#define k_to_click(n) ((n) * (1024 / CLICK_SIZE))
#else
#define k_to_click(n) ((n) / (CLICK_SIZE / 1024))
#endif

#if INET_KERNEL
#define INET_PROC_NR       2	/* process number of network task */
#define INIT_PROC_NR       3	/* init -- the process that goes multiuser */
#define LOW_USER           3	/* first user not part of operating system */
#else
#define INIT_PROC_NR       2	/* init -- the process that goes multiuser */
#define LOW_USER           2	/* first user not part of operating system */
#endif

#define ABS             -999	/* this process means absolute memory */

/* Flag bits for i_mode in the inode. */
#define I_TYPE          0170000	/* this field gives inode type */
#define I_REGULAR       0100000	/* regular file, not dir or special */
#define I_BLOCK_SPECIAL 0060000	/* block special file */
#define I_DIRECTORY     0040000	/* file is a directory */
#define I_CHAR_SPECIAL  0020000	/* character special file */
#define I_NAMED_PIPE	0010000 /* named pipe (FIFO) */
#define I_SET_UID_BIT   0004000	/* set effective uid_t on exec */
#define I_SET_GID_BIT   0002000	/* set effective gid_t on exec */
#define ALL_MODES       0006777	/* all bits for user, group and others */
#define RWX_MODES       0000777	/* mode bits for RWX only */
#define R_BIT           0000004	/* Rwx protection bit */
#define W_BIT           0000002	/* rWx protection bit */
#define X_BIT           0000001	/* rwX protection bit */
#define I_NOT_ALLOC     0000000	/* this inode is free */

/* Some limits. */
#define MAX_BLOCK_NR  ((block_t) 077777777)	/* largest block number */
#define HIGHEST_ZONE   ((zone_t) 077777777)	/* largest zone number */
#define MAX_INODE_NR      ((ino_t) 0177777)	/* largest inode number */
#define MAX_FILE_POS ((off_t) 037777777777)	/* largest legal file offset */

#define NO_BLOCK              ((block_t) 0)	/* absence of a block number */
#define NO_ENTRY                ((ino_t) 0)	/* absence of a dir entry */
#define NO_ZONE                ((zone_t) 0)	/* absence of a zone number */
#define NO_DEV                  ((dev_t) 0)	/* absence of a device numb */
