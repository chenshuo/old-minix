/*==========================================================================*
 *		rs232.c - serial driver for 8250 and 16450 UARTs 	    *
 *		Added support for Atari ST M68901 and YM-2149	--kub	    *
 *==========================================================================*/

#include "kernel.h"
#include <sgtty.h>
#include "tty.h"

#if (CHIP != INTEL) && (MACHINE != ATARI)
#error				/* rs232.c only supports PC/AT and Atari ST */
#endif

#if (MACHINE == ATARI)
#include "staddr.h"
#include "stsound.h"
#include "stmfp.h"
#if (NR_RS_LINES > 1)
#error				/* Only one physical RS232 line available */
#endif
#endif

/* Switches.
 * #define C_RS232_INT_HANDLERS to use the interrupt handlers in this file.
 * #define NO_HANDSHAKE to avoid requiring CTS for output.
 */

#if (CHIP == INTEL)		/* PC/AT 8250/16450 chip combination */

/* 8250 constants. */
#define UART_FREQ         115200L	/* timer frequency */

/* Interrupt enable bits. */
#define IE_RECEIVER_READY       1
#define IE_TRANSMITTER_READY    2
#define IE_LINE_STATUS_CHANGE   4
#define IE_MODEM_STATUS_CHANGE  8

/* Interrupt status bits. */
#define IS_MODEM_STATUS_CHANGE  0
#define IS_TRANSMITTER_READY    2
#define IS_RECEIVER_READY       4
#define IS_LINE_STATUS_CHANGE   6

/* Line control bits. */
#define LC_NO_PARITY            0
#define LC_DATA_BITS            3
#define LC_ODD_PARITY           8
#define LC_EVEN_PARITY       0x18
#define LC_ADDRESS_DIVISOR   0x80
#define LC_STOP_BITS_SHIFT      2

/* Line status bits. */
#define LS_OVERRUN_ERR          2
#define LS_PARITY_ERR           4
#define LS_FRAMING_ERR          8
#define LS_BREAK_INTERRUPT   0x10
#define LS_TRANSMITTER_READY 0x20

/* Modem control bits. */
#define MC_DTR                  1
#define MC_RTS                  2
#define MC_OUT2                 8	/* required for PC & AT interrupts */

/* Modem status bits. */
#define MS_CTS               0x10
#define MS_RLSD              0x80       /* Received Line Signal Detect */
#define MS_DRLSD             0x08       /* RLSD Delta */

#else /* MACHINE == ATARI */		/* Atari ST 68901 USART */

/* Most of the USART constants are already defined in stmfp.h . The local
 * definitions made here are for keeping C code changes smaller.   --kub
 */

#define UART_FREQ          19200L	/* timer frequency */

/* Line control bits. */
#define LC_NO_PARITY            0
#define LC_DATA_BITS            3
#define LC_ODD_PARITY       U_PAR
#define LC_EVEN_PARITY     (U_PAR|U_EVEN)

/* Line status bits. */
#define LS_OVERRUN_ERR       R_OE
#define LS_PARITY_ERR        R_PE
#define LS_FRAMING_ERR       R_FE
#define LS_BREAK_INTERRUPT   R_BREAK

/* Modem status bits. */
#define MS_CTS               IO_SCTS	/* 0x04 */

#endif /* CHIP/MACHINE */

#define DATA_BITS_SHIFT         8	/* amount data bits shifted in mode */
#define DEF_BAUD             1200	/* default baud rate */

/* Input buffer watermarks.
 * The external device is asked to stop sending when the buffer
 * exactly reaches high water, or when TTY requests it.
 * TTY is also notified directly (rather than at a later clock tick) when
 * this watermark is reached.
 * A lower threshold could be used, but the buffer size and wakeup intervals
 * are chosen so the watermark shouldn't be hit at reasonable baud rates,
 * so this is unnecessary - the throttle is applied when TTY's buffers
 * get too full.
 * The low watermark is invisibly 0 since the buffer is always emptied all
 * at once.
 */
#define RS_IHIGHWATER (3 * RS_IBUFSIZE / 4)

#if (CHIP == INTEL)

