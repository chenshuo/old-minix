/*
hw_conf.h
*/

#ifndef HW_CONF_H
#define HW_CONF_H

#define EHW_PORT_NR	1

#define ehw_dp8390port ((union dp8390reg *)0x290)
#define ehw_eplport ((struct eplusreg *)0x280)
#define EHW_LINMEM	EPLUS_BASE

#define ehw_sendpage	0
#define ehw_startpage	6
#define ehw_stoppage	32

#define EHW_PAGESIZE	256

#define read_reg_epl(reg) in_byte ((port_t)&(ehw_eplport->reg))
#define write_reg_epl(reg,data) out_byte ((port_t)&(ehw_eplport->reg), data)

#define read_reg0(reg)	in_byte	((port_t)&(ehw_dp8390port->dp_pg0rd.reg))
#define write_reg0(reg,data) out_byte ((port_t)&(ehw_dp8390port->dp_pg0wr.reg), data)

#define read_reg1(reg)	in_byte ((port_t)&(ehw_dp8390port->dp_pg1rdwr.reg))
#define write_reg1(reg,data) out_byte ((port_t)&(ehw_dp8390port->dp_pg1rdwr.reg),data)

#endif /* HW_CONF_H */
