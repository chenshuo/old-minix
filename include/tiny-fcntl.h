/* These prototypes from fcntl.h are needed by the system.  fcntl.h may be
 * too big to include, so have all users of these prototypes (including
 * fcntl.h) include this file.  The macros and types must be set up already.
 */
_PROTOTYPE( int creat, (const char *_path, Mode_t _mode)		);
_PROTOTYPE( int open,  (const char *_path, int _oflag, ...) 		);