/* Macros to handle flow control.
 * Interrupts must be off when they are used.
 * Time is critical - already the function call for out_byte() is annoying.
 * If out_byte() can be done in-line, tests to avoid it can be dropped.
 * istart() tells external device we are ready by raising RTS.
 * istop() tells external device we are not ready by dropping RTS.
 * DTR is kept high all the time (it probably should be raised by open and
 * dropped by close of the device).
 * OUT2 is also kept high all the time.
 */
#define istart(rs) \
  (out_byte( (rs)->modem_ctl_port, MC_OUT2 | MC_RTS | MC_DTR), \
   (rs)->idevready = TRUE)
#define istop(rs) \
  (out_byte( (rs)->modem_ctl_port, MC_OUT2 | MC_DTR), (rs)->idevready = FALSE)

/* Macro to tell if device is ready.
 * Don't require DSR, since modems drop this to indicate the line is not
 * ready even when the modem itself is ready.
 * If NO_HANDSHAKE, read the status port to clear the interrupt, then force
 * the ready bit.
 */
#if NO_HANDSHAKE
# define devready(rs) (in_byte(rs->modem_status_port), MS_CTS)
#else
# define devready(rs) (in_byte(rs->modem_status_port) & MS_CTS)
#endif

/* Macro to tell if transmitter is ready. */
#define txready(rs) (in_byte(rs->line_status_port) & LS_TRANSMITTER_READY)

/* Macro to tell if carrier has dropped.
 * The RS232 Carrier Detect (CD) line is usually connected to the 8250
 * Received Line Signal Detect pin, reflected by bit MS_RLSD in the Modem
 * Status Register.  The MS_DRLSD bit tells if MS_RLSD has just changed state.
 * So if MS_DRLSD is set and MS_RLSD cleared, we know that carrier has just
 * dropped.
 */
#define devhup(rs)	\
	(in_byte(rs->modem_status_port) & (MS_RLSD|MS_DRLSD) == MS_DRLSD)

#else /* MACHINE == ATARI */

/* Macros to handle flow control.
 * Time is critical - already the function call for lock()/restore() is
 * annoying.
 * istart() tells external device we are ready by raising RTS.
 * istop() tells external device we are not ready by dropping RTS.
 * DTR is kept high all the time (it probably should be raised by open and
 * dropped by close of the device). NOTE: The modem lines are active low.
 */
#define set_porta(msk,val) { register int s = lock();		\
			     SOUND->sd_selr = YM_IOA;		\
			     SOUND->sd_wdat =			\
				SOUND->sd_rdat & (msk) | (val);	\
			     restore(s);	}
#define istart(rs)         { set_porta( ~(PA_SRTS|PA_SDTR),0 ); \
			     (rs)->idevready = TRUE;	}
#define istop(rs)          { set_porta( ~PA_SDTR, PA_SRTS );	\
			     (rs)->idevready = FALSE;	}

/* Macro to tell if device is ready.
 * Don't require DSR, since modems drop this to indicate the line is not
 * ready even when the modem itself is ready.
 * If NO_HANDSHAKE, force the ready bit.
 */
#if NO_HANDSHAKE
#define devready(rs)         MS_CTS
#else
#define devready(rs)         (~MFP->mf_gpip & MS_CTS)
#endif

/* Transmitter ready test */
#define txready(rs)          (MFP->mf_tsr & (T_EMPTY | T_UE))

#endif /* CHIP/MACHINE */

/* Types. */
typedef unsigned char bool_t;	/* boolean */

/* RS232 device structure, one per device. */
struct rs232_s {
  int minor;			/* minor number of this line (base 0) */

  bool_t idevready;		/* nonzero if we are ready to receive (RTS) */
  bool_t ittyready;		/* nonzero if TTY is ready to receive */
  char *ibuf;			/* start of input buffer */
  char *ibufend;		/* end of input buffer */
  char *ihighwater;		/* threshold in input buffer */
  char *iptr;			/* next free spot in input buffer */

