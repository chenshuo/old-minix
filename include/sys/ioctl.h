/* The ioctl.h header is used for controlling network lines */

#ifndef _IOCTL_H
#define _IOCTL_H

/* ioctl's have the command encoded in the low-order word, and the size
 * of the parameter in the high-order word. The 2 high bits of the high-
 * order word are used to encode the in/out status of the parameter.
*/

#define IOCPARM_MASK	0x3FFFL
#define IOCTYPE_MASK	0xFFFFL
#define IOC_IN		0x40000000L
#define IOC_OUT		0x80000000L
#define IOC_INOUT	(IOC_IN | IOC_OUT)

#define _IO(x,y)	((x << 8) | y)
#define _IOR(x,y,t)	((x << 8) | y  | ((sizeof(t) & IOCPARM_MASK) << 16) |\
				IOC_IN)
#define _IOW(x,y,t)	((x << 8) | y | ((sizeof(t) & IOCPARM_MASK) << 16) |\
				IOC_OUT)
#define _IORW(x,y,t)	((x << 8) | y | ((sizeof(t) & IOCPARM_MASK) << 16) |\
				IOC_INOUT)


#define NWIOSETHOPT	_IOR('n',1,struct nwio_ethopt)
#define NWIOGETHOPT	_IOW('n',2,struct nwio_ethopt)
#define NWIOGETHSTAT	_IOW('n',3,struct nwio_ethstat)

#define NWIOSIPCONF	_IOR('n',16,struct nwio_ipconf)
#define NWIOGIPCONF	_IOW('n',17,struct nwio_ipconf)
#define NWIOSIPOPT	_IOR('n',18,struct nwio_ipopt)
#define NWIOGIPOPT	_IOW('n',19,struct nwio_ipopt)

#define NWIOSTCPOPT	_IOR('n',32,struct nwio_tcpopt)
#define NWIOGTCPOPT	_IOW('n',33,struct nwio_tcpopt)
#define NWIOTCPCONN	_IOR('n',34,struct nwio_tcpcl)
#define NWIOTCPLISTEN	_IOR('n',35,struct nwio_tcpcl)
#define NWIOTCPATTACH	_IOR('n',36,struct nwio_tcpatt)
#define NWIOTCPSHUTDOWN	_IO('n', 37)

#ifndef _ANSI_H
#include <ansi.h>
#endif

_PROTOTYPE (int nwioctl, (int _fd, unsigned long _req,
	_VOIDSTAR _data) );

#endif /* _IOCTL_H */
