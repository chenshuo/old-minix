/* The <ar.h> header gives the layout of archives. */

#ifndef _AR_H
#define _AR_H

/* When CHIP is not defined, set it to something other than SPARC. */
#ifndef CHIP
#define CHIP -9999
#endif

#if CHIP != SPARC
/* Normal (default) case: not a SPARC.  Use V7 archive format. */
#define	ARMAG	0177545
#define _NAME_MAX    14

struct ar_hdr {
  char  ar_name[_NAME_MAX];
  char  ar_date[4];		/* long in byte order 2 3 1 0 */
  char  ar_uid;
  char  ar_gid;
  char  ar_mode[2];		/* short in byte order 0 1 */
  char  ar_size[4];		/* long in byte order 2 3 1 0 */
};

#else	
/* SparcStation uses ASCII type of ar header. */

#define ARMAG	"!<arch>\n"
#define SARMAG	8
#define ARFMAG	"`\n"

struct ar_hdr {
	char	ar_name[16];
	char	ar_date[12];
	char	ar_uid[6];
	char	ar_gid[6];
	char	ar_mode[8];
	char	ar_size[10];
	char	ar_fmag[2];
};

#endif /* CHIP != SPARC */

#endif /* _AR_H */
