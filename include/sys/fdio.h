/* Data structures for disk ioctl */

struct hparam {
	unsigned char max_cyl;
	unsigned char dense;	/* :6 */
	unsigned char size;	/* :2 */
	unsigned char sides;	/* :1 */
	unsigned char slow;	/* :1 */
};

#define SIDES1	0x00		/* 1 side */
#define SIDES2	0x01		/* 2 sides */
#define FAST	0x00
#define SLOW	0x01
#define I3	0x00
#define I5	0x01

struct fparam {
	int  sector_size;		/* sector size in bytes */
	unsigned char sector_0;		/* first sector on a track */
	unsigned char nr_sectors;	/* sectors per track */
	unsigned char cylinder_0;	/* cylinder offset */
	unsigned char nr_cylinders;	/* number of cylinders */
	unsigned char nr_sides;		/* :1 single/double sided */
	unsigned char density;		/* :3 density code */
	unsigned char stepping;		/* :1 double stepping */
	unsigned char autocf;		/* :1 autoconfig */
};

#define NSTP	0x00		/* normal stepping */
#define DSTP	0x01		/* double stepping */
#define DD	0x00		/* double density */
#define	QD	0x01		/* hyper format */
#define	HD	0x02		/* high density */
#define DD5	0x03		/* normal density for 5.25'' */

#define HARD	0x00		/* hard configuration */
#define AUTO	0x01		/* auto configuration */

#define DIOGETP 	(('d'<<8)|'G')
#define DIOSETP 	(('d'<<8)|'S')
#define DIOGETHP	(('d'<<8)|'g')
#define DIOSETHP	(('d'<<8)|'s')
#define DIOFMTLOCK 	(('d'<<8)|'L')
#define DIOFMTFREE 	(('d'<<8)|'F')
