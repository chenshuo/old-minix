/* kill - send a signal to a process	Author: Adri Koppes  */

#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void usage, (void));

int main(argc, argv)
int argc;
char **argv;
{
  int proc, signal = SIGTERM;

  if (argc < 2) usage();
  if (argc > 1 && *argv[1] == '-') {
	signal = atoi(&argv[1][1]);
	if (!signal) usage();
	argv++;
	argc--;
  }
  while (--argc) {
	argv++;
	proc = atoi(*argv);
	if (!proc && strcmp(*argv, "0")) usage();
	if (kill(proc, signal)) {
		printf("kill: %d: %s\n", proc, strerror(errno));
		exit(1);
	}
  }
  return(0);
}

void usage()
{
  printf("Usage: kill [-sig] pid\n");
  exit(1);
}
