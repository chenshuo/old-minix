/* Port i/o functions using /dev/port.
 * Callers now ought to check the return values.
 * Calling either of these functions consumes a file descriptor.
 */

#include <lib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#define P_READ 1
#define P_WRITE 2

PRIVATE int portfd = -1;
PRIVATE int mode_opened = 0;

int port_in(port, valuep)
unsigned port;
unsigned *valuep;
{
  unsigned char chvalue;

  if ( !( mode_opened & P_READ )) {
     if ( mode_opened & P_WRITE ) {
        close(portfd);
        portfd = open("/dev/port", O_RDWR);
      }
      else {
        portfd = open("/dev/port", O_RDONLY);
      }
      mode_opened |= P_READ ;
    }

  if (portfd < 0 ||
      lseek(portfd, (long) port, 0) < 0 ||
      read(portfd, (char *) &chvalue, (size_t) 1) != 1)
	return(*valuep = -1);
  return(*valuep = chvalue);
}

int port_out(port, value)
unsigned port;
unsigned value;
{
  unsigned char chvalue;

  chvalue = value;
  if ( !( mode_opened & P_WRITE )) {
     if ( mode_opened & P_READ ) {
        close(portfd);
        portfd = open("/dev/port", O_RDWR);
      }
      else {
        portfd = open("/dev/port", O_WRONLY);
      }
      mode_opened |= P_WRITE ;
    }

  if (portfd < 0 || lseek(portfd, (long) port, 0) < 0 ||
      write(portfd, (char *) &chvalue, (size_t) 1) != 1)
	return(-1);
  return(chvalue);
}
