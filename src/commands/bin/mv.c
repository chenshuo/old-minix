/* mv - move files		Author: Adri Koppes 
 *
 * 4/25/87 - J. Paradis		Bug fixes for directory handling
 * 3/15/88 - P. Housel		More directory bug fixes
 */

#include <signal.h>
#include <sys/stat.h>

extern char *rindex();

struct stat st, st1;

main (argc, argv)
int     argc;
char  **argv;
{
    char   *destdir;

    if (argc < 3) {
	std_err ("Usage: mv file1 file2 or mv dir1 dir2 or mv file1 file2 ... dir\n");
	exit (1);
    }
    if (argc == 3) {
	if (stat (argv[1], &st)) {
	    std_err ("mv: ");
	    std_err (argv[1]);
	    std_err (" doesn't exist\n");
	    exit (1);
	}

	move (argv[1], argv[2]);
    }
    else {
	destdir = argv[--argc];
	if (stat (destdir, &st)) {
	    std_err ("mv: target directory ");
	    std_err (destdir);
	    std_err (" doesn't exist\n");
	    exit(1);
	}
	if ((st.st_mode & S_IFMT) != S_IFDIR) {
	    std_err ("mv: target ");
	    std_err (destdir);
	    std_err (" not a directory\n");
	    exit (1);
	}
	while (--argc)
	    move (*++argv, destdir);
    }
    exit(0);
}

move (old, new)
char   *old,
       *new;
{
    int     retval;
    char    name[64];
    char    parent[64];
    char    *oldbase;
    int     i;

    if((oldbase = rindex(old, '/')) == 0)
	oldbase = old;
    else
	++oldbase;

    /* It's too dangerous to fool with "." or ".." ! */
    if((strcmp(oldbase, ".") == 0) || (strcmp(oldbase, "..") == 0)) {
	cant(old);
    }

    /* Don't move a file to itself. */
    if (stat(old, &st)==0 && stat(new, &st1)==0 && st.st_dev == st1.st_dev &&
        st.st_ino == st1.st_ino)
	cant(old);

    /* If source is not writeable, don't move it. */
    if (access(old, 2) != 0) cant(old);

    if (stat (new, &st1) == 0)
	if((st1.st_mode & S_IFMT) != S_IFDIR)
	    unlink (new);
	else {
	    if ((strlen(oldbase) + strlen(new) + 2) > 64) {
		cant(old);
	    }
	    strcpy(name, new);
	    strcat(name, "/");
	    strcat(name, oldbase);
	    new = name;

	    /* do the 'move-to-itself' check again for the new name */
	    if (stat(new, &st1)==0 && st.st_dev == st1.st_dev
		&& st.st_ino == st1.st_ino)
	        cant(old);
	}

    strcpy(parent, new);

    for(i = (strlen(parent) - 1); i > 0; i--) {
	if(parent[i] == '/') break;
    }

    if(i == 0) {
	strcpy(parent, ".");
    }
    else {
	/* null-terminate name at last slash */
	parent[i] = '\0';
    }

    /* prevent moving a directory into its own subdirectory */
    if((st.st_mode & S_IFMT) == S_IFDIR) {
	char lower[128];
        short int prevdev = -1;
	unsigned short previno;

	strcpy(lower, parent);
	while(1) {
	    if(stat(lower, &st1) || (st1.st_dev == st.st_dev
				     && st1.st_ino == st.st_ino))
		cant(old);

	    /* stop at root */
	    if(st1.st_dev == prevdev && st1.st_ino == previno)
		break;
	    prevdev = st1.st_dev;
	    previno = st1.st_ino;
	    strcat(lower, "/..");
	}
    }

    if (link (old, new))
	if ((st.st_mode & S_IFMT) != S_IFDIR) {
	    switch (fork ()) {
		case 0: 
		    setgid (getgid ());
		    setuid (getuid ());
		    execl ("/bin/cp", "cp", old, new, (char *) 0);
		    cant(old);
		case -1: 
		    std_err ("mv: can't fork\n");
		    exit (1);
		default:
		    wait (&retval);
		    if (retval)
			cant(old);
	    }
	} else
	    cant(old);

    /* If this was a directory that we moved, then we have
    ** to update its ".." entry (in case it was moved some-
    ** where else in the tree...)
    */
    if ((st.st_mode & S_IFMT) == S_IFDIR) {
	char	dotdot[64];

	/* Unlink the ".." entry */
	strcpy(dotdot, new);
	strcat(dotdot, "/..");
	unlink(dotdot);

	/* Now link it to its parent */
	link(parent, dotdot);
    }

    utime (new, &st.st_atime);
    unlink(old);
}

cant(name)
char *name;
{
	std_err("mv: can't move ");
	std_err (name);
	std_err ("\n");
	exit (1);
}