  unsigned char ostate;		/* combination of flags: */
#define ODONE          1	/* output completed (< output enable bits) */
#define ORAW           2	/* raw mode for xoff disable (< enab. bits) */
#define OWAKEUP        4	/* tty_wakeup() pending (asm code only) */
#define ODEVREADY MS_CTS	/* external device hardware ready (CTS) */
#define OQUEUED     0x20	/* output buffer not empty */
#define OSWREADY    0x40	/* external device software ready (no xoff) */
#define ODEVHUP  MS_RLSD 	/* external device has dropped carrier */
#define OSOFTBITS  (ODONE | ORAW | OWAKEUP | OQUEUED | OSWREADY)
				/* user-defined bits */
#if (OSOFTBITS | ODEVREADY | ODEVHUP) == OSOFTBITS
				/* a weak sanity check */
#error				/* bits are not unique */
#endif
  unsigned char oxoff;		/* char to stop output */
  char *obufend;		/* end of output buffer */
  char *optr;			/* next char to output */

#if (CHIP == INTEL)
  port_t xmit_port;		/* i/o ports */
  port_t recv_port;
  port_t div_low_port;
  port_t div_hi_port;
  port_t int_enab_port;
  port_t int_id_port;
  port_t line_ctl_port;
  port_t modem_ctl_port;
  port_t line_status_port;
  port_t modem_status_port;
#endif

  unsigned char lstatus;	/* last line status */
  unsigned char pad;		/* ensure alignment for 16-bit ints */
  unsigned framing_errors;	/* error counts (no reporting yet) */
  unsigned overrun_errors;
  unsigned parity_errors;
  unsigned break_interrupts;

  char ibuf1[RS_IBUFSIZE + 1];	/* 1st input buffer, guard at end */
  char ibuf2[RS_IBUFSIZE + 1];	/* 2nd input buffer (for swapping) */
};

/* Table and macro to translate an RS232 minor device number to its
 * struct rs232_s pointer.
 */
struct rs232_s *p_rs_addr[NR_RS_LINES];

#define rs_addr(minor) (p_rs_addr[minor])

#if (CHIP == INTEL)
/* 8250 base addresses. */
PRIVATE port_t addr_8250[] = {
  0x3F8,			/* COM1: (line 0); COM3 might be at 0x3E8 */
  0x2F8,			/* COM2: (line 1); COM4 might be at 0x2E8 */
};
#endif

PUBLIC struct rs232_s rs_lines[NR_RS_LINES];

#if C_RS232_INT_HANDLERS
FORWARD _PROTOTYPE( int rs232_1handler, (int irq) );
FORWARD _PROTOTYPE( int rs232_2handler, (int irq) );
FORWARD _PROTOTYPE( void in_int, (struct rs232_s *rs) );
FORWARD _PROTOTYPE( void line_int, (struct rs232_s *rs) );
FORWARD _PROTOTYPE( void modem_int, (struct rs232_s *rs) );
#else
/* rs2.s */
PUBLIC _PROTOTYPE( int rs232_1handler, (int irq) );
PUBLIC _PROTOTYPE( int rs232_2handler, (int irq) );
#endif

FORWARD _PROTOTYPE( void out_int, (struct rs232_s *rs) );
FORWARD _PROTOTYPE( int rs_config, (int minor, int in_baud, int out_baud,
		int parity, int stop_bits, int data_bits, int mode) );


/* High level routines (should only be called by TTY). */

/*==========================================================================*
 *				rs_config			 	    *
 *==========================================================================*/
PRIVATE int rs_config(minor, in_baud, out_baud, parity, stop_bits, data_bits,
		      mode)
