/* $Header: mktemp.c,v 1.1 91/02/01 10:30:41 ceriel Exp $ */
/* mktemp - make a name for a temporary file; only here for backwards compat */
/* no _-protected system-calls? */

#include <sys/types.h>
#include <unistd.h>

char *mktemp(template)
char *template;
{
  register int pid, k;
  register char *p;

  pid = getpid();		/* get process id as semi-unique number */
  p = template;
  while (*p) p++;		/* find end of string */

  /* Replace XXXXXX at end of template with pid. */
  while (*--p == 'X') {
	*p = '0' + (pid % 10);
	pid /= 10;
  }
  p++;
  for (k = 'a'; k <= 'z'; k++) {
	*p = k;
	if (access(template, 0) < 0) {
		return template;
	}
  }
  return("/");
}
