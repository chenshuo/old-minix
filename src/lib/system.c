#include	<stdio.h>

#ifndef NIL
#define NIL 0
#endif


system( cmd )
char *cmd;
{
    int retstat, procid, waitstat;

    if( (procid = fork()) == 0 )
    {
        execl( "/bin/sh", "sh", "-c", cmd, NIL );
        exit( 127 );
    }

    while( (waitstat = wait(&retstat)) != procid && waitstat != -1 )
        ;
    if (waitstat == -1)
        retstat = -1;

    return( retstat );
}
