/* cron - clock daemon			Author: S.R. Sampson */

/*	Cron is the clock daemon.  It is typically started up from the
 *	/etc/rc file by the line:
 *		/usr/bin/cron
 *	Cron automatically puts itself in the background, so no & is needed.
 *	If cron is used, it runs all day, spending most of its time asleep.
 *	Once a minute it wakes up and examines /usr/lib/crontab to see if there
 *	are any commands to be executed.  The format of this table is the same
 *	as in UNIX, except that % is not allowed to indicate 'new line.'
 *	
 *	Each crontab entry has six fields:
 *	   minute    hour  day-of-the-month  month  day-of-the-week  command
 *	Each entry is checked in turn, and any entry matching the current time
 *	is executed.  The entry * matches anything.  Some examples:
 *
 *   min hr dat mo day   command
 *    *  *   *  *   *    /usr/bin/date >/dev/tty0   #print date every minute
 *    0  *   *  *   *    /usr/bin/date >/dev/tty0   #print date on the hour
 *   30  4   *  *  1-5   /bin/backup /dev/fd1       #do backup Mon-Fri at 0430
 *   30 19   *  *  1,3,5 /etc/backup /dev/fd1       #Mon, Wed, Fri at 1930
 *    0  9  25 12   *    /usr/bin/sing >/dev/tty0   #Xmas morning at 0900 only
 */

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define CRONTAB "/usr/lib/crontab"
#define MAXLINE	132
#define SIZE	64
#define	TRUE	1
#define	FALSE	0

extern	int	errno;

FILE *fd;
int eof;
char min[SIZE], hour[SIZE], day[SIZE], month[SIZE], wday[SIZE], command[SIZE];
char *tokv[] = { min, hour, day, month, wday };

main()
{
	int status,  wakeup(), no_action();

	/* Put cron in the background by forking and having the parent exit. */
	status=fork();

	if ( status == (-1) ) {	/* Error - can't fork */
		fprintf(stderr,"\007Cron: Error - can't fork!\n");
		exit(-1); 
	}

	if (status > 0) exit(0);	/* parent exits */

	signal(SIGINT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	close(0); close(1); close(2);	/* close all I/O paths */

	for (;;)  {
		signal(SIGALRM, wakeup);
		alarm(59);
		pause();		/* check for work every minute */

		signal(SIGALRM, no_action);
		alarm(1);
		wait((int *)NULL);	/* clean up zombie children */
	}
}


int no_action()
{
  /* Do nothing.  Just return. */
}


wakeup()
{
	register struct tm *tm;
	long cur_time;
	extern struct tm *localtime();

	time(&cur_time);		/* get the current time */
	tm = localtime(&cur_time);	/* break it down */

	fd = fopen(CRONTAB, "r");
	eof = FALSE;

	while (!eof)  {
		if (getline() && match(min,tm->tm_min) &&
		   match(hour,tm->tm_hour) && match(day,tm->tm_mday) &&
		   match(month,tm->tm_mon) && match(wday,tm->tm_wday))  {

		   /*  Execute command in the shell. */
		   if (fork() == 0)  {
			execl("/bin/sh", "/bin/sh", "-c", command, 0);
			exit(1);
		   }

		}
	}
	fclose(fd);
}


/*
 *	A line consists of six fields.  The first five are:
 *
 *		minute:         0-59
 *		hour:           0-23
 *		day:            1-31
 *		month:          1-12
 *		weekday:        0-6 (Sunday = 0)
 *
 *	The fields are seperated by spaces or tabs, with the
 *	first field left justified (no leading spaces or tabs).
 *	See below for optional field syntax.
 *
 *	The last field is the command field.  This command will
 *	be executed by the shell just as if typed from a console.
 */

getline()
{
	register char *p;
	register int   i;
	char     buffer[MAXLINE];
	extern	 char *scanner();

	if (fgets(buffer, sizeof buffer, fd) == (char *)NULL)  {
		eof = TRUE;
		return(FALSE);
	}

	for (p = buffer, i = 0; i < 5; i++)  {
		if ((p = scanner(tokv[i], p)) == (char *)NULL)
			return(FALSE);
	}

	strcpy(command, p);     /* scoop the command */
	return(TRUE);
}


char *scanner(token, offset)
register char   *token;		/* target buffer to receive scanned token */
register char   *offset;	/* place holder into source buffer */
{
	while ((*offset != ' ') && (*offset != '\t') && *offset)
		*token++ = *offset++;

	/*
	 *      Check for possible error condition
	 */
         
	if (!*offset)
		return ((char *)NULL);

	*token = '\0';
        
	while ((*offset == ' ') || (*offset == '\t'))
		offset++;

	return (offset);
}


/*
 *	This routine will match the left string with the right number.
 *
 *	The string can contain the following syntax:
 *
 *	*		This will return TRUE for any number
 *	x,y [,z, ...]	This will return TRUE for any number given.
 *	x-y		This will return TRUE for any number within
 *			the range of x thru y.
 */

match(left, right)
register char   *left;
register int    right;
{
	register int	n;
	register char	c;

	n = 0;
	if (!strcmp(left, "*"))
		return(TRUE);

	while ((c = *left++) && (c >= '0') && (c <= '9'))
		n  =  (n * 10) + c - '0';

	switch (c)  {
		case '\0':
			return (right == n);

		case ',':
			if (right == n)
				return(TRUE);
			do {
			      n = 0;
			      while ((c = *left++) && (c >= '0') && (c <= '9'))
					n = (n * 10) + c - '0';

			      if (right == n)
					return(TRUE);
			} while (c == ',');
			return(FALSE);

		case '-':
			if (right < n)
				return(FALSE);

			n = 0;
			while ((c = *left++) && (c >= '0') && (c <= '9'))
				n = (n * 10) + c - '0';

			return(right <= n);
	}
}
