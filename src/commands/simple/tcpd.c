/*
tcpd.c
*/

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <minix/config.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/netdb.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/hton.h>

#if __STDC__
#define PROTOTYPE(func,args) func args
#else
#define PROTOTYPE(func,args) func()
#endif

PROTOTYPE (int main, (int argc, char *argv[]) );

int main(argc, argv)
int argc;
char *argv[];
{
	tcpport_t port;
	struct nwio_tcpcl tcplistenopt;
	struct nwio_tcpconf tcpconf;
	struct servent *servent;
	int result, child;
	int tcp_fd, tmp, count;
	char *arg0, *program, **args;
	int debug= 0;

	arg0= argv[0];
	if (argc > 1 && strcmp(argv[1], "-d") == 0)
	{
		debug= 1;
		argc--;
		argv++;
	}

	if (argc < 3)
	{
		fprintf(stderr,
			"Usage: %s [-d] port program [arg ...]\n", arg0);
		exit(1);
	}

	servent= getservbyname (argv[1], "tcp");
	if (!servent)
	{
		port= htons(strtol (argv[1], (char **)0, 0));
		if (!port)
		{
			fprintf(stderr, "%s: unknown port (%s)\n",
				arg0, argv[1]);
			exit(1);
		}
	}
	else
		port= (tcpport_t)(servent->s_port);

	if (!port)
	{
		fprintf(stderr, "wrong port number (==0)\n");
		exit(1);
	}

	if (debug)
	{
		fprintf (stderr, "%s: listening to port: %u\n", arg0, port);
	}

	program= argv[2];
	args= argv+2;

	for (;;)
	{
		tcp_fd= open("/dev/tcp", O_RDWR);
		if (tcp_fd<0)
		{
			perror("unable to open /dev/tcp");
			exit(1);
		}

		tcpconf.nwtc_flags= NWTC_LP_SET | NWTC_UNSET_RA | NWTC_UNSET_RP;
		tcpconf.nwtc_locport= port;

		result= ioctl (tcp_fd, NWIOSTCPCONF, &tcpconf);
		if (result<0)
		{
			perror ("unable to NWIOSTCPCONF");
			exit(1);
		}

		tcplistenopt.nwtcl_flags= 0;

		do
		{
			result= ioctl (tcp_fd, NWIOTCPLISTEN, &tcplistenopt);
			if (result == -1 && errno == EAGAIN)
			{
				if (debug)
				{
					fprintf(stderr,
					"%s: got EAGAIN sleeping 1 second\n",
					arg0);
				}
				sleep(1);
			}
		} while (result == -1 && errno == EAGAIN);

		if (result<0)
		{
			perror ("unable to NWIOTCPLISTEN");
			exit(1);
		}

		if (debug)
		{
			result= ioctl (tcp_fd, NWIOGTCPCONF, &tcpconf);
			if (result<0)
			{
				perror ("unable to NWIOGTCPCONF");
				exit(1);
			}
			fprintf(stderr, "connection accepted from %s, %u",
				inet_ntoa(tcpconf.nwtc_remaddr),
				ntohs(tcpconf.nwtc_remport));
			fprintf(stderr," for %s, %u (%s)\n",
				inet_ntoa(tcpconf.nwtc_locaddr),
				ntohs(tcpconf.nwtc_locport), argv[1]);
		}

		child= fork();
		switch (child)
		{
		case -1:
			perror("fork");
			break;
		case 0:
			if (!(child= fork()))
			{
				dup2(tcp_fd, 0);
				dup2(tcp_fd, 1);
				close(tcp_fd);
				execv(program, args);
				printf("Unable to exec %s\n", program);
				fflush(stdout);
				_exit(1);
			}
			if (child<0) perror("fork");
			exit(0);
			break;
		default:
			close(tcp_fd);
			wait(&child);
			break;
		}
	}
}
