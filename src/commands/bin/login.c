/* login - log into the system		Author: Patrick van Kleef */

/* Peter S. Housel   Jan. 1988
 *  - Set up $USER, $HOME and $TERM.
 *  - Set signals to SIG_DFL.
 *
 * Terrence W. Holm   June 1988
 *  - Allow a username as an optional argument.
 *  - Time out if a password is not typed within 30 seconds.
 *  - Perform a dummy delay after a bad username is entered.
 *  - Don't allow a login if "/etc/nologin" exists.
 *  - Cause a failure on bad "pw_shell" fields.
 *  - Record the login in "/usr/adm/wtmp".
 */

#include <signal.h>
#include <sgtty.h>
#include <pwd.h>
#include <sys/stat.h>

#define  NULL   (char *) 0
#define WTMPSIZE           8
#define DIGIT 3

extern char *crypt();
extern struct passwd *getpwnam();
extern long time();
extern long lseek();
int Time_out();

int  time_out;

char user[ 32 ];
char logname[ 35 ];
char home[ 64 ];
char shell[ 64 ];

char *env[] = {
	      user,
	      logname,
	      home,
	      shell,
	      "TERM=minix",
	      NULL
};
char wtmpfile[] = {"/usr/adm/wtmp"};
char ttyname[] = {"tty?"};


main( argc, argv ) 
int   argc;
char *argv[];
{
	char    name[30];
	char	password[30];
	int     bad;
	int	n;
	int 	ttynr;
	struct  sgttyb args;
	struct  passwd *pwd;
	struct stat statbuf;
	char   *sh = "/bin/sh";

	/* Reset some of the line parameters in case they have been mashed */
	if ( ioctl(0, TIOCGETP, &args) < 0 ) exit( 1 );

	args.sg_kill  = '@';
	args.sg_erase = '\b';
	args.sg_flags = (args.sg_flags & 01700) | XTABS | CRMOD | ECHO;
	ioctl (0, TIOCSETP, &args);

	/* Get login name and passwd. */
	for (;;) {
		bad = 0;

		if ( argc > 1 ) {
		    strcpy( name, argv[1] );
		    argc = 1;
		} else {
		    do {
			write(1,"login: ",7);
			n = read (0, name, 30);
		    } while (n < 2);
		    name[n - 1] = 0;
		}

		/* Look up login/passwd. */
		if ((pwd = getpwnam (name)) == 0) bad++;

		/* If login name wrong or password exists, ask for pw. */
		if (bad || strlen (pwd->pw_passwd) != 0) {
			args.sg_flags &= ~ECHO;
			ioctl (0, TIOCSETP, &args);
			write(1,"Password: ",10);

			time_out = 0;
			signal( SIGALRM, Time_out );
			alarm( 30 );

			n = read (0, password, 30);

			alarm( 0 );
			if ( time_out ) {
				n = 1;
				bad++;
			}

			password[n - 1] = 0;
			write(1,"\n",1);
			args.sg_flags |= ECHO;
			ioctl (0, TIOCSETP, &args);

			if (bad && crypt(password, "aaaa") ||
			 strcmp (pwd->pw_passwd, crypt(password, pwd->pw_passwd))) {
				write (1,"Login incorrect\n",16);
				continue;
			}
		}

		/*  Check if the system is going down  */
		if ( access( "/etc/nologin", 0 ) == 0  &&  
						strcmp( name, "root" ) != 0 ) {
			write( 1, "System going down\n\n", 19 );
			continue;
		}


		/* Look up /dev/tty number. */
		fstat(0, &statbuf);
		ttynr = statbuf.st_rdev & 0377;
		ttyname[DIGIT] = '0' + ttynr;

		/*  Write login record to /usr/adm/wtmp  */
		wtmp(ttyname, name);

		setgid( pwd->pw_gid );
		setuid( pwd->pw_uid );

		if (pwd->pw_shell[0]) sh = pwd->pw_shell;

		/*  Set the environment  */
		strcpy( user,  "USER=" );
		strcat( user,   name );
		strcpy( logname, "LOGNAME=" );
		strcat( logname, name );
		strcpy( home,  "HOME=" );
		strcat( home,  pwd->pw_dir );
		strcpy( shell, "SHELL=" );
		strcat( shell, sh );

		chdir( pwd->pw_dir );

		/* Reset signals to default values. */

		for ( n = 1;  n <= NR_SIGS;  ++n )
		    signal( n, SIG_DFL );

		execle( sh, "-", NULL, env );
		write(1,"exec failure\n",13);
		exit(1);
	}
}



Time_out( )
{
  time_out = 1;
}

wtmp(tty, name)
{
/* Make an entry in /usr/adm/wtmp. */

  int i, fd;
  long t, time();
  char ttybuff[WTMPSIZE], namebuff[WTMPSIZE];

  fd = open(wtmpfile, 2);
  if (fd < 0) return;		/* if wtmp does not exist, no accounting */
  lseek(fd, 0L, 2);		/* append to file */

  for (i = 0; i < WTMPSIZE; i++) {
	ttybuff[i] = 0;
	namebuff[i] = 0;
  }
  strncpy(ttybuff, tty, 8);
  strncpy(namebuff, name, 8);
  time(&t);
  write(fd, ttybuff, WTMPSIZE);
  write(fd, namebuff, WTMPSIZE);
  write(fd, &t, sizeof(t));
  close(fd);
}
