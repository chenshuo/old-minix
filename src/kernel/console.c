/* Code and data for the IBM console driver. */

#include "kernel.h"
#include <sgtty.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include "protect.h"
#include "tty.h"

/* Definitions used by the console driver. */
#define C_VID_MASK    0x3FFF	/* mask for 16K video RAM */
#define M_VID_MASK    0x0FFF	/* mask for  4K video RAM */
#define C_RETRACE     0x0300	/* how many characters to display at once */
#define M_RETRACE     0x7000	/* how many characters to display at once */
#define BLANK         0x0700	/* determines  cursor color on blank screen */
#define LINE_WIDTH        80	/* # characters on a line */
#define SCR_LINES         25	/* # lines on the screen */
#define SCR_BYTES	8000	/* size video RAM. multiple of 2*LINE_WIDTH */
#define GO_FORWARD         0	/* scroll forward */
#define GO_BACKWARD        1	/* scroll backward */

/* Constants relating to the controller chips. */
#define M_6845         0x3B0	/* port for 6845 mono */
#define C_6845         0x3D0	/* port for 6845 color */
#define EGA            0x3C0	/* port for EGA card */
#define INDEX              4	/* 6845's index register */
#define DATA               5	/* 6845's data register */
#define CUR_SIZE          10	/* 6845's cursor size register */
#define VID_ORG           12	/* 6845's origin register */
#define CURSOR            14	/* 6845's cursor register */

/* Beeper. */
#define BEEP_FREQ     0x0533	/* value to put into timer to set beep freq */
#define B_TIME		   3	/* length of CTRL-G beep is ticks */

/* Global variables used by the console driver. */
PUBLIC int vid_mask;		/* 037777 for color (16K) or 07777 for mono */
PUBLIC int vid_port;		/* I/O port for accessing 6845 */
PUBLIC int blank_color = 0x0700; /* display code for blank */

/* Private variables used by the console driver. */
PRIVATE vid_retrace;		/* how many characters to display per burst */
PRIVATE int softscroll;		/* 1 = software scrolling, 0 = hardware */
PRIVATE unsigned vid_base;	/* base of video ram (0xB000 or 0xB800) */
PRIVATE int one_con_attribute;	/* current attribute byte << 8 */

/* Map from ANSI colors to the attributes used by the PC */
PRIVATE int ansi_colors[8] = {0, 4, 2, 6, 1, 5, 3, 7};

FORWARD _PROTOTYPE( void beep, (void) );
FORWARD _PROTOTYPE( void do_escape, (struct tty_struct *tp, int c) );
FORWARD _PROTOTYPE( void l_scr_up, (unsigned int base, int src, int dst,
								int count) );
FORWARD _PROTOTYPE( void l_scr_down, (unsigned int base, int src, int dst, 
								int count) );
FORWARD _PROTOTYPE( void long_vid_copy, (char *src, unsigned int base,
				unsigned int offset, unsigned int count) );
FORWARD _PROTOTYPE( void move_to, (struct tty_struct *tp, int x, int y) );
FORWARD _PROTOTYPE( void parse_escape, (struct tty_struct *tp, int c) );
FORWARD _PROTOTYPE( void scroll_screen, (struct tty_struct *tp, int dir) );
FORWARD _PROTOTYPE( void set_6845, (int reg, int val) );
FORWARD _PROTOTYPE( void stop_beep, (void) );
FORWARD _PROTOTYPE( void cons_org0, (void) );

/*===========================================================================*
 *				console					     *
 *===========================================================================*/
