/* Function prototypes. */

#ifndef PROTO_H
#define PROTO_H

/* Struct declarations. */
struct proc;
struct tty_struct;

/* at_wini.c, bios_wini.c, esdi_wini.c, ps_wini.c, xt_wini.c, stacsi.c */
_PROTOTYPE( void winchester_task, (void)				);

/* clock.c */
_PROTOTYPE( void clock_handler, (void)					);
_PROTOTYPE( void clock_task, (void)					);
_PROTOTYPE( clock_t get_uptime, (void)					);
_PROTOTYPE( void syn_alrm_task, (void)					);

/* dmp.c */
_PROTOTYPE( void fork_name, (int p1, int p2)				);
_PROTOTYPE( void map_dmp, (void)					);
_PROTOTYPE( void mem_dmp, (char *adr, int len)				);
_PROTOTYPE( void p_dmp, (void)						);
_PROTOTYPE( void reg_dmp, (struct proc *rp)				);
_PROTOTYPE( void set_name, (int source_nr, int proc_nr, char *ptr)	);
_PROTOTYPE( void tty_dmp, (void)					);

/* floppy.c, stfloppy.c */
_PROTOTYPE( void floppy_task, (void)					);

/* main.c, stmain.c */
_PROTOTYPE( void main, (void)						);
_PROTOTYPE( void panic, (const char *s, int n)				);

/* memory.c */
_PROTOTYPE( void mem_task, (void)					);

/* misc.c */
_PROTOTYPE( int do_vrdwt, (message *m_ptr, rdwt_t do_rdwt)		);

/* printer.c, stprint.c */
_PROTOTYPE( void printer_task, (void)					);

/* proc.c */
_PROTOTYPE( void interrupt, (int task)					);
_PROTOTYPE( int lock_mini_send, (struct proc *caller_ptr,
		int dest, message *m_ptr)				);
_PROTOTYPE( void lock_pick_proc, (void)					);
_PROTOTYPE( void lock_ready, (struct proc *rp)				);
_PROTOTYPE( void lock_sched, (void)					);
_PROTOTYPE( void lock_unready, (struct proc *rp)			);
_PROTOTYPE( int sys_call, (int function, int src_dest, message *m_ptr)	);
_PROTOTYPE( void unhold, (void)						);

/* rs232.c */
_PROTOTYPE( void rs_inhibit, (int minor, int inhibit)			);
_PROTOTYPE( int rs_init, (int minor)					);
_PROTOTYPE( int rs_ioctl, (int minor, int mode, int speeds)		);
_PROTOTYPE( int rs_read, (int minor, char **bufindirect,
		unsigned char *odoneindirect)				);
_PROTOTYPE( void rs_istart, (int minor)					);
_PROTOTYPE( void rs_istop, (int minor)					);
_PROTOTYPE( void rs_ocancel, (int minor)				);
_PROTOTYPE( void rs_setc, (int minor, int xoff)				);
_PROTOTYPE( void rs_write, (int minor, char *buf, int nbytes)		);

/* system.c */
_PROTOTYPE( void cause_sig, (int proc_nr, int sig_nr)			);
_PROTOTYPE( void inform, (void)						);
_PROTOTYPE( phys_bytes numap, (int proc_nr, vir_bytes vir_addr, 
		vir_bytes bytes)					);
_PROTOTYPE( void sys_task, (void)					);
_PROTOTYPE( phys_bytes umap, (struct proc *rp, int seg, vir_bytes vir_addr,
		vir_bytes bytes)					);

/* tty.c */
_PROTOTYPE( void finish, (struct tty_struct *tp, int code)		);
_PROTOTYPE( void sigchar, (struct tty_struct *tp, int sig)		);
_PROTOTYPE( void tty_task, (void)					);
_PROTOTYPE( void tty_wakeup, (void)					);

/* library */
_PROTOTYPE( void *memcpy, (void *_s1, const void *_s2, size_t _n)	);
_PROTOTYPE( void printk, (const char *mess,...)				);
_PROTOTYPE( int receive, (int source, message *mess)			);
_PROTOTYPE( int send, (int to, message *mess)				);
_PROTOTYPE( int sendrec, (int _src_dest, message *_m_ptr)		);

#if (CHIP == INTEL)

/* clock.c */
_PROTOTYPE( void milli_delay, (unsigned millisec)			);
_PROTOTYPE( unsigned read_counter, (void)				);

/* console.c */
_PROTOTYPE( void console, (struct tty_struct *tp)			);
_PROTOTYPE( void flush, (struct tty_struct *tp)				);
_PROTOTYPE( void out_char, (struct tty_struct *tp, int c)		);
_PROTOTYPE( void putk, (int c)						);
_PROTOTYPE( void scr_init, (int minor)					);
_PROTOTYPE( void toggle_scroll, (void)					);

