int logit = 0; /*DEBUG*/
/* Keyboard driver for PC's and AT's. */

#include "kernel.h"
#include <sgtty.h>
#include <signal.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include "tty.h"

/* Standard and AT keyboard.  (PS/2 MCA implies AT throughout.) */
#define KEYBD		0x60	/* I/O port for keyboard data */

/* AT keyboard.  Most of these values are only used for rebooting. */
#define KB_COMMAND	0x64	/* I/O port for commands on AT */
#define KB_GATE_A20	0x02	/* bit in output port to enable A20 line */
#define KB_PULSE_OUTPUT	0xF0	/* base for commands to pulse output port */
#define KB_RESET	0x01	/* bit in output port to reset CPU */
#define KB_STATUS	0x64	/* I/O port for status on AT */

/* PS/2 model 30 keyboard. */
#define PS_KB_STATUS	0x72	/* I/O port for status on ps/2 (?) */
#define PS_KEYBD	0x68	/* I/O port for data on ps/2 */

/* AT and PS/2 model 30 keyboards. */
#define KB_ACK		0xFA	/* keyboard ack response */
#define KB_BUSY		0x02	/* status bit set when KEYBD port ready */
#define LED_CODE	0xED	/* command to keyboard to set LEDs */
#define MAX_KB_ACK_RETRIES 0x1000	/* max #times to wait for kb ack */
#define MAX_KB_BUSY_RETRIES 0x1000	/* max #times to loop while kb busy */

/* All keyboards. */
#define KBIT		0x80	/* bit used to ack characters to keyboard */

/* Scan codes whose action is not completely captured by maps. */
#define DEL_SCAN	  83	/* DEL for use in CTRL-ALT-DEL reboot */
#define DUTCH_EXT_SCAN	  32	/* 'd' */
#define ESCAPE_CODE	0xE0	/* beginning of escape sequence */
#define F1		  59	/* function key F1, others follow */
#define F2		  60
#define F3		  61
#define F4		  62
#define F5		  63
#define F6		  64
#define F7		  65
#define F8		  66
#define F9		  67
#define F10		  68
#define MINUS_DU	0x35	/* '-' on Dutch extended keybd */
#define NUM_SLASH_DU	0x57	/* numeric keypad slash on Dutch extended kb */
#define OLIVETTI_SCAN	  12	/* '=' key on olivetti */
#define SCODE1		  71	/* Home on numeric pad */
#define SCODE2		  81	/* PgDn on numeric pad */
#define STANDARD_SCAN	  13	/* '=' key on IBM */
#define TOP_ROW		  14	/* codes below this are shifted if CTRL */
#define US_EXT_SCAN	  22	/* 'u' */

#define NR_SCAN_CODES	0x69	/* Number of scan codes */

/* Keyboard types. */
#define IBM_PC		   1	/* Standard IBM keyboard */
#define OLIVETTI	   2	/* Olivetti keyboard */
#define DUTCH_EXT	   3	/* Dutch extended IBM keyboard */
#define US_EXT		   4	/* U.S. extended keyboard */

/* Miscellaneous. */
#define CTRL              29	/* scan code for CTRL */
#define CAPSLOCK          58	/* scan code for Caps lock */
#define CTRL_S		  31	/* scan code for letter S (for CRTL-S) */
#define CONSOLE		   0	/* line number for console */
#define MEMCHECK_ADR   0x472	/* address to stop memory check after reboot */
#define MEMCHECK_MAG  0x1234	/* magic number to stop memory check */

#define kb_addr(n)	(&kb_lines[CONSOLE])	/* incorrectly ignore n */
#define KB_IBUFSIZE	  32	/* size of keyboard input buffer */

