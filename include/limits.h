/*  limits.h  */

#define  CHAR_BIT		  8	/* number of bits in char	*/
#define  WORD_BIT		 16	/* number of bits in int	*/
#define  CHAR_MAX		127	/* max value of char		*/
#define  CHAR_MIN	       -128	/* min value of char		*/
#define  SCHAR_MAX		127	/* max value of signed char	*/
#define  SCHAR_MIN	       -128	/* min value of signed char	*/
#define	 UCHAR_MAX		255	/* max value of unsigned char	*/
#define  SHRT_MAX	      32767	/* max value of short		*/
#define  SHRT_MIN	     -32768	/* min value of short		*/
#define  USHRT_MAX	      65535	/* max value of unsigned short	*/
#define  INT_MAX	      32767	/* max value of int		*/
#define  INT_MIN	     -32768	/* min value of int		*/
#define  UINT_MAX	      65535	/* max value of unsigned int	*/
#define  LONG_MAX	 2147483647	/* max value of long		*/
#define  LONG_MIN	-2147483648	/* min value of long		*/
#define  ULONG_MAX	 4294967295	/* max value of unsigned long	*/

#define  NAME_MAX	14		/* characters in a file name	*/
#define  PATH_MAX	127		/* number chars in path name	*/
					/* SHOULD BE >= 255		*/
#define  FCHR_MAX	67108864	/* max file size		*/
#define  LINK_MAX	127		/* max links to a file		*/
#define  LOCK_MAX	0		/* max number of file locks	*/
					/* SHOULD BE >= 32		*/
#define  OPEN_MAX	20		/* max number open files	*/
#define  SYS_OPEN	64		/* max open files per system	*/
#define  STD_BLK	1024		/* bytes per block		*/
#define  PIPE_MAX	7168		/* max size of write to pipe	*/
#define  PIPE_BUF	PIPE_MAX	/* size of atomic write to pipe	*/

#define  PID_MAX	30000		/* max process id		*/
#define  PROC_MAX	16		/* max number of processes	*/
#define  CHILD_MAX	(PROC_MAX-1)	/* number of process children	*/
#define  NGROUPS_MAX	0		/* no multiple group ids	*/
#define  UID_MAX	255		/* max user or group id		*/
					/* SHOULD BE >= 32000		*/
#define  ARG_MAX	2048		/* execve() arg & environ space	*/
					/* SHOULD BE >= 4096		*/

#define  CLK_TCK	60		/* clock ticks per second	*/
#define  MAX_CHAR	256		/* characters for terminal i/p	*/
#define  PASS_MAX	8		/* max number chars in password	*/
#define  SYS_NMLN	9		/* length of uname() strings	*/
					/*  including '\0'		*/
