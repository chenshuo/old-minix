/* Global variables used in the kernel. */

/* Clocks and timers */
EXTERN real_time realtime;	/* real time clock */
EXTERN int lost_ticks;		/* incremented when clock int can't send mess*/

/* Processes, signals, and messages. */
EXTERN int cur_proc;		/* current process */
EXTERN int prev_proc;		/* previous process */
EXTERN int sig_procs;		/* number of procs with p_pending != 0 */
EXTERN message int_mess;	/* interrupt routines build message here */

/* CPU type. */
EXTERN int pc_at;		/* PC-AT type diskette drives (360K/1.2M) ? */
EXTERN int ps;			/* are we dealing with a ps? */
EXTERN int port_65;		/* saved contents of Planar Control Register */

/* Video cards and keyboard types. */
EXTERN int color;		/* 1 if console is color, 0 if it is mono */
EXTERN int ega;			/* 1 if console is EGA, 0 if not */
EXTERN int scan_code;		/* scan code of key pressed to start minix */