PRIVATE int alt;		/* alt key state */
PRIVATE int capslock;		/* caps lock key state */
PRIVATE int esc;		/* escape scan code detected? */
PRIVATE int control;		/* control key state */
PRIVATE int caps_off;		/* 1 = normal position, 0 = depressed */
PRIVATE int keyb_type;		/* type of keyboard attached */
PRIVATE int minus_code;		/* numeric minus on dutch extended keyboard */
PRIVATE int numlock;		/* number lock key state */
PRIVATE int num_off;		/* 1 = normal position, 0 = depressed */
PRIVATE int num_slash;		/* numeric slash on dutch extended keyboard */
PRIVATE int shift1;		/* left shift key state */
PRIVATE int shift2;		/* right shift key state */

/* Scan codes to ASCII for alt keys (IBM Extended keyboard) */
PRIVATE char alt_c[NR_SCAN_CODES];

/* Scan codes to ASCII for unshifted keys for IBM-PC (default) */
PRIVATE char unsh[NR_SCAN_CODES] = {
 0,033,'1','2','3','4','5','6',        '7','8','9','0','-','=','\b','\t',
 'q','w','e','r','t','y','u','i',      'o','p','[',']',015,0202,'a','s',
 'd','f','g','h','j','k','l',';',      047,0140,0200,0134,'z','x','c','v',
 'b','n','m',',','.','/',0201,'*',     0203,' ',0204,0241,0242,0243,0244,0245,
 0246,0247,0250,0251,0252,0205,0210,0267,  0270,0271,0211,0264,0265,0266,0214
,0261,0262,0263,'0',0177,0,0,0,0,	0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0
};

/* Scan codes to ASCII for shifted keys */
PRIVATE char sh[NR_SCAN_CODES] = {
 0,033,'!','@','#','$','%','^',        '&','*','(',')','_','+','\b','\t',
 'Q','W','E','R','T','Y','U','I',      'O','P','{','}',015,0202,'A','S',
 'D','F','G','H','J','K','L',':',      042,'~',0200,'|','Z','X','C','V',
 'B','N','M','<','>','?',0201,'*',    0203,' ',0204,0221,0222,0223,0224,0225,
 0226,0227,0230,0231,0232,0204,0213,'7',  '8','9',0211,'4','5','6',0214,'1',
 '2','3','0','.',0,0,0,0,		0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0
};

/* Scan codes to ASCII for Olivetti M24 for unshifted keys. */
PRIVATE char unm24[NR_SCAN_CODES] = {
 0,033,'1','2','3','4','5','6',        '7','8','9','0','-','^','\b','\t',
 'q','w','e','r','t','y','u','i',      'o','p','@','[','\r',0202,'a','s',
 'd','f','g','h','j','k','l',';',      ':',']',0200,'\\','z','x','c','v',
 'b','n','m',',','.','/',0201,'*',     0203,' ',0204,0241,0242,0243,0244,0245,
0246,0247,0250,0251,0252,023,0210,0267,0270,0271,0211,0264,0265,0266,0214,0261,
0262,0263,'0','.',' ',014,0212,'\r',   0264,0262,0266,0270,032,0213,' ','/',
0253,0254,0255,0256,0257,0215,0216,0217
};

/* Scan codes to ASCII for Olivetti M24 for shifted keys. */
PRIVATE char m24[NR_SCAN_CODES] = {
 0,033,'!','"','#','$','%','&',        047,'(',')','_','=','~','\b','\t',
 'Q','W','E','R' ,'T','Y','U','I',     'O','P',0140,'{','\r',0202,'A','S',
 'D','F','G','H','J','K','L','+',      '*','}',0200,'|','Z','X','C','V',
 'B','N','M','<','>','?',0201,'*',     0203,' ',0204,0221,0222,0223,0224,0225,
 0226,0227,0230,0231,0232,0270,023,'7', '8','9',0211,'4','5','6',0214,'1',
 '2','3',0207,0177,0271,014,0272,'\r', '\b','\n','\f',036,032,0273,0274,'/',
 0233,0234,0235,0236,0237,0275,0276,0277
};

