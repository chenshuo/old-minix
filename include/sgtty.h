/* The <sgtty.h> header contains data structures for ioctl(). */

#ifndef _SGTTY_H
#define _SGTTY_H

struct sgttyb {
  char sg_ispeed;		/* input speed */
  char sg_ospeed;		/* output speed */
  char sg_erase;		/* erase character */
  char sg_kill;			/* kill character */
  int  sg_flags;		/* mode flags */
};

struct tchars {
  char t_intrc;			/* SIGINT char */
  char t_quitc;			/* SIGQUIT char */
  char t_startc;		/* start output (initially CTRL-Q) */
  char t_stopc;			/* stop output	(initially CTRL-S) */
  char t_eofc;			/* EOF (initially CTRL-D) */
  char t_brkc;			/* input delimiter (like nl) */
};

/* Field names */
#define XTABS	     0006000	/* do tab expansion */
#define BITS8        0001400	/* 8 bits/char */
#define BITS7        0001000	/* 7 bits/char */
#define BITS6        0000400	/* 6 bits/char */
#define BITS5        0000000	/* 5 bits/char */
#define EVENP        0000200	/* even parity */
#define ODDP         0000100	/* odd parity */
#define RAW	     0000040	/* enable raw mode */
#define CRMOD	     0000020	/* map lf to cr + lf */
#define ECHO	     0000010	/* echo input */
#define CBREAK	     0000002	/* enable cbreak mode */
#define COOKED       0000000	/* neither CBREAK nor RAW */

#define DCD          0100000	/* Data Carrier Detect */

/* Line speeds */
#define B0		   0	/* code for line-hangup */
#define B110		   1
#define B300		   3
#define B1200		  12
#define B2400		  24
#define B4800		  48
#define B9600 		  96
#define B19200		 192

#define TIOCGETP (('t'<<8) | 8)
#define TIOCSETP (('t'<<8) | 9)
#define TIOCGETC (('t'<<8) | 18)
#define TIOCSETC (('t'<<8) | 17)
#define TIOCFLUSH (('t'<<8) | 16)

/* Things Minix supports but not properly */
/* the divide-by-100 encoding ain't too hot */
#define ANYP         0000300
#define B50                0
#define B75                0
#define B134               0
#define B150               0
#define B200               2
#define B600               6
#define B1800             18
#define B3600             36
#define B7200             72
#define EXTA             192
#define EXTB               0

/* Things Minix doesn't support but are fairly harmless if used */
#define NLDELAY      0001400
#define TBDELAY      0006000
#define CRDELAY      0030000
#define VTDELAY      0040000
#define BSDELAY      0100000
#define ALLDELAY     0177400

/* screen stuff */

#define	SCR	('S'<<8)
#define SCR_ATTR	(SCR | 1)
#define SCR_SETCOL	(SCR | 2)
#define SCR_GETCOL	(SCR | 3)
#define SCR_SETCOLMAP	(SCR | 4)
#define SCR_GETCOLMAP	(SCR | 5)

#ifdef MACHINE

#if MACHINE == ATARI

/* declaration for font header */
struct fnthdr {
	char width;
	char heigth;
	void *addr;
};

#define VDU_LOADFONT ('F' << 8)

/* ST specific clock stuff */
#define	 DCLOCK	('D'<<8)

#define	DC_RBMS100	(DCLOCK|1)
#define	DC_RBMS200	(DCLOCK|2)
#define	DC_RSUPRA	(DCLOCK|3)
#define	DC_RICD  	(DCLOCK|4)
#define	DC_WBMS100	(DCLOCK|8)
#define	DC_WBMS200	(DCLOCK|9)
#endif /* MACHINE == ATARI */

#if MACHINE == SUN_4_60
/* Window size support, not really SparcStation specific */
struct winsize {
  unsigned short ws_row;	/* number of character rows */
  unsigned short ws_col;	/* number of characters per line */
  unsigned short ws_xpixel;	/* horizontal character size in pixels */
  unsigned short ws_ypixel;	/* vertical character size in pixels */
};

#define TIOCGWINSZ (('t'<<8) | 104)	/* get window size */
#define TIOCSWINSZ (('t'<<8) | 103)	/* set window size */

/* SparcStation specific floppy support: */
struct diskio {
  int  f_cyl;			/* disk cylinder number */
  unsigned char f_head;		/* disk head (aka 'side' or 'track') number */
  unsigned char f_sec;		/* sector number (not used at the moment */
  unsigned char f_density;	/* bit density (data rate): HIGH_D or LOW_D */
  char *f_buf;			/* address of data */
  int  f_cnt;			/* length of buffer in bytes */
};

#define TIOCEJECT (('f'<<8) | 0)	/* Floppy disk eject */
#define TIOCFORMAT (('f'<<8) | 1)	/* Format one track */

#define LOW_D		1	/* Actually: `normal' density MFM (250Kb/s) */
#define HIGH_D		0	/* `high' density MFM (500Kb/s) */
#endif /* MACHINE == SUN_4_60 */

#endif /* MACHINE defined */

#include <ansi.h>

_PROTOTYPE( int gtty, (int _fd, struct sgttyb *_argp)			);
_PROTOTYPE( int ioctl, (int _fd, int _request, struct sgttyb * _argp)	);
_PROTOTYPE( int stty, (int _fd, struct sgttyb *_argp)			);

#endif /* _SGTTY_H */
