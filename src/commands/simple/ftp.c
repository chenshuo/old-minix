/* ftp.c by Michael Temari 06/21/92
 *
 * ftp          An ftp client program for use with TNET.
 *
 * Usage:       ftp [[host] [port]]
 *
 * Version:     0.10    06/21/92 (pre-release not yet completed)
 *              0.20    07/01/92
 *              0.30    01/15/96 (Minix 1.7.1 initial release)
 *
 * Author:      Michael Temari, <temari@ix.netcom.com>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <net/netlib.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/netdb.h>
#include <net/hton.h>
#include <errno.h>
#include <fcntl.h>
#include <sgtty.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>

#define RETR    0
#define STOR    1

#define	TYPE_A	0
#define	TYPE_I	1

static char comline[1024];
static char line[1024];
static char line2[1024];

static char host[128];
static ipaddr_t hostip;
static ipaddr_t myip;
#define NUMARGS 10
static int cmdargc;
static char *cmdargv[NUMARGS];

static int ftpcomm_fd;  /* ftp command connection */
static FILE *fpcommin;  /* ftp command connection input fopen */
static FILE *fpcommout; /* ftp command connection output fopen */
static int loggedin;    /* are we logged in? */
static int linkopen;    /* is the command link open? */
static int type, format, mode, structure, passive;
static int lpid;

_PROTOTYPE(static int init, (void));
_PROTOTYPE(char *dir, (char *path, int full));
_PROTOTYPE(static int asciisend, (int fd, int fdout));
_PROTOTYPE(static int binarysend, (int fd, int fdout));
_PROTOTYPE(static int sendfile, (int fd, int fdout));
_PROTOTYPE(static int asciirecv, (int fd, int fdin));
_PROTOTYPE(static int binaryrecv, (int fd, int fdin));
_PROTOTYPE(static int recvfile, (int fd, int fdin));
_PROTOTYPE(static int DOgetreply, (void));
_PROTOTYPE(static int DOcmdcheck, (void));
_PROTOTYPE(static int DOcommand, (char *ftpcommand));
_PROTOTYPE(static int DOopen, (void));
_PROTOTYPE(static int DOpass, (void));
_PROTOTYPE(static int DOuser, (void));
_PROTOTYPE(static int DOascii, (void));
_PROTOTYPE(static int DObinary, (void));
_PROTOTYPE(static int DOclose, (void));
_PROTOTYPE(static int DOquit, (void));
_PROTOTYPE(static int DOpwd, (void));
_PROTOTYPE(static int DOcd, (void));
_PROTOTYPE(static int DOlpwd, (void));
_PROTOTYPE(static int DOlcd, (void));
_PROTOTYPE(static int DOmkdir, (void));
_PROTOTYPE(static int DOrmdir, (void));
_PROTOTYPE(static int DOlmkdir, (void));
_PROTOTYPE(static int DOlrmdir, (void));
_PROTOTYPE(static int DOdelete, (void));
_PROTOTYPE(static int DOnoop, (void));
_PROTOTYPE(static int DOpassive, (void));
_PROTOTYPE(static int DOmdtm, (void));
_PROTOTYPE(static int DOsize, (void));
_PROTOTYPE(static int DOstat, (void));
_PROTOTYPE(static int DOsyst, (void));
_PROTOTYPE(static int DOremotehelp, (void));
_PROTOTYPE(static int DOdata, (char *datacom, int direction, int fd));
_PROTOTYPE(static int DOlist, (void));
_PROTOTYPE(static int DOnlst, (void));
_PROTOTYPE(static int DOretr, (void));
_PROTOTYPE(static int DOMretr, (void));
_PROTOTYPE(static int DOappe, (void));
_PROTOTYPE(static int DOstor, (void));
_PROTOTYPE(static int DOstou, (void));
_PROTOTYPE(static int DOMstor, (void));
_PROTOTYPE(static int DOquote, (void));
_PROTOTYPE(static int DOhelp, (void));
_PROTOTYPE(static int DOllist, (void));
_PROTOTYPE(static int DOlnlst, (void));
_PROTOTYPE(static int DOshell, (void));
_PROTOTYPE(int makeargs, (void));
_PROTOTYPE(void dodir, (char *path, int full));
_PROTOTYPE(static void scrpause, (void));
_PROTOTYPE(static int readline, (char *prompt, char *buff));
_PROTOTYPE(int main, (int argc, char *argv[]));

_PROTOTYPE(void donothing, (int sig));

void donothing(sig)
int sig;
{
}

static int init()
{
   linkopen = 0;
   loggedin = 0;
   type = TYPE_A;
   format = 0;
   mode = 0;
   structure = 0;
   passive = 0;
}

char *dir(path, full)
char *path;
int full;
{
char cmd[128];
static char name[32];

   tmpnam(name);

   if(full)
	sprintf(cmd, "ls -lg %s > %s", path, name);
   else
	sprintf(cmd, "ls %s > %s", path, name);

   system(cmd);

   return(name);
}

