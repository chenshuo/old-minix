/* This file contains the hardware dependant netwerk device driver
 *
 * The valid messages and their parameters are:
 *
 *   m_type	  DL_PORT    DL_PROC   DL_COUNT   DL_MODE   DL_ADDR
 * |------------+----------+---------+----------+---------+---------|
 * | HARDINT	|          |         |          |         |         |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_WRITE	| port nr  | proc nr | count    | mode    | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_WRITEV	| port nr  | proc nr | count    | mode    | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_READ	| port nr  | proc nr | count    |         | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_READV	| port nr  | proc nr | count    |         | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_INIT	| port nr  | proc nr | mode     |         | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_GETSTAT	| port nr  | proc nr |          |         | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_STOP	|	   |         |          |         |         |
 * |------------|----------|---------|----------|---------|---------|
 *
 * The messages sent are:

 *   m-type	  DL_POR T   DL_PROC   DL_COUNT   DL_STAT   DL_CLCK
 * |------------|----------|---------|----------|---------|---------|
 * | DL_INT-TASK| port nr  | proc nr | rd-count | err|stat| clock   |
 * |------------|----------|---------|----------|---------|---------|
 * |DL_TASK-REPL| port nr  | proc nr | rd-count | 0| stat | clock   |
 * |------------|----------|---------|----------|---------|---------|
 *
 *   m_type	  m3_i1     m3_i2       m3_ca1
 * |------------+---------+-----------+---------------|
 * |DL_INIT-REPL| port nr | last port | ethernet addr |
 * |------------|---------|-----------|---------------|
 * */


#include "kernel.h"
#include <minix/com.h>
#include <minix/dl_eth.h>
#include <net/ether.h>
#include "protect.h"
#include "assert.h"
#if 0
#include "eth_hw.h"
#endif
#include "hw_conf.h"
#include "dp8390.h"
#include "epl.h"

#define IOVEC_NR	16	/* we like iovec's of 16 buffers or less but
				 * we support all requests */

typedef struct iovec_dat {
  iovec_t iod_iovec[IOVEC_NR];
  int iod_iovec_s;
  int iod_proc_nr;
  vir_bytes iod_iovec_addr;
} iovec_dat_t;


typedef struct ehw_tab {
  int et_flags;
  ether_addr_t et_address;
  eth_stat_t et_stat;
  iovec_dat_t et_read_iovec;
  vir_bytes et_read_s;
  int et_client;
} ehw_tab_t;

#define ETF_FLAGS	0x3FF;
#define ETF_EMPTY	0x000
#define ETF_PACK_SEND	0x001
#define ETF_PACK_RECV	0x002
#define ETF_SENDINT_EN	0x004
#define ETF_SENDING	0x008
#define ETF_READING	0x010
#define ETF_PACKAVAIL	0x020
#define ETF_PROMISC	0x040
#define ETF_MULTI	0x080
#define ETF_BROAD	0x100
#define ETF_ENABLED	0x200
#define ETF_READ_SU	0x400

PRIVATE phys_bytes ehw_linmem;
PRIVATE segm_t ehw_memsegm;
PRIVATE ehw_tab_t ehw_table[EHW_PORT_NR];
PRIVATE ehw_tab_t *ehw_port;
PRIVATE message mess, reply_mess;
PRIVATE iovec_dat_t tmp_iovec, tmp1_iovec;
PRIVATE int throw_away_packet = FALSE;

#define ehw_cp_loc2user ehw_loc2user

FORWARD _PROTOTYPE( void do_write, (message *mess) );
FORWARD _PROTOTYPE( void do_vwrite, (message *mess) );
FORWARD _PROTOTYPE( void do_vread, (message *mess) );
FORWARD _PROTOTYPE( void do_read, (message *mess) );
FORWARD _PROTOTYPE( void do_init, (message *mess) );
FORWARD _PROTOTYPE( void do_getstat, (message *mess) );
FORWARD _PROTOTYPE( void do_stop, (void) );
FORWARD _PROTOTYPE( void do_int, (void) );
FORWARD _PROTOTYPE( void ehw_init, (void) );
FORWARD _PROTOTYPE( int epl_init, (ether_addr_t *ea) );
FORWARD _PROTOTYPE( int dp_init, (ether_addr_t *ea) );
FORWARD _PROTOTYPE( int dp_reinit, (void) );
FORWARD _PROTOTYPE( void err_reply, (message *m, int err) );
FORWARD _PROTOTYPE( void mess_reply, (message *req, message *reply) );
FORWARD _PROTOTYPE( void ehw_getheader, (int page, rcvdheader_t *h) );
FORWARD _PROTOTYPE( int ehw_data2user, (int page, int length) );
FORWARD _PROTOTYPE( int ehw_user2hw, (vir_bytes user_adddr, int hw_addr,
		      vir_bytes count) );
FORWARD _PROTOTYPE( int ehw_hw2user, (int hw_addr, vir_bytes user_offs,
		      vir_bytes count) );
FORWARD _PROTOTYPE( int ehw_cp_user2loc, (int user_proc, vir_bytes user_addr,
			  char *loc_addr, vir_bytes count) );
FORWARD _PROTOTYPE( int ehw_cp_loc2user, (char *loc_addr, int user_proc,
		     vir_bytes user_addr, vir_bytes count) );
FORWARD _PROTOTYPE( int ehw_iovec_size, (void) );
FORWARD _PROTOTYPE( int ehw_next_iovec, (void) );
FORWARD _PROTOTYPE( void ehw_recv, (void) );
FORWARD _PROTOTYPE( int ehw_inttask, (void) );

