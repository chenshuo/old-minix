/* Western Digital Ethercard Plus, or WD8003E card. */

struct eplusreg {
  char epl_reg0;		/* Control(write) and status(read) */
  char epl_reg1;
  char epl_reg2;
  char epl_reg3;
  char epl_reg4;
  char epl_reg5;
  char epl_reg6;
  char epl_reg7;
  char epl_ea0;			/* Most significant eaddr byte */
  char epl_ea1;
  char epl_ea2;
  char epl_ea3;
  char epl_ea4;
  char epl_ea5;			/* Least significant eaddr byte */
  char epl_tlb;
  char epl_chksum;		/* sum from epl_ea0 upto here is 0xFF */
  dp8390 epl_dp8390;		/* NatSemi chip */
};

#define epl_ctlstatus	epl_reg0

/* WD83C583 registers (WD8013 special) */
#define epl_bsr 	epl_reg1	/* bus size register (read only) */
#define epl_laar	epl_reg5	/* LA address register (write only) */
#define epl_gp2		epl_reg7	/* general purpose register 2 */

/* Bits in epl_ctlstatus */
#define E_CTL_RESET	0x80		/* Software Reset */
#define E_CTL_MENABLE	0x40		/* Memory Enable */
#define E_CTL_MEMADDR	0x3F		/* Bits SA18-SA13, SA19 implicit 1 */

#define E_TLB_EB	0x05		/* WD8013EB */
#define E_TLB_E		0x27		/* WD8013 Elite */
#define E_TLB_SMCE	0x29		/* SMC Elite 16 */

#define E_TLB_REV	0x0F		/* revision mask */
#define E_TLB_RAM	0x40		/* extra ram bit */

#define E_BSR_16BIT	0x01		/* 16 bit bus */
#define E_REG1_MEMBIT	0x08		/* 583 mem size mask */

#define E_LAAR_A19	0x01		/* address lines for above 1M ram */
#define E_LAAR_A20	0x02		/* address lines for above 1M ram */
#define E_LAAR_A21	0x04		/* address lines for above 1M ram */
#define E_LAAR_A22	0x08		/* address lines for above 1M ram */
#define E_LAAR_A23	0x10		/* address lines for above 1M ram */
#define E_LAAR_SOFTINT	0x20		/* enable software interrupt */
#define E_LAAR_LAN16E	0x40		/* enables 16 bit RAM for LAN */
#define E_LAAR_MEM16E	0x80		/* enables 16 bit RAM for host */
