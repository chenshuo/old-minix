/*
 *     hard disk driver for ibm ps/2 esdi adapter
 *
 *     written by doug burks, based on other minix wini drivers.
 *     some additions by art roberts
 *     last changed:  22 Jan 1991
 *
 *     references:
 *        ibm personal system/2 hardware technical reference  (1988)
 *        ibm 60/120mb fixed disk drive technical reference  (1988)
 *
 *     caveats:
 *       * this driver has been reported to work on ibm ps/2 models 50 and
 *         70 with ibm's 60/120mb hard disks.
 *       * for a true esdi adapter, changes will have to be made, but this
 *         certainly serves as a good start.
 *       * no timeouts are implemented, so this driver could hang under
 *         adverse conditions.
 *       * the error processing has not been tested.  my disk works too well.
 *
 * The driver supports the following operations (using message format m2):
 *
 *    m_type      DEVICE    PROC_NR     COUNT    POSITION  ADRRESS
 * ----------------------------------------------------------------
 * |  DEV_OPEN  |         |         |         |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_CLOSE |         |         |         |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_READ  | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_WRITE | device  | proc nr |  bytes  |  offset | buf ptr |
 * ----------------------------------------------------------------
 * |SCATTERED_IO| device  | proc nr | requests|         | iov ptr |
 * ----------------------------------------------------------------
 *
 * The file contains one entry point:
 *
 *   winchester_task:	main entry when system is brought up
 */
#include "kernel.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/partition.h>


/*****  esdi i/o adapter ports  */

#define  CMD_REG   0x3510          /*  command interface register            */
#define  STAT_REG  0x3510          /*  status interface register             */
#define  BCTL_REG  0x3512          /*  basic control register                */
#define  BST_REG   0x3512          /*  basic status register                 */
#define  ATT_REG   0x3513          /*  attention register                    */
#define  INT_REG   0x3513          /*  interrupt status register             */


/*****  basic status register bits  */

#define  DMA_ENA   0x80            /*  DMA enabled?                          */
#define  INT_PND   0x40            /*  interrupt pending?                    */
#define  CMD_PRG   0x20            /*  command in progress?                  */
#define  BUSY      0x10            /*  is adapter busy?                      */
#define  STR_FUL   0x08            /*  status interface register set?        */
#define  CMD_FUL   0x04            /*  command interface register full?      */
#define  XFR_REQ   0x02            /*  data transfer operation ready?        */
#define  INT_SET   0x01            /*  adapter sending interrupt?            */


/*****  attention register commands  */

#define  ATT_CMD  0x01             /*  command request                       */
#define  ATT_EOI  0x02             /*  end of interrupt processing           */
#define  ATT_ABT  0x03             /*  abort the current command             */
#define  ATT_RST  0xE4             /*  reset the esdi adapter                */


/*****  dma register addresses  */

#define  DMA_EXTCMD  0x18          /*  extended function register            */
#define  DMA_EXEC    0x1A          /*  extended function execute             */


/*****  miscellaneous  */

#define  ERR            -1         /*  general error code                    */
#define  SECTOR_SIZE    512        /*  hard disk sector size  [bytes]        */
#define  MAX_ERRORS     4          /*  maximum number of read/write retries  */
#define  MAX_DRIVES     2          /*  maximum number of physical drives     */
#define  DEV_PER_DRIVE  (1+NR_PARTITIONS)
                                   /*  number of logical devices on each     */
                                   /*    physical hard disk drive            */
#define  NR_DEVICES     (MAX_DRIVES*DEV_PER_DRIVE)
                                   /*  maximum number of logical devices     */
#define  SYS_PORTA      0x92       /*  system control port a                 */
#define  LIGHT_ON       0xC0       /*  fixed-disk activity light reg. mask   */


/*****  variables  */

PRIVATE struct wini {              /*  disk/partition information            */
   int wn_opcode;                  /*  requested disk operation              */
                                   /*    DEV_READ/DEV_WRITE                  */
   int wn_procnr;                  /*  process requesting operation          */
   int wn_drive;                   /*  physical hard disk drive number       */
   long wn_offset;                 /*  offset [sectors on full disk] to the  */
                                   /*    requested disk block                */
   long wn_low;                    /*  lowest sector for the partition       */
   long wn_size;                   /*  disk partition size  [sectors]        */
   int wn_count;                   /*  read/write request size  [bytes]      */
   vir_bytes wn_address;           /*  buffer address for requested i/o      */
} wini[NR_DEVICES];                /*  index:  0, 5 -- full disk 1, 2        */
                                   /*     others:  add partition number      */
