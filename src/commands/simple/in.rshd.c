/*
in.rshd.c
*/

/*
	main channel:

	back channel\0
	remuser\0
	locuser\0
	command\0
	data

	back channel:
	signal\0

*/

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <minix/config.h>
#include <sys/ioctl.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/netdb.h>
#include <net/gen/socket.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/hton.h>
#include <net/netlib.h>

char cmdbuf[_POSIX_ARG_MAX+1], locuser[16], remuser[16];
extern char **environ;
char username[20]="USER=";
char homedir[64]="HOME=";
char shell[64]="SHELL=";
char *envinit[]= {homedir, shell, username, 0};
char *prog_name;
char buffer[PIPE_BUF];
pid_t pidlist[3], *pidp= pidlist;

#if __STDC__
#define PROTO(func, args) func args
#else
#define PROTO(func, args) func ()
#endif

PROTO (int main, (int argc, char *argv[]));
PROTO (void death, (int sig));
PROTO (void getstr, (char*buf, int cnt, char *err));

int main(argc, argv)
int argc;
char *argv[];
{
	int result, result1;
	nwio_tcpconf_t tcpconf, err_tcpconf;
	nwio_tcpcl_t tcpconnopt;
	nwio_tcpatt_t tcpattachopt;
	tcpport_t tcpport;
	tcpport_t err_port;
	int err_fd, pid, pid1, pds[2];
#if USEATTACH
	int err2_fd;
#endif
	struct hostent *hostent;
	struct passwd *pwent;
	char *cp, *buff_ptr;
	char sig;

	prog_name= argv[0];
	if (argc != 1)
	{
		fprintf(stderr, "%s: wrong number of arguments (%d)\n",
			argv[0], argc);
		exit(1);
	}

	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTERM, death);
	*pidp++ = getpid();

	result= ioctl (0, NWIOGTCPCONF, &tcpconf);
	if (result<0)
	{
		fprintf(stderr, "%s: ioctl(NWIOTCPCONF)= %d : %s\n", errno,
			strerror(errno));
		exit(1);
	}

	tcpport= ntohs(tcpconf.nwtc_remport);
	if (tcpport >= TCPPORT_RESERVED || tcpport < TCPPORT_RESERVED/2)
	{
		printf("\1%s: unprotected port (%d)\n", argv[0], tcpport);
		exit(1);
	}
	alarm(60);
	err_port= 0;
	for (;;)
	{
		char c;
		result= read(0, &c, 1);
		if (result <0)
		{
			fprintf(stderr, "%s: read= %d : %s\n", errno,
				strerror(errno));
		}
		if (result<1)
			exit(1);
		if (c == 0)
			break;
		err_port= err_port*10 + c - '0';
	}
	alarm(0);
	if (err_port != 0)
	{
		int lport;

		for (lport= TCPPORT_RESERVED -1; lport >= TCPPORT_RESERVED/2;
			lport--)
		{
			err_fd= open ("/dev/tcp", O_RDWR);
			if (err_fd<0)
			{
				fprintf(stderr, "%s: open= %d : %s\n", errno,
					strerror(errno));
				exit(1);
			}
			err_tcpconf.nwtc_flags= NWTC_LP_SET | NWTC_SET_RA |
				NWTC_SET_RP;
			err_tcpconf.nwtc_locport= htons(lport);
			err_tcpconf.nwtc_remport= htons(err_port);
			err_tcpconf.nwtc_remaddr= tcpconf.nwtc_remaddr;

			result= ioctl (err_fd, NWIOSTCPCONF, &err_tcpconf);
			if (result<0)
			{
				if (errno == EADDRINUSE)
				{
					close(err_fd);
					continue;
				}
				fprintf(stderr, 
					"%s: ioctl(NWIOTCPCONF)= %d : %s\n",
					errno, strerror(errno));
				exit(1);
			}
			tcpconnopt.nwtcl_flags= 0;

			do
			{
				result= ioctl (err_fd, NWIOTCPCONN,
					&tcpconnopt);
				if (result<0 && errno == EAGAIN)
				{
					sleep(2);
				}
			} while (result <0 && errno == EAGAIN);
			if (result <0  && errno != EADDRINUSE)
			{
				fprintf(stderr, "%s: ioctl(NWIOTCPCONN)= %d : %s\n",
					errno, strerror(errno));
				exit(1);
			}
			if (result>=0)
				break;
		}
		if (lport<TCPPORT_RESERVED/2)
		{
			printf("\1can't get stderr port\n");
			exit(1);
		}
#if USEATTACH
		err2_fd= open ("/dev/tcp", O_RDWR);
		if (err2_fd<0)
		{
			fprintf(stderr, "%s: open= %d : %s\n", errno,
				strerror(errno));
			exit(1);
		}
		result= ioctl (err2_fd, NWIOSTCPCONF, &err_tcpconf);
		if (result<0)
		{
			fprintf(stderr, "%s: ioctl(NWIOTCPCONF)= %d : %s\n",
				errno, strerror(errno));
			exit(1);
		}
		tcpattachopt.nwta_flags= 0;
		result= ioctl (err2_fd, NWIOTCPATTACH, &tcpattachopt);
		if (result<0)
		{
			fprintf(stderr, "%s: ioctl(NWIOTCPATTACH)= %d : %s\n",
				errno, strerror(errno));
			exit(1);
		}
#endif
	}
	hostent= gethostbyaddr((char *)&tcpconf.nwtc_remaddr,
		sizeof(tcpconf.nwtc_remaddr), AF_INET);
	if (!hostent)
	{
		printf("\1Host name for your adress (%s) unknown\n",
			inet_ntoa(tcpconf.nwtc_remaddr));
		exit(1);
	}
	getstr(remuser, sizeof(remuser), "remuser");
	getstr(locuser, sizeof(locuser), "locuser");
	getstr(cmdbuf, sizeof(cmdbuf), "cmdbuf");
	setpwent();
	pwent= getpwnam(locuser);
	if (!pwent)
	{
		printf("\1Login incorrect.\n");
		exit(1);
	}
	endpwent();
	if (chdir(pwent->pw_dir) < 0)
	{
		chdir("/");
	}
	if (ruserok(hostent->h_name, !pwent->pw_uid, remuser, locuser) < 0)
	{
		printf("\1Permission denied.\n");
		exit(1);
	}
	if (err_port)
	{
		pid= fork();
		if (pid<0)
		{
			if (errno != EAGAIN)
			{
				fprintf(stderr, "%s: fork()= %d : %s\n",
					prog_name, errno, strerror(errno));
			}
			printf("\1Try again.\n");
			exit(1);
		}
		if (pid)
		{
			*pidp++ = pid;
			close(0);	/* stdin */
			close(1);	/* stdout */
#if USEATTACH
			close(err_fd);	/* stderr for shell */
#endif
			dup2(2,0);
			dup2(2,1);
			for (;;)
			{
#if !USEATTACH
				if (read(err_fd, &sig, 1) <= 0) exit(0);
#else
				if (read(err2_fd, &sig, 1) <= 0) exit(0);
#endif
				death(sig);
			}
		}
#if USEATTACH
		close(err2_fd);	/* signal channel for parent */
#endif
		result= pipe(pds);
		if (result<0)
		{
			printf("\1Can't make pipe\n");
			death(SIGTERM);
			exit(1);
		}
		pid1= fork();
		if (pid1<0)
		{
			if (errno != EAGAIN)
			{
				fprintf(stderr, "%s: fork()= %d : %s\n",
					prog_name, errno, strerror(errno));
			}
			printf("\1Try again.\n");
			death(SIGTERM);
			exit(1);
		}
		if (pid1)
		{
			*pidp++ = pid1;
			close(pds[1]);	/* write side of pipe */
			for (;;)
			{
				result= read(pds[0], buffer, sizeof(buffer));
				if (result<=0)
				{
					death(SIGTERM);
					exit(0);
				}
				buff_ptr= buffer;
				while (result>0)
				{
					result1= write (err_fd, buff_ptr,
						result);
					if (result1 <= 0)
					{
						fprintf(stderr, "%s: write()= %d : %s\n",
							prog_name, errno,
							strerror(errno));
						death(SIGTERM);
						exit(1);
					}
					result -= result1;
				}
			}
		}
		close(err_fd);	/* file descriptor for error channel */
		close (pds[0]);	/* read side of pipe */
		dup2(pds[1], 2);
		close (pds[1]);	/* write side of pipe */
	}
	if (*pwent->pw_shell == '\0')
		pwent->pw_shell= "/bin/sh";
	setgid(pwent->pw_gid);
	setuid(pwent->pw_uid);
	environ= envinit;
	strncat(homedir, pwent->pw_dir, sizeof(homedir)-6);
	strncat(shell, pwent->pw_shell, sizeof(shell)-7);
	strncat(username, pwent->pw_name, sizeof(username)-6);
	cp= strrchr(pwent->pw_shell, '/');
	if (cp)
		cp++;
	else
		cp= pwent->pw_shell;

	if (!err_port)
		dup2(1, 2);
	write(1, "\0", 1);

	execl(pwent->pw_shell, cp, "-c", cmdbuf, 0);
	close(2);
	open("/dev/tty", O_RDWR);
	fprintf(stderr, "%s: execl(%s, %s, .., %s)= %d : %s\n", prog_name,
		pwent->pw_shell, cp, cmdbuf, errno, strerror(errno));
	death(SIGTERM);
	exit(1);
}

void death(sig)
{
	/* We are to die, tell our neighbours. */
	signal(sig, SIG_IGN);
	while (pidp > pidlist) kill(*--pidp, sig);
}

void getstr(buf, cnt, err)
char *buf;
int cnt;
char *err;
{
	char c;

	do
	{
		if (read(0, &c, 1) != 1)
			exit(1);
		*buf++ = c;
		if (--cnt == 0)
		{
			printf("\1%s too long", err);
			exit(1);
		}
	} while (c != 0);
}