int minor;			/* which rs line */
int in_baud;			/* input speed: 110, 300, 1200, etc */
int out_baud;			/* output speed: 110, 300, 1200, etc */
int parity;			/* LC_something */
int stop_bits;			/* 2 (110 baud) or 1 (other speeds) */
int data_bits;			/* 5, 6, 7, or 8 */
int mode;			/* sgtty.h sg_mode word */
{
/* Set various line control parameters for RS232 I/O.
 * If DataBits == 5 and StopBits == 2, 8250 will generate 1.5 stop bits.
 * The 8250 can't handle split speed, but we have propagated both speeds
 * anyway for the benefit of future UART chips.
 */

  int divisor;
  int line_controls;
  register struct rs232_s *rs;

  rs = rs_addr(minor);

  /* Precalculate divisor and line_controls for reduced latency. */
  if (in_baud < 50) in_baud = DEF_BAUD;	/* prevent divide overflow */
  if (out_baud < 50) out_baud = DEF_BAUD;	/* prevent divide overflow */
  divisor = (int) (UART_FREQ / in_baud);	/* 8250 can't hack 2 speeds */
#if (CHIP == INTEL)
  switch(in_baud) {		/* HACK */
    case 19300: divisor = 1; break;	/* 115200 */
    case 19400: divisor = 2; break;	/* 57600 */
    case 19500: divisor = 3; break;	/* 38400 */
    case 19600: divisor = 4; break;	/* 28800 */
  }
  line_controls = parity | ((stop_bits - 1) << LC_STOP_BITS_SHIFT)
			 | (data_bits - 5);

  /* Lock out interrupts while setting the speed. The receiver register is
   * going to be hidden by the div_low register, but the input interrupt
   * handler relies on reading it to clear the interrupt and avoid looping
   * forever.
   */
  lock();

  /* Select the baud rate divisor registers and change the rate. */
  out_byte(rs->line_ctl_port, LC_ADDRESS_DIVISOR);
  out_byte(rs->div_low_port, divisor);
  out_byte(rs->div_hi_port, divisor >> 8);

  /* Change the line controls and reselect the usual registers. */
  out_byte(rs->line_ctl_port, line_controls);
#else /* MACHINE == ATARI */
  line_controls = parity | U_Q16;
  switch (stop_bits) {
	case 1:	line_controls |= U_ST1; break;
	case 2:	line_controls |= U_ST2; break;
  }
  switch (data_bits) {
	case 5:	line_controls |= U_D5; break;
	case 6:	line_controls |= U_D6; break;
	case 7:	line_controls |= U_D7; break;
	case 8:	line_controls |= U_D8; break;
  }
  lock();
  MFP->mf_ucr = line_controls;
  /* Check if baud rate is valid. Some decent baud rates may not work because
   * the timer cannot be programmed exactly enough, e.g. 7200. That is caught
   * by the expressions below which check that the resulting baud rate has a
   * deviation of max. 5%.
   */
  if (((UART_FREQ - (long)  divisor*in_baud >  UART_FREQ / 20) &&
       (UART_FREQ - (long)++divisor*in_baud < -UART_FREQ / 20))   ||
      divisor == 0 || divisor > 256)
	printf ("tty line %d: can't set speed %d\n", minor, in_baud);
  else
	MFP->mf_tddr = divisor;
#endif /* CHIP/MACHINE */

  if (mode & RAW)
	rs->ostate |= ORAW;
  else
	rs->ostate &= ~ORAW;
  unlock();
  return( (out_baud / 100) << 8) | (in_baud / 100);
}


/*==========================================================================*
 *				rs_ioctl			 	    *
 *==========================================================================*/
PUBLIC int rs_ioctl(minor, mode, speeds)
int minor;			/* which rs line */
int mode;			/* sgtty.h sg_mode word */
int speeds;			/* low byte is input speed, next is output */
{
/* Set the UART parameters. */

  int data_bits;
  int in_baud;
  int out_baud;
  int parity;
  int stop_bits;

  in_baud = 100 * (speeds & BYTE);
  if (in_baud == 100) in_baud = 110;
  out_baud = 100 * ((speeds >> 8) & BYTE);
  if (out_baud == 100) out_baud = 110;
  parity = LC_NO_PARITY;
  if (mode & ODDP) parity = LC_ODD_PARITY;
  if (mode & EVENP) parity = LC_EVEN_PARITY;
  stop_bits = in_baud == 110 ? 2 : 1;	/* not quite cricket */
  data_bits = 5 + ((mode >> DATA_BITS_SHIFT) & LC_DATA_BITS);
  return(rs_config(minor, in_baud, out_baud, parity, stop_bits, data_bits,
		     mode));
}


/*==========================================================================*
 *				rs_inhibit				    *
 *==========================================================================*/
PUBLIC void rs_inhibit(minor, inhibit)
int minor;			/* which rs line */
bool_t inhibit;			/* nonzero to inhibit, zero to uninhibit */
{
/* Update inhibition state to keep in sync with TTY. */

  register struct rs232_s *rs;

  rs = rs_addr(minor);
  lock();
  if (inhibit)
	rs->ostate &= ~OSWREADY;
  else
	rs->ostate |= OSWREADY;
  unlock();
}