PUBLIC void console(tp)
register struct tty_struct *tp;	/* tells which terminal is to be used */
{
/* Copy as much data as possible to the output queue, then start I/O.  On
 * memory-mapped terminals, such as the IBM console, the I/O will also be
 * finished, and the counts updated.  Keep repeating until all I/O done.
 */

  int count;
  int remaining;
  register char *tbuf;

  /* Check quickly for nothing to do, so this can be called often without
   * unmodular tests elsewhere.
   */
  if ( (remaining = tp->tty_outleft) == 0 || tp->tty_inhibited == STOPPED)
	return;

  /* Copy the user bytes to tty_buf for decent addressing. Loop over the
   * copies, since the user buffer may be much larger than tty_buf.
   */
  do {
	if (remaining > sizeof tty_buf) remaining = sizeof tty_buf;
	phys_copy(tp->tty_phys, tty_bphys, (phys_bytes) remaining);
	tbuf = tty_buf;

	/* Output each byte of the copy to the screen.  This is the inner loop
	 * of the output routine, so attempt to make it fast. A more serious
	 * attempt should produce
	 *	while (easy_cases-- != 0 && *tbuf >= ' ') {
	 *		*outptr++ = *tbuf++;
	 *		*outptr++ = one_con_attribute;
	 *	}
	 */
	do {
		if (*tbuf < ' ' || tp->tty_esc_state > 0 ||
		    tp->tty_column >= LINE_WIDTH - 1 ||
		    tp->tty_rwords >= TTY_RAM_WORDS) {
			out_char(tp, *tbuf++);
		} else {
			tp->tty_ramqueue[tp->tty_rwords++] =
				one_con_attribute | (*tbuf++ & BYTE);
			tp->tty_column++;
		}
	}
	while (--remaining != 0 && tp->tty_inhibited == RUNNING);

	/* Update terminal data structure. */
	count = tbuf - tty_buf;	/* # characters printed */
	tp->tty_phys += count;	/* advance physical data pointer */
	tp->tty_cum += count;	/* number of characters printed */
	tp->tty_outleft -= count;
	if (remaining != 0) break;	/* inhibited while in progress */
  }
  while ( (remaining = tp->tty_outleft) != 0 && tp->tty_inhibited == RUNNING);
  flush(tp);			/* transfer anything buffered to the screen */

  /* If output was not inhibited early, send the appropiate completion reply.
   * Otherwise, let TTY handle suspension.
   */
  if (tp->tty_outleft == 0) finish(tp, tp->tty_cum);
}


/*===========================================================================*
 *				out_char				     *
 *===========================================================================*/
PUBLIC void out_char(tp, c)
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
		beep();
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
		if (tp->tty_mode & CRMOD) move_to(tp, 0, tp->tty_row);
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
				if (tp->tty_column >= LINE_WIDTH - 1 ||
				    tp->tty_rwords >= TTY_RAM_WORDS) {
					out_char(tp, ' ');
				} else {
					tp->tty_ramqueue[tp->tty_rwords++] =
						one_con_attribute | ' ';
					tp->tty_column++;
				}
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
#if !LINEWRAP
		if (tp->tty_column >= LINE_WIDTH) return;	/* long line */
#endif
#if LINEWRAP
 		if (tp->tty_column >= LINE_WIDTH) {
 			flush(tp);
 			if (tp->tty_row == SCR_LINES-1)
 				scroll_screen(tp, GO_FORWARD);
 			else
 				tp->tty_row++;
 			move_to(tp, 0, tp->tty_row);
 		}
#endif /* LINEWRAP */
		if (tp->tty_rwords == TTY_RAM_WORDS) flush(tp);
		tp->tty_ramqueue[tp->tty_rwords++]=one_con_attribute|(c&BYTE);
		tp->tty_column++;	/* next column */
		return;
  }
}

/*===========================================================================*
 *				scroll_screen				     *
 *===========================================================================*/
PRIVATE void scroll_screen(tp, dir)
register struct tty_struct *tp;	/* pointer to tty struct */
int dir;			/* GO_FORWARD or GO_BACKWARD */
{
  int offset, bytes;

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
		offset = tp->tty_org + bytes;
	} else {
		scr_down(vid_base,
			 (SCR_LINES - 1) * LINE_WIDTH * 2 - 2,
			 SCR_LINES * LINE_WIDTH * 2 - 2,
			 (SCR_LINES - 1) * LINE_WIDTH);
		offset = tp->tty_org;
	}
  } else {
	/* Use video origin, but don't assume EGA can wrap */
	if (dir == GO_FORWARD) {
		/* after we scroll by one line, end of screen */
		offset = tp->tty_org + (SCR_LINES + 1) * LINE_WIDTH * 2;
		if (offset > vid_mask && ega) {
			scr_up(vid_base, tp->tty_org + LINE_WIDTH * 2, 0, 
			       (SCR_LINES - 1) * LINE_WIDTH);
			tp->tty_org = 0;
		} else {
			tp->tty_org = (tp->tty_org + 2 * LINE_WIDTH) & vid_mask;
		}
		offset = (tp->tty_org + bytes) & vid_mask;
	} else {  /* scroll backwards */
		offset = tp->tty_org - 2 * LINE_WIDTH;
		if (offset < 0 && ega) {
			scr_down(vid_base, 
			   tp->tty_org + (SCR_LINES - 1) * LINE_WIDTH * 2 - 2,
			   vid_mask - 1,
			   (SCR_LINES - 1) * LINE_WIDTH);
			tp->tty_org = vid_mask + 1 - SCR_LINES*LINE_WIDTH * 2;
		} else {
			tp->tty_org = (tp->tty_org - 2 * LINE_WIDTH) & vid_mask;
		}
		offset = tp->tty_org;
	}			
	set_6845(VID_ORG, tp->tty_org >> 1);	/* 6845 thinks in words */
  }
  /* Blank the new line at top or bottom. */
  vid_copy(NIL_PTR, vid_base, offset, LINE_WIDTH);
}

