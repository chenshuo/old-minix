/* perror(s) print the current error message. */

#include <errno.h>
extern int errno;
char *sys_errlist[] = {
        "Error 0",
        "Not owner",
        "No such file or directory",
        "No such process",
        "Interrupted system call",
        "I/O error",
        "No such device or address",
        "Arg list too long",
        "Exec format error",
        "Bad file number",
        "No children",
        "No more processes",
        "Not enough core",
        "Permission denied",
        "Bad address",
        "Block device required",
        "Mount device busy",
        "File exists",
        "Cross-device link",
        "No such device",
        "Not a directory",
        "Is a directory",
        "Invalid argument",
        "File table overflow",
        "Too many open files",
        "Not a typewriter",
        "Text file busy",
        "File too large",
        "No space left on device",
        "Illegal seek",
        "Read-only file system",
        "Too many links",
        "Broken pipe",
        "Math argument",
        "Result too large"
};

int sys_nerr = sizeof(sys_errlist)/sizeof(char *);

perror(s)
char *s;
{
  if (errno < 0 || errno >= sizeof(sys_errlist)/sizeof(char *)) {
	write(2, "Invalid errno\n", 14);
  } else {
	write(2, s, slen(s));
	write(2, ": ", 2);
	write(2, sys_errlist[errno], slen(sys_errlist[errno]));
	write(2, "\n", 1);
  }
}

static int slen(s)
char *s;
{
  int k = 0;

  while (*s++) k++;
  return(k);
}
