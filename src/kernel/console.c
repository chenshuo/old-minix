/* Code and data for the IBM console driver. */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/callnr.h"
#include "../h/com.h"
#include "../h/error.h"
#include "../h/sgtty.h"
#include "../h/signal.h"
#include "const.h"
#include "type.h"
#include "glo.h"
#include "proc.h"
#include "tty.h"

extern char alt_c[], unsh[], sh[], unm24[], m24[];
extern char dutch_unsh[], dutch_sh[], dutch_alt[];
extern char unsh_usx[], sh_usx[], scode_map[];

/* Definitions used by the console driver. */
#define COLOR_BASE    0xB800	/* video ram paragraph for color display */
#define MONO_BASE     0xB000	/* video ram address for mono display */
#define C_VID_MASK    0x3FFF	/* mask for 16K video RAM */
#define M_VID_MASK    0x0FFF	/* mask for  4K video RAM */
#define C_RETRACE     0x0300	/* how many characters to display at once */
#define M_RETRACE     0x7000	/* how many characters to display at once */
#define BEEP_FREQ     0x0533	/* value to put into timer to set beep freq */
#define B_TIME		   3	/* length of CTRL-G beep is ticks */
#define BLANK         0x0700	/* determines  cursor color on blank screen */
#define LINE_WIDTH        80	/* # characters on a line */
#define SCR_LINES         25	/* # lines on the screen */
#define SCR_BYTES	8000	/* size video RAM. multiple of 2*LINE_WIDTH */
#define CTRL_S            31	/* scan code for letter S (for CRTL-S) */
#define MONOCHROME         1	/* value for tty_ioport tells color vs. mono */
#define CONSOLE            0	/* line number for console */
#define GO_FORWARD         0	/* scroll forward */
#define GO_BACKWARD        1	/* scroll backward */
#define TIMER2          0x42	/* I/O port for timer channel 2 */
#define TIMER3          0x43	/* I/O port for timer channel 3 */
#define KEYBD           0x60	/* I/O port for keyboard data */
#define PORT_B          0x61	/* I/O port for 8255 port B */
#define KBIT            0x80	/* bit used to ack characters to keyboard */
#define LED_CODE        0xED	/* command to keyboard to set LEDs */
#define LED_DELAY       0x80	/* device dependent delay needed */

/* Constants relating to the video RAM and 6845. */
#define M_6845         0x3B0	/* port for 6845 mono */
#define C_6845         0x3D0	/* port for 6845 color */
#define EGA            0x3C0	/* port for EGA card */
#define INDEX              4	/* 6845's index register */
#define DATA               5	/* 6845's data register */
#define OVRFL_REG	   7    /* EGA overflow register */
#define CUR_SIZE          10	/* 6845's cursor size register */
#define VID_ORG           12	/* 6845's origin register */
#define CURSOR            14	/* 6845's cursor register */
#define LINE_CMP	0x18	/* EGA line compare register */

/* Definitions used for determining if the keyboard is IBM or Olivetti type. */
#define KB_STATUS	0x64	/* Olivetti keyboard status port */
#define BYTE_AVAIL	0x01	/* there is something in KEYBD port */
#define KB_BUSY	        0x02	/* KEYBD port ready to accept a command */
#define DELUXE		0x01	/* this bit is set up iff deluxe keyboard */
#define GET_TYPE	   5	/* command to get keyboard type */
#define STANDARD_SCAN	  13
#define OLIVETTI_SCAN     12	/* the '=' key is 12 on olivetti, 13 on IBM */
#define DUTCH_EXT_SCAN	  32	/* scan code of 'd' */
#define US_EXT_SCAN	  22	/* scan code of 'u' */
#define SPACE_SCAN	  57	/* a space */
#define PS_LED_DELAY    1200	/* delay for PS/2 */
#define PS_KEYBD	0x68	/* I/O port for data on ps/2 */

/* Scan codes to ASCII for IBM DUTCH extended keyboard */
#define MINUS_DU      0x0035	/* scan code of '-' on Dutch extended keybd */
#define NUM_SLASH_DU  0x0057	/* scan code of numeric keypad slash */

#define ESCAPE_CODE   0x00E0	/* escape scan code */

/* Global variables used by the console driver. */
PUBLIC  message keybd_mess;	/* message used for console input chars */
PUBLIC int vid_mask;		/* 037777 for color (16K) or 07777 for mono */
PUBLIC int vid_port;		/* I/O port for accessing 6845 */
PUBLIC int blank_color = 0x0700; /* display code for blank */
PRIVATE vid_retrace;		/* how many characters to display per burst */
PRIVATE unsigned vid_base;	/* base of video ram (0xB000 or 0xB800) */
PRIVATE int esc;		/* escape scan code detected? */

/* Map from ANSI colors to the attributes used by the PC */
PRIVATE int ansi_colors[8] = {0, 4, 2, 6, 1, 5, 3, 7};

/*===========================================================================*
 *				keyboard				     *
 *===========================================================================*/