PUBLIC void ehw_task()
{
  ehw_tab_t *this_port = &ehw_table[0];	/* this task uses port 0 */

  ehw_port = this_port;
  ehw_init();

  while (TRUE) {
	if (receive(ANY, &mess) < 0) panic("eth_hw: receive failed", NO_NUM);
#if DEBUG
	if (d_eth_hw)
		printk("eth_hw: received %d message from %d\n", mess.m_type,
		       mess.m_source);
#endif
	ehw_port = this_port;

	switch (mess.m_type) {
	    case DL_WRITE:	do_write(&mess);	break;
	    case DL_WRITEV:	do_vwrite(&mess);	break;
	    case DL_READ:	do_read(&mess);	break;
	    case DL_READV:	do_vread(&mess);	break;
	    case DL_INIT:	do_init(&mess);	break;
	    case DL_GETSTAT:	do_getstat(&mess);	break;
	    case DL_STOP:	do_stop();	break;
	    case HARD_INT:	do_int();	break;
	    default:
#if DEBUG
		if (d_eth_hw)
			printf("eth_hw: warning got unknown message (type %d) from %d\n", mess.m_type,
			       mess.m_source);
#endif
		err_reply(&mess, EINVAL);
		break;
	}
	if (ehw_port->et_flags & ETF_ENABLED)
		do_int();	/* we like to know about received or
				 * transmitted packets as soon as
				 * possible */

  }

  panic("eth_hw: task is not allowed to terminate", NO_NUM);
}

PUBLIC void ehw_dump()
{
  ehw_port = &ehw_table[0];

  printf("dumping eth_hw statistics of port 0\r\n\r\n");
  printf("ets_recvErr\t: %ld\t", ehw_port->et_stat.ets_recvErr);
  printf("ets_sendErr\t: %ld\t", ehw_port->et_stat.ets_sendErr);
  printf("ets_OVW\t: %ld\t", ehw_port->et_stat.ets_OVW);
  printf("\r\n");
  printf("ets_CRCerr\t: %ld\t", ehw_port->et_stat.ets_CRCerr);
  printf("ets_frameAll\t: %ld\t", ehw_port->et_stat.ets_frameAll);
  printf("ets_missedP\t: %ld\t", ehw_port->et_stat.ets_missedP);
  printf("\r\n");
  printf("ets_packetR\t: %ld\t", ehw_port->et_stat.ets_packetR);
  printf("ets_packetT\t: %ld\t", ehw_port->et_stat.ets_packetT);
  printf("ets_transDef\t: %ld\t", ehw_port->et_stat.ets_transDef);
  printf("\r\n");
  printf("ets_collision\t: %ld\t", ehw_port->et_stat.ets_collision);
  printf("ets_transAb\t: %ld\t", ehw_port->et_stat.ets_transAb);
  printf("ets_carrSense\t: %ld\t", ehw_port->et_stat.ets_carrSense);
  printf("\r\n");
  printf("ets_fifoUnder\t: %ld\t", ehw_port->et_stat.ets_fifoUnder);
  printf("ets_fifoOver\t: %ld\t", ehw_port->et_stat.ets_fifoOver);
  printf("ets_CDheartbeat\t: %ld\t", ehw_port->et_stat.ets_CDheartbeat);
  printf("\r\n");
  printf("ets_OWC\t: %ld\t", ehw_port->et_stat.ets_OWC);
  printf("\r\n");
  printf("dp_isr=%x, and %x, et_flags= %x\r\n", read_reg0(dp_isr),
         read_reg0(dp_isr),
         ehw_port->et_flags);
  printf("\r\n");
}

PRIVATE void do_write(mess)
message *mess;
{
  int port = mess->DL_PORT;
  int count = mess->DL_COUNT;
  int result;

  if (port < 0 || port >= EHW_PORT_NR || ehw_table + port != ehw_port ||
      !(ehw_port->et_flags & ETF_ENABLED)) {
	err_reply(mess, ENXIO);
	return;
  }
  if (ehw_port->et_flags & ETF_SENDING) {
	ehw_port->et_flags |= ETF_SENDINT_EN;
#if DEBUG
	{
		printW();
		printf("replying SUSPEND because of Send ip\n");
	}
#endif
	err_reply(mess, SUSPEND);
	return;
  }
  assert(!(ehw_port->et_flags & ETF_PACK_SEND));
  if (count < ETH_MIN_PACK_SIZE || count > ETH_MAX_PACK_SIZE) {
	err_reply(mess, EPACKSIZE);
	return;
  }
  tmp_iovec.iod_iovec[0].iov_addr =
	(vir_bytes) mess->DL_ADDR;
  tmp_iovec.iod_iovec[0].iov_size = mess->DL_COUNT;
  tmp_iovec.iod_iovec_s = 1;
  tmp_iovec.iod_proc_nr = mess->DL_PROC;
  tmp_iovec.iod_iovec_addr = 0;

  result = ehw_user2hw(0, ehw_sendpage * EHW_PAGESIZE, count);
  if (result < 0) {
	err_reply(mess, result);
	return;
  }
  write_reg0(dp_tpsr, ehw_sendpage);
  write_reg0(dp_tbcr1, count >> 8);
  write_reg0(dp_tbcr0, count & 0xff);
  write_reg0(dp_cr, CR_TXP);	/* there is goes.. */

  if (mess->DL_MODE & DL_WRITEINT_REQ) {
#if DEBUG
	{
		printW();
		printf("replying SUSPEND because of WRITEINT_REQ\n");
	}
#endif
	ehw_port->et_flags |= ETF_SENDINT_EN;
	result = SUSPEND;
  } else {
#if DEBUG && 0
	{
		printW();
		printf("replying OK because of not WRITEINT_REQ\n");
	}
#endif
	ehw_port->et_flags &= ~ETF_SENDINT_EN;
	result = OK;
  }
  ehw_port->et_flags |= ETF_SENDING;
  assert(!(ehw_port->et_flags & ETF_PACK_SEND));

  err_reply(mess, result);
#if DEBUG && 0
  {
	printW();
	printf("err_replyed\n");
  }
#endif
}

