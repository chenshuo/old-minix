/* This file contains the hardware dependent netwerk device driver
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
 * |DL_TASK-REPL| port nr  | proc nr | rd-count | err|stat| clock   |
 * |------------|----------|---------|----------|---------|---------|
 * |DL_INT-TASK | port nr  | proc nr | rd-count | err|stat| clock   |
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
#include <net/gen/ether.h>
#include "protect.h"
#include "assert.h"
#include "ether.h"
#include "hw_conf.h"
#include "dp8390.h"
#include "epl.h"
#include "proc.h"

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
  int et_type;
  int et_ramsize;
  int et_startpage;
  int et_stoppage;
  message et_sendmsg;
} ehw_tab_t;

#define ETF_FLAGS	0x3FF;
#define ETF_EMPTY	0x000
#define ETF_PACK_SEND	0x001
#define ETF_PACK_RECV	0x002
#define ETF_SEND_AVAIL	0x004
#define ETF_SENDING	0x008
#define ETF_READING	0x010
#define ETF_PROMISC	0x040
#define ETF_MULTI	0x080
#define ETF_BROAD	0x100
#define ETF_ENABLED	0x200

#define ETT_ETHERNET	0x01		/* Ethernet transceiver */
#define ETT_STARLAN	0x02		/* Starlan transceiver */
#define ETT_INTERF_CHIP	0x04		/* has a WD83C583 interface chip */
#define ETT_BRD_16BIT	0x08		/* 16 bit board */
#define ETT_SLT_16BIT	0x10		/* 16 bit slot */

PRIVATE int ehw_enabled;
PRIVATE int ehw_baseport;
PRIVATE int ehw_irq;
PRIVATE phys_bytes ehw_linmem;
PRIVATE segm_t ehw_memsegm;
PRIVATE ehw_tab_t ehw_table[EHW_PORT_NR];
PRIVATE ehw_tab_t *ehw_port;
PRIVATE message mess, reply_mess;
PRIVATE iovec_dat_t tmp_iovec, tmp1_iovec;
PRIVATE int throw_away_packet = FALSE;

#define ehw_cp_loc2user ehw_loc2user

FORWARD _PROTOTYPE( void do_write, (message *mess, int task_int) );
FORWARD _PROTOTYPE( void do_vwrite, (message *mess, int task_int) );
FORWARD _PROTOTYPE( void do_vread, (message *mess) );
FORWARD _PROTOTYPE( void do_read, (message *mess) );
FORWARD _PROTOTYPE( void do_init, (message *mess) );
FORWARD _PROTOTYPE( void do_getstat, (message *mess) );
FORWARD _PROTOTYPE( void do_stop, (void) );
FORWARD _PROTOTYPE( void do_int, (void) );
FORWARD _PROTOTYPE( void check_ints, (void) );
FORWARD _PROTOTYPE( void ehw_init, (void) );
FORWARD _PROTOTYPE( int epl_init, (ether_addr_t *ea) );
FORWARD _PROTOTYPE( int dp_init, (ether_addr_t *ea) );
FORWARD _PROTOTYPE( int dp_reinit, (void) );
FORWARD _PROTOTYPE( void err_reply, (int err, int type) );
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
FORWARD _PROTOTYPE( int ehw_handler, (int irq) );
FORWARD _PROTOTYPE( void ehw_recv, (void) );
FORWARD _PROTOTYPE( void ehw_send, (void) );
FORWARD _PROTOTYPE( int wd_aliasing, (void) );
FORWARD _PROTOTYPE( int wd_interface_chip, (void) );
FORWARD _PROTOTYPE( int wd_16bitboard, (void) );
FORWARD _PROTOTYPE( int wd_16bitslot, (void) );

