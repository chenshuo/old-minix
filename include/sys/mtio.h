/*
 * mtio.h  - defines for magtape device driver ioctls
 *		for aha_scsi, by James da Silva (jds@cs.umd.edu)
 *		slightly modified by Matthias Pfaller (leo@marco.de)
 */

#define MTIOCTOP	(('m'<<8) | 1)	/* do operation */
#define MTIOCGET	(('m'<<8) | 2)	/* get status (not implemented) */

/* data struct for MTIOCTOP */

struct mtop {
	short mt_op;
	int  mt_count;
};

/* values for the mt_op field */

#define MTWEOF	0	/* write EOF marks */
#define MTFSF	1	/* forward-space filemarks */
#define MTBSF	2	/* back-space filemarks */
#define MTFSR	3	/* forward-space records */
#define MTBSR	4	/* back-space records */
#define MTREW	5	/* rewind tape */
#define MTOFFL	6	/* rewind tape and take it offline */
#define MTNOP	7	/* no operation, sets status only */
#define MTRETEN	8	/* retension operation */
#define MTRET	8
#define MTERASE	9	/* erase the entire tape */
#define	MTEOM	10	/* position to end of media */
#define MTAPP	10
#define MTONL	11	/* put drive online */
/* default tape device */

#define DEFTAPE "/dev/nrst0"

