/* chgrp - change group		Authors: P. van Kleef and D. Spinellis */

#include <grp.h>
#include <minix/type.h>
#include <sys/stat.h>
#include <stdio.h>

main (argc, argv)
int   argc;
char *argv[];
{
	int     i,
	status = 0;
	struct group  *grp, *getgrnam ();
	struct stat stbuf;

	if (argc < 3) {
		fprintf (stderr,"Usage: chgrp gid file ...\n");
		exit (1);
	}

	if ((grp = getgrnam (argv[1])) == 0) {
		fprintf (stderr,"Unknown group id: %s\n", argv[1]);
		exit (4);
	}

	for (i = 2; i < argc; i++) {
		if (stat (argv[i], &stbuf) < 0) {
			perror (argv[i]);
			status++;
		}
		else
			if (chown (argv[i], stbuf.st_uid, grp -> gr_gid) < 0) {
				fprintf (stderr,"%s: not changed\n", argv[i]);
				status++;
			}
	}
	exit (status);
}