PRIVATE void do_vwrite(mess)
message *mess;
{
  int port = mess->DL_PORT;
  int count = mess->DL_COUNT;
  int result, size;

  if (port < 0 || port >= EHW_PORT_NR || ehw_table + port != ehw_port ||
      !(ehw_port->et_flags & ETF_ENABLED)) {
	err_reply(mess, ENXIO);
	return;
  }
  if (ehw_port->et_flags & ETF_SENDING) {
#if DEBUG
	{
		printW();
		printf("replying SUSPEND because of Send ip\n");
	}
#endif
	ehw_port->et_flags |= ETF_SENDINT_EN;
	err_reply(mess, SUSPEND);
	return;
  }
  assert(!(ehw_port->et_flags & ETF_PACK_SEND));

  result = ehw_cp_user2loc(mess->DL_PROC, (vir_bytes) mess->DL_ADDR,
			 (char *) tmp1_iovec.iod_iovec,
	   (count > IOVEC_NR ? IOVEC_NR : count) * sizeof(iovec_t));
  if (result < 0) {
	err_reply(mess, result);
	return;
  }
  tmp1_iovec.iod_iovec_s = count;
  tmp1_iovec.iod_proc_nr = mess->DL_PROC;
  tmp1_iovec.iod_iovec_addr = (vir_bytes) mess->DL_ADDR;

  tmp_iovec = tmp1_iovec;
  size = ehw_iovec_size();
  if (size < 0) {
	err_reply(mess, size);
	return;
  }
  if (size < ETH_MIN_PACK_SIZE || size > ETH_MAX_PACK_SIZE) {
	err_reply(mess, EPACKSIZE);
	return;
  }
  tmp_iovec = tmp1_iovec;
  result = ehw_user2hw(0, ehw_sendpage * EHW_PAGESIZE, size);
  if (result < 0) {
	err_reply(mess, result);
	return;
  }
  write_reg0(dp_tpsr, ehw_sendpage);
  write_reg0(dp_tbcr1, size >> 8);
  write_reg0(dp_tbcr0, size & 0xff);
  write_reg0(dp_cr, CR_TXP);	/* there is goes.. */

  if (mess->DL_MODE & DL_WRITEINT_REQ) {
#if DEBUG && 0
	{
		printW();
		printf("replying SUSPEND because of WRITEINT_REQ\n");
	}
#endif
	ehw_port->et_flags |= ETF_SENDINT_EN;
	result = SUSPEND;
  } else {
#if DEBUG && 0
	{
		printW();
		printf("replying OK because of not WRITEINT_REQ\n");
	}
#endif
	ehw_port->et_flags &= ~ETF_SENDINT_EN;
	result = OK;
  }
  ehw_port->et_flags |= ETF_SENDING;

  assert(!(ehw_port->et_flags & ETF_PACK_SEND));
  err_reply(mess, result);
#if DEBUG && 0
  {
	printW();
	printf("err_replyed\n");
  }
#endif
}

PRIVATE void do_read(mess)
message *mess;
{
  int port = mess->DL_PORT;
  int count = mess->DL_COUNT;

  if (port < 0 || port >= EHW_PORT_NR || ehw_table + port != ehw_port ||
      !(ehw_port->et_flags & ETF_ENABLED)) {
	err_reply(mess, ENXIO);
	return;
  }
  if (count < ETH_MAX_PACK_SIZE) {
	err_reply(mess, EPACKSIZE);
	return;
  }
  ehw_port->et_read_iovec.iod_iovec[0].iov_addr =
	(vir_bytes) mess->DL_ADDR;
  ehw_port->et_read_iovec.iod_iovec[0].iov_size = mess->DL_COUNT;
  ehw_port->et_read_iovec.iod_iovec_s = 1;
  ehw_port->et_read_iovec.iod_proc_nr = mess->DL_PROC;
  ehw_port->et_read_iovec.iod_iovec_addr = 0;

  ehw_port->et_flags |= ETF_READING | ETF_READ_SU;

  do_int();			/* check if any packet ready */

  if (ehw_port->et_flags & ETF_READING) err_reply(mess, SUSPEND);
  assert(!(ehw_port->et_flags & ETF_READ_SU));
}

PRIVATE void do_vread(mess)
message *mess;
{
  int port = mess->DL_PORT;
  int count = mess->DL_COUNT;
  int result;

  if (port < 0 || port >= EHW_PORT_NR || ehw_table + port != ehw_port ||
      !(ehw_port->et_flags & ETF_ENABLED)) {
	err_reply(mess, ENXIO);
	return;
  }
  result = ehw_cp_user2loc(mess->DL_PROC, (vir_bytes) mess->DL_ADDR,
			 (char *) ehw_port->et_read_iovec.iod_iovec,
	   (count > IOVEC_NR ? IOVEC_NR : count) * sizeof(iovec_t));
  if (result < 0) {
	err_reply(mess, result);
	return;
  }
  ehw_port->et_read_iovec.iod_iovec_s = count;
  ehw_port->et_read_iovec.iod_proc_nr = mess->DL_PROC;
  ehw_port->et_read_iovec.iod_iovec_addr = (vir_bytes) mess->DL_ADDR;
#if DEBUG
  if (d_eth_hw) {
	int i;
	for (i = 0; i < (count < IOVEC_NR ? count : IOVEC_NR); i++)
		printf("iovec[%d]: iov_addr= %x, iov_size= %x\n", i,
		       ehw_port->et_read_iovec.iod_iovec[i].iov_addr,
		     ehw_port->et_read_iovec.iod_iovec[i].iov_size);
  }
#endif

  tmp_iovec = ehw_port->et_read_iovec;
  if (ehw_iovec_size() < ETH_MAX_PACK_SIZE) {
	err_reply(mess, EPACKSIZE);
	return;
  }
  ehw_port->et_flags |= ETF_READING | ETF_READ_SU;

  do_int();			/* check if any packet ready */

  if (ehw_port->et_flags & ETF_READING) err_reply(mess, SUSPEND);

  assert(!(ehw_port->et_flags & ETF_READ_SU));
}