static char buffer[8192];

static int asciisend(fd, fdout)
int fd;
int fdout;
{
int s, len;
char *p, *pp;
long total=0L;

   printf("Sent ");
   fflush(stdout);

   while((s = read(fd, buffer, sizeof(buffer))) > 0) {
	total += (long)s;
	p = buffer;
	while(s > 0) {
		if((pp = memchr(p, '\n', s)) == (char *)NULL) {
			write(fdout, p, s);
			break;
		}
		len = pp - p;
		write(fdout, p, len);
		write(fdout, "\r\n", 2);
		p = pp + 1;
		s = s - len - 1;
	}
	printf("%8ld bytes\b\b\b\b\b\b\b\b\b\b\b\b\b\b", total);
	fflush(stdout);
   }
   printf("\n");
   fflush(stdout);

   return(s);
}

static int binarysend(fd, fdout)
int fd;
int fdout;
{
int s;
long total=0L;

   printf("Sent ");
   fflush(stdout);

   while((s = read(fd, buffer, sizeof(buffer))) > 0) {
	write(fdout, buffer, s);
	total += (long)s;
	printf("%8ld bytes\b\b\b\b\b\b\b\b\b\b\b\b\b\b", total);
	fflush(stdout);
   }
   printf("\n");
   fflush(stdout);

   return(s);
}

static int sendfile(fd, fdout)
int fd;
int fdout;
{
int s;

   switch(type) {
	case TYPE_A:
		s = asciisend(fd, fdout);
		break;
	default:
		s = binarysend(fd, fdout);
   }

   if(s < 0)
	return(-1);
   else
	return(0);
}

static int asciirecv(fd, fdin)
int fd;
int fdin;
{
int s, len;
char *p, *pp;
long total=0L;

   if(fd > 2) {
	printf("Received ");
	fflush(stdout);
   }
   while((s = read(fdin, buffer, sizeof(buffer))) > 0) {
	p = buffer;
	total += (long)s;
	while(s > 0) {
		if((pp = memchr(p, '\r', s)) == (char *)NULL) {
			write(fd, p, s);
			break;
		}
		len = pp - p;
		write(fd, p, len);
		p = pp + 1;
		s = s - len - 1;
	}
	if(fd > 2) {
		printf("%8ld bytes\b\b\b\b\b\b\b\b\b\b\b\b\b\b", total);
		fflush(stdout);
	}
   }
   if(fd > 2) {
	printf("\n");
	fflush(stdout);
   }
   return(s);
}

static binaryrecv(fd, fdin)
int fd;
int fdin;
{
int s;
long total=0L;

   if(fd > 2) {
	printf("Received ");
	fflush(stdout);
   }
   while((s = read(fdin, buffer, sizeof(buffer))) > 0) {
	write(fd, buffer, s);
	total += (long)s;
	if(fd > 2) {
		printf("%8ld bytes\b\b\b\b\b\b\b\b\b\b\b\b\b\b", total);
		fflush(stdout);
	}
   }
   if(fd > 2) {
	printf("\n");
	fflush(stdout);
   }
   return(s);
}

static int recvfile(fd, fdin)
int fd;
int fdin;
{
int s;

   switch(type) {
	case TYPE_A:
		s = asciirecv(fd, fdin);
		break;
	default:
		s = binaryrecv(fd, fdin);
   }

   if(s < 0)
	return(-1);
   else
	return(0);
}

static int DOgetreply()
{
char *p;
char buff[6];
int s;
int firsttime;

   do {
	firsttime = 1;
	do {
		if(fgets(line, sizeof(line), fpcommin) == (char *)0)
			return(-1);
		p = line + strlen(line) - 1;
		while(p != line)
			if(*p == '\r' || *p == '\n' || isspace(*p))
				*p-- = '\0';
			else
				break;
		printf("%s\n", line); fflush(stdout);
		if(firsttime) {
			firsttime = 0;
			strncpy(buff, line, 4);
			buff[3] = ' ';
		}
	   } while(strncmp(line, buff, 4));
	   s = atoi(buff);
   } while(s<200 && s != 125 & s != 150);

   return(s);
}

static int DOcmdcheck()
{
   if(!linkopen) {
	printf("You must \"OPEN\" a connection first.\n");
	return(1);
   }

   if(!loggedin) {
	printf("You must login first.\n");
	return(1);
   }

   return(0);
}

static int DOcommand(ftpcommand)
char *ftpcommand;
{
   fprintf(fpcommout, "%s\r\n", ftpcommand);
   fflush(fpcommout);

   return(DOgetreply());
}