/*==========================================================================*
 *				rs_init					    *
 *==========================================================================*/
PUBLIC int rs_init(minor)
int minor;			/* which rs line */
{
/* Initialize RS232 for one line. */

  register struct rs232_s *rs;
  int speed;
#if (CHIP == INTEL)
  port_t this_8250;
#endif

  rs = rs_addr(minor) = &rs_lines[minor];

  /* Record minor number. */
  rs->minor = minor;

  /* Set up input queue. */
  rs->iptr = rs->ibuf = rs->ibuf1;
  rs->ibufend = rs->ibuf1 + RS_IBUFSIZE;
  rs->ihighwater = rs->ibuf1 + RS_IHIGHWATER;
  rs->ittyready = TRUE;		/* idevready set to TRUE by istart() */

#if (CHIP == INTEL)
  /* Precalculate port numbers for speed. Magic numbers in the code (once). */
  this_8250 = addr_8250[minor];
  rs->xmit_port = this_8250 + 0;
  rs->recv_port = this_8250 + 0;
  rs->div_low_port = this_8250 + 0;
  rs->div_hi_port = this_8250 + 1;
  rs->int_enab_port = this_8250 + 1;
  rs->int_id_port = this_8250 + 2;
  rs->line_ctl_port = this_8250 + 3;
  rs->modem_ctl_port = this_8250 + 4;
  rs->line_status_port = this_8250 + 5;
  rs->modem_status_port = this_8250 + 6;
#endif

  /* Set up the hardware to a base state, in particular
   *	o turn off DTR (MC_DTR) to try to stop the external device.
   *	o be careful about the divisor latch.  Some BIOS's leave it enabled
   *	  here and that caused trouble (no interrupts) in version 1.5 by
   *	  hiding the interrupt enable port in the next step, and worse trouble
   *	  (continual interrupts) in an old version by hiding the receiver
   *	  port in the first interrupt.  Call rs_config() early to avoid this.
   *	o disable interrupts at the chip level, to force an edge transition
   *	  on the 8259 line when interrupts are next enabled and active.
   *	  RS232 interrupts are guaranteed to be disabled now by the 8259
   *	  mask, but there used to be trouble if the mask was set without
   *	  handling a previous interrupt.
   */
  istop(rs);			/* sets modem_ctl_port */
  speed = rs_config(minor, DEF_BAUD, DEF_BAUD, LC_NO_PARITY, 1, 8, RAW);
#if (CHIP == INTEL)
  out_byte(rs->int_enab_port, 0);
#endif

  /* Clear any harmful leftover interrupts.  An output interrupt is harmless
   * and will occur when interrupts are enabled anyway.  Set up the output
   * queue using the status from clearing the modem status interrupt.
   */
#if (CHIP == INTEL)
  in_byte(rs->line_status_port);
  in_byte(rs->recv_port);
#endif
  rs->ostate = devready(rs) | ORAW | OSWREADY;	/* reads modem_ctl_port */

#if (CHIP == INTEL)
  /* Enable interrupts for both interrupt controller and device. */
  if (minor & 1) {		/* COM2 on IRQ3 */
	put_irq_handler(SECONDARY_IRQ, rs232_2handler);
	enable_irq(SECONDARY_IRQ);
  } else {			/* COM1 on IRQ4 */
	put_irq_handler(RS232_IRQ, rs232_1handler);
	enable_irq(RS232_IRQ);
  }
  out_byte(rs->int_enab_port, IE_LINE_STATUS_CHANGE | IE_MODEM_STATUS_CHANGE
				| IE_RECEIVER_READY | IE_TRANSMITTER_READY);
#else /* MACHINE == ATARI */
  /* Initialize the 68901 chip, then enable interrupts. */
  MFP->mf_scr = 0x00;
  MFP->mf_tcdcr |= T_Q004;
  MFP->mf_rsr = R_ENA;
  MFP->mf_tsr = T_ENA;
  MFP->mf_aer = (MFP->mf_aer | (IO_SCTS|IO_SDCD)) ^
		 (MFP->mf_gpip & (IO_SCTS|IO_SDCD));
  MFP->mf_ddr = (MFP->mf_ddr & ~ (IO_SCTS|IO_SDCD));
  MFP->mf_iera |= (IA_RRDY|IA_RERR|IA_TRDY|IA_TERR);
  MFP->mf_imra |= (IA_RRDY|IA_RERR|IA_TRDY|IA_TERR);
  MFP->mf_ierb |= (IB_SCTS|IB_SDCD);
  MFP->mf_imrb |= (IB_SCTS|IB_SDCD);
#endif /* CHIP/MACHINE */

  /* Tell external device we are ready. */
  istart(rs);

  return(speed);
}


