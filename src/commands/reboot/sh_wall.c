/* wall - write to all logged in users			Author: V. Archer */
/*
   Edvard Tuinder    v892231@si.hhs.NL
    Modified some things to include this with my shutdown/halt
    package
 */

#define _POSIX_SOURCE	1
#include <sys/types.h>
#include <fcntl.h>
#include <pwd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <utmp.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#undef UTMP

static char UTMP[] = "/etc/utmp";	/* Currently logged in users. */

void wall _ARGS(( char *when, char *extra ));

void
wall(when, extra)
char *when;			/* When is shutdown */
char *extra;			/* If non-nil, why is the shutdown */
{
  struct utmp utmp;
  char utmptty[5 + sizeof(utmp.ut_line) + 1];
  char message[1024];
  struct passwd *pw;
  int utmpfd, ttyfd;
  char *ourtty, *ourname;
  time_t now;
  struct utsname utsname;
  struct stat con_st, tty_st;

  if (ourtty = ttyname(1)) {
	if (ourname = strrchr(ourtty, '/')) ourtty = ourname+1;
  } else ourtty = "system task";
  if (pw = getpwuid(getuid())) ourname = pw->pw_name;
  else ourname = "unknown";

  time(&now);
  if (uname(&utsname) != 0) strcpy(utsname.nodename, "?");
  sprintf(message, "\nBroadcast message from %s@%s (%s) %.24s...\007\007\007\n",
		ourname, utsname.nodename, ourtty, ctime(&now));

  if (strlen(when))
    strcat (message,when);
  if (strlen(extra))
    strcat (message,extra);

  if (!strlen(message))
    strcpy (message,"System is going down within considerable time\nPlease finish your jobs ASAP\n");
/* Search the UTMP database for all logged-in users. */

  if ((utmpfd = open(UTMP, O_RDONLY,0)) < 0) {
	fprintf(stderr, "Cannot open utmp file\n");
	return;
  }

  /* first the console */
  strcpy(utmptty, "/dev/console");
  if ((ttyfd = open(utmptty, O_WRONLY,0)) < 0) {
	perror(utmptty);
  } else {
	fstat(ttyfd, &con_st);
	write(ttyfd, message, strlen(message));
	close(ttyfd);
  }

  while (read(utmpfd, (char *) &utmp, sizeof(utmp)) == sizeof(utmp)) {
	/* is this the user we are looking for? */
	if (utmp.ut_type != USER_PROCESS) continue;

	strncpy(utmptty+5, utmp.ut_line, sizeof(utmp.ut_line));
	utmptty[5 + sizeof(utmp.ut_line) + 1] = 0;
	if ((ttyfd = open(utmptty, O_WRONLY,0)) < 0) {
		perror(utmptty);
		continue;
	}
	fstat(ttyfd, &tty_st);
	if (tty_st.st_rdev != con_st.st_rdev)
		write(ttyfd, message, strlen(message));
	close(ttyfd);
  }
  close(utmpfd);
  return;
}