static int DOopen()
{
nwio_tcpconf_t tcpconf;
nwio_tcpcl_t tcpcopt;
char *tcp_device;
tcpport_t port;
int s;
struct hostent *hp;

   if(linkopen) {
	printf("Use \"CLOSE\" to close the connection first.\n");
	return(0);
   }

   strncpy(host, cmdargv[1], 128);
   port = (tcpport_t)21;

   if(cmdargc < 2) {
	readline("Host: ", line2);
	strncpy(host, line2, 128);
   }

   if(cmdargc > 2) {
	port = (tcpport_t)atoi(cmdargv[2]);
	if(port == (tcpport_t)0)
		port = (tcpport_t)21;
   }

  hp = gethostbyname(host);
  if (hp == (struct hostent *)NULL) {
	hostip = (ipaddr_t)0;
	printf("Unresolved host %s\n", host);
	return(0);
  } else
	memcpy((char *) &hostip, (char *) hp->h_addr, hp->h_length);

  /* This HACK allows the server to establish data connections correctly */
  /* when using the loopback device to talk to ourselves */
  if(hostip == inet_addr("127.0.0.1"))
	hostip = myip;

   port = htons(port);

   if((tcp_device = getenv("TCP_DEVICE")) == NULL)
	tcp_device = "/dev/tcp";

   if((ftpcomm_fd = open(tcp_device, O_RDWR)) < 0) {
	perror("ftp: open error on tcp device");
	return(-1);
   }

   tcpconf.nwtc_flags = NWTC_LP_SEL | NWTC_SET_RA | NWTC_SET_RP;
   tcpconf.nwtc_remaddr = hostip;
   tcpconf.nwtc_remport = port;

   s = ioctl(ftpcomm_fd, NWIOSTCPCONF, &tcpconf);
   if(s < 0) {
	perror("ftp: ioctl error on NWIOSTCPCONF");
	close(ftpcomm_fd);
	return(s);
   }

   tcpcopt.nwtcl_flags = 0;

   s = ioctl(ftpcomm_fd, NWIOTCPCONN, &tcpcopt);
   if(s < 0) {
	perror("ftp: ioctl error on NWIOTCPCONN");
	close(ftpcomm_fd);
	return(s);
   }

   s = ioctl(ftpcomm_fd, NWIOGTCPCONF, &tcpconf);
   if(s < 0) {
	perror("ftp: ioctl error on NWIOGTCPCONF");
	close(ftpcomm_fd);
	return(s);
   }

   fpcommin  = fdopen(ftpcomm_fd, "r");
   fpcommout = fdopen(ftpcomm_fd, "w");

   s = DOgetreply();

   if(s < 0) {
	fclose(fpcommin);
	fclose(fpcommout);
	close(ftpcomm_fd);
	return(s);
   }

   if(s != 220) {
	fclose(fpcommin);
	fclose(fpcommout);
	close(ftpcomm_fd);
	return(0);
   }

   linkopen = 1;

   return(s);
}

static int DOpass()
{
int s;
struct sgttyb oldtty, newtty;
char *pass;

   if(!linkopen) {
	printf("You must \"OPEN\" a connection first.\n");
	return(0);
   }

   pass = cmdargv[1];

   if(cmdargc < 2) {
	ioctl(fileno(stdout), TIOCGETP, &newtty);
	oldtty = newtty;
	newtty.sg_flags &= ~ECHO;
	ioctl(fileno(stdout), TIOCSETP, &newtty);
	readline("Password: ", line2);
	ioctl(fileno(stdout), TIOCSETP, &oldtty);
	printf("\n");
	pass = line2;
   }

   sprintf(comline, "PASS %s", pass);

   s = DOcommand(comline);

   return(s);
}

static int DOuser()
{
char *user;
int s;

   if(!linkopen) {
	printf("You must \"OPEN\" a connection first.\n");
	return(0);
   }

   loggedin = 0;

   user = cmdargv[1];

   if(cmdargc < 2) {
	readline("Username: ", line2);
	user = line2;
   }

   sprintf(comline, "USER %s", user);

   s = DOcommand(comline);

   if(s == 331) {
	sprintf(line, "password");
	makeargs();
	s = DOpass();
   }

   if(s == 230)
	loggedin = 1;

   return(s);
}

static int DOascii()
{
int s;

   if(DOcmdcheck())
	return(0);

   sprintf(comline, "TYPE A");

   s = DOcommand(comline);

   type = TYPE_A;

   return(s);
}

static int DObinary()
{
int s;

   if(DOcmdcheck())
	return(0);

   sprintf(comline, "TYPE I");

   s = DOcommand(comline);

   type = TYPE_I;

   return(s);
}

static int DOclose()
{
   if(!linkopen) {
	printf("You can't close a connection that isn't open.\n");
	return(0);
   }

   fclose(fpcommin);
   fclose(fpcommout);
   close(ftpcomm_fd);

   linkopen = 0;
   loggedin = 0;

   return(0);
}

static int DOquit()
{
int s;

   if(linkopen) {
	sprintf(comline, "QUIT");
	s = DOcommand(comline);
	linkopen = 0;
	fclose(fpcommin);
	fclose(fpcommout);
	close(ftpcomm_fd);
   }

   printf("FTP done.\n");

   exit(0);
}

static int DOpwd()
{
int s;

   if(DOcmdcheck())
	return(0);

   s = DOcommand("PWD");

   if(s == 500 || s == 502)
	s = DOcommand("XPWD");

   return(s);
}

