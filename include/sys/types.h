typedef long		off_t;
typedef long		time_t;
typedef unsigned short	ino_t;
typedef short		dev_t;

#define makedev(maj, min)	(((maj)<<8) | (min))
#define minor(dev)		((dev) & 0xFF)
#define major(dev)		(((dev)>>8) & 0xFF)