/* cstart.c */
_PROTOTYPE( void cstart, (U16_t cs, U16_t ds,
		char *parmoff, U16_t parmseg, size_t parmsize)		);

/* ether.c */
_PROTOTYPE( void ehw_task, (void)					);
_PROTOTYPE( void ehw_dump, (void)					);

/* exception.c */
_PROTOTYPE( void exception, (unsigned vec_nr)				);

/* i8259.c */
_PROTOTYPE( void enable_irq, (unsigned irq_nr)				);
_PROTOTYPE( void init_8259, (unsigned master_base, unsigned slave_base)	);
_PROTOTYPE( void soon_reboot, (void)					);

/* keyboard.c */
_PROTOTYPE( int func_key, (int ch)					);
_PROTOTYPE( void kb_init, (int minor)					);
_PROTOTYPE( int kb_read, (int minor, char **bufindirect,
		unsigned char *odoneindirect)				);
_PROTOTYPE( void keyboard, (void)					);
_PROTOTYPE( int letter_code, (int scode)				);
_PROTOTYPE( int make_break, (int ch)					);
_PROTOTYPE( void wreboot, (void)					);

/* klib*.x */
_PROTOTYPE( void bios13, (void)						);
_PROTOTYPE( void build_sig, (char *sig_stuff, struct proc *rp, int sig)	);
_PROTOTYPE( phys_bytes check_mem, (phys_bytes base, phys_bytes size)	);
_PROTOTYPE( void cim_at_wini, (void)					);
_PROTOTYPE( void cim_floppy, (void)					);
_PROTOTYPE( void cim_printer, (void)					);
_PROTOTYPE( void cim_xt_wini, (void)					);
_PROTOTYPE( void cp_mess, (int src,phys_clicks src_clicks,vir_bytes src_offset,
		phys_clicks dst_clicks, vir_bytes dst_offset)		);
_PROTOTYPE( int in_byte, (port_t port)					);
_PROTOTYPE( int in_word, (port_t port)					);
_PROTOTYPE( void klib_1hook, (void)					);
_PROTOTYPE( void klib_2hook, (void)					);
_PROTOTYPE( void lock, (void)						);
_PROTOTYPE( u16_t mem_rdw, (segm_t segm, vir_bytes offset)		);
_PROTOTYPE( void mpx_1hook, (void)					);
_PROTOTYPE( void mpx_2hook, (void)					);
_PROTOTYPE( void out_byte, (port_t port, int value)			);
_PROTOTYPE( void out_word, (port_t port, int value)			);
_PROTOTYPE( void phys_copy, (phys_bytes source, phys_bytes dest,
		phys_bytes count)					);
_PROTOTYPE( void port_read, (unsigned port, phys_bytes destination,
		unsigned bytcount)					);
_PROTOTYPE( void port_write, (unsigned port, phys_bytes source,
		unsigned bytcount)					);
_PROTOTYPE( void reset, (void)						);
_PROTOTYPE( void scr_down, (unsigned videoseg, int source,int dest,int count));
_PROTOTYPE( void scr_up, (unsigned videoseg, int source, int dest, int count));
_PROTOTYPE( void sim_printer, (void)					);
_PROTOTYPE( unsigned tasim_printer, (void)				);
_PROTOTYPE( int test_and_set, (int *flag)				);
_PROTOTYPE( void unlock, (void)						);
_PROTOTYPE( void vid_copy, (char *buffer, unsigned videobase,
		int offset, int words)					);
_PROTOTYPE( void wait_retrace, (void)					);

/* misc.c */
_PROTOTYPE( void mem_init, (void)					);

/* mpx*.x */
_PROTOTYPE( void idle_task, (void)					);
_PROTOTYPE( void restart, (void)					);
_PROTOTYPE( void db, (void)                                             );
_PROTOTYPE( u16_t get_word, (U16_t segment, u16_t *offset)		);
_PROTOTYPE( void put_word, (U16_t segment, u16_t *offset, U16_t value)	);

/* The following are never called from C (pure asm procs). */