PRIVATE void do_init(mess)
message *mess;
{
  int result;
  int port = mess->DL_PORT;

  if (port < 0 || port >= EHW_PORT_NR || ehw_table + port != ehw_port) {
#if DEBUG
	if (d_eth_hw)
		printf("%s, %d: going to error reply\n", __FILE__, __LINE__);
#endif
	err_reply(mess, ENXIO);
	return;
  }
  if (!(ehw_port->et_flags & ETF_ENABLED))
	ehw_port->et_flags = ETF_EMPTY;
  else
	ehw_port->et_flags &= ~(ETF_ENABLED | ETF_PROMISC | ETF_MULTI |
				ETF_BROAD);

  if (mess->DL_MODE & DL_PROMISC_REQ)
	ehw_port->et_flags |= ETF_PROMISC | ETF_MULTI |
		ETF_BROAD;
  if (mess->DL_MODE & DL_MULTI_REQ) ehw_port->et_flags |= ETF_MULTI;
  if (mess->DL_MODE & DL_BROAD_REQ) ehw_port->et_flags |= ETF_BROAD;

  ehw_port->et_client = mess->m_source;
  if (!(ehw_port->et_flags & ETF_ENABLED)) {
	result = epl_init(&ehw_port->et_address);
	if (result < 0) {
#if DEBUG
		if (d_eth_hw)
			printf("%s, %d: going to error reply\n", __FILE__, __LINE__);
#endif
		err_reply(mess, result);
		return;
	}
	result = dp_init(&ehw_port->et_address);
	if (result < 0) {
#if DEBUG
		if (d_eth_hw)
			printf("%s, %d: going to error reply\n", __FILE__, __LINE__);
#endif
		err_reply(mess, result);
		return;
	}
  } else {
	result = dp_reinit();
	if (result < 0) {
#if DEBUG
		if (d_eth_hw)
			printf("%s, %d: going to error reply\n", __FILE__, __LINE__);
#endif
		err_reply(mess, result);
		return;
	}
  }

  ehw_port->et_flags |= ETF_ENABLED;

  reply_mess.m_type = DL_INIT_REPLY;
  reply_mess.m3_i1 = mess->DL_PORT;
  reply_mess.m3_i2 = EHW_PORT_NR;
  *(ether_addr_t *) reply_mess.m3_ca1 = ehw_port->et_address;

  mess_reply(mess, &reply_mess);

  return;
}


PRIVATE void do_int()
{
  int isr;
  int i;

  isr = read_reg0(dp_isr);

  if (isr & ISR_PTX) {
	int tsr = read_reg0(dp_tsr);

	if (!(read_reg0(dp_cr) & CR_TXP)) {
		ehw_port->et_flags &= ~ETF_SENDING;
		if (ehw_port->et_flags & ETF_SENDINT_EN)
			ehw_port->et_flags |= ETF_PACK_SEND;
	}
	if (tsr & TSR_PTX) ehw_port->et_stat.ets_packetT++;
	if (tsr & TSR_DFR) ehw_port->et_stat.ets_transDef++;
	if (tsr & TSR_COL) ehw_port->et_stat.ets_collision++;
	if (tsr & TSR_ABT) ehw_port->et_stat.ets_transAb++;
	if (tsr & TSR_CRS) {
		ehw_port->et_stat.ets_carrSense++;
#if DEBUG
		if (d_eth_hw) printk("eth_hw: carrier sense lost\n");
#endif
	}
	if (tsr & TSR_FU) {
		ehw_port->et_stat.ets_fifoUnder++;
		printk("eth_hw: fifo underrun\n");
	}
	if (tsr & TSR_CDH) {
		ehw_port->et_stat.ets_CDheartbeat++;
		printk("eth_hw: CD heart beat failure\n");
	}
	if (tsr & TSR_OWC) {
		ehw_port->et_stat.ets_OWC++;
		printk("eth_hw: out of window collision\n");
	}
	write_reg0(dp_isr, ISR_PTX);
  }
  if (isr & ISR_PRX) {
	ehw_recv();

	if (!(ehw_port->et_flags & ETF_READING)) write_reg0(dp_isr, ISR_PRX);
  }
  if (isr & ISR_RXE) {
#if DEBUG && 0
	{
		printW();
		printf("got Receive Error\n");
	}
#endif
	ehw_port->et_stat.ets_recvErr++;

	write_reg0(dp_isr, ISR_RXE);
  }
  if (isr & ISR_TXE) {
	printf("eth_hw: got Send Error\n");
	ehw_port->et_stat.ets_sendErr++;
	if (ehw_port->et_flags & ETF_SENDINT_EN)
		ehw_port->et_flags |= ETF_PACK_SEND;

	write_reg0(dp_isr, ISR_TXE);
  }
  if (isr & ISR_OVW) {
#if DEBUG && 0
	{
		printW();
		printf("got Overwrite Warning\n");
	}
#endif
	ehw_port->et_stat.ets_OVW++;
	for (i = 0; i < 4; i++) {
		throw_away_packet = TRUE;
		ehw_recv();
	}

	write_reg0(dp_isr, ISR_OVW);
  }
  if (isr & ISR_CNT) {
	ehw_port->et_stat.ets_CRCerr += read_reg0(dp_cntr0);
	ehw_port->et_stat.ets_frameAll += read_reg0(dp_cntr1);
	ehw_port->et_stat.ets_missedP += read_reg0(dp_cntr2);

	write_reg0(dp_isr, ISR_CNT);
  }
  if (isr & ISR_RDC)		/* remote DMA complete, but we don't use
				 * remote DMA */
	panic("eth_hw: remote dma complete", NO_NUM);

  if (isr & ISR_RST) {		/* this means we got an interrupt but the
				 * ethernet chip is shutdown. We don't do
				 * anything. */
#if DEBUG && 0
	{
		printW();
		printf("eth_hw: NIC stopped\n");
	}
#endif
#if 0
	panic("nic stopped", NO_NUM);
#else
	write_reg0(dp_cr, CR_STA);
#endif
  }
  if (ehw_port->et_flags & ETF_PACKAVAIL) ehw_recv();

  if (ehw_port->et_flags & ETF_PACK_SEND) ehw_inttask();
}

PRIVATE void do_getstat(mess)
message *mess;
{
  int port = mess->DL_PORT;
  int result;

  if (port < 0 || port >= EHW_PORT_NR || ehw_table + port != ehw_port ||
      !(ehw_port->et_flags & ETF_ENABLED)) {
	err_reply(mess, ENXIO);
	return;
  }
  ehw_port->et_stat.ets_CRCerr += read_reg0(dp_cntr0);
  ehw_port->et_stat.ets_frameAll += read_reg0(dp_cntr1);
  ehw_port->et_stat.ets_missedP += read_reg0(dp_cntr2);

  result = ehw_cp_loc2user((char *) &ehw_port->et_stat, mess->DL_PROC,
    (vir_bytes) mess->DL_ADDR, (vir_bytes) sizeof(ehw_port->et_stat));

  err_reply(mess, result);
}