static int DOcd()
{
char *path;
int s;

   if(DOcmdcheck())
	return(0);

   path = cmdargv[1];

   if(cmdargc < 2) {
	readline("Path: ", line2);
	path = line2;
   }

   if(!strcmp(path, ".."))
	sprintf(comline, "CDUP");
   else
	sprintf(comline, "CWD %s", path);

   s = DOcommand(comline);

   if(s == 500 || s == 502) {
	if(!strcmp(path, ".."))
		sprintf(comline, "XCUP");
	else
		sprintf(comline, "XCWD %s", path);
	s = DOcommand(comline);
   }
   return(s);
}

static int DOlpwd()
{
   if(getcwd(line2, sizeof(line2)) == (char *)0)
	printf("Could not determine local directory. %s\n", strerror(errno));
   else
	printf("Current local directory: %s\n", line2);

   return(0);
}

static int DOlcd()
{
char *path;
int s;

   path = cmdargv[1];

   if(cmdargc < 2) {
	readline("Path: ", line2);
	path = line2;
   }

   if(chdir(path))
	printf("Could not change local directory. %s\n", strerror(errno));
   else
	DOlpwd();
   
   return(0);
}

static int DOmkdir()
{
char *path;
int s;

   if(DOcmdcheck())
	return(0);

   path = cmdargv[1];

   if(cmdargc < 2) {
	readline("Directory: ", line2);
	path = line2;
   }

   sprintf(comline, "MKD %s", path);
   s = DOcommand(comline);

   if(s == 500 || s == 502) {
	sprintf(comline, "XMKD %s", path);
	s = DOcommand(comline);
   }

   return(s);
}

static int DOrmdir()
{
char *path;
int s;

   if(DOcmdcheck())
	return(0);

   path = cmdargv[1];

   if(cmdargc < 2) {
	readline("Directory: ", line2);
	path = line2;
   }

   sprintf(comline, "RMD %s", path);
   s = DOcommand(comline);

   if(s == 500 || s == 502) {
	sprintf(comline, "XRMD %s", path);
	s = DOcommand(comline);
   }

   return(s);
}

static int DOlmkdir()
{
char *path;
int s;

   path = cmdargv[1];

   if(cmdargc < 2) {
	readline("Directory: ", line2);
	path = line2;
   }

   if(mkdir(path, 0777))
	printf("Could not make directory %s. %s\n", path, strerror(errno));
   else
	printf("Directory created.\n");
   
   return(0);
}

static int DOlrmdir()
{
char *path;
int s;

   path = cmdargv[1];

   if(cmdargc < 2) {
	readline("Directory: ", line2);
	path = line2;
   }

   if(rmdir(path))
	printf("Could not remove directory %s. %s\n", path, strerror(errno));
   else
	printf("Directory removed.\n");
   
   return(0);
}

static int DOdelete()
{
char *file;

   if(DOcmdcheck())
	return(0);

   file = cmdargv[1];

   if(cmdargc < 2) {
	readline("File: ", line2);
	file = line2;
   }

   sprintf(comline, "DELE %s", file);

   return(DOcommand(comline));
}

static int DOnoop()
{
   if(DOcmdcheck())
	return(0);

   return(DOcommand("NOOP"));
}

static int DOpassive()
{
   passive = 1 - passive;

   printf("Passive mode is now %s\n", (passive ? "ON" : "OFF"));

   return(0);
}

static int DOmdtm()
{
char *file;

   if(DOcmdcheck())
	return(0);

   file = cmdargv[1];

   if(cmdargc < 2) {
	readline("File: ", line2);
	file = line2;
   }

   sprintf(comline, "MDTM %s", file);

   return(DOcommand(comline));
}

static int DOsize()
{
char *file;

   if(DOcmdcheck())
	return(0);

   file = cmdargv[1];

   if(cmdargc < 2) {
	readline("File: ", line2);
	file = line2;
   }

   sprintf(comline, "SIZE %s", file);

   return(DOcommand(comline));
}

static int DOstat()
{
char *file;

   if(cmdargc < 2)
	if(!linkopen) {
		printf("You must \"OPEN\" a connection first.\n");
		return(0);
	} else
		return(DOcommand("STAT"));

   if(DOcmdcheck())
	return(0);

   file = cmdargv[1];

   if(cmdargc < 2) {
	readline("File: ", line2);
	file = line2;
   }

   sprintf(comline, "STAT %s", file);

   return(DOcommand(comline));
}

static int DOsyst()
{
   if(DOcmdcheck())
	return(0);

   return(DOcommand("SYST"));
}

static int DOremotehelp()
{
   if(!linkopen) {
	printf("You must \"OPEN\" a connection first.\n");
	return(0);
   }

   return(DOcommand("HELP"));
}

