/* mv - move files		Author: Adri Koppes 
 *
 * 4/25/87 - J. Paradis		Bug fixes for directory handling
 */

#include "signal.h"
#include "stat.h"

int     error = 0;
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
    if (error) exit (1);
    exit(0);
}

move (old, new)
char   *old,
       *new;
{
    int     retval;
    char    name[64];

    /* It's too dangerous to fool with "." or ".." ! */
    if((strcmp(old, ".") == 0) || (strcmp(old, "..") == 0)) {
	cant(old);
    }

    /* Don't move a file to itself. */
    if (stat(old, &st)==0 && stat(new, &st1)==0 && st.st_dev == st1.st_dev &&
        st.st_ino == st1.st_ino)
	cant(old);

    if (!stat (new, &st))
	if((st.st_mode & S_IFMT) != S_IFDIR)
	    unlink (new);
    else {
	char *p, *rindex();

	if ((strlen(old) + strlen(new) + 2) > 64) {
		cant(old);
		error++;
		return;
	}
	strcpy(name, new);
	strcat(name, "/");
	p = rindex(old, '/');
	strcat(name, p ? p : old);
	new = name;
    }
    stat (old, &st);
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
	int	i;
	char	parent[64], dotdot[64];

	strcpy(parent, new);

	/* Determine the name for the parent of
	** the new name by counting back until we
	** hit a '/' or the begining of the string
	*/

	for(i = (strlen(parent) - 1); i > 0; i--) {
	    if(parent[i] == '/') break;
	}

	/* If there are no slashes, then the name is
	** in the current directory, so its parent
	** is ".".  Otherwise, the parent is the name
	** up to the last slash.
	*/
	if(i == 0) {
		strcpy(parent, ".");
	}
	else {
		/* null-terminate name at last slash */
		parent[i] = '\0';
	}

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