PUBLIC keyboard()
{
/* A keyboard interrupt has occurred.  Process it. */

  int val, code, k, raw_bit;
  char stopc;

  /* Fetch the character from the keyboard hardware and acknowledge it. */
  if (ps) {
	port_in(PS_KEYBD, &code);	/* get the scan code for key struck */
	ack_char();			/* acknowledge the character */
  } else {
	port_in(KEYBD, &code);	/* get the scan code for the key struck */
	port_in(PORT_B, &val);	/* strobe the keyboard to ack the char */
	port_out(PORT_B, val | KBIT);	/* strobe the bit high */
	port_out(PORT_B, val);	/* now strobe it low */
  }

  /* The IBM keyboard interrupts twice per key, once when depressed, once when
   * released.  Filter out the latter, ignoring all but the shift-type keys.
   * The shift-type keys 29, 42, 54, 56, 58, and 69 must be processed normally.
   */

  if (keyb_type == DUTCH_EXT)
	if (esc) {
		/* Numeric slash gives scan codes 0xE0 0x35. */
		if (code == minus_code) code = num_slash;
		esc = FALSE;
	} else 
		esc = (code == ESCAPE_CODE);

  k = code - 0200;		/* codes > 0200 mean key release */
  if (k > 0) {
	/* A key has been released. */
	if (k != 29 && k != 42 && k != 54 && k != 56 && k != 58 && k != 69) {
		port_out(INT_CTL, ENABLE);	/* re-enable interrupts */
	 	return;		/* don't call tty_task() */
	}
  } else {
	/* Check to see if character is CTRL-S, to stop output. Setting xoff
	 * to anything other than CTRL-S will not be detected here, but will
	 * be detected later, in the driver.  A general routine to detect any
	 * xoff character here would be complicated since we only have the
	 * scan code here, not the ASCII character.
	 */
	raw_bit = tty_struct[CONSOLE].tty_mode & RAW;
	stopc = tty_struct[CONSOLE].tty_xoff;
	if (raw_bit == 0 && control && code == CTRL_S && stopc == XOFF_CHAR) {
		tty_struct[CONSOLE].tty_inhibited = STOPPED;
		port_out(INT_CTL, ENABLE);
		return;
	}
  }

  /* Check for CTRL-ALT-DEL, and if found, reboot the computer. */
  if (control && alt && code == DEL_CODE) reboot();	/* CTRL-ALT-DEL */

  /* Store the character in memory so the task can get at it later.
   * tty_driver_buf[0] is the current count, and tty_driver_buf[2] is the
   * maximum allowed to be stored.
   */
  if ( (k = tty_buf_count(tty_driver_buf)) < tty_buf_max(tty_driver_buf)) {
	/* There is room to store this character; do it. */
	k = k + k;			/* each entry contains two bytes */
	tty_driver_buf[k+4] = code;	/* store the scan code */
	tty_driver_buf[k+5] = CONSOLE;	/* tell which line it came from */
	tty_buf_count(tty_driver_buf)++;		/* increment counter */

	/* Build and send the interrupt message. */
	keybd_mess.m_type = TTY_CHAR_INT;
	keybd_mess.ADDRESS = tty_driver_buf;
	interrupt(TTY, &keybd_mess);	/* send a message to the tty task */
  } else {
	/* Too many characters have been buffered.  Discard excess. */
	port_out(INT_CTL, ENABLE);	/* re-enable 8259A controller */
  }
}


/*===========================================================================*
 *				console					     *
 *===========================================================================*/
PRIVATE console(tp)
register struct tty_struct *tp;	/* tells which terminal is to be used */
{
/* Copy as much data as possible to the output queue, then start I/O.  On
 * memory-mapped terminals, such as the IBM console, the I/O will also be
 * finished, and the counts updated.  Keep repeating until all I/O done.
 */

  extern char get_byte();
  int count;
  char c;
  unsigned segment, offset, offset1;

  /* Loop over the user bytes one at a time, outputting each one. */
  segment = (tp->tty_phys >> 4) & WORD_MASK;
  offset = tp->tty_phys & OFF_MASK;
  offset1 = offset;
  count = 0;

  while (tp->tty_outleft > 0 && tp->tty_inhibited == RUNNING) {
	c = get_byte(segment, offset);	/* fetch 1 byte from user space */
	out_char(tp, c);	/* write 1 byte to terminal */
	offset++;		/* advance one character in user buffer */
	tp->tty_outleft--;	/* decrement count */
  }
  flush(tp);			/* clear out the pending characters */

  /* Update terminal data structure. */
  count = offset - offset1;	/* # characters printed */
  tp->tty_phys += count;	/* advance physical data pointer */
  tp->tty_cum += count;		/* number of characters printed */

  /* If all data has been copied to the terminal, send the reply. */
  if (tp->tty_outleft == 0) finish(tp, tp->tty_cum);
}


/*===========================================================================*
 *				out_char				     *
 *===========================================================================*/
