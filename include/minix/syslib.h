/* Prototypes for system library functions. */

/* Minix user+system library. */
#ifndef _SYSLIB_H
#define _SYSLIB_H
_PROTOTYPE( void printk, (char *_fmt, ...)				);
_PROTOTYPE( int _sendrec, (int _src_dest, message *_m_ptr)		);
_PROTOTYPE( int _taskcall, (int _who, int _syscallnr, message *_msgptr)	);

/* Minix system library. */
_PROTOTYPE( int _receive, (int _src, message *_m_ptr)			);
_PROTOTYPE( int _send, (int _dest, message *_m_ptr)			);

_PROTOTYPE( void sys_abort, (int _how)					);
_PROTOTYPE( void sys_copy, (message *_mptr)				);

#endif /* _SYSLIB_H */
