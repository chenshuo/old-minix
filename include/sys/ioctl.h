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

/* Terminal ioctl's */
#define TIOCSFON	_IOW('T', 20, u8_t [8192])

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

/* Cdrom ioctls */
#define	CDIOPLAYTI	_IOR('c', 1, struct cd_play_track)
#define CDIOPLAYMSS	_IOR('c', 2, struct cd_play_mss)
#define CDIOREADTOCHDR	_IOW('c', 3, struct cd_toc_entry)
#define CDIOREADTOC	_IOW('c', 4, struct cd_toc_entry)
#define CDIOREADSUBCH	_IOW('c', 5, struct cd_toc_entry)
#define CDIOSTOP	_IO ('c', 10)
#define CDIOPAUSE	_IO ('c', 11)
#define CDIORESUME	_IO ('c', 12)
#define CDIOEJECT	DIOCEJECT

/* Soundcard DSP ioctls */
#define	DSPIORATE	_IOR('s', 1, unsigned int)
#define DSPIOSTEREO	_IOR('s', 2, unsigned int)
#define DSPIOSIZE	_IOR('s', 3, unsigned int)
#define DSPIOBITS	_IOR('s', 4, unsigned int)
#define DSPIOSIGN	_IOR('s', 5, unsigned int)
#define DSPIOMAX	_IOW('s', 6, unsigned int)
#define DSPIORESET	_IO ('s', 7)

/* Soundcard mixer ioctls */
#define MIXIOGETVOLUME		_IORW('s', 10, struct volume_level)
#define MIXIOGETINPUTLEFT	_IORW('s', 11, struct inout_ctrl)
#define MIXIOGETINPUTRIGHT	_IORW('s', 12, struct inout_ctrl)
#define MIXIOGETOUTPUT		_IORW('s', 13, struct inout_ctrl)
#define MIXIOSETVOLUME		_IORW('s', 20, struct volume_level)
#define MIXIOSETINPUTLEFT	_IORW('s', 21, struct inout_ctrl)
#define MIXIOSETINPUTRIGHT	_IORW('s', 22, struct inout_ctrl)
#define MIXIOSETOUTPUT		_IORW('s', 23, struct inout_ctrl)

#ifndef _ANSI
#include <ansi.h>
#endif

_PROTOTYPE( int ioctl, (int _fd, int _request, void *_data)		);

#endif /* _IOCTL_H */