PRIVATE char dutch_unsh[NR_SCAN_CODES] = {
 0,033,'1','2','3','4','5','6',		'7','8','9','0','/',0370,'\b','\t',
 'q','w','e','r','t','y','u','i',	'o','p',0,'*','\r',0202,'a','s',
 'd','f','g','h','j','k','l','+',	'\'',0100,0200,'<','z','x','c','v',
 'b','n','m',',','.','-',0201,'*',	0203,' ',0204,0,0,0,0,0,
 0,0,0,0,0,0205,0,183,			184,185,137,180,'5',182,140,177,
 178,179,0,0177,0,0,']','/',0,		0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0
};

PRIVATE char dutch_sh[NR_SCAN_CODES] = {
 0,033,'!','\"','#','$','%','&',	'_','(',')','\'','?',0176,'\b','\t',
 'Q','W','E','R','T','Y','U','I',	'O','P','^','|','\r',0202,'A','S',
 'D','F','G','H','J','K','L',0361,	0,025,0200,'>','Z','X','C','V',
 'B','N','M',';',':','=',0201,'*',	0203,' ',0204,0,0,0,0,0,
 0,0,0,0,0,0205,0,'7',			'8','9',137,'4','5','6',140,'1',
 '2','3','0',',',0,0,'[','/',0,		0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0
};

/* Code table for alt key */
PRIVATE char dutch_alt[NR_SCAN_CODES] = {
 0,0,0,253,0,172,171,0,		156,'{','}',0,'\\',0,0,0,
 0,0,0,0,0,0,0,0,		0,0,0,0,0,0202,0,0,
 0,0,0,0,0,0,0,0,		0,170,0200,0,174,175,0,0,
 0,0,0,0,0,0,0201,0,		0203,0,0204,0,0,0,0,0,
 0,0,0,0,0,0205,0,0,		0,0,0,0,0,0,0,0,
 0,0,0,0177,0,0,'|',0,0,	0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0
};

/* Unshifted U.S. extended kbd */
PRIVATE char unsh_usx[NR_SCAN_CODES] = {
 0,'`','1','2','3','4','5','6',        '7','8','9','0','-','=','\b','\t',
 'q','w','e','r','t','y','u','i',      'o','p','[',']',015,0202,'a','s',
 'd','f','g','h','j','k','l',';',      047,033,0200,0134,'z','x','c','v',
 'b','n','m',',','.','/',0201,'*',     0203,' ',0202,0241,0242,0243,0244,0245,
 0246,0247,0250,0251,0252,0205,0210,0267,  0270,0271,0211,0264,0265,0266,0214
,0261,0262,0263,'0',0177,0,0,0,0,	0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0
};

/* Shifted U.S. extended kbd */
PRIVATE char sh_usx[NR_SCAN_CODES] = {
 0,033,'!','@','#','$','%','^',        '&','*','(',')','_','+','\b','\t',
 'Q','W','E','R','T','Y','U','I',      'O','P','{','}',015,0202,'A','S',
 'D','F','G','H','J','K','L',':',      042,'~',0200,'|','Z','X','C','V',
 'B','N','M','<','>','?',0201,'*',    0203,' ',0202,0221,0222,0223,0224,0225,
 0226,0227,0230,0231,0232,0204,0213,'7',  '8','9',0211,'4','5','6',0214,'1',
 '2','3','0','.',0,0,0,0,		0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0
};

PRIVATE char scode_map[] =
		{'H', 'A', 'V', 'S', 'D', 'G', 'C', 'T', 'Y', 'B', 'U'};

/* Keyboard structure, 1 per console. */
struct kb_s {
  int minor;			/* minor number of this line (base 0) */

  char *ibuf;			/* start of input buffer */
  char *ibufend;		/* end of input buffer */
  char *iptr;			/* next free spot in input buffer */

