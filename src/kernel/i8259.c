/* This file contains routines for initializing the 8259 interrupt controller:
 *	enable_irq:	enable an interrupt line.  The cim...() functions in
 *			klib88 are specialized versions of this
 *	init_8259:	initialize the 8259(s), since the BIOS does it poorly
 */

#include "kernel.h"

#define ICW1_AT         0x11	/* edge triggered, cascade, need ICW4 */
#define ICW1_PC         0x13	/* edge triggered, no cascade, need ICW4 */
#define ICW1_PS         0x19	/* level triggered, cascade, need ICW4 */
#define ICW3_M          0x04	/* bit 2 for slave on channel 2 */
#define ICW3_S          0x02	/* slave identity is 2 */
#define ICW4_AT         0x01	/* not SFNM, not buffered, normal EOI, 8086 */
#define ICW4_PC         0x09	/* not SFNM, buffered, normal EOI, 8086 */


/*==========================================================================*
 *				enable_irq				    *
 *==========================================================================*/
PUBLIC void enable_irq(irq_nr)
unsigned irq_nr;
{
/* Clear the corresponding 8259 register bit.
 * Be careful not to call this early from main() since it calls unlock().
 */

  lock();
  if (irq_nr < 8)
	out_byte(INT_CTLMASK, in_byte(INT_CTLMASK) & ~(1 << irq_nr));
  else
	out_byte(INT2_MASK, in_byte(INT2_MASK) & ~(1 << (irq_nr - 8)));
  unlock();
}


/*==========================================================================*
 *				init_8259				    *
 *==========================================================================*/
PUBLIC void init_8259(master_base, slave_base)
unsigned master_base;
unsigned slave_base;
{
/* Initialize the 8259(s), finishing with all interrupts disabled. */

  if (pc_at) {
	out_byte(INT_CTL, ps_mca ? ICW1_PS : ICW1_AT);
	out_byte(INT_CTLMASK, master_base);	/* ICW2 for master */
	out_byte(INT_CTLMASK, ICW3_M);
	out_byte(INT_CTLMASK, ICW4_AT);
	out_byte(INT2_CTL, ps_mca ? ICW1_PS : ICW1_AT);
	out_byte(INT2_MASK, slave_base);	/* ICW2 for slave */
	out_byte(INT2_MASK, ICW3_S);
	out_byte(INT2_MASK, ICW4_AT);
	out_byte(INT2_MASK, ~0);
  } else {
	out_byte(INT_CTL, ICW1_PC);
	out_byte(INT_CTLMASK, master_base);	/* no slave */
	out_byte(INT_CTLMASK, ICW4_PC);
  }
  out_byte(INT_CTLMASK, ~0);
}
