/* Prototypes for system library functions. */

/* Minix user+system library. */
#ifndef _SYSLIB_H
#define _SYSLIB_H
_PROTOTYPE( void printk, (char *_fmt, ...)				);
_PROTOTYPE( int sendrec, (int _src_dest, message *_m_ptr)		);
_PROTOTYPE( int _taskcall, (int _who, int _syscallnr, message *_msgptr)	);

/* Minix system library. */
_PROTOTYPE( int receive, (int _src, message *_m_ptr)			);
_PROTOTYPE( int send, (int _dest, message *_m_ptr)			);

_PROTOTYPE( void sys_abort, (void)					);
_PROTOTYPE( void sys_copy, (message *_mptr)				);

#endif /* _SYSLIB_H */
