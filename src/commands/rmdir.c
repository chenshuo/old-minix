/* rmdir - remove a directory		Author: Adri Koppes
 * (modified by Paul Polderman)
 */

#include "../include/signal.h"
#include "../include/stat.h"

struct direct {
    unsigned short  d_ino;
    char    d_name[14];
};
int     error = 0;

main (argc, argv)
register int argc;
register char  **argv;
{
    if (argc < 2) {
	prints ("Usage: rmdir dir ...\n");
	exit (1);
    }
    signal (SIGHUP, SIG_IGN);
    signal (SIGINT, SIG_IGN);
    signal (SIGQUIT, SIG_IGN);
    signal (SIGTERM, SIG_IGN);
    while (--argc)
	remove (*++argv);
    if (error)
	exit (1);
}

extern char *rindex();

remove (dirname)
char   *dirname;
{
    struct direct   d;
    struct stat s,
                cwd;
    register int fd = 0, sl = 0;
    char    dots[128];
    register char *p;

    if (stat (dirname, &s)) {
	stderr2(dirname, " doesn't exist\n");
	error++;
	return;
    }
    if ((s.st_mode & S_IFMT) != S_IFDIR) {
	stderr2(dirname, " not a directory\n");
	error++;
	return;
    }
    if (p = rindex(dirname, '/'))
	p++;
    else
	p = dirname;

    if (strcmp(p, ".") == 0 || strcmp(p, "..") == 0) {
	stderr2(dirname, " will not remove \".\" or \"..\"\n");
	error++;
	return;
    }

    strcpy (dots, dirname);
    while (dirname[fd])
	if (dirname[fd++] == '/')
	    sl = fd;
    dots[sl] = '\0';
    if (access (dots, 2)) {
	stderr2(dirname, " no permission\n");
	error++;
	return;
    }
    stat ("", &cwd);
    if ((s.st_ino == cwd.st_ino) && (s.st_dev == cwd.st_dev)) {
	std_err ("rmdir: can't remove current directory\n");
	error++;
	return;
    }
    if ((fd = open (dirname, 0)) < 0) {
	stderr2("can't read ", dirname);
	std_err("\n");
	error++;
	return;
    }
    while (read (fd, (char *) & d, sizeof (struct direct)) == sizeof (struct direct))
    if (d.d_ino != 0)
	if (strcmp (d.d_name, ".") && strcmp (d.d_name, "..")) {
            stderr2(dirname, " not empty\n");
	    close(fd);
	    error++;
	    return;
	}
    close (fd);
    strcpy (dots, dirname);
    strcat (dots, "/..");
    patch_path(dots);
    for (p = dots; *p; p++)	/* find end of dots */
	;
    unlink(dots);		/* dirname/.. */
    *(p - 1) = '\0';
    unlink(dots);		/* dirname/. */
    *(p - 3) = '\0';
    if (unlink(dots)) {		/* dirname */
	stderr2("can't remove ", dots);
	std_err("\n");
	error++;
	return;
    }
}

stderr2(s1, s2)
char *s1, *s2;
{
	std_err("rmdir: ");
	std_err(s1);
	std_err(s2);
}

patch_path(dir)
char *dir;	/* pathname ending with "/.." */
{
	register char *p, *s;
	struct stat pst, st;

	if (stat(dir, &pst) < 0)
		return;
	p = dir;
	while (*p == '/') p++;
	while (1) {
		s = p;		/* remember start of new pathname part */
		while (*p && *p != '/')
			p++;	/* find next slash */
		if (*p == '\0')
			return;	/* if end of pathname, return */

		/* check if this part of pathname == the original pathname */
		*p = '\0';
		stat(dir, &st);
		if (st.st_ino == pst.st_ino && st.st_dev == pst.st_dev
			&& strcmp(s, "..") == 0)
				return;	
		/* if not, try next part */
		*p++ = '/';
		while (*p == '/') p++;
	}
}
