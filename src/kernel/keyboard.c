/* Keyboard driver for PC's and AT's.
 *
 * Changed by Marcus Hampel	(04/02/1994)
 *  - Loadable keymaps
 */

#include "kernel.h"
#include <sgtty.h>
#include <signal.h>
#include <unistd.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/keymap.h>
#include "tty.h"
#include "keymaps/us-std.src"

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

/* Miscellaneous. */
#define ESC_SCAN	   1	/* Reboot key when panicking */
#define SLASH_SCAN	  53	/* to recognize numeric slash */
#define HOME_SCAN	  71	/* first key on the numeric keypad */
#define DEL_SCAN	  83	/* DEL for use in CTRL-ALT-DEL reboot */
#define CONSOLE		   0	/* line number for console */
#define MEMCHECK_ADR   0x472	/* address to stop memory check after reboot */
#define MEMCHECK_MAG  0x1234	/* magic number to stop memory check */

#define kb_addr(n)	(&kb_lines[CONSOLE])	/* incorrectly ignore n */
#define KB_IBUFSIZE	  32	/* size of keyboard input buffer */

PRIVATE int alt1;		/* left alt key state */
PRIVATE int alt2;		/* right alt key state */
PRIVATE int capslock;		/* caps lock key state */
PRIVATE int esc;		/* escape scan code detected? */
PRIVATE int control;		/* control key state */
PRIVATE int caps_off;		/* 1 = normal position, 0 = depressed */
PRIVATE int numlock;		/* number lock key state */
PRIVATE int num_off;		/* 1 = normal position, 0 = depressed */
PRIVATE int slock;		/* scroll lock key state */
PRIVATE int slock_off;		/* 1 = normal position, 0 = depressed */
PRIVATE int shift;		/* shift key state */

PRIVATE char scode_map[] =
		{'H', 'Y', 'A', 'B', 'D', 'C', 'V', 'U', 'G', 'S', 'T', '@'};

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
FORWARD _PROTOTYPE( int scan_keyboard, (void) );
FORWARD _PROTOTYPE( void set_leds, (void) );
FORWARD _PROTOTYPE( int kbd_hw_int, (int irq) );
FORWARD _PROTOTYPE( unsigned map_key, (int scode) );


/*===========================================================================*
 *				map_key0				     *
 *===========================================================================*/
/* Map a scan code to an ASCII code ignoring modifiers. */
#define map_key0(scode)	 \
	((unsigned) keymap[(scode) * MAP_COLS])


/*===========================================================================*
 *				map_key					     *
 *===========================================================================*/
PRIVATE unsigned map_key(scode)
int scode;
{
/* Map a scan code to an ASCII code. */

  int caps, column;
  u16_t *keyrow;

  if (scode == SLASH_SCAN && esc) return '/';	/* don't map numeric slash */

  keyrow = &keymap[scode * MAP_COLS];

  caps = shift;
  if (numlock && HOME_SCAN <= scode && scode <= DEL_SCAN) caps = !caps;
  if (capslock && (keyrow[0] & HASCAPS)) caps = !caps;

  if (alt1 || alt2) {
	column = 2;
	if (control || alt2) column = 3;	/* Ctrl + Alt1 == Alt2 */
	if (caps) column = 4;
  } else {
	column = 0;
	if (caps) column = 1;
	if (control) column = 5;
  }
  return keyrow[column] & ~HASCAPS;
}


/*===========================================================================*
 *				kbd_hw_int				     *
 *===========================================================================*/
