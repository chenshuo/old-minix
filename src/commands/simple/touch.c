/* touch - force file creation time to the present time */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>
#include <minix/minlib.h>

int no_creat = 0;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(int doit, (char *name));
_PROTOTYPE(void usage, (void));

int main(argc, argv)
int argc;
char *argv[];
{
  char *path;
  int i = 1;

  if (argc == 1) usage();
  while (i < argc) {
	if (argv[i][0] == '-') {
		if (argv[i][1] == 'f') {
			i += 1;
		} else if (argv[i][1] == 'c') {
			no_creat = 1;
			i += 1;
		} else {
			usage();
		}
	} else {
		path = argv[i];
		i += 1;
		if (doit(path) > 0) {
			std_err("touch: cannot touch ");
			std_err(path);
			std_err("\n");
		}
	}
  }
  return(0);
}


int doit(name)
char *name;
{
  int fd;
  time_t tim;
  struct stat buf;
  mode_t tmp;
  time_t tvp[2];

  if (!access(name, 0)) {	/* change date if possible */
	stat(name, &buf);
	tmp = (buf.st_mode & S_IFREG);
	if (tmp != S_IFREG) return(1);

	tim = time((long *) 0);
	tvp[0] = tim;
	tvp[1] = tim;
	if (!utime(name, (struct utimbuf *)tvp))
		return(0);
	else
		return(1);

  } else {
	/* File does not exist */
	if (no_creat == 1)
		return(0);
	else if ((fd = creat(name, 0666)) < 0) {
		return(1);
	} else {
		close(fd);
		return(0);
	}
  }
}


void usage()
{
  std_err("Usage: touch [-c] file...\n");
  exit(1);
}