  char ibuf1[KB_IBUFSIZE + 1];	/* 1st input buffer, guard at end */
  char ibuf2[KB_IBUFSIZE + 1];	/* 2nd input buffer (for swapping) */
};

PRIVATE struct kb_s kb_lines[NR_CONS];

FORWARD _PROTOTYPE( int kb_ack, (int data_port) );
FORWARD _PROTOTYPE( int kb_wait, (int status_port) );
FORWARD _PROTOTYPE( void load_dutch_table, (void) );
FORWARD _PROTOTYPE( void load_olivetti, (void) );
FORWARD _PROTOTYPE( void load_us_ext, (void) );
FORWARD _PROTOTYPE( int scan_keyboard, (void) );
FORWARD _PROTOTYPE( void set_leds, (void) );
FORWARD _PROTOTYPE( void reboot, (void) );
FORWARD _PROTOTYPE( void waitkey, (char *prompt) );

/*===========================================================================*
 *				keyboard				     *
 *===========================================================================*/
PUBLIC void keyboard()
{
/* A keyboard interrupt has occurred.  Process it. */

  int code, k;
  register struct kb_s *kb;

  /* Fetch the character from the keyboard hardware and acknowledge it. */
  code = scan_keyboard();

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
	if (k != 29 && k != 42 && k != 54 && k != 56 && k != 58 && k != 69)
		return;		/* don't call tty_task() */
  } else {
	/* Check to see if character is CTRL-S, to stop output. Setting xoff
	 * to anything other than CTRL-S will not be detected here, but will
	 * be detected later, in the driver.  A general routine to detect any
	 * xoff character here would be complicated since we only have the
	 * scan code here, not the ASCII character.
	 */
	if (!(tty_struct[CONSOLE].tty_mode & RAW) &&
	    control && code == CTRL_S &&
	    tty_struct[CONSOLE].tty_xoff == XOFF_CHAR) {
		tty_struct[CONSOLE].tty_inhibited = STOPPED;
		return;
	}
  }

  /* Call debugger? (as early as practical, not in TTY which may be hung) */
  if (code == F10 && db_exists) {
	db_enabled = TRUE;
	db();
  }

  /* Store the character in memory so the task can get at it later. */
  kb = kb_addr(-NR_CONS);
  *kb->iptr = code;
  if (kb->iptr < kb->ibufend) {
	lock();			/* protect shared variable */
	tty_events += EVENT_THRESHOLD;	/* C doesn't guarantee atomic */
	unlock();
	++kb->iptr;
  }
  /* Else it doesn't fit - discard it. */
}


/*==========================================================================*
 *				kb_read					    *
 *==========================================================================*/
PUBLIC int kb_read(minor, bufindirect, odoneindirect)
int minor;
char **bufindirect;
unsigned char *odoneindirect;
{
/* Swap the keyboard input buffers, giving the old one to TTY. */

  register char *ibuf;
  register struct kb_s *kb;
  int nread;

  kb = kb_addr(minor);
  *odoneindirect = FALSE;
  if (kb->iptr == (ibuf = kb->ibuf)) return 0;
  *bufindirect = ibuf;
  lock();
  nread = kb->iptr - ibuf;
  tty_events -= nread * EVENT_THRESHOLD;
  if (ibuf == kb->ibuf1)
	ibuf = kb->ibuf2;
  else
	ibuf = kb->ibuf1;
  kb->ibufend = ibuf + KB_IBUFSIZE;
  kb->iptr = ibuf;
  unlock();
  kb->ibuf = ibuf;
  return nread;
}


/*===========================================================================*
 *				letter_code				     *
 *===========================================================================*/
PUBLIC int letter_code(scode)
int scode;			/* scan code from key press */
{
/* Convert scan codes from numeric keypad to letters for use in escape seqs. */

  if (scode >= SCODE1 && scode <= SCODE2 && (shift1 || shift2 || !numlock))
	return scode_map[scode - SCODE1];
  return 0;
}