PUBLIC out_char(tp, c)
register struct tty_struct *tp;	/* pointer to tty struct */
char c;				/* character to be output */
{
/* Output a character on the console.  Check for escape sequences first. */

  if (tp->tty_esc_state > 0) {
	parse_escape(tp, c);
	return;
  }

  switch(c) {
	case 000:		/* null is typically used for padding */
		return;		/* better not do anything */
	case 007:		/* ring the bell */
		flush(tp);	/* print any chars queued for output */
		beep(BEEP_FREQ);/* BEEP_FREQ gives bell tone */
		return;

	case 013:		/* CTRL-K */
		move_to(tp, tp->tty_column, tp->tty_row - 1);
		return;

	case 014:		/* CTRL-L */
		move_to(tp, tp->tty_column + 1, tp->tty_row);
		return;

	case 016:		/* CTRL-N */
		move_to(tp, tp->tty_column + 1, tp->tty_row);
		return;

	case '\b':		/* backspace */
		move_to(tp, tp->tty_column - 1, tp->tty_row);
		return;

	case '\n':		/* line feed */
		if (tp->tty_mode & CRMOD) out_char(tp, '\r');
		if (tp->tty_row == SCR_LINES-1)
			scroll_screen(tp, GO_FORWARD);
		else
			tp->tty_row++;

		move_to(tp, tp->tty_column, tp->tty_row);
		return;

	case '\r':		/* carriage return */
		move_to(tp, 0, tp->tty_row);
		return;

	case '\t':		/* tab */
		if ( (tp->tty_mode & XTABS) == XTABS) {
			do {
				out_char(tp, ' ');
			} while (tp->tty_column & TAB_MASK);
			return;
		}
		/* Ignore tab if XTABS is off--video RAM has no hardware tab */
		return;

	case 033:		/* ESC - start of an escape sequence */
		flush(tp);	/* print any chars queued for output */
		tp->tty_esc_state = 1;	/* mark ESC as seen */
		return;

	default:		/* printable chars are stored in ramqueue */
#ifndef LINEWRAP
		if (tp->tty_column >= LINE_WIDTH) return;	/* long line */
#endif
		if (tp->tty_rwords == TTY_RAM_WORDS) flush(tp);
		tp->tty_ramqueue[tp->tty_rwords++]=tp->tty_attribute|(c&BYTE);
		tp->tty_column++;	/* next column */
#ifdef LINEWRAP
 		if (tp->tty_column >= LINE_WIDTH) {
 			flush(tp);
 			if (tp->tty_row == SCR_LINES-1)
 				scroll_screen(tp, GO_FORWARD);
 			else
 				tp->tty_row++;
 			move_to(tp, 0, tp->tty_row);
 		}
#endif /* LINEWRAP */
		return;
  }
}

/*===========================================================================*
 *				scroll_screen				     *
 *===========================================================================*/
PRIVATE scroll_screen(tp, dir)
register struct tty_struct *tp;	/* pointer to tty struct */
int dir;			/* GO_FORWARD or GO_BACKWARD */
{
  int amount, offset, bytes, old_state;

  flush(tp);
  bytes = 2 * (SCR_LINES - 1) * LINE_WIDTH;	/* 2 * 24 * 80 bytes */

  /* Scrolling the screen is a real nuisance due to the various incompatible
   * video cards.  This driver supports hardware scrolling (mono and CGA cards)
   * and software scrolling (EGA cards).
   */
  if (softscroll) {
	/* Software scrolling for non-IBM compatible EGA cards. */
	if (dir == GO_FORWARD) {
		scr_up(vid_base, LINE_WIDTH * 2, 0,
		       (SCR_LINES - 1) * LINE_WIDTH);
		vid_copy(NIL_PTR, vid_base, tp->tty_org+bytes, LINE_WIDTH);
	} else {
		scr_down(vid_base,
			 (SCR_LINES - 1) * LINE_WIDTH * 2 - 2,
			 SCR_LINES * LINE_WIDTH * 2 - 2,
			 (SCR_LINES - 1) * LINE_WIDTH);
		vid_copy(NIL_PTR, vid_base, tp->tty_org, LINE_WIDTH);
	}
  } else if (ega) {
	/* Use video origin, but don't assume the hardware can wrap */
	if (dir == GO_FORWARD) {
		/* after we scroll by one line, end of screen */
		offset = tp->tty_org + (SCR_LINES + 1) * LINE_WIDTH * 2;
		if (offset > vid_mask) {
			scr_up(vid_base, tp->tty_org + LINE_WIDTH * 2, 0, 
			       (SCR_LINES - 1) * LINE_WIDTH);
			tp->tty_org = 0;
		} else
			tp->tty_org += 2 * LINE_WIDTH;
		offset = tp->tty_org + bytes;
	} else {  /* scroll backwards */
		offset = tp->tty_org - 2 * LINE_WIDTH;
		if (offset < 0) {
			scr_down(vid_base, 
			   tp->tty_org + (SCR_LINES - 1) * LINE_WIDTH * 2 - 2,
			   vid_mask - 1,
			   (SCR_LINES - 1) * LINE_WIDTH);
			tp->tty_org = vid_mask + 1 - SCR_LINES*LINE_WIDTH * 2;
		} else
			tp->tty_org -= 2 * LINE_WIDTH;
		offset = tp->tty_org;
	}			
	/* Blank the new line at top or bottom. */
	vid_copy(NIL_PTR, vid_base, offset, LINE_WIDTH);
	set_6845(VID_ORG, tp->tty_org >> 1);	/* 6845 thinks in words */
  } else {
	/* Normal scrolling using the 6845 registers. */
	amount = (dir == GO_FORWARD ? 2 * LINE_WIDTH : -2 * LINE_WIDTH);
	tp->tty_org = (tp->tty_org + amount) & vid_mask;
	if (dir == GO_FORWARD)
		offset = (tp->tty_org + bytes) & vid_mask;
	else
		offset = tp->tty_org;

	/* Blank the new line at top or bottom. */
	vid_copy(NIL_PTR, vid_base, offset, LINE_WIDTH);
	set_6845(VID_ORG, tp->tty_org >> 1);	/* 6845 thinks in words */
  }
}