PUBLIC void ehw_task()
{
  ehw_tab_t *this_port = &ehw_table[0];	/* this task uses port 0 */

  ehw_port = this_port;
  ehw_init();

  while (TRUE) {
	if (receive(ANY, &mess) < 0) panic("eth_hw: receive failed", NO_NUM);
#if DEBUG & 256
 { printW(); printf("eth_hw: received %d message from %d\n", mess.m_type,
							       mess.m_source); }
#endif
	ehw_port = this_port;

	if (mess.m_type != HARD_INT && mess.m_type != DL_INIT &&
		mess.m_type != DL_STOP)
	{
		if (mess.DL_PROC != ehw_port->et_client ||
			mess.m_source != ehw_port->et_client ||
			mess.DL_PORT != ehw_port - ehw_table)
		{
			mess.DL_STAT= EINVAL;
			mess.m_type= DL_TASK_REPLY;
#if DEBUG
 { printW(); printf("calling send\n"); }
#endif
			send(mess.m_source, &mess);
			continue;
		}
	}
			
	switch (mess.m_type) {
	    case DL_WRITE:	do_write(&mess, FALSE);		break;
	    case DL_WRITEV:	do_vwrite(&mess, FALSE);	break;
	    case DL_READ:	do_read(&mess);			break;
	    case DL_READV:	do_vread(&mess);		break;
	    case DL_INIT:	do_init(&mess);			break;
	    case DL_GETSTAT:	do_getstat(&mess);		break;
	    case DL_STOP:	do_stop();			break;
	    case HARD_INT:
	if (ehw_port->et_flags & ETF_ENABLED || 1)
		check_ints();	/* we like to know about received or
				 * transmitted packets as soon as
				 * possible */
	do_int();			break;
	    default:
#if DEBUG
 { printW(); printf("eth_hw: warning got unknown message (type %d) from %d\n", 
						 mess.m_type, mess.m_source); }
#endif
		err_reply(EINVAL, DL_TASK_REPLY);
		break;
	}
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
  printf("dp_isr=0x%x, and 0x%x, et_flags= 0x%x\r\n", read_reg0(dp_isr),
         read_reg0(dp_isr),
         ehw_port->et_flags);
  printf("\r\n");
}

PUBLIC void ehw_stop()
{
	ehw_port= &ehw_table[0];

	do_stop();
}

PRIVATE void do_write(mess, task_int)
message *mess;
int task_int;
{
  int port = mess->DL_PORT;
  int count = mess->DL_COUNT;
  int result;

assert(!(ehw_port->et_flags & ETF_SEND_AVAIL));
  if (port < 0 || port >= EHW_PORT_NR || ehw_table + port != ehw_port ||
	!(ehw_port->et_flags & ETF_ENABLED)) {
	if (task_int)
		ehw_port->et_flags |= ETF_PACK_SEND;
	else
	{
#if DEBUG
 { printW(); printf("calling err_reply\n"); }
#endif
		err_reply(ENXIO, DL_TASK_REPLY);
	}
	return;
  }
  if (ehw_port->et_flags & ETF_SENDING) {
	if (task_int)
	{
		panic("should not be sending\n", NO_NUM);
	}
#if DEBUG
 { int isr, cr; cr= read_reg0(dp_cr); isr= read_reg0(dp_isr);
   if(!(read_reg0(dp_cr) & CR_TXP) && !(read_reg0(dp_isr) & ISR_PTX))
   {
	printf("cr= 0x%x, isr= 0x%x\n", cr, isr);
	panic("send error or lost int", NO_NUM);
   }
 }
#endif
	ehw_port->et_sendmsg= *mess;
	ehw_port->et_flags |= ETF_SEND_AVAIL;
#if DEBUG
 { printW(); printf("send ip, packet queued\n"); }
#endif
	err_reply(OK, DL_TASK_REPLY);
	return;
  }
  assert(!(ehw_port->et_flags & ETF_PACK_SEND));
  if (count < ETH_MIN_PACK_SIZE || count > ETH_MAX_PACK_SIZE) {
	if (task_int)
		ehw_port->et_flags |= ETF_PACK_SEND;
	else
	{
#if DEBUG
 { printW(); printf("calling err_reply\n"); }
#endif
		err_reply(EPACKSIZE, DL_TASK_REPLY);
	}
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
	if (task_int)
		ehw_port->et_flags |= ETF_PACK_SEND;
	else
	{
#if DEBUG
 { printW(); printf("calling err_reply\n"); }
#endif
		err_reply(result, DL_TASK_REPLY);
	}
	return;
  }
  write_reg0(dp_tpsr, ehw_sendpage);
  write_reg0(dp_tbcr1, count >> 8);
  write_reg0(dp_tbcr0, count & 0xff);
  write_reg0(dp_cr, CR_TXP);	/* there is goes.. */

  ehw_port->et_flags |= (ETF_SENDING | ETF_PACK_SEND);

  if (task_int)
	return;
#if DEBUG & 256
 { printW(); printf("calling err_reply\n"); }
#endif
  err_reply(OK, DL_TASK_REPLY);
}

PRIVATE void do_vwrite(mess, task_int)
message *mess;
int task_int;
{
  int port = mess->DL_PORT;
  int count = mess->DL_COUNT;
  int result, size;

  assert(!(ehw_port->et_flags & ETF_SEND_AVAIL));
  if (port < 0 || port >= EHW_PORT_NR || ehw_table + port != ehw_port ||
	!(ehw_port->et_flags & ETF_ENABLED)) {
	if (task_int)
		ehw_port->et_flags |= ETF_PACK_SEND;
	else
	{
#if DEBUG
 { printW(); printf("calling err_reply\n"); }
#endif
		err_reply(ENXIO, DL_TASK_REPLY);
	}
	return;
  }
  if (ehw_port->et_flags & ETF_SENDING) {
	if (task_int)
		panic("should not be sending", NO_NUM);
	ehw_port->et_sendmsg= *mess;
	ehw_port->et_flags |= ETF_SEND_AVAIL;
#if DEBUG & 256
 { printW(); printf("send ip, packet queued\n"); }
#endif
	err_reply(OK, DL_TASK_REPLY);
	return;
  }
  assert(!(ehw_port->et_flags & ETF_PACK_SEND));

  result = ehw_cp_user2loc(mess->DL_PROC, (vir_bytes) mess->DL_ADDR,
			 (char *) tmp1_iovec.iod_iovec,
	   (count > IOVEC_NR ? IOVEC_NR : count) * sizeof(iovec_t));
  if (result < 0) {
	if (task_int)
		ehw_port->et_flags |= ETF_PACK_SEND;
	else
	{
#if DEBUG
 { printW(); printf("calling err_reply\n"); }
#endif
		err_reply(result, DL_TASK_REPLY);
	}
	return;
  }
  tmp1_iovec.iod_iovec_s = count;
  tmp1_iovec.iod_proc_nr = mess->DL_PROC;
  tmp1_iovec.iod_iovec_addr = (vir_bytes) mess->DL_ADDR;

  tmp_iovec = tmp1_iovec;
  size = ehw_iovec_size();
  if (size < 0) {
	if (task_int)
		ehw_port->et_flags |= ETF_PACK_SEND;
	else
	{
#if DEBUG
 { printW(); printf("calling err_reply\n"); }
#endif
		err_reply(size, DL_TASK_REPLY);
	}
	return;
  }
  if (size < ETH_MIN_PACK_SIZE || size > ETH_MAX_PACK_SIZE) {
	if (task_int)
		ehw_port->et_flags |= ETF_PACK_SEND;
	else
	{
#if DEBUG
 { printW(); printf("calling err_reply\n"); }
#endif
		err_reply(EPACKSIZE, DL_TASK_REPLY);
	}
	return;
  }
  tmp_iovec = tmp1_iovec;
  result = ehw_user2hw(0, ehw_sendpage * EHW_PAGESIZE, size);
  if (result < 0) {
	if (task_int)
		ehw_port->et_flags |= ETF_PACK_SEND;
	else
	{
#if DEBUG
 { printW(); printf("calling err_reply\n"); }
#endif
		err_reply(result, DL_TASK_REPLY);
	}
	return;
  }
  write_reg0(dp_tpsr, ehw_sendpage);
  write_reg0(dp_tbcr1, size >> 8);
  write_reg0(dp_tbcr0, size & 0xff);
  write_reg0(dp_cr, CR_TXP);	/* there is goes.. */

  ehw_port->et_flags |= (ETF_SENDING | ETF_PACK_SEND);

  if (task_int)
	return;
#if DEBUG & 256
 { printW(); printf("calling err_reply\n"); }
#endif
  err_reply(OK, DL_TASK_REPLY);
}

PRIVATE void do_read(mess)
message *mess;
{
  int port = mess->DL_PORT;
  int count = mess->DL_COUNT;

  if (port < 0 || port >= EHW_PORT_NR || ehw_table + port != ehw_port ||
      !(ehw_port->et_flags & ETF_ENABLED)) {
#if DEBUG
 { printW(); printf("calling err_reply\n"); }
#endif
	err_reply(ENXIO, DL_TASK_REPLY);
	return;
  }
  if (count < ETH_MAX_PACK_SIZE) {
#if DEBUG
 { printW(); printf("calling err_reply\n"); }
#endif
	err_reply(EPACKSIZE, DL_TASK_REPLY);
	return;
  }
  ehw_port->et_read_iovec.iod_iovec[0].iov_addr =
	(vir_bytes) mess->DL_ADDR;
  ehw_port->et_read_iovec.iod_iovec[0].iov_size = mess->DL_COUNT;
  ehw_port->et_read_iovec.iod_iovec_s = 1;
  ehw_port->et_read_iovec.iod_proc_nr = mess->DL_PROC;
  ehw_port->et_read_iovec.iod_iovec_addr = 0;

#if DEBUG
 { printW(); printf("setting reading\n"); }
#endif

  ehw_port->et_flags |= ETF_READING;

  ehw_recv();

#if DEBUG
 { printW(); printf("calling err_reply\n"); }
#endif
  err_reply(OK, DL_TASK_REPLY);
}

PRIVATE void do_vread(mess)
message *mess;
{
  int port = mess->DL_PORT;
  int count = mess->DL_COUNT;
  int result;

  if (port < 0 || port >= EHW_PORT_NR || ehw_table + port != ehw_port ||
      !(ehw_port->et_flags & ETF_ENABLED)) {
#if DEBUG
 { printW(); printf("calling err_reply\n"); }
#endif
	err_reply(ENXIO, DL_TASK_REPLY);
	return;
  }
  result = ehw_cp_user2loc(mess->DL_PROC, (vir_bytes) mess->DL_ADDR,
			 (char *) ehw_port->et_read_iovec.iod_iovec,
	   (count > IOVEC_NR ? IOVEC_NR : count) * sizeof(iovec_t));
  if (result < 0) {
#if DEBUG
 { printW(); printf("calling err_reply\n"); }
#endif
	err_reply(result, DL_TASK_REPLY);
	return;
  }
  ehw_port->et_read_iovec.iod_iovec_s = count;
  ehw_port->et_read_iovec.iod_proc_nr = mess->DL_PROC;
  ehw_port->et_read_iovec.iod_iovec_addr = (vir_bytes) mess->DL_ADDR;
#if DEBUG & 256
 {
	int i;
	printW(); for (i = 0; i < (count < IOVEC_NR ? count : IOVEC_NR); i++)
		printf("iovec[%d]: iov_addr= %x, iov_size= %x\n", i,
		       ehw_port->et_read_iovec.iod_iovec[i].iov_addr,
		     ehw_port->et_read_iovec.iod_iovec[i].iov_size); }
#endif

  tmp_iovec = ehw_port->et_read_iovec;
  if (ehw_iovec_size() < ETH_MAX_PACK_SIZE) {
#if DEBUG
 { printW(); printf("calling err_reply\n"); }
#endif
	err_reply(EPACKSIZE, DL_TASK_REPLY);
	return;
  }
#if DEBUG & 256
 { printW(); printf("setting reading\n"); }
#endif
  ehw_port->et_flags |= ETF_READING;

  ehw_recv();

#if DEBUG & 256
 { printW(); printf("calling err_reply\n"); }
#endif
  err_reply(OK, DL_TASK_REPLY);
}

PRIVATE void do_init(mess)
message *mess;
{
  int result;
  int port = mess->DL_PORT;

  if (!ehw_enabled || port < 0 || port >= EHW_PORT_NR
  					|| ehw_table + port != ehw_port) {
#if DEBUG
 { printW(); printf("%s, %d: going to error reply\n", __FILE__, __LINE__); }
#endif
	reply_mess.m_type= DL_INIT_REPLY;
	reply_mess.m3_i1= ENXIO;
	mess_reply(mess, &reply_mess);
	return;
  }


  if (!(ehw_port->et_flags & ETF_ENABLED))
  {
	ehw_stop();
	milli_delay(1);
	ehw_port->et_flags = ETF_EMPTY;
  }
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
 { printW(); printf("%s, %d: going to error reply\n", __FILE__, __LINE__); }
#endif
		reply_mess.m_type= DL_INIT_REPLY;
		reply_mess.m3_i1= result;
		mess_reply(mess, &reply_mess);
		return;
	}
	result = dp_init(&ehw_port->et_address);
	if (result < 0) {
#if DEBUG
 { printW(); printf("%s, %d: going to error reply\n", __FILE__, __LINE__); }
#endif
		reply_mess.m_type= DL_INIT_REPLY;
		reply_mess.m3_i1= result;
		mess_reply(mess, &reply_mess);
		return;
	}
  } else {
	result = dp_reinit();
	if (result < 0) {
#if DEBUG
 { printW(); printf("%s, %d: going to error reply\n", __FILE__, __LINE__); }
#endif
		reply_mess.m_type= DL_INIT_REPLY;
		reply_mess.m3_i1= result;
		mess_reply(mess, &reply_mess);
		return;
	}
  }

  ehw_port->et_flags |= ETF_ENABLED;
  put_irq_handler(ehw_irq, ehw_handler);
  enable_irq(ehw_irq);

  reply_mess.m_type = DL_INIT_REPLY;
  reply_mess.m3_i1 = mess->DL_PORT;
  reply_mess.m3_i2 = EHW_PORT_NR;
  *(ether_addr_t *) reply_mess.m3_ca1 = ehw_port->et_address;

  mess_reply(mess, &reply_mess);

  return;
}

