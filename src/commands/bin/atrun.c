/* atrun - perform the work 'at' has squirreled away	Author: Jan Looyen */

/*-------------------------------------------------------------------------*
 *	atrun scans directory DIRNAME for 'at' jobs to be executed         *
 *	finished jobs have been moved to directory DIRNAME/past		   *
 *-------------------------------------------------------------------------*/
#define			DIRNAME			"/usr/spool/at
#include		<stdio.h>
#include 		<sys/types.h>
#include		<sys/dir.h>
#include		<time.h>

main()
{
	int fd, nr;
	char realtime[15], procname[35], procpast[35];
	struct direct dirbuf;
	struct tm *p, *localtime();
	long clock;

/*-------------------------------------------------------------------------*
 *	compute real time,  move 'at' jobs whose filenames < real time to  *
 *	DIRNAME/past and start a sh for each job			   * 
 *-------------------------------------------------------------------------*/
	time(&clock);
	p = localtime(&clock);
	sprintf(realtime, "%02d.%03d.%02d%02d.00",
		p->tm_year%100, p->tm_yday, p->tm_hour, p->tm_min);
	if ((fd = open(DIRNAME", 0)) > 0)
	    while (read(fd, (char *)&dirbuf, sizeof(dirbuf)) > 0)
	        if (dirbuf.d_ino > 0 &&
		    dirbuf.d_name[0] != '.' &&
		    dirbuf.d_name[0] != 'p' &&
		    strncmp(dirbuf.d_name, realtime, 11) < 0 ) {

		    sprintf(procname, DIRNAME/%.14s", dirbuf.d_name);
		    sprintf(procpast, DIRNAME/past/%.14s", dirbuf.d_name);

		    if (fork() == 0)              	 /* code for child */
	       		if (link(procname, procpast) == 0){    /* link ok? */
		            unlink(procname);            
		            execl("/bin/sh", "sh", procpast, 0);
		            fprintf(stderr,"proc %s can't start\n", procpast);
		            exit(1);
		        }
		}

}