/*===========================================================================*
 *				make_break				     *
 *===========================================================================*/
PUBLIC int make_break(ch)
char ch;			/* scan code of key just struck or released */
{
/* This routine can handle keyboards that interrupt only on key depression,
 * as well as keyboards that interrupt on key depression and key release.
 * For efficiency, the interrupt routine filters out most key releases.
 */

  int c, make, code;

  /* Check for CTRL-ALT-DEL, and if found, reboot the computer. This would
   * be better done in keyboard() in case TTY is hung, except control and
   * alt are set in the high level code.
   */
  if (control && alt && ch == DEL_SCAN) reboot();	/* CTRL-ALT-DEL */

  c = ch & 0177;		/* high-order bit set on key release */
  make = (ch & 0200 ? 0 : 1);	/* 1 when key depressed, 0 when key released */

  /* Until IBM invented the 101-key keyboard, the CTRL key was always to the
   * left of the 'A'.  This fix puts it back there on the 101-key keyboard.
   */
#if KEYBOARD_84
  if (c == CTRL) 
	c = CAPSLOCK;
  else if (c == CAPSLOCK)
	c = CTRL;
#endif

  if (alt && keyb_type == DUTCH_EXT)
	code = alt_c[c];
  else
	code = (shift1 || shift2 ? sh[c] : unsh[c]);

  if (control && c < TOP_ROW) code = sh[c];	/* CTRL-(top row) */

  if (c >= SCODE1 && c <= SCODE2 + 2)	/* numeric pad including DEL, INS */
	code = (shift1 || shift2 || !numlock ? unsh[c] : sh[c]);

  code &= BYTE;
  if (code < 0200 || code >= 0206) {
	/* Ordinary key, i.e. not shift, control, alt, etc. */
	if (capslock)
		if (code >= 'A' && code <= 'Z')
			code += 'a' - 'A';
		else if (code >= 'a' && code <= 'z')
			code -= 'a' - 'A';
	if (alt && keyb_type != DUTCH_EXT) code |= 0200;  /* alt ORs in 0200 */
	if (control) code &= 037;
	if (make == 0) code = -1;	/* key release */
	return(code);
  }

  /* Table entries 0200 - 0206 denote special actions. */
  switch(code - 0200) {
    case 0:	shift1 = make;		break;	/* shift key on left */
    case 1:	shift2 = make;		break;	/* shift key on right */
    case 2:	control = make;		break;	/* control */
    case 3:	alt = make;		break;	/* alt key */

    case 4:	if (make && caps_off) {
			capslock = 1 - capslock;
			set_leds();
		}
		caps_off = 1 - make;    break;	/* caps lock */

    case 5:	if (make && num_off) {
			numlock  = 1 - numlock;
			set_leds();
		}
		num_off = 1 - make;
		break;	/* num lock */
  }
  return(-1);
}


/*===========================================================================*
 *				set_leds				     *
 *===========================================================================*/
PRIVATE void set_leds()
{
/* Set the LEDs on the caps lock and num lock keys */

  int leds, data_port, status_port;

  if (!pc_at && !ps) return;	/* PC/XT doesn't have LEDs */
  leds = (numlock << 1) | (capslock << 2);	/* encode LED bits */

  if (ps) {
	data_port = PS_KEYBD;
	status_port = PS_KB_STATUS;
  } else {
	data_port = KEYBD;
	status_port = KB_STATUS;
  }

  kb_wait(status_port);		/* wait for buffer empty  */
  out_byte(data_port, LED_CODE);   /* prepare keyboard to accept LED values */
  kb_ack(data_port);		/* wait for ack response  */

  kb_wait(status_port);		/* wait for buffer empty  */
  out_byte(data_port, leds);	/* give keyboard LED values */
  kb_ack(data_port);		/* wait for ack response  */
}