PRIVATE void do_getstat(mess)
message *mess;
{
  int port = mess->DL_PORT;
  int result;

  if (port < 0 || port >= EHW_PORT_NR || ehw_table + port != ehw_port ||
      !(ehw_port->et_flags & ETF_ENABLED)) {
#if DEBUG
 { printW(); printf("calling err_reply\n"); }
#endif
	err_reply(ENXIO, DL_TASK_REPLY);
	return;
  }
  ehw_port->et_stat.ets_CRCerr += read_reg0(dp_cntr0);
  ehw_port->et_stat.ets_frameAll += read_reg0(dp_cntr1);
  ehw_port->et_stat.ets_missedP += read_reg0(dp_cntr2);

  result = ehw_cp_loc2user((char *) &ehw_port->et_stat, mess->DL_PROC,
    (vir_bytes) mess->DL_ADDR, (vir_bytes) sizeof(ehw_port->et_stat));

#if DEBUG & 256
 { printW(); printf("calling err_reply\n"); }
#endif
  err_reply(result, DL_TASK_REPLY);
}

PRIVATE void check_ints()
{
  int isr;
  int i;


#if DEBUG & 256
 { printW(); printf("check_ints called\n"); }
#endif
  if (!(ehw_port->et_flags & ETF_ENABLED))
  {
	panic("ether: got premature interrupt", NO_NUM);
  }

  isr = read_reg0(dp_isr);
  write_reg0(dp_isr, isr & ~ISR_PRX);

  if (isr & ISR_PRX) {
/*	write_reg0(dp_imr, IMR_PTXE | IMR_RXEE | IMR_TXEE | 
			IMR_OVWE | IMR_CNTE | IMR_RDCE); */
	write_reg0(dp_isr, ISR_PRX);
	ehw_recv();
  }
  if (isr & ISR_PTX) {
	int tsr = read_reg0(dp_tsr);

	if (tsr & TSR_PTX) ehw_port->et_stat.ets_packetT++;
	if (tsr & TSR_DFR) ehw_port->et_stat.ets_transDef++;
	if (tsr & TSR_COL) ehw_port->et_stat.ets_collision++;
	if (tsr & TSR_ABT) ehw_port->et_stat.ets_transAb++;
	if (tsr & TSR_CRS) {
		ehw_port->et_stat.ets_carrSense++;
#if DEBUG
 { printW(); printf("eth_hw: carrier sense lost\n"); }
#endif
	}
	if (tsr & TSR_FU) {
		ehw_port->et_stat.ets_fifoUnder++;
		printf("eth_hw: fifo underrun\n");
	}
	if (tsr & TSR_CDH) {
		ehw_port->et_stat.ets_CDheartbeat++;
		printf("eth_hw: CD heart beat failure\n");
	}
	if (tsr & TSR_OWC) {
		ehw_port->et_stat.ets_OWC++;
#if DEBUG
 { printf("eth_hw: out of window collision\n"); }
#endif
	}

assert(!(read_reg0(dp_cr) & CR_TXP));
	if (!(read_reg0(dp_cr) & CR_TXP)) {
assert(ehw_port->et_flags & ETF_SENDING);
		ehw_port->et_flags &= ~ETF_SENDING;
		ehw_send();
	}
  }
  if (isr & ISR_RXE) {
#if DEBUG & 256
 { printW(); printf("got Receive Error\n"); }
#endif
	ehw_port->et_stat.ets_recvErr++;
  }
  if (isr & ISR_TXE) {
#if DEBUG
 { printf("eth_hw: got Send Error\n"); }
#endif
	ehw_port->et_stat.ets_sendErr++;

assert(!(read_reg0(dp_cr) & CR_TXP));
assert(ehw_port->et_flags & ETF_SENDING);
	ehw_port->et_flags &= ~ETF_SENDING;
	ehw_send();
  }
  if (isr & ISR_OVW) {
#if DEBUG
 { printW(); printf("got Overwrite Warning\n"); }
#endif
#if 1
	ehw_port->et_stat.ets_OVW++;
	for (i = 0; i < 4; i++) {
		throw_away_packet = TRUE;
		ehw_recv();
	}
#else
#if DEBUG
 { printW(); printf("resetting dp8390\n"); }
#endif
	dp_init(&ehw_port->et_address);
	return;
#endif
  }
  if (isr & ISR_CNT) {
	ehw_port->et_stat.ets_CRCerr += read_reg0(dp_cntr0);
	ehw_port->et_stat.ets_frameAll += read_reg0(dp_cntr1);
	ehw_port->et_stat.ets_missedP += read_reg0(dp_cntr2);
  }
  if (isr & ISR_RDC)		/* remote DMA complete, but we don't use
			 * remote DMA */
	panic("eth_hw: remote dma complete", NO_NUM);

  if (isr & ISR_RST) {		/* this means we got an interrupt but the
				 * ethernet chip is shutdown. We don't do
				 * anything. */
#if DEBUG
 { printW(); printf("eth_hw: NIC stopped\n"); }
#endif
#if 0
	panic("nic stopped", NO_NUM);
#else
	write_reg0(dp_cr, CR_STA);
#endif
  }
  if (read_reg0(dp_isr))
	interrupt(DL_ETH);
}

