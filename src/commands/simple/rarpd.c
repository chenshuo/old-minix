/*
rarpd.c

Created:	Nov 12, 1992 by Philip Homburg
*/

#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/hton.h>
#include <net/netlib.h>
#include <net/gen/socket.h>
#include <net/gen/netdb.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include <net/gen/if_ether.h>

typedef struct rarp46
{
	ether_addr_t a46_dstaddr;
	ether_addr_t a46_srcaddr;
	ether_type_t a46_ethtype;
	union
	{
		struct
		{
			u16_t a_hdr, a_pro;
			u8_t a_hln, a_pln;
			u16_t a_op;
			ether_addr_t a_sha;
			u8_t a_spa[4];
			ether_addr_t a_tha;
			u8_t a_tpa[4];
		} a46_data;
		char    a46_dummy[ETH_MIN_PACK_SIZE-ETH_HDR_SIZE];
	} a46_data;
} rarp46_t;

#define a46_hdr a46_data.a46_data.a_hdr
#define a46_pro a46_data.a46_data.a_pro
#define a46_hln a46_data.a46_data.a_hln
#define a46_pln a46_data.a46_data.a_pln
#define a46_op a46_data.a46_data.a_op
#define a46_sha a46_data.a46_data.a_sha
#define a46_spa a46_data.a46_data.a_spa
#define a46_tha a46_data.a46_data.a_tha
#define a46_tpa a46_data.a46_data.a_tpa

#define RARP_ETHERNET	1

#define RARP_REQUEST	3
#define RARP_REPLY	4

unsigned char packet[ETH_MAX_PACK_SIZE];
char hostname[1024];
char *prog_name;
int debug;

/* Old file reading function to map a name to an address. */
struct hostent *_gethostbyname(char *);

int main(int argc, char *argv[])
{
	int eth_fd;
	nwio_ethopt_t ethopt;
	nwio_ethstat_t ethstat;
	rarp46_t *rarp_ptr= (rarp46_t *)packet;
	ether_addr_t my_ether_addr;
	ipaddr_t my_ip_addr= 0;
	struct hostent *hostent;
	int result;
	char *eth_dev;
	int self= 1;

	prog_name= argv[0];

	if (argc == 2 && strcmp(argv[1], "-d") == 0)
	{
		debug= 1;
	}
	else if (argc > 1)
	{
		fprintf(stderr, "Usage: %s [-d]\n", prog_name);
		exit(1);
	}
	eth_dev= getenv("ETH_DEVICE");
	if (eth_dev == NULL)
		eth_dev= ETH_DEVICE;
	eth_fd= open (eth_dev, O_RDWR);
	if (eth_fd<0)
	{
		fprintf(stderr, "%s: unable to open '%s': %s\n", prog_name,
			eth_dev, strerror(errno));
		exit(1);
	}
	ethopt.nweo_flags= NWEO_COPY | NWEO_EN_LOC | NWEO_EN_BROAD |
		NWEO_TYPESPEC;
	ethopt.nweo_type= htons(ETH_RARP_PROTO);

	result= ioctl (eth_fd, NWIOSETHOPT, &ethopt);
	if (result<0)
	{
		fprintf(stderr, "%s: unable to set eth options: %s\n",
			prog_name, strerror(errno));
		exit(1);
	}

	result= ioctl (eth_fd, NWIOGETHSTAT, &ethstat);
	if (result<0)
	{
		fprintf(stderr, "%s: unable to get eth statistics: %s\n",
			prog_name, strerror(errno));
		exit(1);
	}
	my_ether_addr= ethstat.nwes_addr;
	if (debug)
	{
		printf("%s: my ethernet address is %s\n",
			prog_name, ether_ntoa(&my_ether_addr));
	}

	for(;; self= 0)
	{
		fflush(stdout);
		fflush(stderr);

		if (self) {
			/* The first packet out is an unsolicited "reply"
			 * to the local machine to tell it its IP address.
			 * Simulate a local RARP request:
			 */
			rarp_ptr->a46_tha= my_ether_addr;
		} else {
			/* Wait for a RARP request and answer it.
			 */
			result= read (eth_fd, (char *)packet, sizeof(packet));
			if (result<0)
			{
				fprintf(stderr,
				    "%s: unable to read (from eth_fd): %s\n",
						prog_name, strerror(errno));
				exit(1);
			}

			if (result < sizeof(rarp46_t))
			{
				if (debug)
				{
					printf("%s: packet too small\n",
						prog_name);
				}
				continue;
			}
			if (rarp_ptr->a46_hdr != htons(RARP_ETHERNET))
			{
				if (debug)
				{
					printf("%s: wrong hardware type\n",
						prog_name);
				}
				continue;
			}
			if (rarp_ptr->a46_pro != htons(ETH_IP_PROTO))
			{
				if (debug)
				{
					printf("%s: wrong protocol\n",
						prog_name);
				}
				continue;
			}
			if (rarp_ptr->a46_hln != 6)
			{
				if (debug)
				{
					printf(
					"%s: wrong hardware address lenght\n", 
						prog_name);
				}
				continue;
			}
			if (rarp_ptr->a46_pln != 4)
			{
				if (debug)
				{
					printf(
					"%s: wrong protocol address length\n",
						prog_name);
				}
				continue;
			}
			if (rarp_ptr->a46_op != htons(RARP_REQUEST))
			{
				if (debug)
				{
					printf("%s: wrong request type\n",
						prog_name);
				}
				continue;
			}
		}
		result= ether_ntohost (hostname, &rarp_ptr->a46_tha);
		if (result)
		{
			if (debug)
			{
				printf(
				"%s: no hostname for ethernet addr: %s\n",
					prog_name,
					ether_ntoa(&rarp_ptr->a46_tha));
			}
			continue;
		}
		if (self) {
			hostent= _gethostbyname (hostname);
		} else {
			hostent= gethostbyname (hostname);
		}
		if (!hostent)
		{
			if (debug)
			{
				printf("%s: %s: unknown host\n",
					prog_name, hostname);
			}
			continue;
		}
		if (self) {
			memcpy(&my_ip_addr, hostent->h_addr, sizeof(ipaddr_t));
		}
		if (hostent->h_addrtype != AF_INET)
		{
			if (debug)
			{
				printf("%s: %s: no internet address\n",
					prog_name, hostname);
			}
			continue;
		}
		rarp_ptr->a46_hdr= htons(RARP_ETHERNET);
		rarp_ptr->a46_pro= htons(ETH_IP_PROTO);
		rarp_ptr->a46_hln= 6;
		rarp_ptr->a46_pln= 4;
		memcpy(rarp_ptr->a46_tpa, hostent->h_addr, sizeof(ipaddr_t));
		rarp_ptr->a46_sha= my_ether_addr;
		memcpy(rarp_ptr->a46_spa, &my_ip_addr, sizeof(ipaddr_t));
		rarp_ptr->a46_op= htons(RARP_REPLY);
		rarp_ptr->a46_dstaddr= rarp_ptr->a46_tha;
		if (debug)
		{
			printf("%s: replying IP address %s for host %s\n", 
				prog_name,
				inet_ntoa(*(ipaddr_t *)hostent->h_addr),
				hostname);
		}
		write (eth_fd, (char *)packet, sizeof(rarp46_t));
	}
	exit(0);
}