/*===========================================================================*
 *				flush					     *
 *===========================================================================*/
PUBLIC void flush(tp)
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
PRIVATE void move_to(tp, x, y)
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
PRIVATE void parse_escape(tp, c)
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
PRIVATE void do_escape(tp, c)
register struct tty_struct *tp;	/* pointer to tty struct */
char c;				/* next character in escape sequence */
{
  int n, vx, value, attr, src, dst, count;

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
		attr = one_con_attribute;
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

		    /* cursor mods from dweaver@clover.cleaf.com 
		       (David Weaver) from comp.os.minix 12 Sep 1994
		       This adds full support for the ED and EL sequences.
		    */
	            case 'J':		/* ESC [sJ clears in display */
		      switch (value)
		      {
		        case 0: /* Clear from cursor to end of screen */
		          n = 2 * ((SCR_LINES - (tp->tty_row + 1)) * LINE_WIDTH
		     		       + LINE_WIDTH - (tp->tty_column));
		          vx = tp->tty_vid;
		          break;
		        case 1:	/* Clear from start of screen to cursor */
		          n = 2 * (tp->tty_row * LINE_WIDTH + tp->tty_column);
	  	          vx = tp->tty_vid -
		 	    (2 * (tp->tty_row * LINE_WIDTH + tp->tty_column));
	 	          break;
		        case 2:		/* Clear entire screen */
		          n = 2 * SCR_LINES * LINE_WIDTH;
		          vx = tp->tty_vid -
			    (2 * (tp->tty_row * LINE_WIDTH + tp->tty_column));
	  	          break;
	  	        default:	/* Do nothing */
		          n = 0;
	  	          vx = tp->tty_vid;
		      }
	  	      vid_copy(NIL_PTR, vid_base, vx, n / 2);
		      break;

		    case 'K':		/* ESC [sK clears in line */
		      switch (value)
		      {
	  	        case 0:		/* Clear from cursor to end of line */
		          n = 2 * (LINE_WIDTH - (tp->tty_column));
		          vx = tp->tty_vid;
		          break;
		        case 1:	/* Clear from beginning of line to cursor */
		      	  n = 2 * (tp->tty_column);
		          vx = tp->tty_vid - tp->tty_column;
		          break;
	 	        case 2:		/* Clear entire line */
		          n = 2 * LINE_WIDTH;
		          vx = tp->tty_vid;
	 	          break;
		        default:		/* Do nothing */
		          n = 0;
		          vx = tp->tty_vid;
		      }
		      vid_copy(NIL_PTR, vid_base, vx, n / 2);
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
			  long_vid_copy(NIL_PTR, vid_base, dst, n*LINE_WIDTH);
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
	 				one_con_attribute = /* yellow fg */
					 	(attr & 0xf0ff) | 0x0E00;
				else
		 			one_con_attribute |= 0x0800; /* inten*/
 				break;
 
 			    case 4: /*  UNDERLINE */
				if (color)
					one_con_attribute = /* lt green fg */
					 (attr & 0xf0ff) | 0x0A00;
				else
					one_con_attribute = /* ul */
					 (attr & 0x8900);
 				break;
 
 			    case 5: /*  BLINKING */
				if (color) /* can't blink color */
					one_con_attribute = /* magenta fg */
					 (attr & 0xf0ff) | 0x0500;
				else
		 			one_con_attribute |= /* blink */
					 0x8000;
 				break;
 
 			    case 7: /*  REVERSE  (black on light grey)  */
				if (color)
	 				one_con_attribute = 
					 ((attr & 0xf000) >> 4) |
					 ((attr & 0x0f00) << 4);
				else if ((attr & 0x7000) == 0)
					one_con_attribute =
					 (attr & 0x8800) | 0x7000;
				else
					one_con_attribute =
					 (attr & 0x8800) | 0x0700;
  				break;

 			    default: if (value >= 30 && value <= 37) {
					one_con_attribute = 
					 (attr & 0xf0ff) |
					 (ansi_colors[(value - 30)] << 8);
					blank_color = 
					 (blank_color & 0xf0ff) |
					 (ansi_colors[(value - 30)] << 8);
				} else if (value >= 40 && value <= 47) {
					one_con_attribute = 
					 (attr & 0x0fff) |
					 (ansi_colors[(value - 40)] << 12);
					blank_color =
					 (blank_color & 0x0fff) |
					 (ansi_colors[(value - 40)] << 12);
				} else
	 				one_con_attribute = blank_color;
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
PRIVATE void long_vid_copy(src, base, offset, count)
char *src;
unsigned int base, offset, count;
{
/* Break up a call to vid_copy for machines that can only write
 * during vertical retrace.  Vid_copy itself does the wait.
 */

  int ct;	

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
PRIVATE void l_scr_up(base, src, dst, count)
unsigned int base;
int src, dst, count;
{
/* Break up a call to scr_up for machines that can only write
 * during vertical retrace.  scr_up doesn't do the wait, so we do.
 * Note however that we keep interrupts on during the scr_up.  This
 * could lead to snow if an interrupt happens while we are doing
 * the display.  Sorry, but I don't see any good alternative.
 * Turning off interrupts makes us lose RS232 input chars.
 */

  int ct, wait;

  wait = color && ! ega;
  while (count > 0) {
	if (wait) wait_retrace();
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
PRIVATE void l_scr_down(base, src, dst, count)
unsigned int base;
int src, dst, count;
{
/* Break up a call to scr_down as for scr_up. */

  int ct, wait;

  wait = color && ! ega;
  while (count > 0) {
	if (wait) wait_retrace();
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
PRIVATE void set_6845(reg, val)
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
  lock();			/* try to stop h/w loading in-between value */
  out_byte(vid_port + INDEX, reg);	/* set the index register */
  out_byte(vid_port + DATA, (val>>8) & BYTE);	/* output high byte */
  out_byte(vid_port + INDEX, reg + 1);	/* again */
  out_byte(vid_port + DATA, val&BYTE);	/* output low byte */
  unlock();
}


/*===========================================================================*
 *				beep					     *
 *===========================================================================*/
PRIVATE int beeping = FALSE;

PRIVATE void beep()
{
/* Making a beeping sound on the speaker (output for CRTL-G).
 * This routine works by turning on the bits 0 and 1 in port B of the 8255
 * chip that drive the speaker.
 */

  message mess;

  if (beeping) return;
  out_byte(TIMER_MODE, 0xB6);	/* set up timer channel 2 (square wave) */
  out_byte(TIMER2, BEEP_FREQ & BYTE);	/* load low-order bits of frequency */
  out_byte(TIMER2, (BEEP_FREQ >> 8) & BYTE);	/* now high-order bits */
  lock();			/* guard PORT_B from keyboard intr handler */
  out_byte(PORT_B, in_byte(PORT_B) | 3);	/* turn on beep bits */
  unlock();
  beeping = TRUE;

  mess.m_type = SET_ALARM;
  mess.CLOCK_PROC_NR = TTY;
  mess.DELTA_TICKS = B_TIME;
  mess.FUNC_TO_CALL = (sighandler_t) stop_beep;
  sendrec(CLOCK, &mess);
}


/*===========================================================================*
 *				stop_beep				     *
 *===========================================================================*/
PRIVATE void stop_beep()
{
/* Turn off the beeper by turning off bits 0 and 1 in PORT_B. */

  lock();			/* guard PORT_B from keyboard intr handler */
  out_byte(PORT_B, in_byte(PORT_B) & ~3);
  beeping = FALSE;
  unlock();
}


/*===========================================================================*
 *				scr_init				     *
 *===========================================================================*/
PUBLIC void scr_init(minor)
int minor;
{
/* Initialize the screen driver. */

  one_con_attribute = BLANK;
  if (ps) softscroll = TRUE;

  /* Tell the EGA card, if any, to simulate a 16K CGA card. */
  out_byte(EGA + INDEX, 4);	/* register select */
  out_byte(EGA + DATA, 1);	/* no extended memory to be used */

  if (color) {
	vid_base = protected_mode ? COLOR_SELECTOR:physb_to_hclick(COLOR_BASE);
	vid_mask = C_VID_MASK;
	vid_port = C_6845;
	vid_retrace = C_RETRACE;
  } else {
	vid_base = protected_mode ? MONO_SELECTOR : physb_to_hclick(MONO_BASE);
	vid_mask = M_VID_MASK;
	vid_port = M_6845;
	vid_retrace = M_RETRACE;
  }

  if (ega) {
	vid_mask = C_VID_MASK;
	vid_retrace = C_VID_MASK + 1;
  }

  if (0)
  set_6845(CUR_SIZE, CURSOR_SHAPE);	/* set cursor shape */
  set_6845(VID_ORG, 0);			/* use page 0 of video ram */
  move_to(&tty_struct[0], 0, 0);	/* move cursor to upper left */
}

/*===========================================================================*
 *				putk					     *
 *===========================================================================*/
PUBLIC void putk(c)
int c;				/* character to print */
{
/* This procedure is used by the version of printf() that is linked with
 * the kernel itself.  The one in the library sends a message to FS, which is
 * not what is needed for printing within the kernel.  This version just queues
 * the character and starts the output.
 */

  if (c != 0) out_char(&tty_struct[0], (int) c);
}


/*===========================================================================*
 *				toggle_scroll				     *
 *===========================================================================*/
PUBLIC void toggle_scroll()
{
/* Toggle between hardware and software scroll. */

  cons_org0();
  softscroll = 1 - softscroll;
  printf("%sware scrolling enabled.\n", softscroll ? "Soft" : "Hard");
}


/*===========================================================================*
 *				cons_stop				     *
 *===========================================================================*/
PUBLIC void cons_stop()
{
/* Prepare for halt or reboot. */
  cons_org0();
  softscroll = 1;
}


/*===========================================================================*
 *				cons_org0				     *
 *===========================================================================*/
PRIVATE void cons_org0()
{
/* Reset the video origin. */
  struct tty_struct *cons;

  cons= &tty_struct[0];
  scr_up(vid_base, cons->tty_org, 0, SCR_LINES * LINE_WIDTH);
  cons->tty_vid -= cons->tty_org;
  cons->tty_org = 0;
  set_6845(VID_ORG, 0);
}


/*===========================================================================*
 *				con_loadfont				     *
 *===========================================================================*/

#define GA_SEQUENCER_INDEX	0x3C4
#define GA_SEQUENCER_DATA	0x3C5
#define GA_GRAPHICS_INDEX	0x3CE
#define GA_GRAPHICS_DATA	0x3CF
#define GA_VIDEO_ADDRESS	0xA0000L
#define GA_FONT_SIZE		8192

struct sequence {
	unsigned short index;
	unsigned char port;
	unsigned char value;
};

FORWARD _PROTOTYPE( void ga_program, (struct sequence *seq) );

PRIVATE void ga_program(seq)
struct sequence *seq;
{
  int len= 7;
  do {
	out_byte(seq->index, seq->port);
	out_byte(seq->index+1, seq->value);
	seq++;
  } while (--len > 0);
}

PUBLIC int con_loadfont(proc_nr, font_vir)
int proc_nr;
vir_bytes font_vir;
{
/* Load a font into the EGA or VGA adapter. */
  phys_bytes user_phys;
  static struct sequence seq1[7] = {
	{ GA_SEQUENCER_INDEX, 0x00, 0x01 },
	{ GA_SEQUENCER_INDEX, 0x02, 0x04 },
	{ GA_SEQUENCER_INDEX, 0x04, 0x07 },
	{ GA_SEQUENCER_INDEX, 0x00, 0x03 },
	{ GA_GRAPHICS_INDEX, 0x04, 0x02 },
	{ GA_GRAPHICS_INDEX, 0x05, 0x00 },
	{ GA_GRAPHICS_INDEX, 0x06, 0x00 },
  };
  static struct sequence seq2[7] = {
	{ GA_SEQUENCER_INDEX, 0x00, 0x01 },
	{ GA_SEQUENCER_INDEX, 0x02, 0x03 },
	{ GA_SEQUENCER_INDEX, 0x04, 0x03 },
	{ GA_SEQUENCER_INDEX, 0x00, 0x03 },
	{ GA_GRAPHICS_INDEX, 0x04, 0x00 },
	{ GA_GRAPHICS_INDEX, 0x05, 0x10 },
	{ GA_GRAPHICS_INDEX, 0x06,    0 },
  };

  seq2[6].value= color ? 0x0E : 0x0A;

  user_phys = numap(proc_nr, font_vir, GA_FONT_SIZE);
  if (user_phys == 0) return(EFAULT);
  if (!ega) return(ENOTTY);

  lock();
  ga_program(seq1);	/* bring font memory into view */

  phys_copy(user_phys, (phys_bytes)GA_VIDEO_ADDRESS, (phys_bytes)GA_FONT_SIZE);

  ga_program(seq2);	/* restore */
  unlock();

  return(OK);
}