PRIVATE int ehw_data2user(page, length)
int page, length;
{
  int last, count;
  int result;

  if (!(ehw_port->et_flags & ETF_READING)) {
	if (!throw_away_packet) return ERROR;
#if DEBUG && 0
	{
		printW();
		printf("throwing away a packet\n");
	}
#endif
	throw_away_packet = FALSE;
	return OK;		/* this means: we throw away one packet */
  }
  last = page + (length - 1) / EHW_PAGESIZE;
  if (last >= ehw_stoppage) {
	count = (ehw_stoppage - page) * EHW_PAGESIZE -
		sizeof(rcvdheader_t);
	tmp_iovec = ehw_port->et_read_iovec;
	result = ehw_hw2user(page * EHW_PAGESIZE + sizeof(rcvdheader_t),
			     0, count);
	if (result >= 0) {
		tmp_iovec = ehw_port->et_read_iovec;
		result = ehw_hw2user(ehw_startpage * EHW_PAGESIZE, count,
				     length - count);
	}
  } else {
	tmp_iovec = ehw_port->et_read_iovec;
	result = ehw_hw2user(page * EHW_PAGESIZE + sizeof(rcvdheader_t),
			     0, length);
  }


  ehw_port->et_read_s = result < 0 ? result : length;
  ehw_port->et_flags |= ETF_PACK_RECV;
  ehw_port->et_flags &= ~ETF_READING;

  return ehw_inttask();
}

PRIVATE void ehw_init()
{
  static eth_stat_t empty_stat = {0, 0, 0, 0, 0, 0 	/* ,... */ };
  ehw_linmem = EHW_LINMEM;
  ehw_memsegm = protected_mode ? EPLUS_SELECTOR :
	physb_to_hclick(EHW_LINMEM);

  ehw_port->et_flags = ETF_EMPTY;
  ehw_port->et_stat = empty_stat;
}

PRIVATE int epl_init(ea)
ether_addr_t *ea;
{
  int sum;

  ea->ea_addr[0] = read_reg_epl(epl_ea0);
  ea->ea_addr[1] = read_reg_epl(epl_ea1);
  ea->ea_addr[2] = read_reg_epl(epl_ea2);
  ea->ea_addr[3] = read_reg_epl(epl_ea3);
  ea->ea_addr[4] = read_reg_epl(epl_ea4);
  ea->ea_addr[5] = read_reg_epl(epl_ea5);
  sum = ea->ea_addr[0] + ea->ea_addr[1] + ea->ea_addr[2] + ea->ea_addr[3] +
	ea->ea_addr[4] + ea->ea_addr[5] +
	read_reg_epl(epl_res2) +
	read_reg_epl(epl_chksum);
  if ((sum & 0xFF) != 0xFF) {
	printf("No ethernet+ board\n");
	return ERROR;
  }
  printf("Found an ethernet+ card\n");

  write_reg_epl(epl_ctlstatus, CTL_RESET);
  write_reg_epl(epl_ctlstatus, CTL_MENABLE |
	      ((ehw_linmem >> 13) & CTL_MEMADDR));
  return OK;
}

PRIVATE int dp_init(ea)
ether_addr_t *ea;
{
  int dp_rcr_reg;

  write_reg0(dp_cr, CR_PS_P0 | CR_DM_ABORT);
  write_reg0(dp_pstart, ehw_startpage);
  write_reg0(dp_pstop, ehw_stoppage);
  write_reg0(dp_bnry, ehw_startpage);
  write_reg0(dp_rcr, RCR_MON);
  write_reg0(dp_tcr, TCR_NORMAL);
  write_reg0(dp_dcr, DCR_BYTEWIDE | DCR_8BYTES);
  write_reg0(dp_rbcr0, 0);
  write_reg0(dp_rbcr1, 0);
  write_reg0(dp_isr, 0xFF);
  write_reg0(dp_cr, CR_PS_P1 | CR_DM_ABORT);

  write_reg1(dp_par0, ea->ea_addr[0]);
  write_reg1(dp_par1, ea->ea_addr[1]);
  write_reg1(dp_par2, ea->ea_addr[2]);
  write_reg1(dp_par3, ea->ea_addr[3]);
  write_reg1(dp_par4, ea->ea_addr[4]);
  write_reg1(dp_par5, ea->ea_addr[5]);

  write_reg1(dp_mar0, 0xff);
  write_reg1(dp_mar1, 0xff);
  write_reg1(dp_mar2, 0xff);
  write_reg1(dp_mar3, 0xff);
  write_reg1(dp_mar4, 0xff);
  write_reg1(dp_mar5, 0xff);
  write_reg1(dp_mar6, 0xff);
  write_reg1(dp_mar7, 0xff);

  write_reg1(dp_curr, ehw_startpage + 1);
  write_reg1(dp_cr, CR_PS_P0 | CR_DM_ABORT);

  dp_rcr_reg = 0;
  if (ehw_port->et_flags & ETF_PROMISC)
	dp_rcr_reg |= RCR_AB | RCR_PRO | RCR_AM;
  if (ehw_port->et_flags & ETF_BROAD) dp_rcr_reg |= RCR_AB;
  if (ehw_port->et_flags & ETF_MULTI) dp_rcr_reg |= RCR_AM;
  write_reg0(dp_rcr, dp_rcr_reg);
  read_reg0(dp_cntr0);		/* reset counters by reading */
  read_reg0(dp_cntr1);
  read_reg0(dp_cntr2);

  write_reg0(dp_imr, IMR_PTXE | IMR_TXEE | IMR_PRXE | IMR_CNTE |
	   IMR_OVWE);
  write_reg0(dp_cr, CR_STA | CR_DM_ABORT);

  return OK;
}

