/* whoami - print the current user name 	Author: Terrence W. Holm */

#include <stdio.h>
#include <pwd.h>

struct passwd *getpwuid();

main()
{
  struct passwd *pw_entry;

  pw_entry = getpwuid(geteuid());
  if (pw_entry == NULL) exit(1);
  puts(pw_entry->pw_name);
  exit(0);
}