PRIVATE int ehw_data2user(page, length)
int page, length;
{
  int last, count;
  int result;

  if (!(ehw_port->et_flags & ETF_READING)) {
	if (!throw_away_packet) return ERROR;
#if DEBUG & 256
 { printW(); printf("throwing away a packet\n"); }
#endif
	throw_away_packet = FALSE;
	return OK;		/* this means: we throw away one packet */
  }
  last = page + (length - 1) / EHW_PAGESIZE;
  if (last >= ehw_port->et_stoppage) {
	count = (ehw_port->et_stoppage - page) * EHW_PAGESIZE -
		sizeof(rcvdheader_t);
	tmp_iovec = ehw_port->et_read_iovec;
	result = ehw_hw2user(page * EHW_PAGESIZE + sizeof(rcvdheader_t),
			     0, count);
	if (result >= 0) {
		tmp_iovec = ehw_port->et_read_iovec;
		result = ehw_hw2user(ehw_port->et_startpage * EHW_PAGESIZE, 
							count, length - count);
	}
  } else {
	tmp_iovec = ehw_port->et_read_iovec;
	result = ehw_hw2user(page * EHW_PAGESIZE + sizeof(rcvdheader_t),
			     0, length);
  }


  ehw_port->et_read_s = result < 0 ? result : length;
  ehw_port->et_flags |= ETF_PACK_RECV;
  ehw_port->et_flags &= ~ETF_READING;

  return OK;
}

