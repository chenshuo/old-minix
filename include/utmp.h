/*  utmp.h - Used by login(1), init, and who(1)  */

#define WTMP  "/usr/adm/wtmp"

struct  utmp
{
  char ut_line[8];		/* terminal name */
  char ut_name[8];		/* user name */
  long ut_time;			/* login/out time */
};