PRIVATE int dp_reinit()
{
  int dp_rcr_reg;

  write_reg0(dp_cr, CR_PS_P0);

  dp_rcr_reg = 0;
  if (ehw_port->et_flags & ETF_PROMISC)
	dp_rcr_reg |= RCR_AB | RCR_PRO | RCR_AM;
  if (ehw_port->et_flags & ETF_BROAD) dp_rcr_reg |= RCR_AB;
  if (ehw_port->et_flags & ETF_MULTI) dp_rcr_reg |= RCR_AM;
  write_reg0(dp_rcr, dp_rcr_reg);

  return OK;
}

PRIVATE void ehw_recv()
{
  rcvdheader_t header;
  unsigned pageno, curr, next;
  vir_bytes length;
  int packet_processed = FALSE;

  pageno = read_reg0(dp_bnry) + 1;
  if (pageno == ehw_stoppage) pageno = ehw_startpage;

  do {
	write_reg0(dp_cr, CR_PS_P1);
	curr = read_reg1(dp_curr);
	write_reg0(dp_cr, CR_PS_P0);
	/* Warning: this can cause race conditions if some other
	 * thread also uses this registers */
	if (pageno == curr) break;

	ehw_getheader(pageno, &header);
	length = (header.rp_rbcl | (header.rp_rbch << 8)) -
		sizeof(rcvdheader_t);
	next = header.rp_next;
	if (header.rp_status & RSR_FO) {
		/* This is very serious, so we issue a warnig and
		 * reset the buffers */
		printk("eth_hw: fifo overrun, reseting receive buffer\n");
		ehw_port->et_stat.ets_fifoOver++;
		next = curr;
	} else if ((header.rp_status & RSR_PRX) &&
		   (ehw_port->et_flags & ETF_ENABLED)) {
		int result = ehw_data2user(pageno, length);

		if (result < 0) return;
		packet_processed = TRUE;
		ehw_port->et_stat.ets_packetR++;
	}
	if (next == ehw_startpage)
		write_reg0(dp_bnry, ehw_stoppage - 1);
	else
		write_reg0(dp_bnry, next - 1);

	pageno = next;
	assert(pageno >= ehw_startpage && pageno < ehw_stoppage);
  }
  while (!packet_processed);
  if (pageno == curr) {		/* if receive buffer empty then the receive
				 * interrupt can be acknowledged */
	if (read_reg0(dp_isr) & ISR_PRX) write_reg0(dp_isr, ISR_PRX);
	ehw_port->et_flags &= ~ETF_PACKAVAIL;
  } else
	ehw_port->et_flags |= ETF_PACKAVAIL;
}

PRIVATE void do_stop()
{
#if DEBUG && 0
  {
	printW();
	printf("got a stop request\n");
  }
#endif
  write_reg0(dp_cr, CR_STP);
  ehw_port->et_flags &= ETF_EMPTY;
}

PRIVATE void mess_reply(req, reply)
message *req, *reply;
{
  ehw_tab_t *save_port_nr = ehw_port;	/* save port because a Send
					 * may block */
#if DEBUG && 0
  {
	printW();
	printk("eth_hw: mess_reply to %d\n",
	       req->m_source);
  }
#endif
#if DEBUG && 0
  {
	printW();
	printf("sending\n");
  }
#endif
  if (send(req->m_source, reply) < 0)
	panic("eth_hw: unable to mess_reply()", NO_NUM);
#if DEBUG && 0
  {
	printW();
	printf("Send completed\n");
  }
#endif

  ehw_port = save_port_nr;
}

PRIVATE void err_reply(m, err)
message *m;
int err;
{
  ehw_tab_t *save_port_nr = ehw_port;	/* save port because a Send
					 * may block */
  message reply;
  int status;

#if DEBUG
  if (d_eth_hw) printk("eth_hw: err_reply() %d to %d\n", err, m->m_source);
#endif
  status = 0;
  if (ehw_port->et_flags & ETF_PACK_SEND) {
	assert(ehw_port->et_flags & ETF_SENDINT_EN);
	status |= DL_PACK_SEND;
#if DEBUG
	{
		printW();
		printf("replying pack sent\n");
	}
#endif
  }
  if (ehw_port->et_flags & ETF_PACK_RECV) {
	status |= DL_PACK_RECV;
#if DEBUG && 0
	{
		printW();
		printf("replying pack received\n");
	}
#endif
  }
  if (!(ehw_port->et_flags & ETF_ENABLED)) status |= DL_DISABLED;

  reply.m_type = DL_TASK_REPLY;
  reply.DL_PORT = m->DL_PORT;
  reply.DL_PROC = m->DL_PROC;
  reply.DL_STAT = status | ((u32_t) err << 16);
  reply.DL_COUNT = ehw_port->et_read_s;
  reply.DL_CLCK = get_uptime();
#if DEBUG && 0
  {
	printW();
	printf("sending DL_TASK_REPLY\n");
  }
#endif
  if (send(m->m_source, &reply) < 0)
	panic("eth_hw: unable to err_reply()", NO_NUM);
#if DEBUG && 0
  {
	printW();
	printf("Send completed\n");
  }
#endif

  ehw_port = save_port_nr;
  ehw_port->et_read_s = 0;
  if (ehw_port->et_flags & ETF_PACK_SEND) {
	ehw_port->et_flags &= ~(ETF_PACK_SEND | ETF_SENDING |
				ETF_SENDINT_EN);
#if DEBUG
	{
		printW();
		printf("clearing SENDINT_EN\n");
	}
#endif
  }
  ehw_port->et_flags &= ~(ETF_PACK_RECV | ETF_READ_SU);
}

