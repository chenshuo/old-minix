/* Miscellaneous constants used in assembler code. */
#define HCHIGH_MASK	0x0F	/* h/w click mask for low byte of hi word */
#define HCLOW_MASK	0xF0	/* h/w click mask for low byte of low word */
#define TEST1PATTERN	0x55	/* memory test pattern 1 */
#define TEST2PATTERN	0xAA	/* memory test pattern 2 */

/* Magic numbers for color status port. */
#define COLOR_STATUS_PORT	0x3DA
#	define VERTICAL_RETRACE_MASK	0x08

/* Offsets in struct proc. They MUST match proc.h. */
#define P_STACKBASE	0
#define ESREG		0
#define DSREG		2
#define DIREG		4
#define SIREG		6
#define BPREG		8
#define STREG		10	/* hole for another SP */
#define BXREG		12
#define DXREG		14
#define CXREG		16
#define AXREG		18
#define RETADR		20	/* return address for save() call */
#define PCREG		22
#define CSREG		24
#define PSWREG		26
#define SPREG		28
#define SSREG		30
#define P_STACKTOP	32

#if _WORD_SIZE == 4
/* The registers are twice as wide. There are 2 extra segment regs, but
 * these are packed in the double-width es and ds slots.
 */
EAXREG		=	2 * AXREG	/* use "=" to avoid parentheses */
ERETADR		=	2 * RETADR
EPCREG		=	2 * PCREG
ECSREG		=	2 * CSREG
EPSWREG		=	2 * PSWREG
P3_STACKTOP	=	2 * P_STACKTOP
#define P_LDT_SEL	P3_STACKTOP
#define Msize		18
#define SIZEOF_INT	4
#else
#define P_LDT_SEL	P_STACKTOP
#define Msize		12	/* size of a message in 16-bit words */
#define SIZEOF_INT	2
#endif

P_LDT		=	P_LDT_SEL + SIZEOF_INT
