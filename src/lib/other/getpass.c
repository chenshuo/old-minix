/*
 * getpass - ask for a password
 */
/* $Header: getpass.c,v 1.2 90/01/22 11:43:26 eck Exp $ */

#include	<sys/types.h>
#include	<signal.h>
#include	<string.h>
#include	<sgtty.h>
#include	<fcntl.h>

_PROTOTYPE(char *getpass, (_CONST char *prompt ));

#ifdef _ANSI
int _open(const char *path, int flags);
ssize_t _write(int d, const char *buf, size_t nbytes);
ssize_t _read(int d, char *buf, size_t nbytes);
int _close(int d);
int _stty(int, struct sgttyb *);
int _gtty(int, struct sgttyb *);
void (*savesig)(int);
#else
void (*savesig)();
#endif

char *
getpass(prompt)
_CONST char *prompt;
{
	int i = 0;
	struct sgttyb tty, ttysave;
	static char pwdbuf[9];
	int fd;

	if ((fd = _open("/dev/tty", O_RDONLY)) < 0) fd = 0;
	savesig = signal(SIGINT, SIG_IGN);
	_write(2, prompt, strlen(prompt));
	_gtty(fd, &tty);
	ttysave = tty;
	tty.sg_flags &= ~ECHO;
	_stty(fd, &tty);
	i = _read(fd, pwdbuf, 9);
	while (pwdbuf[i - 1] != '\n')
		_read(fd, &pwdbuf[i - 1], 1);
	pwdbuf[i - 1] = '\0';
	_stty(fd, &ttysave);
	_write(2, "\n", 1);
	if (fd != 0) _close(fd);
	signal(SIGINT, savesig);
	return(pwdbuf);
}
