/* MM must occasionally print some message.  It uses the standard library
 * routine prink().  (The name "printf" is really a macro defined as "printk").
 * Printing is done by calling the TTY task directly, not going through FS.
 */

#include "mm.h"
#include <minix/com.h>

#define BUF_SIZE          100	/* print buffer size */

PRIVATE int buf_count;		/* # characters in the buffer */
PRIVATE char print_buf[BUF_SIZE];	/* output is buffered here */
PRIVATE message putch_msg;	/* used for message to TTY task */

_PROTOTYPE( FORWARD void flush, (void) );

/*===========================================================================*
 *				putk					     *
 *===========================================================================*/
PUBLIC void putk(c)
int c;
{
/* Accumulate another character.  If '\n' or buffer full, print it. */

  if (c == 0) {
	flush();
	return;
  }
  print_buf[buf_count++] = (char) c;
  if (c == '\n' || buf_count == BUF_SIZE) flush();
}


/*===========================================================================*
 *				flush					     *
 *===========================================================================*/
PRIVATE void flush()
{
/* Flush the print buffer by calling TTY task. */

  if (buf_count == 0) return;
  putch_msg.m_type = DEV_WRITE;
  putch_msg.PROC_NR  = 0;
  putch_msg.TTY_LINE = 0;
  putch_msg.ADDRESS  = print_buf;
  putch_msg.COUNT = buf_count;
  sendrec(TTY, &putch_msg);
  buf_count = 0;
}