static int DOdata(datacom, direction, fd)
char *datacom;
int direction;  /* RETR or STOR */
int fd;
{
nwio_tcpconf_t tcpconf;
nwio_tcpcl_t tcplopt, tcpcopt;
char *tcp_device;
int ftpdata_fd;
char *buff;
ipaddr_t ripaddr;
tcpport_t rport;
tcpport_t lport;
int s;
int i;
int cs;
int pfd[2];
char dummy;

   ripaddr = hostip;
   rport = htons(20);
   lport = htons(0);

   /* here we set up a connection to listen on if not passive mode */
   /* otherwise we use this to connect for passive mode */

   if((tcp_device = getenv("TCP_DEVICE")) == NULL)
	tcp_device = "/dev/tcp";

   if((ftpdata_fd = open(tcp_device, O_RDWR)) < 0) {
	perror("ftp: open error on tcp device");
	return(-1);
   }

   if(passive) {
	sprintf(comline, "PASV");
	s = DOcommand(comline);
	if(s != 227) {
		close(ftpdata_fd);
		return(s);
	}
	/* decode host and port */
	buff = line;
	while(*buff && (*buff != '(')) buff++;
	buff++;
	ripaddr = (ipaddr_t)0;
	for(i = 0; i < 4; i++) {
		ripaddr = (ripaddr << 8) + (ipaddr_t)atoi(buff);
		if((buff = strchr(buff, ',')) == (char *)0) {
			printf("Could not parse PASV reply\n");
			return(-1);
		}
		buff++;
	}
	rport = (tcpport_t)atoi(buff);
	if((buff = strchr(buff, ',')) == (char *)0) {
		printf("Could not parse PASV reply\n");
		return(-1);
	}
	buff++;
	rport = (rport << 8) + (tcpport_t)atoi(buff);
	ripaddr = ntohl(ripaddr);
	rport = ntohs(rport);
   }

   tcpconf.nwtc_flags = NWTC_LP_SEL | NWTC_SET_RA | NWTC_SET_RP;

   tcpconf.nwtc_remaddr = ripaddr;
   tcpconf.nwtc_remport = rport;

   s = ioctl(ftpdata_fd, NWIOSTCPCONF, &tcpconf);
   if(s < 0) {
	perror("ftp: ioctl error on NWIOSTCPCONF");
	close(ftpdata_fd);
	return(s);
   }

   s = ioctl(ftpdata_fd, NWIOGTCPCONF, &tcpconf);
   if(s < 0) {
	perror("ftp: ioctl error on NWIOGTCPCONF");
	close(ftpdata_fd);
	return(s);
   }
   lport = tcpconf.nwtc_locport;

   if(passive) {
	tcplopt.nwtcl_flags = 0;
	s = ioctl(ftpdata_fd, NWIOTCPCONN, &tcpcopt);
	if(s < 0) {
		perror("ftp: error on ioctl NWIOTCPCONN");
		close(ftpdata_fd);
		return(0);
	}
	s = ioctl(ftpdata_fd, NWIOGTCPCONF, &tcpconf);
	if(s < 0) {
		perror("ftp: error on ioctl NWIOGTCPCONF");
		close(ftpdata_fd);
		return(0);
	}
   } else {
	tcplopt.nwtcl_flags = 0;

	if (pipe(pfd) < 0) {
		perror("ftp: could not create a pipe");
		return(s);
	}
	lpid = fork();
	if(lpid < 0) {
		perror("ftp: could not fork listener");
		close(ftpdata_fd);
		close(pfd[0]);
		close(pfd[1]);
		return(s);
	} else if(lpid == 0) {
		close(pfd[0]);
		signal(SIGALRM, donothing);
		alarm(15);
		close(pfd[1]);
		s = ioctl(ftpdata_fd, NWIOTCPLISTEN, &tcplopt);
		alarm(0);
		if(s < 0)
			if(errno == EINTR)
				exit(1);	/* timed out */
			else
				exit(-1);	/* error */
		else
			exit(0);		/* connection made */
	}
	/* Wait for the pipe to close, then the listener is ready (almost). */
	close(pfd[1]);
	(void) read(pfd[0], &dummy, 1);
	close(pfd[0]);
	while(1) {
		signal(SIGALRM, donothing);
		alarm(1);
		s = ioctl(ftpdata_fd, NWIOGTCPCONF, &tcpconf);
		alarm(0);
		if(s == -1) break;
	}
   }

#define hiword(x)       ((u16_t)((x) >> 16))
#define loword(x)       ((u16_t)(x & 0xffff)) 
#define hibyte(x)       (((x) >> 8) & 0xff)
#define lobyte(x)       ((x) & 0xff)

   if(!passive) {
	sprintf(comline, "PORT %u,%u,%u,%u,%u,%u",
		hibyte(hiword(ntohl(myip))), lobyte(hiword(ntohl(myip))),
		hibyte(loword(ntohl(myip))), lobyte(loword(ntohl(myip))),
		hibyte(ntohs(lport)), lobyte(ntohs(lport)));
	s = DOcommand(comline);
	if(s != 200) {
		close(ftpdata_fd);
		kill(lpid, SIGKILL);
		return(s);
	}
   }

   s = DOcommand(datacom);
   if(s == 125 || s == 150) {
	if(!passive) {
		while(1) {
			s = wait(&cs);
			if(s < 0 || s == lpid)
				break;
		}
		if(s < 0) {
			perror("wait error:");
			close(ftpdata_fd);
			kill(lpid, SIGKILL);
			return(s);
		}
		if((cs & 0x00ff)) {
			printf("Child listener failed %04x\n", cs);
			close(ftpdata_fd);
			return(-1);
		}
		cs = (cs >> 8) & 0x00ff;
		if(cs == 1) {
			printf("Child listener timed out\n");
			return(DOgetreply());
		} else if(cs) {
			printf("Child listener returned %02x\n", cs);
			close(ftpdata_fd);
			return(-1);
		}
	}
	switch(direction) {
		case RETR:
			s = recvfile(fd, ftpdata_fd);
			break;
		case STOR:
			s = sendfile(fd, ftpdata_fd);
			break;
	}
	close(ftpdata_fd);
	s = DOgetreply();
   } else {
	if(!passive)
		kill(lpid, SIGKILL);
	close(ftpdata_fd);
   }

   return(s);
}