/*==========================================================================*
 *				rs_istop				    *
 *==========================================================================*/
PUBLIC void rs_istop(minor)
int minor;			/* which rs line */
{
/* TTY wants RS232 to stop input.
 * RS232 drops RTS but keeps accepting input until its buffer overflows.
 */

  register struct rs232_s *rs;

  rs = rs_addr(minor);
  lock();
  rs->ittyready = FALSE;
  istop(rs);
  unlock();
}


/*==========================================================================*
 *				rs_istart				    *
 *==========================================================================*/
PUBLIC void rs_istart(minor)
int minor;			/* which rs line */
{
/* TTY is ready for another buffer full of input from RS232.
 * RS232 raises RTS unless its own buffer is already too full.
 */

  register struct rs232_s *rs;

  rs = rs_addr(minor);
  lock();
  rs->ittyready = TRUE;
  if (rs->iptr < rs->ihighwater) istart(rs);
  unlock();
}


/*==========================================================================*
 *				rs_ocancel				    *
 *==========================================================================*/
PUBLIC void rs_ocancel(minor)
int minor;			/* which rs line */
{
/* Cancel pending output. */

  register struct rs232_s *rs;

  lock();
  rs = rs_addr(minor);
  if (rs->ostate & ODONE) tty_events -= EVENT_THRESHOLD;
  rs->ostate &= ~(ODONE | OQUEUED);
  unlock();
}


/*==========================================================================*
 *				rs_read					    *
 *==========================================================================*/
PUBLIC int rs_read(minor, bufindirect, odoneindirect)
int minor;			/* which rs line */
char **bufindirect;		/* where to return pointer to our buffer */
bool_t *odoneindirect;		/* where to return output-done status */
{
/* Swap the input buffers, giving the old one to TTY, and restart input. */

  register char *ibuf;
  int nread;
  register struct rs232_s *rs;

  rs = rs_addr(minor);
  *odoneindirect = rs->ostate & ODONE;
  if (rs->iptr == (ibuf = rs->ibuf)) return(0);
  *bufindirect = ibuf;
  lock();
  nread = rs->iptr - ibuf;
  tty_events -= nread;
  if (ibuf == rs->ibuf1)
	ibuf = rs->ibuf2;
  else
	ibuf = rs->ibuf1;
  rs->ibufend = ibuf + RS_IBUFSIZE;
  rs->ihighwater = ibuf + RS_IHIGHWATER;
  rs->iptr = ibuf;
  if (!rs->idevready && rs->ittyready) istart(rs);
  unlock();
  rs->ibuf = ibuf;
  return(nread);
}


/*==========================================================================*
 *				rs_setc					    *
 *==========================================================================*/
PUBLIC void rs_setc(minor, xoff)
int minor;			/* which rs line */
int xoff;			/* xoff character */
{
/* RS232 needs to know the xoff character. */

  rs_addr(minor)->oxoff = xoff;
}


/*==========================================================================*
 *				rs_write				    *
 *==========================================================================*/
PUBLIC void rs_write(minor, buf, nbytes)
int minor;			/* which rs line */
char *buf;			/* pointer to buffer to write */
int nbytes;			/* number of bytes to write */
{
/* Tell RS232 about the buffer to be written and start output.
 * Previous output must have completed.
 */

  register struct rs232_s *rs;

  rs = rs_addr(minor);
  lock();
  rs->obufend = (rs->optr = buf) + nbytes;
  rs->ostate |= OQUEUED;
  if (txready(rs)) out_int(rs);
  unlock();
}


/*==========================================================================*
 *				rs_dcd					    *
 *==========================================================================*/
PUBLIC int rs_dcd(minor)
int minor;
{
/* Return state of dcd line - could be a macro if PRIVATE */
  struct rs232_s *rs = rs_addr(minor);

  return((in_byte(rs->modem_status_port) & MS_RLSD) == 0);
}