PRIVATE void ehw_init()
{
  static eth_stat_t empty_stat; /* = {0, 0, 0, 0, 0, 0,... } */
  long v;
  static char envvar[] = "DPETH0";
  static char ehw_fmt[] = "x:d:x";

  v = 0x280;
  if (env_parse(envvar, ehw_fmt, 0, &v, 0x000L, 0x3FFL) == EP_OFF) return;
  ehw_baseport = v;

  v = ETHER_IRQ;
  (void) env_parse(envvar, ehw_fmt, 1, &v, 0L, (long) NR_IRQ_VECTORS - 1);
  ehw_irq = v;

  v = EHW_LINMEM;
  (void) env_parse(envvar, ehw_fmt, 2, &v, 0xC0000L, 0xFFFFFL);
  ehw_linmem = v;

  if (protected_mode) {
	init_dataseg(&gdt[EPLUS_SELECTOR / DESC_SIZE],
			ehw_linmem, (phys_bytes) EPLUS_SIZE, TASK_PRIVILEGE);
	ehw_memsegm = EPLUS_SELECTOR;
  } else {
	ehw_memsegm = physb_to_hclick(ehw_linmem);
  }

  ehw_port->et_flags = ETF_EMPTY;
  ehw_port->et_stat = empty_stat;
  ehw_enabled = TRUE;
}

