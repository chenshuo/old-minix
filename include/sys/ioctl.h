/* The ioctl.h header declares device controlling operations. */

#ifndef _IOCTL_H
#define _IOCTL_H

#if _EM_WSIZE >= 4
/* ioctl's have the command encoded in the low-order word, and the size
 * of the parameter in the high-order word. The 3 high bits of the high-
 * order word are used to encode the in/out/void status of the parameter.
*/

#define _IOCPARM_MASK	0x1FFF
#define _IOC_VOID	0x20000000
#define _IOCTYPE_MASK	0xFFFF
#define _IOC_IN		0x40000000
#define _IOC_OUT	0x80000000
#define _IOC_INOUT	(_IOC_IN | _IOC_OUT)

#define _IO(x,y)	((x << 8) | y | _IOC_VOID)
#define _IOR(x,y,t)	((x << 8) | y | ((sizeof(t) & _IOCPARM_MASK) << 16) |\
				_IOC_OUT)
#define _IOW(x,y,t)	((x << 8) | y | ((sizeof(t) & _IOCPARM_MASK) << 16) |\
				_IOC_IN)
#define _IORW(x,y,t)	((x << 8) | y | ((sizeof(t) & _IOCPARM_MASK) << 16) |\
				_IOC_INOUT)
#else
/* No fancy encoding on a 16-bit machine. */

#define _IO(x,y)	((x << 8) | y)
#define _IOR(x,y,t)	_IO(x,y)
#define _IOW(x,y,t)	_IO(x,y)
#define _IORW(x,y,t)	_IO(x,y)
#endif


/* Network ioctl's. */
#define NWIOSETHOPT	_IOW('n', 16, struct nwio_ethopt)
#define NWIOGETHOPT	_IOR('n', 17, struct nwio_ethopt)
#define NWIOGETHSTAT	_IOR('n', 18, struct nwio_ethstat)

#define NWIOSIPCONF	_IOW('n', 32, struct nwio_ipconf)
#define NWIOGIPCONF	_IOR('n', 33, struct nwio_ipconf)
#define NWIOSIPOPT	_IOW('n', 34, struct nwio_ipopt)
#define NWIOGIPOPT	_IOR('n', 35, struct nwio_ipopt)

#define NWIOIPGROUTE	_IORW('n', 40, struct nwio_route)
#define NWIOIPSROUTE	_IOW ('n', 41, struct nwio_route)
#define NWIOIPDROUTE	_IOW ('n', 42, struct nwio_route)

#define NWIOSTCPCONF	_IOW('n', 48, struct nwio_tcpconf)
#define NWIOGTCPCONF	_IOR('n', 49, struct nwio_tcpconf)
#define NWIOTCPCONN	_IOW('n', 50, struct nwio_tcpcl)
#define NWIOTCPLISTEN	_IOW('n', 51, struct nwio_tcpcl)
#define NWIOTCPATTACH	_IOW('n', 52, struct nwio_tcpatt)
#define NWIOTCPSHUTDOWN	_IO ('n', 53)
#define NWIOSTCPOPT	_IOW('n', 54, struct nwio_tcpopt)
#define NWIOGTCPOPT	_IOR('n', 55, struct nwio_tcpopt)

#define NWIOSUDPOPT	_IOW('n', 64, struct nwio_udpopt)
#define NWIOGUDPOPT	_IOR('n', 65, struct nwio_udpopt)

/* Disk ioctl's */
#define DIOCSETP	_IOW('d', 3, struct part_entry)
#define DIOCGETP	_IOR('d', 4, struct part_entry)
#define DIOCEJECT	_IO ('d', 5)

/* Keyboard ioctl's. */
#define KIOCSMAP	_IOW('k', 3, keymap_t)

/* Magnetic tape ioctls */
#define MTIOCTOP	_IOW('M', 1, struct mtop)
#define MTIOCGET	_IOR('M', 2, struct mtget)

/* SCSI command */
#define SCIOCCMD	_IOW('S', 1, struct scsicmd)

#ifndef _ANSI
#include <ansi.h>
#endif

_PROTOTYPE( int ioctl, (int _fd, int _request, void *_data)		);

#endif /* _IOCTL_H */