PRIVATE int ehw_inttask()
{
  ehw_tab_t *save_port_nr;	/* save port because a Send may block */
  message reply;
  int status, result;

  save_port_nr = ehw_port;
  reply.m_type = DL_INT_TASK;
  status = 0;

  if (ehw_port->et_flags & ETF_PACK_SEND) {
	assert(ehw_port->et_flags & ETF_SENDINT_EN);
	status |= DL_PACK_SEND;
#if DEBUG && 0
	{
		printW();
		printf("replying pack sent\n");
	}
#endif
  }
  if (ehw_port->et_flags & ETF_PACK_RECV) {
	status |= DL_PACK_RECV;
	if (ehw_port->et_flags & ETF_READ_SU) {
		reply.m_type = DL_TASK_REPLY;
		ehw_port->et_flags &= ~ETF_READ_SU;
	}
#if DEBUG && 0
	{
		printW();
		printf("replying pack received\n");
	}
#endif
  }
  if (!(ehw_port->et_flags & ETF_ENABLED)) status |= DL_DISABLED;

  reply.DL_PORT = ehw_port - ehw_table;
  reply.DL_PROC = ehw_port->et_client;
  reply.DL_STAT = status;
  reply.DL_COUNT = ehw_port->et_read_s;
  reply.DL_CLCK = get_uptime();

#if DEBUG && 0
  {
	printW();
	printf("sending\n");
  }
#endif
  result = send(ehw_port->et_client, &reply);
#if DEBUG && 0
  {
	printW();
	printf("Send completed\n");
  }
#endif

  ehw_port = save_port_nr;
  if (result < 0) {
#if DEBUG
	{
		printW();
		printf("Send had error %d\n", result);
	}
#endif
	return result;
	/* The only error posible should be a deadlock but we're able
	 * to regenerate this message so we just don't reset some
	 * status flags. */
  }
  ehw_port->et_read_s = 0;
  if (ehw_port->et_flags & ETF_PACK_SEND) {
	ehw_port->et_flags &= ~(ETF_PACK_SEND | ETF_SENDING |
				ETF_SENDINT_EN);
#if DEBUG && 0
	{
		printW();
		printf("clearing SENDINT_EN\n");
	}
#endif
  }
  ehw_port->et_flags &= ~ETF_PACK_RECV;
  return OK;
}

PRIVATE void ehw_getheader(page, h)
int page;
rcvdheader_t *h;
{
  u16_t *ha = (u16_t *) h;
  u16_t offset = page * EHW_PAGESIZE;

#if DEBUG
  if (d_eth_hw) printk("eth_hw: mem_rdw(0x%x:0x%x)\n", ehw_memsegm, offset);
#endif
  *ha = mem_rdw(ehw_memsegm, offset);
  ha++;
  offset += sizeof(*ha);

#if DEBUG
  if (d_eth_hw) printk("eth_hw: mem_rdw(0x%x:0x%x)\n", ehw_memsegm, offset);
#endif
  *ha = mem_rdw(ehw_memsegm, offset);
}

PRIVATE int ehw_cp_user2loc(user_proc, user_addr, loc_addr, count)
int user_proc;
vir_bytes user_addr;
char *loc_addr;
vir_bytes count;
{
  phys_bytes src, dest;

  src = numap(user_proc, user_addr, count);
  if (!src) {
#if DEBUG
	if (d_eth_hw) printk("ehw_cp_user2loc: user umap failed\n");
#endif
	return EFAULT;
  }
#if DEBUG
  if (d_eth_hw)
	printk("ehw_cp_user2loc: %d bytes from %x in proc %d to %x in %d\n",
	       count, user_addr, user_proc, (vir_bytes) loc_addr, proc_number(proc_ptr));
#endif

  dest = umap(proc_ptr, D, (vir_bytes) loc_addr, count);
  assert(dest != 0);

  phys_copy(src, dest, (phys_bytes) count);

  return OK;
}

PRIVATE int ehw_cp_loc2user(loc_addr, user_proc, user_addr, count)
char *loc_addr;
int user_proc;
vir_bytes user_addr;
vir_bytes count;
{
  phys_bytes src, dst;

  dst = numap(user_proc, user_addr, count);
  if (!dst) {
#if DEBUG
	if (d_eth_hw) printk("ehw_cp_loc2user: user umap failed\n");
#endif
	return EFAULT;
  }
#if DEBUG
  if (d_eth_hw)
	printk("ehw_cp_loc2user: %d bytes to %x in proc %d from %x in %d\n",
	       count, user_addr, user_proc, (vir_bytes) loc_addr, proc_number(proc_ptr));
#endif

  src = umap(proc_ptr, D, (vir_bytes) loc_addr, count);
  assert(src != 0);

  phys_copy(src, dst, (phys_bytes) count);

  return OK;
}

PRIVATE int ehw_iovec_size()
{
  int size = 0;
  int i, n, result;

  while (TRUE) {
	n = tmp_iovec.iod_iovec_s;
	if (n > IOVEC_NR) n = IOVEC_NR;

	for (i = 0; i < n; i++) size += tmp_iovec.iod_iovec[i].iov_size;

	if (tmp_iovec.iod_iovec_s == n) break;

	result = ehw_next_iovec();
	if (result < 0) return result;
  }
  return size;
}

