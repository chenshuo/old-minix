#include <signal.h>

abort()
{
  return(kill(getpid(), SIGIOT));
}