PRIVATE int epl_init(ea)
ether_addr_t *ea;
{
  int sum;
  int tlb, rambit, revision;
  static int seen= 0;

  ea->ea_addr[0] = read_reg_epl(epl_ea0);
  ea->ea_addr[1] = read_reg_epl(epl_ea1);
  ea->ea_addr[2] = read_reg_epl(epl_ea2);
  ea->ea_addr[3] = read_reg_epl(epl_ea3);
  ea->ea_addr[4] = read_reg_epl(epl_ea4);
  ea->ea_addr[5] = read_reg_epl(epl_ea5);
  sum = ea->ea_addr[0] + ea->ea_addr[1] + ea->ea_addr[2] + ea->ea_addr[3] +
	ea->ea_addr[4] + ea->ea_addr[5] +
	read_reg_epl(epl_tlb) +
	read_reg_epl(epl_chksum);
  if ((sum & 0xFF) != 0xFF) {
	printf("No ethernet+ board at I/O address 0x%x\n", ehw_eplport);
	return ERROR;
  }
  ehw_port->et_type= 0;

  ehw_port->et_type |= ETT_ETHERNET;	/* assume ethernet */
  if (!wd_aliasing())
  {
	if (wd_interface_chip())
		ehw_port->et_type |= ETT_INTERF_CHIP;
	if (wd_16bitboard())
	{
		ehw_port->et_type |= ETT_BRD_16BIT;
		if (wd_16bitslot())
			ehw_port->et_type |= ETT_SLT_16BIT;
	}
  }

  /* let's look at the on board ram size. */
  tlb= read_reg_epl(epl_tlb);
  revision= tlb & E_TLB_REV;
  rambit= tlb & E_TLB_RAM;

  if (revision < 2)
  {
	ehw_port->et_ramsize= 0x2000;			/* 8K */
	if (ehw_port->et_type & ETT_BRD_16BIT)
		ehw_port->et_ramsize= 0x4000;		/* 16K */
	else if ((ehw_port->et_type & ETT_INTERF_CHIP) &&
		read_reg_epl(epl_reg1) & E_REG1_MEMBIT)
		ehw_port->et_ramsize= 0x8000;		/* 32K */
  }
  else
  {
	if (ehw_port->et_type & ETT_BRD_16BIT)
		ehw_port->et_ramsize= rambit ? 0x8000 : 0x4000;	/* 32K or 16K */
	else
		ehw_port->et_ramsize= rambit ? 0x8000 : 0x2000;	/* 32K or 8K */
  }


  if (!seen) {
	seen= 1;
#if 1
	printf("ether: WD80%d3 at %x:%d:%lx\n",
		ehw_port->et_type & ETT_BRD_16BIT ? 1 : 0,
		ehw_eplport, ehw_irq, ehw_linmem);
#else
	printf("ether: Western Digital %s%s card %s%s", 
		ehw_port->et_type & ETT_BRD_16BIT ? "16 bit " : "", 
		ehw_port->et_type & ETT_ETHERNET ? "Ethernet" : 
		ehw_port->et_type & ETT_STARLAN ? "Starlan" : "Network",
		ehw_port->et_type & ETT_INTERF_CHIP ? "with an interface chip " : "",
		ehw_port->et_type & ETT_SLT_16BIT ? "in a 16-bit slot " : "");
	printf("at I/O address 0x%x, memory address 0x%lx, memory size 0x%x\n",
		ehw_eplport, ehw_linmem, ehw_port->et_ramsize);
#endif
  }

  /* special setup for WD8013 boards */
  if (ehw_port->et_type & ETT_BRD_16BIT)
  {
	if (ehw_port->et_type & ETT_SLT_16BIT)
		write_reg_epl(epl_laar, E_LAAR_A19 | E_LAAR_SOFTINT |
			E_LAAR_LAN16E | E_LAAR_MEM16E);
	else
		write_reg_epl(epl_laar, E_LAAR_A19 | E_LAAR_SOFTINT |
			E_LAAR_LAN16E);
  }

  write_reg_epl(epl_ctlstatus, E_CTL_RESET);
  write_reg_epl(epl_ctlstatus, E_CTL_MENABLE |
	      ((ehw_linmem >> 13) & E_CTL_MEMADDR));
  ehw_port->et_startpage= ehw_startpage(ehw_port->et_ramsize);
  ehw_port->et_stoppage= ehw_stoppage(ehw_port->et_ramsize);
  return OK;
}

/* Determine whether wd8003 hardware performs register aliasing. This implies 
 * an old WD8003E board. */

PRIVATE int wd_aliasing()
{
	if (read_reg_epl(epl_reg1) != read_reg_epl(epl_ea1))
		return 0;
	if (read_reg_epl(epl_reg2) != read_reg_epl(epl_ea2))
		return 0;
	if (read_reg_epl(epl_reg3) != read_reg_epl(epl_ea3))
		return 0;
	if (read_reg_epl(epl_reg4) != read_reg_epl(epl_ea4))
		return 0;
	if (read_reg_epl(epl_reg7) != read_reg_epl(epl_chksum))
		return 0;
	return 1;
}

/* Determine whether the board is capable of doing 16 bit memory moves.
 * If the 16 bit enable bit is unchangable by software we'll assume an 8 bit
 * board 
 */

PRIVATE int wd_16bitboard()
{
	int tlb, bsreg;

	bsreg= read_reg_epl(epl_bsr);

	write_reg_epl(epl_bsr, bsreg ^ E_BSR_16BIT);
	if (read_reg_epl(epl_bsr) == bsreg)
	{
		tlb= read_reg_epl(epl_tlb);
		return tlb= E_TLB_EB || tlb == E_TLB_E || tlb == E_TLB_SMCE;
	}
	write_reg_epl(epl_bsr, bsreg);
	return 1;
}

/* Determine if the 16 bit board in plugged into a 16 bit slot.  */

PRIVATE int wd_16bitslot()
{
	return !!(read_reg_epl(epl_bsr) & E_BSR_16BIT);
}

/* Determine if the board has an interface chip. */

PRIVATE int wd_interface_chip()
{
	write_reg_epl(epl_gp2, 0x35);
	if (read_reg_epl(epl_gp2) != 0x35)
		return 0;
	write_reg_epl(epl_gp2, 0x3A);
	if (read_reg_epl(epl_gp2) != 0x3A)
		return 0;
	return 1;
}

PRIVATE int dp_init(ea)
ether_addr_t *ea;
{
  int dp_rcr_reg;

  write_reg0(dp_cr, CR_PS_P0 | CR_STP | CR_DM_ABORT);
  write_reg0(dp_imr, 0);
  write_reg0(dp_pstart, ehw_port->et_startpage);
  write_reg0(dp_pstop, ehw_port->et_stoppage);
  write_reg0(dp_bnry, ehw_startpage(ehw_port->et_ramsize));
  write_reg0(dp_rcr, RCR_MON);
  write_reg0(dp_tcr, TCR_NORMAL);
  if (ehw_port->et_type & ETT_SLT_16BIT)
	write_reg0(dp_dcr, DCR_WORDWIDE | DCR_BYTEWIDE | DCR_8BYTES);
  else
	write_reg0(dp_dcr, DCR_BYTEWIDE | DCR_BYTEWIDE | DCR_8BYTES);
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

  write_reg1(dp_curr, ehw_startpage(ehw_port->et_ramsize) + 1);
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

  write_reg0(dp_imr, IMR_PRXE | IMR_PTXE | IMR_RXEE | IMR_TXEE | IMR_OVWE |
	IMR_CNTE | IMR_RDCE);
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
  if (pageno == ehw_port->et_stoppage) pageno = ehw_port->et_startpage;

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
		/* This is very serious, so we issue a warning and
		 * reset the buffers */
		printf("eth_hw: fifo overrun, reseting receive buffer\n");
		ehw_port->et_stat.ets_fifoOver++;
		next = curr;
	} else if ((header.rp_status & RSR_PRX) &&
		   (ehw_port->et_flags & ETF_ENABLED)) {
		int result = ehw_data2user(pageno, length);

		if (result < 0) return;
		packet_processed = TRUE;
		ehw_port->et_stat.ets_packetR++;
	}
	if (next == ehw_port->et_startpage)
		write_reg0(dp_bnry, ehw_port->et_stoppage - 1);
	else
		write_reg0(dp_bnry, next - 1);

	pageno = next;
	assert(pageno >= ehw_port->et_startpage && 
						pageno < ehw_port->et_stoppage);
  }
  while (!packet_processed);
