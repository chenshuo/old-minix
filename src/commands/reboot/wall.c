/* wall - write to all logged in users			Author: V. Archer */

#include <sys/types.h>
#include <fcntl.h>
#include <pwd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <utmp.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#undef UTMP

static char UTMP[] = "/etc/utmp";	/* Currently logged in users. */

int main _ARGS(( void ));

main()
{
  struct utmp utmp;
  char utmptty[5 + sizeof(utmp.ut_line) + 1];
  char message[1024];
  struct passwd *pw;
  int utmpfd, ttyfd;
  char *ourtty, *ourname, *msg;
  int remain, n, stored;
  time_t now;
  struct utsname utsname;
  struct stat con_st, tty_st;

  if (ourtty = ttyname(1)) {
	if (msg = strrchr(ourtty, '/')) ourtty = msg+1;
  } else ourtty = "system task";
  if (pw = getpwuid(getuid())) ourname = pw->pw_name;
  else ourname = "unknown";

  time(&now);
  if (uname(&utsname) != 0) strcpy(utsname.nodename, "?");
  sprintf(message, "\nBroadcast message from %s@%s (%s) %.24s...\007\007\007\n",
		ourname, utsname.nodename, ourtty, ctime(&now));

  remain = strlen(message);
  msg = message + remain;
  remain = sizeof(message) - 3 - remain;
  stored = 0;
  while (remain) {
	n = read(0, msg, remain);
	if (n <= 0) break;
	msg += n;
	stored += n;
	remain -= n;
  }

  if (!stored) {
	fprintf(stderr, "[Aborted message]\n");
	exit(0);
  }
  *msg++ = '\n';
  *msg++ = '\n';
  *msg = '\0';

/* Search the UTMP database for all logged-in users. */

  if ((utmpfd = open(UTMP, O_RDONLY,0)) < 0) {
	fprintf(stderr, "Cannot open utmp file\n");
	exit(1);
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
  return(0);
}
