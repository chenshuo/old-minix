#include <lib.h>
#define mknod	_mknod
#define mknod4	_mknod4
#include <unistd.h>

PUBLIC int mknod(name, mode, dev)
_CONST char *name;
Mode_t mode;
Dev_t dev;
{
  return mknod4(name, mode, dev, (long) 0);
}
