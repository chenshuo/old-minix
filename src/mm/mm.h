/* This is the master header for mm.  It includes some other files
 * and defines the principal constants.
 */
#define _POSIX_SOURCE      1	/* tell headers to include POSIX stuff */
#define _MINIX             1	/* tell headers to include MINIX stuff */
#define _SYSTEM            1	/* tell headers that this is the kernel */

/* The ANSI C namespace pollution rules forbid the use of sendrec etc. */
/* No.  ANSI encourages applications (here the kernel(+mm+fs+init) is the
 * application) not to use names starting with underscores. send and
 * receive are only used by the kernel, so they should be in a kernel
 * library.  sendrec is used to support the standard library, so we might
 * as well use the library version.  But because of ANSI, it is now called
 * _sendrec.
*/
#define send _send
#define receive _receive
#define sendrec _sendrec

/* The following are so basic, all the *.c files get them automatically. */
#include <minix/config.h>	/* MUST be first */
#include <ansi.h>		/* MUST be second */
#include <sys/types.h>
#include <minix/const.h>
#include <minix/type.h>

#include <tiny-fcntl.h>
#include <tiny-unistd.h>
#include <minix/syslib.h>

#include <limits.h>
#include <errno.h>

#include "const.h"
#include "type.h"
#include "proto.h"
#include "glo.h"