PUBLIC int using_bios = FALSE;     /*  is the bios used for hard disk i/o?   */
PRIVATE int nr_drives;             /*  actual number of physical disk drive  */
PRIVATE message w_mess;            /*  minix message structure buffer        */
PRIVATE int command[4];            /*  controller command buffer             */
PRIVATE unsigned int status_block[9]; /*  status block output from a command */
PRIVATE unsigned char buf[BLOCK_SIZE];
                                   /*  partition table buffer                */
PRIVATE int dma_channel;           /*  fixed disk dma channel number         */


/*****  functions  */

FORWARD void wake ();
FORWARD void copy_prt ();
FORWARD void sort ();
FORWARD int w_command ();
FORWARD int w_do_rdwt ();
FORWARD int w_transfer ();
FORWARD int w_att_write ();
FORWARD int w_interrupt ();
FORWARD void w_dma_setup();


/*===========================================================================*
 *				winchester_task				     * 
 *===========================================================================*/
PUBLIC void winchester_task () {
/*
 *     services requests for hard disk i/o
 *
 *     after initializing the whole mess, wait for a request message, service
 *     it, send the results, wait for a request message, service it, etc.,
 *     etc.
 */
   int r, caller, proc_nr;

   wake ();

   while (TRUE) {

      receive (ANY, &w_mess);
      if (w_mess.m_source < 0) {
         printf ("winchester task got message from %d\n", w_mess.m_source);
         continue;
      }
      caller = w_mess.m_source;
      proc_nr = w_mess.PROC_NR;

	switch(w_mess.m_type) {
	    case DEV_OPEN:	r = OK;				  break;
	    case DEV_CLOSE:	r = OK;				  break;

	    case DEV_READ:
	    case DEV_WRITE:	r = w_do_rdwt(&w_mess);		  break;

	    case SCATTERED_IO:	r = do_vrdwt(&w_mess, w_do_rdwt); break;
	    default:		r = EINVAL;			  break;
	}
      w_mess.m_type = TASK_REPLY;	
      w_mess.REP_PROC_NR = proc_nr;
      w_mess.REP_STATUS = r;       /*  no. of bytes transferred/error code */
      send (caller, &w_mess);
   }
}


/*===========================================================================*
 *                              wake                                         *
 *===========================================================================*/
PRIVATE void wake () {
/*
 *     initializes everything needed to run the hard disk
 *
 *     the following items are initialized:
 *       -- hard disk attributes stored in bios
 *       -- partition table, read from the disk
 *       -- dma transfer channel, read from system register
 *       -- dma transfer and interrupts [disabled]
 *
 *     the hard disk adapter is initialized when the ibm ps/2 is turned on,
 *     using the programmable option select registers.  thus the only
 *     additional initialization is making sure the dma transfer and interrupts
 *     are disabled.  other initialization problems could be checked for, such
 *     as an operation underway.  the paranoid could add a check for adapter
 *     activity and abort the operations.  the truly paranoid can reset the
 *     adapter.  until such worries are proven, why bother?
 */
   unsigned int drv;               /*  hard disk drive number                */
   phys_bytes address;             /*  real bios address                     */

   /*  retrieve the actual number of hard disk drives from bios  */
   phys_copy (0x475L, umap (proc_ptr, D, (vir_bytes)buf, 1), 1L);
   nr_drives = (int) *buf;
   if (nr_drives > MAX_DRIVES) nr_drives = MAX_DRIVES;
   if (nr_drives <= 0) return;

   for (drv = 0;  drv < nr_drives;  ++drv) {
      if (w_command (drv, 0x0609, 6) != OK) {
         panic ("Unable to get hard disk parameters\n", NO_NUM);
      }
      wini[5*drv].wn_size = ((unsigned long)status_block[3] << 16) |
                             (unsigned long)status_block[2];
   }
   if (w_command (7, 0x060A, 5) != OK) {
      panic ("Unable to get hard disk dma channel\n", NO_NUM);
   }
   dma_channel = (status_block[2] & 0x3C00) >> 10;
/*
 *     retrieves the full partition table
 */
   wini[0].wn_low = wini[5].wn_low = 0L;
   for (drv = 0;  drv < nr_drives;  ++drv) {

      w_mess.DEVICE = 5 * drv;
      w_mess.POSITION = 0L;
      w_mess.COUNT = BLOCK_SIZE;
      w_mess.ADDRESS = (char *) buf;
      w_mess.PROC_NR = WINCHESTER;
      w_mess.m_type = DEV_READ;

      if (w_do_rdwt (&w_mess) != BLOCK_SIZE) {
         printf ("Can't read partition table on winchester %d\n", drv);
         milli_delay (20000);
         continue;
      }
      if (buf[510] != 0x55  ||  buf[511] != 0xAA) {
         printf ("Invalid partition table on winchester %d\n", drv);
         milli_delay (20000);
         continue;
      }
      copy_prt ((int)5*drv);       /*  flesh out the partitions              */
   }
   return;
}