PRIVATE int kbd_hw_int(irq)
int irq;
{
/* A keyboard interrupt has occurred.  Process it. */

  int code;
  unsigned km;
  register struct kb_s *kb;

  /* Fetch the character from the keyboard hardware and acknowledge it. */
  code = scan_keyboard();

  /* The IBM keyboard interrupts twice per key, once when depressed, once when
   * released.  Filter out the latter, ignoring all but the shift-type keys.
   * The shift-type keys 29, 42, 54, 56, 58, and 69 must be processed normally.
   */

  if (code & 0200) {
	/* A key has been released (high bit is set). */
	km = map_key0(code & 0177);
	if (km != CTRL && km != SHIFT && km != ALT && km != CALOCK
			&& km != NLOCK && km != SLOCK && km != EXTKEY)
		return 1;
  }

  /* Store the character in memory so the task can get at it later. */
  kb = kb_addr(-NR_CONS);
  if (kb->iptr < kb->ibufend) {
	*kb->iptr++ = code;
	lock();			/* protect shared variable */
	tty_events += EVENT_THRESHOLD;	/* C doesn't guarantee atomic */
	unlock();
  }
  /* Else it doesn't fit - discard it. */
  return 1;	/* Reenable keyboard interrupt */
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

  unsigned km;
  
  km = map_key(scode);

  if (km >= HOME && km <= INSRT)
	return(scode_map[km-HOME]);
  return 0;
}


/*===========================================================================*
 *				make_break				     *
 *===========================================================================*/
PUBLIC int make_break(ch)
int ch;				/* scan code of key just struck or released */
{
/* This routine can handle keyboards that interrupt only on key depression,
 * as well as keyboards that interrupt on key depression and key release.
 * For efficiency, the interrupt routine filters out most key releases.
 */
  int c, make, code;
  static int CAD_count = 0;

  /* Check for CTRL-ALT-DEL, and if found, halt the computer. This would
   * be better done in keyboard() in case TTY is hung, except control and
   * alt are set in the high level code.
   */
  if (control && (alt1 || alt2) && ch == DEL_SCAN)
  {
  	if (++CAD_count == 2) wreboot(RBT_HALT);
	cause_sig(INIT_PROC_NR, SIGABRT);
	return -1;
  }

  c = ch & 0177;		/* high-order bit set on key release */
  make = (ch & 0200 ? 0 : 1);	/* 1 when key depressed, 0 when key released */

  code = map_key(c);		/* map to ASCII */

  switch (code) {
  case CTRL:
	control = make;
	code = -1;
	break;
  case SHIFT:
	shift = make;
	code = -1;
	break;
  case ALT:
	if (make) {
		if (esc) alt2 = 1; else alt1 = 1;
	} else {
		alt1 = alt2 = 0;
	}
	code = -1;
	break;
  case CALOCK:
	if (make && caps_off) {
		capslock = 1 - capslock;
		set_leds();
	}
	caps_off = 1 - make;
	code = -1;
	break;
  case NLOCK:
	if (make && num_off) {
		numlock = 1 - numlock;
		set_leds();
	}
	num_off = 1 - make;
	code = -1;
	break;
  case SLOCK:
	if (make & slock_off) {
		slock = 1 - slock;
		set_leds();
	}
	slock_off = 1 - make;
	code = -1;
	break;
  case EXTKEY:
	esc = 1;
	return(-1);
  default:
	if (!make) code = -1;
  }

  esc = 0;

  return(code);
}


/*===========================================================================*
 *				set_leds				     *
 *===========================================================================*/
PRIVATE void set_leds()
{
/* Set the LEDs on the caps lock and num lock keys */

  int leds, data_port, status_port;

  if (!pc_at && !ps) return;	/* PC/XT doesn't have LEDs */

  /* encode LED bits */
  leds = (slock << 0) | (numlock << 1) | (capslock << 2);

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
  int irq;

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
  slock_off = 1;
  esc = 0;

  set_leds();			/* turn off numlock led */

  scan_keyboard();		/* stop lockup from leftover keystroke */

  irq = ps ? PS_KEYB_IRQ : KEYBOARD_IRQ;
  put_irq_handler(irq, kbd_hw_int);	/* set the interrupt handler */
  enable_irq(irq);		/* safe now everything initialised! */
}


/*===========================================================================*
 *				kbd_loadmap				     *
 *===========================================================================*/
