#include <lib.h>
#define mknod4	_mknod4
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

PUBLIC int mknod4(name, mode, dev, size)
_CONST char *name;
Mode_t mode;
Dev_t dev;
long size;
{
  message m;

  m.m1_i1 = strlen(name) + 1;
  m.m1_i2 = mode;
  m.m1_i3 = dev;
  m.m1_p1 = (char *) name;
  if (sizeof(char *) == 2 && size > 0xFFFF) size = 0;
  m.m1_p2 = (char *) ((int) size);
  return(_syscall(FS, MKNOD, &m));
}