#if 1
  if (pageno == curr) {		/* if receive buffer empty then the receive
				 * interrupt can be acknowledged */
#if DEBUG & 256
 { printW(); printf("acking recv int\n"); }
#endif
/*		write_reg0(dp_imr, IMR_PRXE | IMR_PTXE | IMR_RXEE | IMR_TXEE | 
			IMR_OVWE | IMR_CNTE | IMR_RDCE); */
/*	if (read_reg0(dp_isr) & ISR_PRX) write_reg0(dp_isr, ISR_PRX); */
  }
#endif
}

PRIVATE void do_stop()
{
#if DEBUG
  {
	printW();
	printf("got a stop request\n");
  }
#endif
  write_reg0(dp_cr, CR_STP | CR_DM_ABORT);
  if (ehw_port->et_type & ETT_SLT_16BIT)
	write_reg_epl(epl_laar, E_LAAR_A19 | E_LAAR_LAN16E);
  write_reg_epl(epl_ctlstatus, E_CTL_RESET|E_CTL_MENABLE);
  milli_delay(5);
  write_reg_epl(epl_ctlstatus, 0);
	
  write_reg0(dp_imr, 255);
  milli_delay(1);
  write_reg0(dp_isr, read_reg0(dp_isr));
  milli_delay(1);
  write_reg0(dp_imr, 0);
  ehw_port->et_flags= ETF_EMPTY;
}

PRIVATE void mess_reply(req, reply)
message *req, *reply;
{
  ehw_tab_t *save_port_nr = ehw_port;	/* save port because a Send
					 * may block */
#if DEBUG & 256
  {
	printW();
	printf("eth_hw: mess_reply to %d\n",
	       req->m_source);
  }
#endif
#if DEBUG & 256
  {
	printW();
	printf("sending\n");
  }
#endif
  if (send(req->m_source, reply) < 0)
	panic("eth_hw: unable to mess_reply()", NO_NUM);
#if DEBUG & 256
  {
	printW();
	printf("Send completed\n");
  }
#endif

  ehw_port = save_port_nr;
}

PRIVATE void err_reply(err, type)
int err;
int type;
{
  ehw_tab_t *save_port_nr = ehw_port;	/* save port because a Send
					 * may block */
  message reply;
  int status;
  int result;

#if DEBUG & 256
 { printW(); printf("eth_hw: err_reply(%d, %d) to %d\n", err, type,
	ehw_port->et_client); }
#endif
  status = 0;
  if (ehw_port->et_flags & ETF_PACK_SEND) {
	status |= DL_PACK_SEND;
#if DEBUG & 256
	{
		printW();
		printf("replying pack sent\n");
	}
#endif
  }
  if (ehw_port->et_flags & ETF_PACK_RECV) {
	status |= DL_PACK_RECV;
#if DEBUG & 256
	{
		printW();
		printf("replying pack received\n");
	}
#endif
  }
  if (!(ehw_port->et_flags & ETF_ENABLED)) status |= DL_DISABLED;

  reply.m_type = type;
  reply.DL_PORT = ehw_port - ehw_table;
  reply.DL_PROC = ehw_port->et_client;
  reply.DL_STAT = status | ((u32_t) err << 16);
  reply.DL_COUNT = ehw_port->et_read_s;
  reply.DL_CLCK = get_uptime();
#if DEBUG & 256
  {
	printW();
	printf("sending %d\n", type);
  }
#endif
  result= send(ehw_port->et_client, &reply);
  if (result == ELOCKED && type == DL_INT_TASK)
	return;
  if (result < 0)
  {
	panic("ether: send failed:", result);
  }
#if DEBUG & 256
  {
	printW();
	printf("Send completed\n");
  }
#endif

  ehw_port = save_port_nr;
  ehw_port->et_read_s = 0;
#if DEBUG & 256
 if (ehw_port->et_flags & ETF_PACK_RECV)
 { printW(); printf("clearing ETF_PACK_RECV\n"); }
#endif
  ehw_port->et_flags &= ~(ETF_PACK_SEND | ETF_PACK_RECV);
}

PRIVATE void ehw_getheader(page, h)
int page;
rcvdheader_t *h;
{
  u16_t *ha = (u16_t *) h;
  u16_t offset = page * EHW_PAGESIZE;

#if DEBUG & 256
 { printW(); printf("eth_hw: mem_rdw(0x%x:0x%x)\n", ehw_memsegm, offset); }
#endif
  *ha = mem_rdw(ehw_memsegm, offset);
  ha++;
  offset += sizeof(*ha);

#if DEBUG & 256
 { printW(); printf("eth_hw: mem_rdw(0x%x:0x%x)\n", ehw_memsegm, offset); }
#endif
  *ha = mem_rdw(ehw_memsegm, offset);
}