/*===========================================================================*
 *				flush					     *
 *===========================================================================*/
PUBLIC flush(tp)
register struct tty_struct *tp;	/* pointer to tty struct */
{
/* Have the characters in 'ramqueue' transferred to the screen. */

  if (tp->tty_rwords == 0) return;
  vid_copy((char *)tp->tty_ramqueue, vid_base, tp->tty_vid, tp->tty_rwords);

  /* Update the video parameters and cursor. */
  tp->tty_vid = (tp->tty_vid + 2 * tp->tty_rwords);
  set_6845(CURSOR, tp->tty_vid >> 1);	/* cursor counts in words */
  tp->tty_rwords = 0;
}


/*===========================================================================*
 *				move_to					     *
 *===========================================================================*/
PRIVATE move_to(tp, x, y)
struct tty_struct *tp;		/* pointer to tty struct */
int x;				/* column (0 <= x <= 79) */
int y;				/* row (0 <= y <= 24, 0 at top) */
{
/* Move the cursor to (x, y). */

  flush(tp);			/* flush any pending characters */
  if (x < 0 || x >= LINE_WIDTH || y < 0 || y >= SCR_LINES) return;
  tp->tty_column = x;		/* set x co-ordinate */
  tp->tty_row = y;		/* set y co-ordinate */
  tp->tty_vid = (tp->tty_org + 2*y*LINE_WIDTH + 2*x);

  set_6845(CURSOR, tp->tty_vid >> 1);	/* cursor counts in words */
}


/*===========================================================================*
 *				parse_escape				     *
 *===========================================================================*/
PRIVATE parse_escape(tp, c)
register struct tty_struct *tp;	/* pointer to tty struct */
char c;				/* next character in escape sequence */
{
/* The following ANSI escape sequences are currently supported.
 * If n and/or m are omitted, they default to 1.
 *   ESC [nA moves up n lines
 *   ESC [nB moves down n lines
 *   ESC [nC moves right n spaces
 *   ESC [nD moves left n spaces
 *   ESC [m;nH" moves cursor to (m,n)
 *   ESC [J clears screen from cursor
 *   ESC [K clears line from cursor
 *   ESC [nL inserts n lines ar cursor
 *   ESC [nM deletes n lines at cursor
 *   ESC [nP deletes n chars at cursor
 *   ESC [n@ inserts n chars at cursor
 *   ESC [nm enables rendition n (0=normal, 4=bold, 5=blinking, 7=reverse)
 *   ESC M scrolls the screen backwards if the cursor is on the top line
 */

  switch (tp->tty_esc_state) {
	case 1: 		/* ESC seen */
		tp->tty_esc_intro = '\0';
		tp->tty_esc_parmp = tp->tty_esc_parmv;
		tp->tty_esc_parmv[0] = tp->tty_esc_parmv[1] = 0;
		switch (c) {
		  case '[': 	/* Control Sequence Introducer */
			tp->tty_esc_intro = c;
			tp->tty_esc_state = 2; 
			break;
		  case 'M': 	/* Reverse Index */
			do_escape(tp, c);
			break;
		  default: 
			tp->tty_esc_state = 0; 
			break;
		}
		break;

	case 2: 		/* ESC [ seen */
		if (c >= '0' && c <= '9') {
			if (tp->tty_esc_parmp 
					< tp->tty_esc_parmv + MAX_ESC_PARMS)
				*tp->tty_esc_parmp =
				  *tp->tty_esc_parmp * 10 + (c - '0');
			break;
		}
		else if (c == ';') {
			if (++tp->tty_esc_parmp 
					< tp->tty_esc_parmv + MAX_ESC_PARMS)
				*tp->tty_esc_parmp = 0;
			break;
		}
		else {
			do_escape(tp, c);
		}
		break;
	default:		/* illegal state */
		tp->tty_esc_state = 0;
		break;
  }
}