/*==========================================================================*
 *				kb_wait					    *
 *==========================================================================*/
PRIVATE int kb_wait(status_port)
int status_port;
{
/* Wait until the controller is ready; return zero if this times out. */

  int retries;

  retries = MAX_KB_BUSY_RETRIES + 1;
  while (--retries != 0 && in_byte(status_port) & KB_BUSY)
	;			/* wait until not busy */
  return(retries);		/* nonzero if ready */
}


/*==========================================================================*
 *				kb_ack					    *
 *==========================================================================*/
PRIVATE int kb_ack(data_port)
int data_port;
{
/* Wait until kbd acknowledges last command; return zero if this times out. */

  int retries;

  retries = MAX_KB_ACK_RETRIES + 1;
  while (--retries != 0 && in_byte(data_port) != KB_ACK)
	;			/* wait for ack */
  return(retries);		/* nonzero if ack received */
}

/*===========================================================================*
 *				kb_init					     *
 *===========================================================================*/
PUBLIC void kb_init(minor)
int minor;
{
/* Initialize the keyboard driver. */

  register struct kb_s *kb;

  kb = kb_addr(minor);

  /* Record minor number. */
  kb->minor = minor;

  /* Set up input queue. */
  kb->iptr = kb->ibuf = kb->ibuf1;
  kb->ibufend = kb->ibuf1 + KB_IBUFSIZE;
  kb->iptr = kb->ibuf1;

  /* Set initial values. */
  caps_off = 1;
  num_off = 1;

  set_leds();			/* turn off numlock led */

  /* Determine which keyboard type is attached.  The bootstrap program asks
   * the user to type an '='.  The scan codes for '=' differ depending on the
   * keyboard in use.
   */
  switch(scan_code) {
	case STANDARD_SCAN:	keyb_type = IBM_PC; break;
	case OLIVETTI_SCAN:	keyb_type = OLIVETTI; load_olivetti(); break;
	case DUTCH_EXT_SCAN:	keyb_type = DUTCH_EXT;
				load_dutch_table(); break;
	case US_EXT_SCAN:	keyb_type = US_EXT;
				load_us_ext(); break;
  }

  scan_keyboard();		/* stop lockup from leftover keystroke */
  enable_irq(KEYBOARD_IRQ);	/* safe now everything initialised! */
}

/*===========================================================================*
 *				load_dutch_table			     *
 *===========================================================================*/
PRIVATE void load_dutch_table()
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
 *				load_olivetti				     *
 *===========================================================================*/
PRIVATE void load_olivetti()
{
/* Load the scan code to ASCII table for olivetti type keyboard. */

  register int i;

  for (i = 0; i < NR_SCAN_CODES; i++) {
	sh[i] = m24[i];
	unsh[i] = unm24[i];
  }
}

/*===========================================================================*
 *				load_us_ext				     *
 *===========================================================================*/
PRIVATE void load_us_ext()
{
/* Load the scan code to ASCII table for US extended keyboard. */

  register int i;

  for (i = 0; i < NR_SCAN_CODES; i++) {
	sh[i] = sh_usx[i];
	unsh[i] = unsh_usx[i];
  }
}

/*===========================================================================*
 *				func_key				     *
 *===========================================================================*/
PUBLIC int func_key(ch)
char ch;			/* scan code for a function key */
{
/* This procedure traps function keys for debugging and control purposes. */

  if (ch < F1 || ch > F10) return(FALSE);	/* not our job */
  if (ch == F1) p_dmp();	/* print process table */
  if (ch == F2) map_dmp();	/* print memory map */
  if (ch == F3) toggle_scroll();	/* hardware vs. software scrolling */

#if NETWORKING_ENABLED
  if (ch == F5) ehw_dump();
#endif
if(ch == F6) logit = 1 - logit; /*DEBUG*/
  if (ch == F7 && control) sigchar(&tty_struct[CONSOLE], SIGQUIT);
  if (ch == F8 && control) sigchar(&tty_struct[CONSOLE], SIGINT);
  if (ch == F9 && control) sigchar(&tty_struct[CONSOLE], SIGKILL);
  return(TRUE);
}


