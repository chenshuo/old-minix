#include <stdio.h>

system(cmd)
char *cmd;
{
    int retstat, procid, waitstat;

    if ( (procid = fork()) == 0) {
	/* Child does an exec of the command. */
        execl( "/bin/sh", "sh", "-c", cmd, 0 );
        exit( 127 );
    }

    /* Check to see if fork failed. */
    if (procid < 0) exit(1);

    while ( (waitstat = wait(&retstat)) != procid && waitstat != -1 ) ;
    if (waitstat == -1) retstat = -1;
    return(retstat);
}