/*============================================================================*
 *                              copy_prt                                      *
 *============================================================================*/
PRIVATE void copy_prt (base_dev)
int base_dev; {                  /* base device for drive */
/*
 *     fills out the hard disk partition initial block and number of blocks
 *
 *     the old minix partition support was removed, since this driver didn't
 *     exist for old partitions.
 */
   register struct part_entry *pe;
   register struct wini *wn;

   for (pe = (struct part_entry *) &buf[PART_TABLE_OFF],
           wn = &wini[base_dev + 1];
        pe < ((struct part_entry *) &buf[PART_TABLE_OFF]) + NR_PARTITIONS;
        ++pe, ++wn) {
      wn->wn_low = pe->lowsec;
      wn->wn_size = pe->size;
   }
   sort (&wini[base_dev+1]);
}



/*==========================================================================*
 *                                sort                                      *
 *==========================================================================*/
PRIVATE void sort (wn)
register struct wini wn[]; {
/*
 *     sorts the partition table according to the initial block number
 *
 *     this apparently eliminates missing partitions, though it screws up
 *     partition numbers compared with using fdisk on different operating
 *     systems.
 */
   register int i,j;
   struct wini tmp;

   for (i = 0;  i < NR_PARTITIONS;  i++) {
      for (j = 0;  j < NR_PARTITIONS-1;  j++) {
         if ((wn[j].wn_low == 0  &&  wn[j+1].wn_low != 0)  ||
             (wn[j].wn_low > wn[j+1].wn_low  &&  wn[j+1].wn_low != 0)) {
            tmp = wn[j];
            wn[j] = wn[j+1];
            wn[j+1] = tmp;
         }
      }
   }
   return;
}



/*===========================================================================*
 *                          w_command                                        *
 *===========================================================================*/
PRIVATE int w_command (device, cmd, num_words)
int device;                        /*  i device to operate on                */
                                   /*    1-2   physical disk drive number    */
                                   /*    7     hard disk controller          */
int cmd;                           /*  i command to execute                  */
int num_words; {                   /*  i expected size of status block       */
/*
 *     executes a command for a particular device
 *
 *     the operation is conducted as follows:
 *       -- create the command block
 *       -- initialize for command reading by the controller
 *       -- write the command block to the controller, making sure the
 *          controller has digested the previous command word, before shoving
 *          the next down its throat
 *       -- wait for an interrupt
 *       -- read expected number of words of command status information
 *       -- return the command status block
 *
 *     reading and writing registers is accompanied by enabling and disabling
 *     interrupts to ensure that the status register contents still apply when
 *     the desired register is read/written.
 */
   register int ki;                /*  -- scratch --                         */
   int status;                     /*  disk adapter status register value    */

   device <<= 5;                   /*  adjust device for our use             */
   command[0] = cmd | device;      /*  build command block                   */
   command[1] = 0;

   w_att_write (device | ATT_CMD);

   for (ki = 0;  ki < 2;  ++ki) {
      out_word (CMD_REG, command[ki]);
      unlock ();
      while (TRUE) {
         lock ();
         status = in_byte (BST_REG);
         if (!(status & CMD_FUL)) break;
         unlock ();
      }
   }
   unlock ();

   status = w_interrupt (0);
   if (status != (device | 0x01)) {
      w_att_write (device | ATT_ABT);
      w_interrupt (0);
      return  ERR;
   }
   for (ki = 0;  ki < num_words;  ++ki) {
      while (TRUE) {
         lock ();
         status = in_byte (BST_REG);
         if (status & STR_FUL) break;
         unlock ();
      }
      status_block[ki] = in_word (STAT_REG);
      unlock ();
   }
   w_att_write (device | ATT_EOI);

   return  OK;
}