PUBLIC int kbd_loadmap(proc_nr, map_vir)
int proc_nr;
vir_bytes map_vir;
{
/* Load a new keymap. */

  phys_bytes user_phys, kbd_phys;

  user_phys = numap(proc_nr, map_vir, sizeof(keymap));
  if (user_phys == 0) return(EFAULT);
  phys_copy(user_phys, vir2phys(keymap), (phys_bytes) sizeof(keymap));
  return(OK);
}


/*===========================================================================*
 *				func_key				     *
 *===========================================================================*/
PUBLIC int func_key(ch)
char ch;			/* scan code for a function key */
{
/* This procedure traps function keys for debugging and control purposes. */

  unsigned code;

  code = map_key0(ch);				/* first ignore modifiers */
  if (code < F1 || code > F12) return(FALSE);	/* not our job */
  code = map_key(ch);				/* include modifiers */

  if (code == F1) p_dmp();		/* print process table */
  if (code == F2) map_dmp();		/* print memory map */
  if (code == F3) toggle_scroll();	/* hardware vs. software scrolling */

#if ENABLE_NETWORKING
  if (code == F5) dp_dump();		/* network statistics */
#endif
  if (code == CF7) sigchar(&tty_struct[CONSOLE], SIGQUIT);
  if (code == CF8) sigchar(&tty_struct[CONSOLE], SIGINT);
  if (code == CF9) sigchar(&tty_struct[CONSOLE], SIGKILL);
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
 *				wreboot					    *
 *==========================================================================*/
PUBLIC void wreboot(how)
int how; 		/* 0 = halt, 1 = reboot, 2 = panic!, ... */
{
/* Wait for keystrokes for printing debugging info and reboot. */

  int quiet, code;
  static u16_t magic = MEMCHECK_MAG;
  struct tasktab *ttp;

  /* Mask all interrupts. */
  out_byte(INT_CTLMASK, ~0);

  /* Tell several tasks to stop. */
  cons_stop();
#if ENABLE_NETWORKING
  dp8390_stop();
#endif
  floppy_stop();
  clock_stop();

  if (how == RBT_HALT) {
	printf("System Halted\n");
	if (!mon_return) how = RBT_PANIC;
  }

  if (how == RBT_PANIC) {
	/* A panic! */
	printf("Hit ESC to reboot, F-keys for debug dumps\n");

	(void) scan_keyboard();	/* ack any old input */
	quiet = scan_keyboard();/* quiescent value (0 on PC, last code on AT)*/
	for (;;) {
		milli_delay(100);	/* pause for a decisecond */
		code = scan_keyboard();
		if (code != quiet) {
			/* A key has been pressed. */
			if (code == ESC_SCAN) break; /* reboot if ESC typed */
			(void) func_key(code);	     /* process function key */
			quiet = scan_keyboard();
		}
	}
	how = RBT_REBOOT;
  }

  if (how == RBT_REBOOT) printf("Rebooting\n");

  if (mon_return && how != RBT_RESET) {
	/* Reinitialize the interrupt controllers to the BIOS defaults. */
	init_8259(BIOS_IRQ0_VEC, BIOS_IRQ8_VEC);
	out_byte(INT_CTLMASK, 0);
	if (pc_at) out_byte(INT2_CTLMASK, 0);

	/* Return to the boot monitor. */
	if (how == RBT_HALT) {
		reboot_code = vir2phys("");
	} else
	if (how == RBT_REBOOT) {
		reboot_code = vir2phys("delay;boot");
	}
	level0(monitor);
  }

  /* Stop BIOS memory test. */
  phys_copy(vir2phys(&magic), (phys_bytes) MEMCHECK_ADR,
  						(phys_bytes) sizeof(magic));

  if (protected_mode) {
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
	milli_delay(10);

	/* If the nice method fails then do a reset.  In protected
	 * mode this means a processor shutdown.
	 */
	printf("Hard reset...\n");
	milli_delay(250);
  }
  /* In real mode, jumping to the reset address is good enough. */
  level0(reset);
}