PRIVATE int ehw_cp_user2loc(user_proc, user_addr, loc_addr, count)
int user_proc;
vir_bytes user_addr;
char *loc_addr;
vir_bytes count;
{
  phys_bytes src;

  src = numap(user_proc, user_addr, count);
  if (!src) {
#if DEBUG
 { printW(); printf("ehw_cp_user2loc: user umap failed\n"); }
#endif
	return EFAULT;
  }
#if DEBUG & 256
 { printW(); printf(
		"ehw_cp_user2loc: %d bytes from %x in proc %d to %x in %d\n",
       count, user_addr, user_proc, (vir_bytes) loc_addr, 
       proc_number(proc_ptr)); }
#endif

  phys_copy(src, vir2phys(loc_addr), (phys_bytes) count);

  return OK;
}

PRIVATE int ehw_cp_loc2user(loc_addr, user_proc, user_addr, count)
char *loc_addr;
int user_proc;
vir_bytes user_addr;
vir_bytes count;
{
  phys_bytes dst;

  dst = numap(user_proc, user_addr, count);
  if (!dst) {
#if DEBUG
 { printW(); printf("ehw_cp_loc2user: user umap failed\n"); }
#endif
	return EFAULT;
  }
#if DEBUG
 { printW(); printf(
		"ehw_cp_loc2user: %d bytes to %x in proc %d from %x in %d\n",
	count, user_addr, user_proc, (vir_bytes) loc_addr, 
	proc_number(proc_ptr)); }
#endif

  phys_copy(vir2phys(loc_addr), dst, (phys_bytes) count);

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

#if DEBUG & 256
 { printW(); printf("ehw_hw2user (%x, %x, %x)\n", hw_addr, user_offs, count); }
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
 { printW(); printf("ehw_hw2user: 1st numap(%d, %x, %x) failed\n", 
	tmp_iovec.iod_proc_nr, tmp_iovec.iod_iovec[i].iov_addr + user_offs, 
	bytes); }
#endif
		return EFAULT;
	}
#if DEBUG & 256
 { printW(); printf("ehw_hw2user: phys_copy(%X,%X,%X)\n", phys_user, phys_hw,
       (phys_bytes) bytes); }
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
#if DEBUG & 256
 { printW(); printf("ehw_hw2user: 2nd numap(%d, %x, %x) failed\n", 
	tmp_iovec.iod_proc_nr, tmp_iovec.iod_iovec[i].iov_addr + user_offs, 
	bytes); }
#endif
		if (!phys_user) {
#if DEBUG
 { printW(); printf("ehw_hw2user: 2nd numap(%d, %x, %x) failed\n", 
	tmp_iovec.iod_proc_nr, tmp_iovec.iod_iovec[i].iov_addr + user_offs, 
	bytes); }
#endif
			return EFAULT;
		}
#if DEBUG & 256
 { printW(); printf("ehw_hw2user: phys_copy(%X,%X,%X)\n", phys_user, phys_hw,
	(phys_bytes) bytes); }
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

#if DEBUG & 256
 { printW(); printf("ehw_user2hw(%x, %x, %x)\n", user_offs, hw_addr, count); }
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
 { printW(); printf("ehw_user2hw: 1st numap(%d, %x, %x) failed\n", 
	tmp_iovec.iod_proc_nr, tmp_iovec.iod_iovec[i].iov_addr + user_offs, 
	bytes); }
#endif
		return EFAULT;
	}
#if DEBUG
 { printW(); printf("ehw_user2hw: phys_copy(%X,%X,%X)\n", phys_user, phys_hw,
	(phys_bytes) bytes); }
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
 { printW(); printf("ehw_user2hw: 2nd numap(%d, %x, %x) failed\n", 
	tmp_iovec.iod_proc_nr, tmp_iovec.iod_iovec[i].iov_addr + user_offs, 
	bytes); }
#endif
			return EFAULT;
		}
#if DEBUG & 256
 { printW(); printf("ehw_user2hw: phys_copy(%X,%X,%X)\n", phys_user, phys_hw,
	(phys_bytes) bytes); }
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
	printf("eth_hw: ehw_next_iovec failed\n");
	return EINVAL;
  }
  tmp_iovec.iod_iovec_s -= IOVEC_NR;

  tmp_iovec.iod_iovec_addr += IOVEC_NR * sizeof(iovec_t);

  return ehw_cp_user2loc(tmp_iovec.iod_proc_nr,
	     tmp_iovec.iod_iovec_addr, (char *) tmp_iovec.iod_iovec,
		       (tmp_iovec.iod_iovec_s > IOVEC_NR ? IOVEC_NR :
			tmp_iovec.iod_iovec_s) * sizeof(iovec_t));

}

PRIVATE int ehw_handler(irq)
int irq;
{
  interrupt(DL_ETH);
  return 1;
}

PRIVATE void ehw_send()
{
#if DEBUG & 256
 { printW(); printf("ehw_send called\n"); }
#endif
	if (!(ehw_port->et_flags & ETF_SEND_AVAIL))
		return;

#if DEBUG & 256
 { printW(); printf("ehw_send restarting write\n"); }
#endif
	
	ehw_port->et_flags &= ~ETF_SEND_AVAIL;
	switch(ehw_port->et_sendmsg.m_type)
	{
	case DL_WRITE:	do_write(&mess, TRUE);	break;
	case DL_WRITEV:	do_vwrite(&mess, TRUE);	break;
	default:
		panic("ether: wrong type:", ehw_port->et_sendmsg.m_type);
		break;
	}
}

PRIVATE void do_int()
{
	if (ehw_port->et_flags & (ETF_PACK_SEND | ETF_PACK_RECV))
	{
#if DEBUG & 256
 { printW(); printf("calling err_reply\n"); }
#endif
#if DEBUG & 256
 if (ehw_port->et_flags & ETF_PACK_SEND)
 { printW(); printf("sending transmit int\n"); }
#endif
		err_reply(OK, DL_INT_TASK);
	}
}
