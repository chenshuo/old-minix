/*
hw_conf.h
*/

#ifndef HW_CONF_H
#define HW_CONF_H

#define EHW_PORT_NR	1

#define ehw_eplport ((struct eplusreg *)ehw_baseport)
#define ehw_dp8390port ((union dp8390reg *)(ehw_baseport+0x10))
#define EHW_LINMEM	EPLUS_BASE

#define ehw_sendpage	0
#define ehw_startpage(memsize)	6
#define ehw_stoppage(memsize)	(memsize/EHW_PAGESIZE)

#define EHW_PAGESIZE	256

#define read_reg_epl(reg) in_byte ((port_t)&(ehw_eplport->reg))
#define write_reg_epl(reg,data) out_byte ((port_t)&(ehw_eplport->reg), data)

#define read_reg0(reg)	in_byte	((port_t)&(ehw_dp8390port->dp_pg0rd.reg))
#define write_reg0(reg,data) out_byte ((port_t)&(ehw_dp8390port->dp_pg0wr.reg), data)

#define read_reg1(reg)	in_byte ((port_t)&(ehw_dp8390port->dp_pg1rdwr.reg))
#define write_reg1(reg,data) out_byte ((port_t)&(ehw_dp8390port->dp_pg1rdwr.reg),data)

#endif /* HW_CONF_H */