/* Exception handlers, in numerical order. */
_PROTOTYPE( void int00, (void) ), _PROTOTYPE( divide_error, (void) );
_PROTOTYPE( void int01, (void) ), _PROTOTYPE( single_step_exception, (void) );
_PROTOTYPE( void int02, (void) ), _PROTOTYPE( nmi, (void) );
_PROTOTYPE( void int03, (void) ), _PROTOTYPE( breakpoint_exception, (void) );
_PROTOTYPE( void int04, (void) ), _PROTOTYPE( overflow, (void) );
_PROTOTYPE( void int05, (void) ), _PROTOTYPE( bounds_check, (void) );
_PROTOTYPE( void int06, (void) ), _PROTOTYPE( inval_opcode, (void) );
_PROTOTYPE( void int07, (void) ), _PROTOTYPE( copr_not_available, (void) );
_PROTOTYPE( void int08, (void) ), _PROTOTYPE( double_fault, (void) );
_PROTOTYPE( void int09, (void) ), _PROTOTYPE( copr_seg_overrun, (void) );
_PROTOTYPE( void int10, (void) ), _PROTOTYPE( inval_tss, (void) );
_PROTOTYPE( void int11, (void) ), _PROTOTYPE( segment_not_present, (void) );
_PROTOTYPE( void int12, (void) ), _PROTOTYPE( stack_exception, (void) );
_PROTOTYPE( void int13, (void) ), _PROTOTYPE( general_protection, (void) );
_PROTOTYPE( void int14, (void) ), _PROTOTYPE( page_fault, (void) );
_PROTOTYPE( void int15, (void) );
_PROTOTYPE( void int16, (void) ), _PROTOTYPE( copr_error, (void) );

/* Hardware interrupt handlers, in numerical order. */
_PROTOTYPE( void clock_int, (void) );
_PROTOTYPE( void tty_int, (void) );
_PROTOTYPE( void secondary_int, (void) ), _PROTOTYPE( psecondary_int, (void) );
_PROTOTYPE( void eth_int, (void) );
_PROTOTYPE( void rs232_int, (void) ), _PROTOTYPE( prs232_int, (void) );
_PROTOTYPE( void disk_int, (void) );
_PROTOTYPE( void lpr_int, (void) );
_PROTOTYPE( void wini_int, (void) );

/* Software interrupt handlers, in numerical order. */
_PROTOTYPE( void trp, (void) );
_PROTOTYPE( void s_call, (void) ), _PROTOTYPE( p_s_call, (void) );

/* printer.c */
_PROTOTYPE( void pr_char, (void)					);
_PROTOTYPE( void pr_restart, (void)					);

/* protect.c */
_PROTOTYPE( void prot_init, (void)					);
_PROTOTYPE( void init_codeseg, (struct segdesc_s *segdp, phys_bytes base,
		phys_bytes size, int privilege)				);
_PROTOTYPE( void init_dataseg, (struct segdesc_s *segdp, phys_bytes base,
		phys_bytes size, int privilege)				);
_PROTOTYPE( void ldt_init, (void)					);

/* rs232.c */
_PROTOTYPE( void rs232_1handler, (void)					);
_PROTOTYPE( void rs232_2handler, (void)					);

/* system.c */
_PROTOTYPE( void alloc_segments, (struct proc *rp)			);

#endif /* (CHIP == INTEL) */

#if (CHIP == M68000)

/* cstart.c */
_PROTOTYPE( void cstart, (char *parmoff, size_t parmsize)		);

/* stfloppy.c */
_PROTOTYPE( void fd_timer, (void)					);

/* stmain.c */
_PROTOTYPE( void none, (void)						);
_PROTOTYPE( void rupt, (void)						);
_PROTOTYPE( void trap, (void)						);
_PROTOTYPE( void checksp, (void)					);
_PROTOTYPE( void aciaint, (void)					);
_PROTOTYPE( void fake_int, (const char *s, int t)				);
_PROTOTYPE( void timint, (int t)					);
_PROTOTYPE( void mdiint, (void)						);
_PROTOTYPE( void iob, (int t)						);
_PROTOTYPE( void idle_task, (void)					);

/* rs232.c */
_PROTOTYPE( void siaint, (int type)					);

/* stcon.c */
_PROTOTYPE( void func_key, (void)					);
_PROTOTYPE( void dump, (void)						);
_PROTOTYPE( void putk, (int c)						);

/* stdma.c */
_PROTOTYPE( void dmagrab, (int p, dmaint_t func)			);
_PROTOTYPE( void dmafree, (int p)					);
_PROTOTYPE( void dmaint, (void)						);
_PROTOTYPE( void dmaaddr, (phys_bytes ad)				);
_PROTOTYPE( int dmardat, (int mode, int delay)				);
_PROTOTYPE( void dmawdat, (int mode, int data, int delay)		);
_PROTOTYPE( void dmawcmd, (int data, unsigned mode)			);
_PROTOTYPE( void dmacomm, (int mode, int data, int delay)		);
_PROTOTYPE( int dmastat, (int mode, int delay)				);

/* stdskclk.c */
_PROTOTYPE( int do_xbms, (phys_bytes address, int count, int rw, int minor) );
 
/* stkbd.c */
_PROTOTYPE( void kbdint, (void)						);
_PROTOTYPE( void kb_timer, (void)					);
_PROTOTYPE( int kb_read, (int minor, char **bufindirect,
		unsigned char *odoneindirect)				);
