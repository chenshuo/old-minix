/* update - do sync periodically		Author: Andy Tanenbaum */

#define _MINIX 1		/* for proto of the non-POSIX sync() */

#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

_PROTOTYPE(int main, (void));

int main()
{
  /* Disable SIGTERM */
  signal(SIGTERM, SIG_IGN);

  /* Release all (?) open file descriptors. */
  close(0);
  close(1);
  close(2);

  /* Release current directory to avoid locking current device. */
  chdir("/");

  /* Open some files to hold their inodes in core. */
#if 0	/* No, don't open them since we often want to mount on them. */
  open("/bin", O_RDONLY);
  open("/etc", O_RDONLY);
  open("/tmp", O_RDONLY);
  open("/usr", O_RDONLY);
  open("/user", O_RDONLY);
#endif

  /* Flush the cache every 30 seconds. */
  while (1) {
	sync();
	sleep(30);
  }
}
