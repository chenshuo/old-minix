/* This file contains routines for initializing the 8259 interrupt controller:
 *	get_irq_handler: address of handler for a given interrupt
 *	put_irq_handler: register an interrupt handler
 *	init_8259:	initialize the 8259(s), since the BIOS does it poorly
 *	soon_reboot:	prepare to reboot the system
 */

#include "kernel.h"

#define ICW1_AT         0x11	/* edge triggered, cascade, need ICW4 */
#define ICW1_PC         0x13	/* edge triggered, no cascade, need ICW4 */
#define ICW1_PS         0x19	/* level triggered, cascade, need ICW4 */
#define ICW4_AT         0x01	/* not SFNM, not buffered, normal EOI, 8086 */
#define ICW4_PC         0x09	/* not SFNM, buffered, normal EOI, 8086 */

FORWARD _PROTOTYPE( int spurious_irq, (int irq) );


/*=========================================================================*
 *				get_irq_handler				   *
 *=========================================================================*/
PUBLIC irq_handler_t get_irq_handler(irq)
int irq;
{
/* Return the handler registered for interrupt irq, null if no handler. */

  if (irq < 0 || irq >= NR_IRQ_VECTORS)
	panic("invalid call to get_irq_handler", irq);

  if (irq_table[irq] == spurious_irq)
	return(0);	/* Not a real handler */

  return irq_table[irq];
}


/*=========================================================================*
*				put_irq_handler				   *
*=========================================================================*/
PUBLIC void put_irq_handler(irq, handler)
int irq;
irq_handler_t handler;
{
/* Register an interrupt handler. */

  if (irq < 0 || irq >= NR_IRQ_VECTORS)
	panic("invalid call to put_irq_handler", irq);

  if (irq_table[irq] == handler)
	return;		/* extra initialization */

  if (irq_table[irq] != spurious_irq)
	panic("attempt to register second irq handler for irq", irq);

  irq_table[irq]= handler;
}


/*=========================================================================*
*				spurious_irq				   *
*=========================================================================*/
PRIVATE int spurious_irq(irq)
int irq;
{
/* Default interrupt handler.  It complains a lot. */

  if (irq < 0 || irq >= NR_IRQ_VECTORS)
	panic("invalid call to spurious_irq", irq);

  printf("spurious irq %d\n", irq);

  return 1;	/* Reenable interrupt */
}


/*==========================================================================*
 *				init_8259				    *
 *==========================================================================*/
PUBLIC void init_8259(master_base, slave_base)
unsigned master_base;
unsigned slave_base;
{
/* Initialize the 8259(s), finishing with all interrupts disabled. */

  int i;

  lock();
  if (pc_at) {
	/* Two interrupt controllers, one master, one slaved at IRQ 2. */
	out_byte(INT_CTL, ps_mca ? ICW1_PS : ICW1_AT);
	out_byte(INT_CTLMASK, master_base);		/* ICW2 for master */
	out_byte(INT_CTLMASK, (1 << CASCADE_IRQ));	/* ICW3 tells slaves */
	out_byte(INT_CTLMASK, ICW4_AT);
	out_byte(INT_CTLMASK, ~(1 << CASCADE_IRQ));	/* IRQ 0-7 mask */
	out_byte(INT2_CTL, ps_mca ? ICW1_PS : ICW1_AT);
	out_byte(INT2_CTLMASK, slave_base);		/* ICW2 for slave */
	out_byte(INT2_CTLMASK, CASCADE_IRQ);		/* ICW3 is slave nr */
	out_byte(INT2_CTLMASK, ICW4_AT);
	out_byte(INT2_CTLMASK, ~0);			/* IRQ 8-15 mask */
  } else {
	/* One interrupt controller. */
	out_byte(INT_CTL, ICW1_PC);
	out_byte(INT_CTLMASK, master_base);
	out_byte(INT_CTLMASK, ICW4_PC);
	out_byte(INT_CTLMASK, ~0);			/* IRQ 0-7 mask */
  }

  /* Initialize the table of interrupt handlers. */
  for (i = 0; i< NR_IRQ_VECTORS; i++) irq_table[i]= spurious_irq;
}


/*==========================================================================*
 *				soon_reboot				    *
 *==========================================================================*/
PUBLIC void soon_reboot()
{
/* Prepare to reboot the system.  This mainly stops all interrupts.  lock()
 * is not enough since functions may call unlock() (e.g. panic calls printf
 * which calls set_6845 which calls unlock).  Set the 'rebooting' flag to
 * show that the system is unreliable.
 */

  out_byte(INT_CTLMASK, ~0);
  rebooting = TRUE;
}
