/* su - become super-user		Author: Patrick van Kleef */

#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <minix/minlib.h>

#ifdef SUPERGROUP
/* If this flag is set then su allows members of group 0 to become each other
 * without a password.  See passwd(1) for more.  (kjb)
 */
#define getid()	getgid()
#else
#define getid()	getuid()
#endif

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(int putenv, (char *env));

int main(argc, argv)
int argc;
char *argv[];
{
  register char *name, *password;
  char *shell = "/bin/sh";
  char *shell2 = "/usr/bin/sh";
  int nr;
  register struct passwd *pwd;
  static char USER[20], LOGNAME[25], HOME[100], SHELL[100];

  if (argc > 1 && strcmp(argv[1], "-") == 0) {
	if (argv[0][0] != 0) argv[0][0] = '-';	/* Read .profile */
	argv[1] = argv[0];
	argv++;
	argc--;
  }
  if (argc > 1) {
	name = argv[1];
	argv[1] = argv[0];
	argv++;
  } else
	name = "root";

  if ((pwd = getpwnam(name)) == 0) {
	std_err("Unknown id: ");
	std_err(name);
	std_err("\n");
	exit(1);
  }
  if (pwd->pw_passwd[0] != '\0' && getid() != 0) {
	password = getpass("Password:");
	if (strcmp(pwd->pw_passwd, crypt(password, pwd->pw_passwd))) {
		std_err("Sorry\n");
		exit(2);
	}
  }
  setgid(pwd->pw_gid);
  setuid(pwd->pw_uid);
  if (pwd->pw_shell[0] != '\0')
	shell = pwd->pw_shell;
  else {
	if (access(shell, 0) < 0) shell = shell2;
  }
  if (argv[0][0] == '-') {
	strcpy(USER, "USER=");
	strcpy(USER + 5, name);
	putenv(USER);
	strcpy(LOGNAME, "LOGNAME=");
	strcpy(LOGNAME + 8, name);
	putenv(LOGNAME);
	strcpy(SHELL, "SHELL=");
	strcpy(SHELL + 6, shell);
	putenv(SHELL);
	strcpy(HOME, "HOME=");
	strcpy(HOME + 5, pwd->pw_dir);
	putenv(HOME);
	(void) chdir(pwd->pw_dir);
  }
  execv(shell, argv);
  std_err("No shell\n");
  return(3);
}
