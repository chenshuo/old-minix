#include <lib.h>
#define mkfifo	_mkfifo
#define mknod4	_mknod4
#include <sys/stat.h>
#include <unistd.h>

PUBLIC int mkfifo(name, mode)
_CONST char *name;
Mode_t mode;
{
  return mknod4(name, mode | S_IFIFO, (Dev_t) 0, (long) 0);
}