/*===========================================================================*
 *				do_escape				     *
 *===========================================================================*/
PRIVATE do_escape(tp, c)
register struct tty_struct *tp;	/* pointer to tty struct */
char c;				/* next character in escape sequence */
{
  int n, ct, vx, value, attr, src, dst, count, limit, m;

  /* Some of these things hack on screen RAM, so it had better be up to date */
  flush(tp);

  /* Handle a sequence beginning with just ESC */
  if (tp->tty_esc_intro == '\0') {
	switch (c) {
		case 'M':		/* Reverse Index */
			if (tp->tty_row == 0)
				scroll_screen(tp, GO_BACKWARD);
			else
				tp->tty_row--;
			move_to(tp, tp->tty_column, tp->tty_row);
			break;

		default: break;
	}
  } else {
	/* Handle a sequence beginning with ESC [ and parameters */
	if (tp->tty_esc_intro == '[') {
		value = tp->tty_esc_parmv[0];
		attr = tp->tty_attribute;
		switch (c) {
		    case 'A': 		/* ESC [nA moves up n lines */
			n = (value == 0 ? 1 : value);
			move_to(tp, tp->tty_column, tp->tty_row - n);
			break;

	    	    case 'B':		/* ESC [nB moves down n lines */
			n = (value == 0 ? 1 : value);
			move_to(tp, tp->tty_column, tp->tty_row + n);
			break;

		    case 'C':		/* ESC [nC moves right n spaces */
			n = (value == 0 ? 1 : value);
			move_to(tp, tp->tty_column + n, tp->tty_row);
			break;

		    case 'D':		/* ESC [nD moves left n spaces */
			n = (value == 0 ? 1 : value);
			move_to(tp, tp->tty_column - n, tp->tty_row);
			break;

		    case 'H':		/* ESC [m;nH" moves cursor to (m,n) */
			move_to(tp, MAX(1, MIN(LINE_WIDTH, 
			    tp->tty_esc_parmv[1])) - 1,
			    MAX(1, MIN(SCR_LINES, tp->tty_esc_parmv[0])) - 1 );
			break;

		    case 'J':		/* ESC [J clears screen from cursor */
			if (value == 0) {
				n=2*((SCR_LINES-(tp->tty_row+1))*LINE_WIDTH
					      + LINE_WIDTH - (tp->tty_column));
				vx = tp->tty_vid;
				long_vid_copy(NIL_PTR,vid_base,vx,n/2);
			}
			break;

		    case 'K':		/* ESC [K clears line from cursor */
			if (value == 0) {
				n = 2 * (LINE_WIDTH - (tp->tty_column));
				vid_copy(NIL_PTR, vid_base, tp->tty_vid, n/2);
			}
			break;

		    case 'L':		/* ESC [nL inserts n lines ar cursor */
			n = value;
			if (n < 1) n = 1;
			if (n > (SCR_LINES - tp->tty_row)) 
				n = SCR_LINES - tp->tty_row;

			src = tp->tty_org+(SCR_LINES - n) * LINE_WIDTH * 2 - 2;
			dst = tp->tty_org+SCR_LINES * LINE_WIDTH * 2 - 2;
			count = (SCR_LINES - n - tp->tty_row) * LINE_WIDTH;
			l_scr_down(vid_base, src, dst, count);
			dst = tp->tty_org + tp->tty_row * LINE_WIDTH * 2;
			long_vid_copy(NIL_PTR, vid_base, dst, n * LINE_WIDTH);
			break;

		    case 'M':		/* ESC [nM deletes n lines at cursor */
			n = value;
			if (n < 1) n = 1;
			if (n > (SCR_LINES - tp->tty_row)) 
				n = SCR_LINES - tp->tty_row;

			src = tp->tty_org + (tp->tty_row + n) * LINE_WIDTH * 2;
			dst = tp->tty_org + (tp->tty_row) * LINE_WIDTH * 2;
			count = (SCR_LINES - n - tp->tty_row) * LINE_WIDTH;
			l_scr_up(vid_base, src, dst, count);
			dst = tp->tty_org + (SCR_LINES - n) * LINE_WIDTH * 2;
		        long_vid_copy(NIL_PTR, vid_base, dst, n * LINE_WIDTH);
			break;

		    case 'P':		/* ESC [nP deletes n chars at cursor */
			n = value;
			if (n < 1) n = 1;
			if (n > (LINE_WIDTH - tp->tty_column))
				n = LINE_WIDTH - tp->tty_column;
			src = (tp->tty_row * LINE_WIDTH + tp->tty_column+n) *2;
			dst = (tp->tty_row * LINE_WIDTH + tp->tty_column) * 2;
			count = LINE_WIDTH - tp->tty_column - n;
			src += tp->tty_org;
			dst += tp->tty_org;
			scr_up(vid_base, src, dst, count);
			vid_copy(NIL_PTR, vid_base, dst + count * 2, n);
			break;

		    case '@':  		/* ESC [n@ inserts n chars at cursor */
			n = value;
			if (n < 1) n = 1;
			if (n > (LINE_WIDTH - tp->tty_column))
				n = LINE_WIDTH - tp->tty_column;
			src = (tp->tty_row * LINE_WIDTH + LINE_WIDTH- n-1) * 2;
			dst = (tp->tty_row * LINE_WIDTH + LINE_WIDTH - 1) * 2;
			count = LINE_WIDTH - tp->tty_column - n;
			src += tp->tty_org;
			dst += tp->tty_org;
			scr_down(vid_base, src, dst, count);
			dst = (tp->tty_row * LINE_WIDTH + tp->tty_column) * 2;
			dst += tp->tty_org;
			vid_copy(NIL_PTR, vid_base, dst, n);
			break;

	   	    case 'm':		/* ESC [nm enables rendition n */
	 		switch (value) {
 			    case 1: /*  BOLD  */
				if (color)
	 				tp->tty_attribute = /* red fg */
					 	(attr & 0xf0ff) | 0x0400;
				else
		 			tp->tty_attribute |= 0x0800; /* inten*/
 				break;
 
 			    case 4: /*  UNDERLINE */
				if (color)
					tp->tty_attribute = /* blue fg */
					 (attr & 0xf0ff) | 0x0100;
				else
					tp->tty_attribute = /* ul */
					 (attr & 0x8900);
 				break;
 
 			    case 5: /*  BLINKING */
				if (color) /* can't blink color */
					tp->tty_attribute = /* magenta fg */
					 (attr & 0xf0ff) | 0x0500;
				else
		 			tp->tty_attribute |= /* blink */
					 0x8000;
 				break;
 
 			    case 7: /*  REVERSE  (black on light grey)  */
				if (color)
	 				tp->tty_attribute = 
					 ((attr & 0xf000) >> 4) |
					 ((attr & 0x0f00) << 4);
				else if ((attr & 0x7000) == 0)
					tp->tty_attribute =
					 (attr & 0x8800) | 0x7000;
				else
					tp->tty_attribute =
					 (attr & 0x8800) | 0x0700;
  				break;

 			    default: if (value >= 30 && value <= 37) {
					tp->tty_attribute = 
					 (attr & 0xf0ff) |
					 (ansi_colors[(value - 30)] << 8);
					blank_color = 
					 (blank_color & 0xf0ff) |
					 (ansi_colors[(value - 30)] << 8);
				} else if (value >= 40 && value <= 47) {
					tp->tty_attribute = 
					 (attr & 0x0fff) |
					 (ansi_colors[(value - 40)] << 12);
					blank_color =
					 (blank_color & 0x0fff) |
					 (ansi_colors[(value - 40)] << 12);
				} else
	 				tp->tty_attribute = blank_color;
  				break;
	 		}
			break;

	   	    default:
			break;
		}	/* closes switch(c) */
	}	/* closes if (tp->tty_esc_intro == '[') */
  }
  tp->tty_esc_state = 0;
}