/*===========================================================================*
 *				w_do_rdwt				     * 
 *===========================================================================*/
PRIVATE int w_do_rdwt(m_ptr)
message *m_ptr;	{		/* pointer to read or write w_message */
/*
 *     reads/writes a block from the hard disk
 *
 *     checks the validity of the request as fully as possible and fills the
 *     disk information structure before calling 'w_transfer' to do the dirty
 *     work.  while unsuccessful operations are re-tried, this may be
 *     superfluous, since the controller does the same on its own.  turns on
 *     the fixed disk activity light, while busy.  computers need blinking
 *     lights, right?
 */
   register struct wini *wn;
   int r, device, errors;

   device = m_ptr->DEVICE;
   if (device < 0 || device >= NR_DEVICES) return  EIO;
   if (m_ptr->COUNT != BLOCK_SIZE) return  EINVAL;

   wn = &wini[device];
   wn->wn_drive = device/DEV_PER_DRIVE;	/* save drive number */
   if (wn->wn_drive >= nr_drives) return  EIO;

   wn->wn_opcode = m_ptr->m_type;	/* DEV_READ or DEV_WRITE */
   if (m_ptr->POSITION % BLOCK_SIZE != 0) return  EINVAL;

   wn->wn_offset = m_ptr->POSITION/SECTOR_SIZE;
   if (wn->wn_offset+BLOCK_SIZE/SECTOR_SIZE > wn->wn_size) return  0;

   wn->wn_offset += wn->wn_low;
   wn->wn_count = m_ptr->COUNT;
   wn->wn_address = (vir_bytes) m_ptr->ADDRESS;
   wn->wn_procnr = m_ptr->PROC_NR;

   out_byte (SYS_PORTA, in_byte (SYS_PORTA) | LIGHT_ON); 
                                   /*  turn on the disk activity light     */

   for (errors = 0;  errors < MAX_ERRORS;  ++errors) {

      r = w_transfer(wn);
      if (r == OK) break;
   }
   out_byte (SYS_PORTA, in_byte (SYS_PORTA) & ~LIGHT_ON); 
                                   /*  turn off the disk activity light    */
   return  (r == OK?  BLOCK_SIZE : EIO);
}



/*===========================================================================*
 *				w_transfer				     * 
 *===========================================================================*/
PRIVATE int w_transfer(wn)
register struct wini *wn; {    /* pointer to the drive struct */
/*
 *     reads/writes a single block of data from/to the hard disk
 *
 *     the read/write operation performs the following steps:
 *       -- create the command block
 *       -- initialize the command reading by the controller
 *       -- write the command block to the controller, making sure the
 *            controller has digested the previous command word, before
 *            shoving the next down its throat.
 *       -- wait for an interrupt, which must return a 'data transfer ready'
 *            status.  abort the command if it doesn't.
 *       -- set up and start up the direct memory transfer
 *       -- wait for an interrupt, signalling the end of the transfer
 */
   int device;                     /*  device mask for the command register  */
   int ki;                         /*  -- scratch --                         */
   long offset;                    /*  ... [blocks] to the requested block   */
   int r;                          /*  function return (success) value       */
   int status;                     /*  basic status register value           */

   device = wn->wn_drive << 5;
   if (wn->wn_opcode == DEV_READ) {
      command[0] = 0x4601 | device;
   } else {
      command[0] = 0x4602 | device;
   }
   command[1] = BLOCK_SIZE / SECTOR_SIZE;
   offset = wn->wn_offset;
   command[2] = (int)(offset & 0xFFFF);
   command[3] = (int)(offset >> 16);

   w_att_write (device | ATT_CMD);

   for (ki = 0;  ki < 4;  ++ki) {
      out_word (CMD_REG, command[ki]);
      unlock ();
      while (TRUE) {
         lock ();
         status = in_byte (BST_REG);
         if (!(status & CMD_FUL)) break;
         unlock ();
      }
   }
   unlock ();

   status = w_interrupt (0);
   if (status != (device | 0x0B)) {
      w_att_write (device | ATT_ABT);
      w_interrupt (0);
      return  ERR;
   }
   w_dma_setup (wn);

   status = w_interrupt (1);

   w_att_write (device | ATT_EOI);

   if ((status & 0x0F) > 8) return  ERR;
   return  OK;
}