static int DOlist()
{
char *path;
char *local;
int fd;
int s;
char datacom[128];

   if(DOcmdcheck())
	return(0);

   path = cmdargv[1];

   if(cmdargc < 2)
	path = "";

   if(cmdargc < 3)
	local = "";
   else
	local = cmdargv[2];

   if(*path == '\0')
	sprintf(datacom, "LIST");
   else
	sprintf(datacom, "LIST %s", path);

   if(*local == '\0')
	fd = 1;
   else
	fd = open(local, O_WRONLY | O_CREAT | O_TRUNC, 0666);

   if(fd < 0) {
	printf("Could not open local file %s. Error %s\n", local, strerror(errno));
	return(0);
   }

   s = DOdata(datacom, RETR, fd);

   if(fd > 2)
	close(fd);

   return(s);
}

static int DOnlst()
{
char *path;
char *local;
int fd;
int s;
char datacom[128];

   if(DOcmdcheck())
	return(0);

   path = cmdargv[1];

   if(cmdargc < 2)
	path = "";

   if(cmdargc < 3)
	local = "";
   else
	local = cmdargv[2];

   if(*path == '\0')
	sprintf(datacom, "NLST");
   else
	sprintf(datacom, "NLST %s", path);

   if(*local == '\0')
	fd = 1;
   else
	fd = open(local, O_WRONLY | O_CREAT | O_TRUNC, 0666);

   if(fd < 0) {
	printf("Could not open local file %s. Error %s\n", local, strerror(errno));
	return(0);
   }

   s = DOdata(datacom, RETR, fd);

   if(fd > 2)
	close(fd);

   return(s);
}

static int DOretr()
{
char *file, *localfile;
int fd;
int s;
char datacom[128];

   if(DOcmdcheck())
	return(0);

   file = cmdargv[1];

   if(cmdargc < 2) {
	readline("Remote File: ", line2);
	file = line2;
   }

   if(cmdargc < 3)
	localfile = file;
   else
	localfile = cmdargv[2];

   fd = open(localfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);

   if(fd < 0) {
	printf("Could not open local file %s. Error %s\n", localfile, strerror(errno));
	return(0);
   }

   sprintf(datacom, "RETR %s", file);

   s = DOdata(datacom, RETR, fd);

   close(fd);

   return(s);
}

static int DOMretr()
{
char *files;
char datacom[128];
int fd, s;
FILE *fp;
char name[32];

   if(DOcmdcheck())
	return(0);

   files = cmdargv[1];

   if(cmdargc < 2) {
	readline("Files: ", line2);
	files = line2;
   }

   tmpnam(name);

   fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0666);

   if(fd < 0) {
	printf("Could not open local file %s. Error %s\n", name, strerror(errno));
	return(0);
   }

   sprintf(datacom, "NLST %s", files);

   s = DOdata(datacom, RETR, fd);

   close(fd);

   if(s == 226) {
	fp = fopen(name, "r");
	unlink(name);
	if(fp == (FILE *)NULL) {
		printf("Unable to open file listing.\n");
		return(0);
	}
	while(fgets(line2, sizeof(line2), fp) != (char *)NULL) {
		line2[strlen(line2)-1] = '\0';
		printf("Retrieving file: %s\n", line2); fflush(stdout);
		fd = open(line2, O_WRONLY | O_CREAT | O_TRUNC, 0666);
		if(fd < 0)
			printf("Unable to open local file %s\n", line2);
		else {
			sprintf(datacom, "RETR %s", line2);
			s = DOdata(datacom, RETR, fd);
			close(fd);
			if(s < 0) break;
		}
	}
	fclose(fp);
   } else
	unlink(name);

   return(s);
}