/*===========================================================================*
 *				long_vid_copy				     *
 *===========================================================================*/
PRIVATE long_vid_copy(src, base, offset, count)
char *src;
unsigned int base, offset, count;
{
  int ct;	
/*
 * break up a call to vid_copy for machines that can only write
 * during vertical retrace.  Vid_copy itself does the wait.
 */

  while (count > 0) {
	ct = MIN (count, vid_retrace >> 1);
	vid_copy(src, base, offset, ct);
	if (src != NIL_PTR) src += ct * 2;
	offset += ct * 2;
	count -= ct;
  }
}

/*===========================================================================*
 *				long_src_up				     *
 *===========================================================================*/
PRIVATE l_scr_up(base, src, dst, count)
unsigned int base, src, dst, count;
{
  int ct, old_state, wait;

  /*
   * Break up a call to scr_up for machines that can only write
   * during vertical retrace.  scr_up doesn't do the wait, so we do.
   * Note however that we keep interrupts on during the scr_up.  This
   * could lead to snow if an interrupt happens while we are doing
   * the display.  Sorry, but I don't see any good alternative.
   * Turning off interrupts makes us loses RS232 input chars.
   */

  wait = color && ! ega;
  while (count > 0) {
	if (wait) {
		old_state = wait_retrace();
		restore(old_state);
	}
	ct = MIN (count, vid_retrace >> 1);
	scr_up(base, src, dst, ct);
	src += ct * 2;
	dst += ct * 2;
	count -= ct;
  }
}

/*===========================================================================*
 *				long_scr_down				     *
 *===========================================================================*/