_PROTOTYPE( void kbdinit, (int minor)					);

/* stshadow.c */
_PROTOTYPE( void mkshadow, (struct proc *p, phys_clicks c2)		);
_PROTOTYPE( void rmshadow, (struct proc *p, phys_clicks *basep,
		phys_clicks *sizep)					);
_PROTOTYPE( void unshadow, (struct proc *p)				);
 
/* stvdu.c */
_PROTOTYPE( void flush, (struct tty_struct *tp)				);
_PROTOTYPE( void console, (struct tty_struct *tp)			);
_PROTOTYPE( void out_char, (struct tty_struct *tp, int c)		);
_PROTOTYPE( void vduinit, (struct tty_struct *tp)			);
_PROTOTYPE( void vduswitch, (struct tty_struct *tp)			);
_PROTOTYPE( void vdusetup, (unsigned int vres, char *vram,
			    unsigned short *vrgb)			);
_PROTOTYPE( void vbl, (void)						);
_PROTOTYPE( int vdu_loadfont, (message *m_ptr)				);

/* stwini.c */
_PROTOTYPE( int wini_open, (message *mp)				);
_PROTOTYPE( int wini_rdwt, (message *mp)				);
_PROTOTYPE( int wini_hvrdwt, (message *mp)				);
_PROTOTYPE( int wini_transfer, (int rw, int pnr, int minor,
		long pos, int count, vir_bytes vadr)			);
_PROTOTYPE( int wini_ioctl, (message *mp)				);
_PROTOTYPE( int wini_close, (message *mp)				);

/* stacsi.c */
_PROTOTYPE( int acsi_cmd, (int drive,  unsigned char *cmd, int cmdlen,
		phys_bytes address, phys_bytes data_len,  int rw)	);

/* stscsi.c */
_PROTOTYPE( void scsi_task, (void)					);
_PROTOTYPE( void scsidmaint, (void)					);
_PROTOTYPE( void scsiint, (void)					);
_PROTOTYPE( int scsi_cmd, (int drive,  unsigned char *cmd, int cmdlen,
		phys_bytes address, phys_bytes data_len,  int rw)	);

/* klib68k.s */
_PROTOTYPE( void flipclicks, (phys_clicks c1, phys_clicks c2, phys_clicks n) );
_PROTOTYPE( void copyclicks, (phys_clicks src, phys_clicks dest,
		phys_clicks nclicks)					);
_PROTOTYPE( void zeroclicks, (phys_clicks dest, phys_clicks nclicks)	);
_PROTOTYPE( void phys_copy, (phys_bytes src, phys_bytes dest, phys_bytes n) );

/* stdskclks.s */
_PROTOTYPE( int rd1byte, (void)						);
_PROTOTYPE( int wr1byte, (int)						);
_PROTOTYPE( long getsupra, (void)					);
_PROTOTYPE( long geticd, (void)						);

/* mpx.s */
_PROTOTYPE( int lock, (void)						);
_PROTOTYPE( void unlock, (void)						);
_PROTOTYPE( void restore, (int oldsr)					);
_PROTOTYPE( void reboot, (void)						);
_PROTOTYPE( int test_and_set, (char *flag)				);
_PROTOTYPE( unsigned long get_mem_size, (char *start_addr)			);

/* stprint.c */
#ifdef DEBOUT
_PROTOTYPE( void prtc, (int c)						);
#endif

#ifdef FPP
/* fpp.c */
_PROTOTYPE( void fppinit, (void)					);
_PROTOTYPE( void fpp_new_state, (struct proc *rp)			);
_PROTOTYPE( void fpp_save, (struct proc *rp, struct cpu_state *p)	);
_PROTOTYPE( struct cpu_state  *fpp_restore, (struct proc *rp)		);

/* fpps.s */
_PROTOTYPE( void _fppsave, (struct state_frame *p)			);
_PROTOTYPE( void _fppsavereg, (struct fpp_model *p)			);
_PROTOTYPE( void _fpprestore, (struct state_frame *p)			);
_PROTOTYPE( void _fpprestreg, (struct fpp_model *p)			);
#endif

#if (SHADOWING == 0)
/* pmmu.c */
_PROTOTYPE(void pmmuinit , (void)					);
_PROTOTYPE(void pmmu_init_proc , (struct proc *rp )			);
_PROTOTYPE(void pmmu_restore , (struct proc *rp )			);
_PROTOTYPE(void pmmu_delete , (struct proc *rp )			);
_PROTOTYPE(void pmmu_flush , (struct proc *rp )				);
#endif

#endif /* (CHIP == M68000) */

#endif /* PROTO_H */