PRIVATE int ehw_hw2user(hw_addr, user_offs, count)
int hw_addr;
vir_bytes user_offs;
vir_bytes count;
{
  phys_bytes phys_hw, phys_user;
  int result, bytes, n, i;

#if DEBUG
  if (d_eth_hw)
	printf("ehw_hw2user (%x, %x, %x)\n", hw_addr, user_offs, count);
#endif

  phys_hw = ehw_linmem + hw_addr;

  do {
	n = tmp_iovec.iod_iovec_s;
	if (n > IOVEC_NR) n = IOVEC_NR;

	for (i = 0; i < n && user_offs >= tmp_iovec.iod_iovec[i].iov_size;
	     i++)
		user_offs -= tmp_iovec.iod_iovec[i].iov_size;

	if (i == n) {
		result = ehw_next_iovec();
		if (result < 0) return result;
	}
  }
  while (i == n);
  if (user_offs) {
	bytes = tmp_iovec.iod_iovec[i].iov_size - user_offs;
	if (bytes > count) bytes = count;

	phys_user = numap(tmp_iovec.iod_proc_nr,
			tmp_iovec.iod_iovec[i].iov_addr + user_offs,
			  bytes);
#if DEBUG
	if (d_eth_hw)
		printk("ehw_hw2user: 1st numap(%d, %x, %x) failed\n", tmp_iovec.iod_proc_nr,
		tmp_iovec.iod_iovec[i].iov_addr + user_offs, bytes);
#endif
	if (!phys_user) {
#if DEBUG
		if (d_eth_hw)
			printk("ehw_hw2user: 1st numap(%d, %x, %x) failed\n", tmp_iovec.iod_proc_nr,
			       tmp_iovec.iod_iovec[i].iov_addr + user_offs, bytes);
#endif
		return EFAULT;
	}
#if DEBUG
	if (d_eth_hw)
		printk("ehw_hw2user: phys_copy(%X,%X,%X)\n", phys_user, phys_hw,
		       (phys_bytes) bytes);
#endif
	phys_copy(phys_hw, phys_user, (phys_bytes) bytes);
	count -= bytes;
	phys_hw += bytes;
	i++;
  }
  while (count) {
	if (i == n) {
		result = ehw_next_iovec();
		if (result < 0) return result;
		i = 0;
	}
	for (; i < n; i++) {
		bytes = tmp_iovec.iod_iovec[i].iov_size;
		if (bytes > count) bytes = count;

		phys_user = numap(tmp_iovec.iod_proc_nr,
				  tmp_iovec.iod_iovec[i].iov_addr,
				  bytes);
#if DEBUG
		if (d_eth_hw)
			printk("ehw_hw2user: 2nd numap(%d, %x, %x) failed\n", tmp_iovec.iod_proc_nr,
			       tmp_iovec.iod_iovec[i].iov_addr + user_offs, bytes);
#endif
		if (!phys_user) {
#if DEBUG
			if (d_eth_hw)
				printk("ehw_hw2user: 2nd numap(%d, %x, %x) failed\n", tmp_iovec.iod_proc_nr,
				       tmp_iovec.iod_iovec[i].iov_addr + user_offs, bytes);
			printk("ehw_hw2user: umap failed\n");
#endif
			return EFAULT;
		}
#if DEBUG
		if (d_eth_hw)
			printk("ehw_hw2user: phys_copy(%X,%X,%X)\n", phys_user, phys_hw,
			       (phys_bytes) bytes);
#endif
		phys_copy(phys_hw, phys_user, (phys_bytes) bytes);
		count -= bytes;
		phys_hw += bytes;
		if (!count) break;
	}
  }
  return OK;
}

PRIVATE int ehw_user2hw(user_offs, hw_addr, count)
vir_bytes user_offs;
int hw_addr;
vir_bytes count;
{
  phys_bytes phys_hw, phys_user;
  int result, bytes, n, i;

#if DEBUG
  if (d1_eth_hw)
	printk("ehw_user2hw(%x, %x, %x)\n", user_offs, hw_addr, count);
#endif

  phys_hw = ehw_linmem + hw_addr;

  do {
	n = tmp_iovec.iod_iovec_s;
	if (n > IOVEC_NR) n = IOVEC_NR;

	for (i = 0; i < n && user_offs >= tmp_iovec.iod_iovec[i].iov_size;
	     i++)
		user_offs -= tmp_iovec.iod_iovec[i].iov_size;

	if (i == n) {
		result = ehw_next_iovec();
		if (result < 0) return result;
	}
  }
  while (i == n);
  if (user_offs) {
	bytes = tmp_iovec.iod_iovec[i].iov_size - user_offs;
	if (bytes > count) bytes = count;

	phys_user = numap(tmp_iovec.iod_proc_nr,
			tmp_iovec.iod_iovec[i].iov_addr + user_offs,
			  bytes);
	if (!phys_user) {
#if DEBUG
		if (d1_eth_hw)
			printk("ehw_user2hw: 1st numap(%d, %x, %x) failed\n", tmp_iovec.iod_proc_nr,
			       tmp_iovec.iod_iovec[i].iov_addr + user_offs, bytes);
		printk("ehw_user2hw: umap failed\n");
#endif
		return EFAULT;
	}
#if DEBUG
	if (d1_eth_hw)
		printk("ehw_user2hw: phys_copy(%X,%X,%X)\n", phys_user, phys_hw,
		       (phys_bytes) bytes);
#endif
	phys_copy(phys_user, phys_hw, (phys_bytes) bytes);
	count -= bytes;
	phys_hw += bytes;
	i++;
  }
  while (count) {
	if (i == n) {
		result = ehw_next_iovec();
		if (result < 0) return result;
		i = 0;
	}
	for (; i < n; i++) {
		bytes = tmp_iovec.iod_iovec[i].iov_size;
		if (bytes > count) bytes = count;

		phys_user = numap(tmp_iovec.iod_proc_nr,
				  tmp_iovec.iod_iovec[i].iov_addr,
				  bytes);
		if (!phys_user) {
#if DEBUG
			if (d1_eth_hw)
				printk("ehw_user2hw: 2nd numap(%d, %x, %x) failed\n", tmp_iovec.iod_proc_nr,
				       tmp_iovec.iod_iovec[i].iov_addr + user_offs, bytes);
			printk("ehw_user2hw: umap failed\n");
#endif
			return EFAULT;
		}
#if DEBUG
		if (d1_eth_hw)
			printk("ehw_user2hw: phys_copy(%X,%X,%X)\n", phys_user, phys_hw,
			       (phys_bytes) bytes);
#endif
		phys_copy(phys_user, phys_hw, (phys_bytes) bytes);
		count -= bytes;
		phys_hw += bytes;
		if (!count) break;
	}
  }
  return OK;
}

PRIVATE int ehw_next_iovec()
{
  if (tmp_iovec.iod_iovec_s <= IOVEC_NR) {
	printk("eth_hw: ehw_next_iovec failed\n");
	return EINVAL;
  }
  tmp_iovec.iod_iovec_s -= IOVEC_NR;

  tmp_iovec.iod_iovec_addr += IOVEC_NR * sizeof(iovec_t);

  return ehw_cp_user2loc(tmp_iovec.iod_proc_nr,
	     tmp_iovec.iod_iovec_addr, (char *) tmp_iovec.iod_iovec,
		       (tmp_iovec.iod_iovec_s > IOVEC_NR ? IOVEC_NR :
			tmp_iovec.iod_iovec_s) * sizeof(iovec_t));

}