/*==========================================================================*
 *                            w_att_write                                   *
 *==========================================================================*/
PRIVATE int w_att_write (value)
register int value; {
/*
 *     writes a command to the esdi attention register
 *
 *     waits for the controller to finish its business before sending the
 *     command to the controller.  note that the interrupts must be off to read
 *     the basic status register and, if the controller is ready, must not be
 *     turned back on until the attention register command is sent.
 */
   int status;                     /*  basic status register value           */

   while (TRUE) {
      lock ();
      status = in_byte (BST_REG);
      if (!(status & (INT_PND | BUSY))) break;
      unlock ();
   }
   out_byte (ATT_REG, value);
   unlock ();

   return  OK;
}



/*===========================================================================*
 *                          w_interrupt                                      *
 *===========================================================================*/
PRIVATE int w_interrupt (dma)
int dma; {                         /*  i dma transfer is underway            */
/*
 *     waits for an interrupt from the hard disk controller
 *
 *     enable interrupts on the hard disk and interrupt controllers (and dma if
 *     necessary).  wait for an interrupt.  when received, return the interrupt
 *     status register value.
 *
 *     an interrupt can be detected either from the basic status register or
 *     through a system interrupt handler.  the handler is used for all
 *     interrupts, due to the expected long times to process reads and writes
 *     and to avoid busy waits.
 */
   message dummy;                  /*  -- scratch --                         */
   int status;                     /*  basic status register value           */

   dma = dma << 1;
   out_byte (BCTL_REG, dma | 1);
   cim_at_wini ();

   receive (HARDWARE, &dummy);

   out_byte (BCTL_REG, 0);
   if (dma) out_byte (DMA_EXTCMD, 0x90+dma_channel);

   status = in_byte (INT_REG);

   return  status;
}



/*==========================================================================*
 *			w_dma_setup				  	    *
 *==========================================================================*/
PRIVATE void w_dma_setup(wn)
register struct wini *wn; {
/*
 *     programs the dma controller to move data to and from the hard disk.
 *
 *     uses the extended mode operation of the ibm ps/2 interrupt controller
 *     chip, rather than the intel 8237 (pc/at) compatible mode.
 */
   int mode, low_addr, high_addr, top_addr, low_ct, high_ct;
   vir_bytes vir, ct;
   phys_bytes user_phys;

   vir = (vir_bytes) wn->wn_address;
   ct = (vir_bytes) BLOCK_SIZE;
   user_phys = numap (wn->wn_procnr, vir, BLOCK_SIZE);

   low_addr = (int) user_phys & BYTE;
   high_addr = (int) (user_phys >> 8) & BYTE;
   top_addr = (int) (user_phys >> 16) & BYTE;
   low_ct = (int) (ct - 1) & BYTE;
   high_ct = (int) ((ct-1) >> 8) & BYTE;
   if (user_phys == 0)
      panic ("FS gave winchester disk driver bad addr %d", (int)vir);

   mode = (wn->wn_opcode == DEV_READ?  0x4C : 0x44);
   lock ();
   out_byte (DMA_EXTCMD, 0x90+dma_channel);
                                   /*  disable access to dma channel 5     */
   out_byte (DMA_EXTCMD, 0x20+dma_channel);
                                   /*  clear the address byte pointer      */
   out_byte (DMA_EXEC, low_addr);  /*  set the lower eight address bits    */
   out_byte (DMA_EXEC, high_addr); /*  set address bits 8-15               */
   out_byte (DMA_EXEC, top_addr);  /*  set the higher four address bits    */
   out_byte (DMA_EXTCMD, 0x40+dma_channel);
                                   /*  clear the count byte pointer        */
   out_byte (DMA_EXEC, low_ct);    /*  set the lower eight bits of count   */
   out_byte (DMA_EXEC, high_ct);   /*  set the higher eight bits of count  */
   out_byte (DMA_EXTCMD, 0x70+dma_channel);
                                   /*  set the transfer mode               */
   out_byte (DMA_EXEC, mode);      /*  set the transfer mode               */
   out_byte (DMA_EXTCMD, 0xA0+dma_channel);
                                   /*  enable access to dma channel 5      */
   unlock ();

   return;
}