static int DOappe()
{
char *file, *remotefile;
int fd;
int s;
char datacom[128];

   if(DOcmdcheck())
	return(0);

   file = cmdargv[1];

   if(cmdargc < 2) {
	readline("Local File: ", line2);
	file = line2;
   }

   if(cmdargc < 3)
	remotefile = file;
   else
	remotefile = cmdargv[2];

   fd = open(file, O_RDONLY);

   if(fd < 0) {
	printf("Could not open local file %s. Error %s\n", file, strerror(errno));
	return(0);
   }

   sprintf(datacom, "APPE %s", remotefile);

   s = DOdata(datacom, STOR, fd);

   close(fd);

   return(s);
}

static int DOstor()
{
char *file, *remotefile;
int fd;
int s;
char datacom[128];

   if(DOcmdcheck())
	return(0);

   file = cmdargv[1];

   if(cmdargc < 2) {
	readline("Local File: ", line2);
	file = line2;
   }

   if(cmdargc < 3)
	remotefile = file;
   else
	remotefile = cmdargv[2];

   fd = open(file, O_RDONLY);

   if(fd < 0) {
	printf("Could not open local file %s. Error %s\n", file, strerror(errno));
	return(0);
   }

   sprintf(datacom, "STOR %s", remotefile);

   s = DOdata(datacom, STOR, fd);

   close(fd);

   return(s);
}

static int DOstou()
{
char *file, *remotefile;
int fd;
int s;
char datacom[128];

   if(DOcmdcheck())
	return(0);

   file = cmdargv[1];

   if(cmdargc < 2) {
	readline("Local File: ", line2);
	file = line2;
   }

   if(cmdargc < 3)
	remotefile = file;
   else
	remotefile = cmdargv[2];

   fd = open(file, O_RDONLY);

   if(fd < 0) {
	printf("Could not open local file %s. Error %s\n", file, strerror(errno));
	return(0);
   }

   sprintf(datacom, "STOU %s", remotefile);

   s = DOdata(datacom, STOR, fd);

   close(fd);

   return(s);
}

static int DOMstor()
{
char *files;
char *name;
char datacom[128];
int fd, s;
FILE *fp;

   if(DOcmdcheck())
	return(0);

   files = cmdargv[1];

   if(cmdargc < 2) {
	readline("Files: ", line2);
	files = line2;
   }

   name = dir(files, 0);

   fp = fopen(name, "r");

   if(fp == (FILE *)NULL) {
	printf("Unable to open listing file.\n");
	return(0);
   }

   while(fgets(line2, sizeof(line2), fp) != (char *)NULL) {
	line2[strlen(line2)-1] = '\0';
	printf("Sending file: %s\n", line2); fflush(stdout);
	fd = open(line2, O_RDONLY);
	if(fd < 0)
		printf("Unable to open local file %s\n", line2);
	else {
		sprintf(datacom, "STOR %s", line2);
		s = DOdata(datacom, STOR, fd);
		close(fd);
		if(s < 0) break;
	}
   }
   fclose(fp);
   unlink(name);

   return(s);
}

static int DOquote()
{
int i;

   comline[0] = '\0';

   for(i = 1; i < cmdargc; i++) {
	if(i != 1)
		strcat(comline, " ");
	strcat(comline, cmdargv[i]);
   }

   return(DOcommand(comline));
}

static int DOllist(void)
{
   dodir(".", 1);
}

static int DOlnlst(void)
{
   dodir(".", 0);
}

static int DOshell(void)
{
   system("$SHELL");
}

static int DOhelp()
{
   printf("Command:      Description\n");
   printf("!             Exit to shell\n");
   printf("append        Append a file to remote host\n");
   printf("ascii         Set file transfer mode to ascii\n");
   printf("binary        Set file transfer mode to binary\n");
   printf("bye           Close connection and exit\n");
   printf("cd            Change directory on remote host\n");
   printf("close         Close connection\n");
   printf("del           Remove file on remote host\n");
   printf("dir           Display long form remote host directory listing\n");
   printf("exit          Close connection and exit\n");
   printf("get           Retrieve a file from remote host\n");
   printf("help          Display this text\n");
   printf("lcd           Change directory on local host\n");
   printf("ldir          Display long form local host directory listing\n");
   printf("lls           Display local host directory listing\n");
   printf("lmkdir        Create directory on local host\n");
   printf("lpwd          Display current directory on local host\n");
   printf("lrmdir        Remove directory on local host\n");
   printf("ls            Display remote host directory listing\n");
   printf("mget          Retrieve multiple files from remote host\n");
   printf("mkdir         Create directory on remote host\n");
   printf("mod           Get file modification time\n");
   scrpause();
   printf("mput          Send multiple files to remote host\n");
   printf("noop          Send the ftp NOOP command\n");
   printf("open          Open connection to remote host\n");
   printf("passive       Toggle passive mode\n");
   printf("put           Send a file to remote host\n");
   printf("putu          Send a file to remote host(unique)\n");
   printf("pwd           Display current directory on remote host\n");
   printf("quit          Close connection and exit\n");
   printf("quote         Send raw ftp command to remote host\n");
   printf("remotehelp    Display ftp commands implemented on remote host\n");
   printf("rm            Remove file on remote host\n");
   printf("rmdir         Remove directory on remote host\n");
   printf("size          Get file size information\n");
   printf("status        Get connection/file status information\n");
   printf("system        Get remote system type information\n");
   printf("user          Enter remote user information\n");
   return(0);
}

