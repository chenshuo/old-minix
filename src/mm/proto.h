/* Function prototypes. */

struct mproc;		/* need types outside of parameter list --kub */
struct stat;

/* alloc.c */
_PROTOTYPE( phys_clicks alloc_mem, (phys_clicks clicks)			);
_PROTOTYPE( void free_mem, (phys_clicks base, phys_clicks clicks)	);
_PROTOTYPE( phys_clicks max_hole, (void)				);
_PROTOTYPE( void mem_init, (phys_clicks *total, phys_clicks *free)	);
_PROTOTYPE( phys_clicks mem_left, (void)				);
_PROTOTYPE( int do_brk3, (void)						);

/* break.c */
_PROTOTYPE( int adjust, (struct mproc *rmp,
			vir_clicks data_clicks, vir_bytes sp)		);
_PROTOTYPE( int do_brk, (void)						);
_PROTOTYPE( int size_ok, (int file_type, vir_clicks tc, vir_clicks dc,
			vir_clicks sc, vir_clicks dvir, vir_clicks s_vir) );
_PROTOTYPE( void stack_fault, (int proc_nr)				);

/* exec.c */
_PROTOTYPE( int do_exec, (void)						);

/* forkexit.c */
_PROTOTYPE( int do_fork, (void)						);
_PROTOTYPE( int do_mm_exit, (void)					);
_PROTOTYPE( int do_waitpid, (void)					);
_PROTOTYPE( void mm_exit, (struct mproc *rmp, int exit_status)		);

/* getset.c */
_PROTOTYPE( int do_getset, (void)					);

/* main.c */
_PROTOTYPE( void main, (void)						);
_PROTOTYPE( int get_mem, (phys_clicks *base, phys_clicks *size,
						phys_clicks *total)	);

#if (MACHINE == MACINTOSH)
_PROTOTYPE( phys_clicks start_click, (void)				);
#endif

_PROTOTYPE( void reply, (int proc_nr, int result, int res2, char *respt));

/* putk.c */
_PROTOTYPE( void putk, (int c)						);

/* signal.c */
_PROTOTYPE( int do_alarm, (void)					);
_PROTOTYPE( int do_kill, (void)						);
_PROTOTYPE( int do_ksig, (void)						);
_PROTOTYPE( int do_pause, (void)					);
_PROTOTYPE( int do_signal, (void)					);
_PROTOTYPE( int set_alarm, (int proc_nr, int sec)			);
_PROTOTYPE( void sig_proc, (struct mproc *rmp, int sig_nr)		);
_PROTOTYPE( int do_sigaction, (void)					);
_PROTOTYPE( int do_sigpending, (void)					);
_PROTOTYPE( int do_sigmask, (void)					);
_PROTOTYPE( int do_sigreturn, (void)					);
_PROTOTYPE( int do_sigsuspend, (void)					);
_PROTOTYPE( int do_reboot, (void)					);

/* trace.c */
_PROTOTYPE( int do_trace, (void)					);
_PROTOTYPE( void stop_proc, (struct mproc *rmp, int sig_nr)		);

/* utility.c */
_PROTOTYPE( int allowed, (char *name_buf, struct stat *s_buf, int mask)	);
_PROTOTYPE( struct mproc * find_proc, (int pid)				);
_PROTOTYPE( int mem_copy, (int src_proc, int src_seg, long src_vir, 
		   int dst_proc, int dst_seg, long dst_vir, long bytes) );
_PROTOTYPE( int no_sys, (void)						);
_PROTOTYPE( void panic, (char *format, int num)				);
_PROTOTYPE( int sys_adjmap, (int proc, struct mem_map *ptr, 
		vir_clicks data_clicks, vir_clicks sp)			);
_PROTOTYPE( void sys_exec, (int proc, char *ptr, int traced, 
				char *aout, vir_bytes initpc)		);
_PROTOTYPE( int sys_execmap, (int proc, struct mem_map *ptr)		);
_PROTOTYPE( int sys_fork, (int parent, int child, int pid, 
					phys_clicks shadow)		);
_PROTOTYPE( void sys_fresh, (int proc, struct mem_map *ptr, 
		phys_clicks dc, phys_clicks *basep, phys_clicks *sizep)	);
_PROTOTYPE( void sys_getsp, (int proc, vir_bytes *newsp)		);
_PROTOTYPE( void sys_newmap, (int proc, struct mem_map *ptr)		);
_PROTOTYPE( void sys_getmap, (int proc, struct mem_map *ptr)		);
_PROTOTYPE( void sys_sendsig, (int proc, struct sigmsg *ptr)		);
_PROTOTYPE( void sys_oldsig, (int proc, int sig, sighandler_t sighandler));
_PROTOTYPE( void sys_endsig, (int proc)					);
_PROTOTYPE( int sys_sigreturn, (int proc, vir_bytes scp, int flags)	);
_PROTOTYPE( int sys_trace,(int req, int procnr, long addr, long *data_p));
_PROTOTYPE( void sys_xit, (int parent, int proc, phys_clicks *basep, 
						 phys_clicks *sizep));
_PROTOTYPE( void tell_fs, (int what, int p1, int p2, int p3)		);