PRIVATE l_scr_down(base, src, dst, count)
unsigned int base, src, dst, count;
{
  int ct, old_state, wait;

  /* Break up a call to scr_down for machines that can only write
   * during vertical retrace.  scr_down doesn't do the wait, so we do.
   * Note however that we keep interrupts on during the scr_down.  This
   * could lead to snow if an interrupt happens while we are doing
   * the display.  Sorry, but I don't see any good alternative.
   * Turning off interrupts makes us loses RS232 input chars.
   */

  wait = color && ! ega;
  while (count > 0) {
	if (wait) {
		old_state = wait_retrace();
		restore(old_state);
	}
	ct = MIN (count, vid_retrace >> 1);
	scr_down(base, src, dst, ct);
	src -= ct * 2;
	dst -= ct * 2;
	count -= ct;
  }
}

/*===========================================================================*
 *				set_6845				     *
 *===========================================================================*/
PRIVATE set_6845(reg, val)
int reg;			/* which register pair to set */
int val;			/* 16-bit value to set it to */
{
/* Set a register pair inside the 6845.  
 * Registers 10-11 control the format of the cursor (how high it is, etc).
 * Registers 12-13 tell the 6845 where in video ram to start (in WORDS)
 * Registers 14-15 tell the 6845 where to put the cursor (in WORDS)
 *
 * Note that registers 12-15 work in words, i.e. 0x0000 is the top left
 * character, but 0x0001 (not 0x0002) is the next character.  This addressing
 * is different from the way the 8088 addresses the video ram, where 0x0002
 * is the address of the next character.
 */
  port_out(vid_port + INDEX, reg);	/* set the index register */
  port_out(vid_port + DATA, (val>>8) & BYTE);	/* output high byte */
  port_out(vid_port + INDEX, reg + 1);	/* again */
  port_out(vid_port + DATA, val&BYTE);	/* output low byte */
}

/*===========================================================================*
 *				beep					     *
 *===========================================================================*/
PRIVATE int beeping = 0;
PRIVATE int stopbeep();

PRIVATE beep(f)
int f;				/* this value determines beep frequency */
{
/* Making a beeping sound on the speaker (output for CRTL-G).  The beep is
 * kept short, because interrupts must be disabled during beeping, and it
 * is undesirable to keep them off too long.  This routine works by turning
 * on the bits in port B of the 8255 chip that drive the speaker.
 */

  int k, s, x;
  message mess;

  if (beeping) return;
  s = lock();			/* disable interrupts */
  port_out(TIMER3,0xB6);	/* set up timer channel 2 mode */
  port_out(TIMER2, f&BYTE);	/* load low-order bits of frequency in timer */
  port_out(TIMER2,(f>>8)&BYTE);	/* now high-order bits of frequency in timer */
  port_in(PORT_B,&x);		/* acquire status of port B */
  port_out(PORT_B, x|3);	 /* turn bits 0 and 1 on to beep */
  beeping = 1;
  restore(s);			/* re-enable interrupts to previous state */

  mess.m_type = SET_ALARM;
  mess.CLOCK_PROC_NR = TTY;
  mess.DELTA_TICKS = B_TIME;
  mess.FUNC_TO_CALL = stopbeep;
  sendrec(CLOCK, &mess);
}

stopbeep() {
  int s, x;

  s = lock();			/* disable interrupts */
  port_in(PORT_B,&x);		/* acquire status of port B */
  port_out(PORT_B, x & 0xfffc);	/* turn bits 0 and 1 on to beep */
  beeping = 0;
  restore(s);			/* re-enable interrupts to previous state */
}

/*===========================================================================*
 *				set_leds				     *
 *===========================================================================*/
PUBLIC set_leds()
{
/* Set the LEDs on the caps lock and num lock keys */

  int count, leds, dummy, i, port;

  if (pc_at == 0 && !ps) return;	/* PC/XT doesn't have LEDs */
  leds = (numlock<<1) | (capslock<<2);	/* encode LED bits */

  if (ps) {
	port = PS_KEYBD;
	count = PS_LED_DELAY;
  } else {
	count = LED_DELAY;
	port = KEYBD;
  }

  port_out(port, LED_CODE);	/* prepare keyboard to accept LED values */
  port_in(port, &dummy);	/* keyboard sends ack; accept it */
  for (i = 0; i < count; i++) ;	/* delay needed */
  port_out(port, leds);		/* give keyboard LED values */
  port_in(port, &dummy);	/* keyboard sends ack; accept it */
}

/*===========================================================================*
 *				tty_init				     *
 *===========================================================================*/
