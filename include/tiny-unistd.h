/* The file <unistd.h> has so many prototypes that it causes the compiler
 * to overflow when compiling some programs.  This is a stripped down
 * version.
 */
/* POSIX requires size_t and ssize_t in <unistd.h> and elsewhere. */
#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

#ifndef _SSIZE_T
#define _SSIZE_T
typedef int ssize_t;
#endif

_PROTOTYPE( int creat, (const char *_path, Mode_t _mode)		);
_PROTOTYPE( int open,  (const char *_path, int _oflag, ...) 		);
_PROTOTYPE( int access, (const char *_path, Mode_t _amode)		);
_PROTOTYPE( int close, (int _fd)					);
_PROTOTYPE( off_t lseek, (int _fd, off_t _offset, int _whence)		);
_PROTOTYPE( ssize_t read, (int _fd, void *_buf, size_t _n)		);
_PROTOTYPE( ssize_t write, (int _fd, const void *_buf, size_t _n)	);

/* These definitions from unistd.h are needed by FS. */
#define SEEK_SET           0	/* offset is absolute  */
#define SEEK_CUR           1	/* offset is relative to current position */
#define SEEK_END           2	/* offset is relative to end of file */