/*==========================================================================*
 *                           rs_hangup                                   *
 *==========================================================================*/
PUBLIC int rs_hangup(minor)
int minor;                   /* which rs line */
{
/* Tell TTY whether a hangup has occured, and if so, clear ODEVHUP state. */

  struct rs232_s *rs = rs_addr(minor);
  int hangup = FALSE;

  lock();
  if ((rs->ostate & ODEVHUP) != 0) {
	rs->ostate &= ~ODEVHUP;
	--tty_events;
	hangup = TRUE;
  }
  unlock();
  return(hangup);
}


/* Low level (interrupt) routines. */

#if C_RS232_INT_HANDLERS
#if (CHIP == INTEL)
/*==========================================================================*
 *				rs232_1handler				    *
 *==========================================================================*/
PRIVATE int rs232_1handler(irq)
int irq;
{
/* Interrupt hander for IRQ4.
 * Only 1 line (usually COM1) should use it.
 */

#if NR_RS_LINES > 0
  register struct rs232_s *rs;

  rs = &rs_lines[0];
  while (TRUE) {
	/* Loop to pick up ALL pending interrupts for device.
	 * This usually just wastes time unless the hardware has a buffer
	 * (and then we have to worry about being stuck in the loop too long).
	 * Unfortunately, some serial cards lock up without this.
	 */
	switch (in_byte(rs->int_id_port)) {
	case IS_RECEIVER_READY:
		in_int(rs);
		continue;
	case IS_TRANSMITTER_READY:
		out_int(rs);
		continue;
	case IS_MODEM_STATUS_CHANGE:
		modem_int(rs);
		continue;
	case IS_LINE_STATUS_CHANGE:
		line_int(rs);
		continue;
	}
	return(1);	/* reenable serial interrupt */
  }
#endif
}


/*==========================================================================*
 *				rs232_2handler				    *
 *==========================================================================*/
PRIVATE int rs232_2handler(irq)
int irq;
{
/* Interrupt hander for IRQ3.
 * Only 1 line (usually COM2) should use it.
 */

#if NR_RS_LINES > 1
  register struct rs232_s *rs;

  rs = &rs_lines[1];
  while (TRUE) {
	switch (in_byte(rs->int_id_port)) {
	case IS_RECEIVER_READY:
		in_int(rs);
		continue;
	case IS_TRANSMITTER_READY:
		out_int(rs);
		continue;
	case IS_MODEM_STATUS_CHANGE:
		modem_int(rs);
		continue;
	case IS_LINE_STATUS_CHANGE:
		line_int(rs);
		continue;
	}
	return(1);	/* reenable serial interrupt */
  }
#endif
}
#else /* MACHINE == ATARI */
/*==========================================================================*
 *				siaint					    *
 *==========================================================================*/
PUBLIC void siaint(type)
int    type;	       /* interrupt type */
{
/* siaint is the rs232 interrupt procedure for Atari ST's. For ST there are
 * as much as 5 interrupt lines used for rs232. The trap type byte left on the
 * stack by the assembler interrupt handler identifies the interrupt cause.
 */

  register unsigned char  code;
  register struct rs232_s *rs = &rs_lines[0];
  int s = lock();

  switch (type & 0x00FF)
  {
	case 0x00:	       /* receive buffer full */
		in_int(rs);
		break;
	case 0x01:	       /* receive error */
		line_int(rs);
		break;
	case 0x02:	       /* transmit buffer empty */
		out_int(rs);
		break;
	case 0x03:	       /* transmit error */
		code = MFP->mf_tsr;
		if (code & ~(T_ENA | T_UE | T_EMPTY))
		{
		    printf("sia: transmit error: status=%x\r\n", code);
		    /* MFP->mf_udr = lastchar; */ /* retry */
		}
		break;
	case 0x04:		/* modem lines change */
		modem_int(rs);
		break;
  }
  restore(s);
}
#endif

/*==========================================================================*
 *				in_int					    *
 *==========================================================================*/