/*==========================================================================*
 *				scan_keyboard				    *
 *==========================================================================*/
PRIVATE int scan_keyboard()
{
/* Fetch the character from the keyboard hardware and acknowledge it. */

  int code;
  int val;

  if (ps) {
	code = in_byte(PS_KEYBD);	/* get the scan code for key struck */
	val = in_byte(0x69);	/* acknowledge it in mysterious ways */
	out_byte(0x69, val ^ 0x10);	/* 0x69 should be equiv to PORT_B */
	out_byte(0x69, val);	/* XOR looks  fishy */
	val = in_byte(0x66);	/* what is 0x66? */
	out_byte(0x66, val & ~0x10);	/* 0x72 for PS_KB_STATUS is fishier */
	out_byte(0x66, val | 0x10);
	out_byte(0x66, val & ~0x10);
  } else {
	code = in_byte(KEYBD);	/* get the scan code for the key struck */
	val = in_byte(PORT_B);	/* strobe the keyboard to ack the char */
	out_byte(PORT_B, val | KBIT);	/* strobe the bit high */
	out_byte(PORT_B, val);	/* now strobe it low */
  }
  return code;
}


/*==========================================================================*
 *				reboot					    *
 *==========================================================================*/
PRIVATE void reboot()
{
/* Reboot the machine. */

  static u16_t magic = MEMCHECK_MAG;

  soon_reboot();

  /* Stop BIOS memory test. */
  phys_copy(numap(TTY, (vir_bytes) &magic, sizeof magic),
	    (phys_bytes) MEMCHECK_ADR, (phys_bytes) sizeof magic);
  if (protected_mode) {
	/* Rebooting is nontrivial because the BIOS reboot code is in real
	 * mode and there is no sane way to return to real mode on 286's.
	 */
	if (pc_at) {
		/* Use the AT keyboard controller to reset the processor.
		 * The A20 line is kept enabled in case this code is ever
		 * run from extended memory, and because some machines
		 * appear to drive the fake A20 high instead of low just
		 * after reset, leading to an illegal opode trap.  This bug
		 * is more of a problem if the fake A20 is in use, as it
		 * would be if the keyboard reset were used for real mode.
		 */
		kb_wait(ps ? PS_KB_STATUS : KB_STATUS);
		out_byte(KB_COMMAND,
			 KB_PULSE_OUTPUT | (0x0F & ~(KB_GATE_A20 | KB_RESET)));
	} else {
		printf("No way to reboot from protected mode on this machine ");
	}
	while (TRUE)
		;		/* no way to recover if the above fails */
  }

  /* In real mode, jumping to the reset address is good enough. */
  reset();
}


/*==========================================================================*
 *				waitkey					    *
 *==========================================================================*/
PRIVATE void waitkey(prompt)
char *prompt;
{
/* Wait for a keystroke.  Use polling, since this is only called after
 * interrupts have been disabled.
 */

  int scancode;

  milli_delay(1000);		/* pause for a second to ignore key release */
  scan_keyboard();		/* ack any old input */
  printf(prompt);
  scancode = scan_keyboard();	/* quiescent value (0 on PC, last code on AT)*/
  while(scan_keyboard() == scancode)
	;			/* loop until new keypress or any release */
}


/*==========================================================================*
 *				wreboot					    *
 *==========================================================================*/
PUBLIC void wreboot()
{
/* Wait for keystrokes before printing debugging info and rebooting. */

  soon_reboot();
  waitkey("Type any key to view process table\r\n");
  p_dmp();
  waitkey("Type any key to view memory map\r\n");
  map_dmp();
  waitkey("Type any key to reboot\r\n");
  reboot();
}