struct commands {
	char *name;
	_PROTOTYPE(int (*func), (void));
};

static struct commands commands[] = {
        "!",            DOshell,
	"append",	DOappe,
	"ascii",        DOascii,
	"binary",       DObinary,
	"bye",          DOquit,
	"cd",           DOcd,
	"close",        DOclose,
	"del",          DOdelete,
	"dir",          DOlist,
	"exit",         DOquit,
	"get",          DOretr,
	"help",         DOhelp,
	"lcd",          DOlcd,
        "ldir",         DOllist,
        "lls",          DOlnlst,
	"lmkdir",       DOlmkdir,
	"lpwd",         DOlpwd,
	"lrmdir",       DOlrmdir,
	"ls",           DOnlst,
	"mget",         DOMretr,
	"mkdir",        DOmkdir,
	"mod",		DOmdtm,
	"mput",         DOMstor,
	"noop",         DOnoop,
	"open",         DOopen,
	"passive",      DOpassive,
	"put",          DOstor,
	"putu",		DOstou,
	"pwd",          DOpwd,
	"quit",         DOquit,
	"quote",        DOquote,
	"remotehelp",   DOremotehelp,
	"rm",           DOdelete,
	"rmdir",        DOrmdir,
	"size",		DOsize,
	"status",	DOstat,
	"system",	DOsyst,
	"user",         DOuser,
	"",     (int (*)())0
};

int makeargs()
{
char *p;
int i;

   for(i = 0; i < NUMARGS; i++)
	cmdargv[i] = (char *)0;

   p = line + strlen(line) - 1;
   while(p != line)
	if(*p == '\r' || *p == '\n' || isspace(*p))
		*p-- = '\0';
	else
		break;

   p = line;
   cmdargc = 0;
   while(cmdargc < NUMARGS) {
	while(*p && isspace(*p))
		p++;
	if(*p == '\0')
		break;
	cmdargv[cmdargc++] = p;
	while(*p && !isspace(*p)) {
		if(cmdargc == 1)
			*p = tolower(*p);
		p++;
	}
	if(*p == '\0')
		break;
	*p = '\0';
	p++;
   }
}


void dodir(path, full)
char *path;
int full;
{
char cmd[128];
static char name[32];
   tmpnam(name);
   if(full)
	sprintf(cmd, "/bin/ls -lg %s > %s", path, name);
   else
	sprintf(cmd, "/bin/ls %s > %s", path, name);
   system(cmd);
   sprintf(cmd, "more %s", name);
   system(cmd);
   sprintf(cmd, "rm %s", name);
   system(cmd);
}

static void scrpause(void)
{
   int c;
   printf("Press ENTER to continue... ");
   c = getchar();
   printf("\n");
}

static int readline(prompt, buff)
char *prompt;
char *buff;
{
   printf(prompt); fflush(stdout);
   if(fgets(buff, 1024, stdin) == (char *)NULL) {
	printf("\nEnd of file on input!\n");
	exit(1);
   }
   *strchr(buff, '\n') = 0;
   return(0);
}

int main(argc, argv)
int argc;
char *argv[];
{
int s;
struct commands *cmd;
char *tcp_device;
int tcp_fd;
nwio_tcpconf_t nwio_tcpconf;

   /* All this just to get our ip address */

   if((tcp_device = getenv("TCP_DEVICE")) == (char *)NULL)
	tcp_device = TCP_DEVICE;

   tcp_fd = open(tcp_device, O_RDWR);
   if(tcp_fd < 0) {
	perror("ftp: Could not open tcp_device");
	exit(-1);
   }
   s = ioctl(tcp_fd, NWIOGTCPCONF, &nwio_tcpconf);
   if(s < 0) {
	perror("ftp: Could not get tcp configuration");
	exit(-1);
   }

   myip = nwio_tcpconf.nwtc_locaddr;

   close(tcp_fd);

   /* now we can begin */

   init();

   s = 0;

   if(argc > 1) {
	sprintf(line, "open %s ", argv[1]);
	if(argc > 2)
		strcat(line, argv[2]);
	makeargs();
	s = DOopen();
	if(s > 0) {
		sprintf(line, "user");
		makeargs();
		s = DOuser();
	}
   }

   while(s >= 0) {
	readline("ftp>", line);
	makeargs();
	for(cmd = commands; *cmd->name != '\0'; cmd++)
		if(!strcmp(cmdargv[0], cmd->name))
			break;
	if(*cmd->name != '\0')
		s = (*cmd->func)();
	else {
		s = 0;
		if(cmdargc > 0)
			printf("Command \"%s\" not recognized.\n", cmdargv[0]);
	}
   }

   return(0);
}