PRIVATE void in_int(rs)
register struct rs232_s *rs;	/* line with input interrupt */
{
/* Read the data which just arrived.
 * If it is the oxoff char, clear OSWREADY, else if OSWREADY was clear, set
 * it and restart output (any char does this, not just xon).
 * Put data in the buffer if room, otherwise discard it.
 * Set a flag for the clock interrupt handler to eventually notify TTY.
 */

#if (CHIP == INTEL)
  if (rs->ostate & ORAW)
	*rs->iptr = in_byte(rs->recv_port);
  else if ( (*rs->iptr = in_byte(rs->recv_port)) == rs->oxoff)
	rs->ostate &= ~OSWREADY;
#else /* MACHINE == ATARI */
  if (rs->ostate & ORAW)
	*rs->iptr = MFP->mf_udr;
  else if ( (*rs->iptr = MFP->mf_udr) == rs->oxoff)
	rs->ostate &= ~OSWREADY;
#endif /* CHIP/MACHINE */
  else if (!(rs->ostate & OSWREADY)) {
	rs->ostate |= OSWREADY;
	if (txready(rs)) out_int(rs);
  }
  if (rs->iptr < rs->ibufend) {
	++tty_events;
	if (++rs->iptr == rs->ihighwater) istop(rs);
  }
}


/*==========================================================================*
 *				line_int				    *
 *==========================================================================*/
PRIVATE void line_int(rs)
register struct rs232_s *rs;	/* line with line status interrupt */
{
/* Check for and record errors. */

#if (CHIP == INTEL)
  rs->lstatus = in_byte(rs->line_status_port);
#else /* MACHINE == ATARI */
  rs->lstatus = MFP->mf_rsr;
  MFP->mf_rsr &= R_ENA;
  rs->pad = MFP->mf_udr;	/* discard char in case of LS_OVERRUN_ERR */
#endif /* CHIP/MACHINE */
  if (rs->lstatus & LS_FRAMING_ERR) ++rs->framing_errors;
  if (rs->lstatus & LS_OVERRUN_ERR) ++rs->overrun_errors;
  if (rs->lstatus & LS_PARITY_ERR) ++rs->parity_errors;
  if (rs->lstatus & LS_BREAK_INTERRUPT) ++rs->break_interrupts;
}


/*==========================================================================*
 *				modem_int				    *
 *==========================================================================*/
PRIVATE void modem_int(rs)
register struct rs232_s *rs;	/* line with modem interrupt */
{
/* Get possibly new device-ready status, and clear ODEVREADY if necessary.
 * If the device just became ready, restart output.
 */

#if (MACHINE == ATARI)
  /* Set active edge interrupt so that next change causes a new interrupt */
  MFP->mf_aer = (MFP->mf_aer | (IO_SCTS|IO_SDCD)) ^
		 (MFP->mf_gpip & (IO_SCTS|IO_SDCD));
#endif

  if (devhup(rs)) {
	rs->ostate |= ODEVHUP;
	++tty_events;
  }

  if (!devready(rs))
	rs->ostate &= ~ODEVREADY;
  else if (!(rs->ostate & ODEVREADY)) {
	rs->ostate |= ODEVREADY;
	if (txready(rs)) out_int(rs);
  }
}

#endif /* C_RS232_INT_HANDLERS (except out_int is used from high level) */


/*==========================================================================*
 *				out_int					    *
 *==========================================================================*/
PRIVATE void out_int(rs)
register struct rs232_s *rs;	/* line with output interrupt */
{
/* If there is output to do and everything is ready, do it (local device is
 * known ready).
 * Interrupt TTY to indicate completion.
 */

  if (rs->ostate >= (ODEVREADY | OQUEUED | OSWREADY)) {
	/* Bit test allows ORAW and requires the others. */
#if (CHIP == INTEL)
	out_byte(rs->xmit_port, *rs->optr);
#else /* MACHINE == ATARI */
	MFP->mf_udr = *rs->optr;
#endif
	if (++rs->optr >= rs->obufend) {
		tty_events += EVENT_THRESHOLD;
		rs->ostate ^= (ODONE | OQUEUED);  /* ODONE on, OQUEUED off */
#if (CHIP == INTEL)
		unlock();	/* safe, for complicated reasons */
		tty_wakeup();
		lock();
#else
		tty_wakeup();	/* not save, for unknown reasons !?! */
#endif
	}
  }
}