PUBLIC tty_init()
{
/* Initialize the tty tables. */

  register struct tty_struct *tp;

  /* Set initial values. */
  caps_off = 1;
  num_off = 1;

  /* Tell the EGA card, if any, to simulate a 16K CGA card. */
  port_out(EGA + INDEX, 4);	/* register select */
  port_out(EGA + DATA, 1);	/* no extended memory to be used */

  for (tp = &tty_struct[0]; tp < &tty_struct[NR_CONS]; tp++) {
	tp->tty_inhead = tp->tty_inqueue;
	tp->tty_intail = tp->tty_inqueue;
	tp->tty_mode = CRMOD | XTABS | ECHO;
	tp->tty_devstart = console;
	tp->tty_makebreak = TWO_INTS;
	tp->tty_attribute = BLANK;
	tp->tty_erase = ERASE_CHAR;
	tp->tty_kill  = KILL_CHAR;
	tp->tty_intr  = INTR_CHAR;
	tp->tty_quit  = QUIT_CHAR;
	tp->tty_xon   = XON_CHAR;
	tp->tty_xoff  = XOFF_CHAR;
	tp->tty_eof   = EOT_CHAR;
  }

  if (color) {
	vid_base = COLOR_BASE;
	vid_mask = C_VID_MASK;
	vid_port = C_6845;
	vid_retrace = C_RETRACE;
  } else {
	vid_base = MONO_BASE;
	vid_mask = M_VID_MASK;
	vid_port = M_6845;
	vid_retrace = M_RETRACE;
  }

  if (ega) {
	vid_mask = C_VID_MASK;
	vid_retrace = C_VID_MASK + 1;
  }
  tty_buf_max(tty_driver_buf) = MAX_OVERRUN;	/* set up limit on keyboard buffering*/
  set_6845(CUR_SIZE, 31);		/* set cursor shape */
  set_6845(VID_ORG, 0);			/* use page 0 of video ram */
  move_to(&tty_struct[0], 0, SCR_LINES-1); /* move cursor to lower left */

  if (ps) {
	set_leds();		/* turn off numlock led */
	softscroll = TRUE;
  }

  /* Determine which keyboard type is attached.  The bootstrap program asks 
   * the user to type an '='.  The scan codes for '=' differ depending on the
   * keyboard in use.
   */
  switch(scan_code) {
	case STANDARD_SCAN:	keyb_type = IBM_PC; break;
	case OLIVETTI_SCAN: 	keyb_type = OLIVETTI; load_olivetti(); break;
	case DUTCH_EXT_SCAN:	keyb_type = DUTCH_EXT;
				load_dutch_table(); break;
	case US_EXT_SCAN:	keyb_type = US_EXT;
				load_us_ext(); break;
  }
}

/*===========================================================================*
 *			load_dutch_table				     *
 *===========================================================================*/
PRIVATE load_dutch_table()
{
/* Load the scan code to ASCII table for extended dutch keyboard. */

  register int i;

  for (i = 0; i < NR_SCAN_CODES; i++) {
	sh[i] = dutch_sh[i];
	unsh[i] = dutch_unsh[i];
	alt_c[i] = dutch_alt[i];
  }

  minus_code = MINUS_DU;
  num_slash = NUM_SLASH_DU;
}

/*===========================================================================*
 *			load_olivetti					     *
 *===========================================================================*/
PRIVATE load_olivetti()
{
/* Load the scan code to ASCII table for olivetti type keyboard. */

  register int i;

  for (i = 0; i < NR_SCAN_CODES; i++) {
	sh[i] = m24[i];
	unsh[i] = unm24[i];
  }
}

/*===========================================================================*
 *			load_us_ext					     *
 *===========================================================================*/
PRIVATE load_us_ext()
{
/* Load the scan code to ASCII table for US extended keyboard. */

  register int i;

  for (i = 0; i < NR_SCAN_CODES; i++) {
	sh[i] = sh_usx[i];
	unsh[i] = unsh_usx[i];
  }
}

/*===========================================================================*
 *				putc					     *
 *===========================================================================*/
PUBLIC putc(c)
char c;				/* character to print */
{
/* This procedure is used by the version of printf() that is linked with
 * the kernel itself.  The one in the library sends a message to FS, which is
 * not what is needed for printing within the kernel.  This version just queues
 * the character and starts the output.
 */

  out_char(&tty_struct[0], c);
}

/*===========================================================================*
 *				func_key				     *
 *===========================================================================*/
PUBLIC func_key(ch)
char ch;			/* scan code for a function key */
{
/* This procedure traps function keys for debugging purposes.  When MINIX is
 * fully debugged, it should be removed.
 */

  if (ch == F1) p_dmp();	/* print process table */
  if (ch == F2) map_dmp();	/* print memory map */
  if (ch == F3) {		/* hardware vs. software scrolling */
	softscroll = 1 - softscroll;	/* toggle scroll mode */
	tty_struct[0].tty_org = 0;
	move_to(&tty_struct[0], 0, SCR_LINES-1); /* cursor to lower left */
	set_6845(VID_ORG, 0);

	if (softscroll)
		printf("\033[H\033[JSoftware scrolling enabled.\n");
	else
		printf("\033[H\033[JHardware scrolling enabled.\n");
  }

#ifdef AM_KERNEL
#ifndef NONET
  if (ch == F4) net_init();	/* re-initialise the ethernet card */
#endif NONET
#endif AM_KERNEL
  if (ch == F9 && control) sigchar(&tty_struct[0], SIGKILL);	/* SIGKILL */
}
