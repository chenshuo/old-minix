| This file is part of the lowest layer of the MINIX kernel.  The other part
| is "proc.c".  The lowest layer does process switching and message handling.

| Every transition to the kernel goes through this file.  Transitions are
| caused by sending/receiving messages and by most interrupts.  (RS232
| interrupts may be handled in the file "rs2.x" and then they rarely enter
| the kernel.)

| Transitions to the kernel may be nested.  The initial entry may be with a
| system call, exception or hardware interrupt; reentries may only be made
| by hardware interrupts.  The count of reentries is kept in 'k_reenter'.
| It is important for deciding whether to switch to the kernel stack and
| for protecting the message passing code in "proc.c".

| For the message passing trap, most of the machine state is saved in the
| proc table.  (Some of the registers need not be saved.)  Then the stack is
| switched to 'k_stack', and interrupts are reenabled.  Finally, the system
| call handler (in C) is called.  When it returns, interrupts are disabled
| again and the code falls into the restart routine, to finish off held-up
| interrupts and run the process or task whose pointer is in 'proc_ptr'.

| Hardware interrupt handlers do the same, except  (1) The entire state must
| be saved.  (2) There are too many handlers to do this inline, so the save
| routine is called.  A few cycles are saved by pushing the address of the
| appropiate restart routine for a return later.  (3) A stack switch is
| avoided when the stack is already switched.  (4) The (master) 8259 interrupt
| controller is reenabled centrally in save().  (5) Each interrupt handler
| masks its interrupt line using the 8259 before enabling (other unmasked)
| interrupts, and unmasks it after servicing the interrupt.  This limits the
| nest level to the number of lines and protects the handler from itself.

| The external entry points into this file are:

.define	_clock_int	| clock interrupt routine (HZ times per second)
.define	_disk_int	| disk interrupt routine
.define	_eth_int	| ethernet interrupt routine
.define	_idle_task	| executed when there is no work
.define	_int00		| handlers for unused interrupt vectors < 17
.define	_int01
.define	_int02
.define	_int03
.define	_int04
.define	_int05
.define	_int06
.define	_int07
.define	_int08
.define	_int09
.define	_int10
.define	_int11
.define	_int12
.define	_int13
.define	_int14
.define	_int15
.define	_int16
.define	_lpr_int	| interrupt routine for each line printer interrupt
.define	_mpx_1hook	| init from real mode for real or protected mode
.define	_mpx_2hook	| init from protected mode for real or protected mode
.define	_restart	| start running a task or process
.define	save		| save the machine state in the proc table
.define	_s_call		| process or task wants to send or receive a message
.define	_trp		| all traps with vector >= 17 are vectored here
.define	_tty_int	| interrupt routine for each key depression and release
.define	_wini_int	| winchester interrupt routine

#include <minix/config.h>
#include <minix/const.h>
#include <minix/com.h>
#include "const.h"
#include "sconst.h"

#if INTEL_32BITS
#error /* this is not the 32-bit version */
#endif

| When using the more portable but slower C-code RS232 interrupt handlers,
| there are a few more entry points:

#if C_RS232_INT_HANDLERS
.define	_prs232_int	| same as rs232_int, different label for prot mode
.define	_psecondary_int	| same as secondary_int, different label for prot mode
.define	_rs232_int 	| interrupt routine for each rs232 interrupt on port 1
.define	_secondary_int 	| interrupt routine for each rs232 interrupt on port 2
#endif

| imported functions

.extern		_clock_handler
.extern		_dp8390_int
.extern		_exception
.extern		_interrupt
.extern		kernel_ds
.extern		_keyboard
.extern		_pr_char
.extern		_sys_call
.extern		_unhold

#if C_RS232_INT_HANDLERS
.extern		_rs232_1handler
.extern		_rs232_2handler
#endif

| imported variables

	.bss
.extern		_break_vector
.extern		_db_enabled
.extern		_held_head
.extern		_k_reenter
.extern		k_stktop
.extern		_pc_at
.extern		_proc_ptr
.extern		_protected_mode
.extern		_ps
.extern		_ps_mca
.extern		_sstep_vector

	.text
|*===========================================================================*
|*				tty_int					     *
|*===========================================================================*
_tty_int:			| Interrupt routine for terminal input.
	call	save		| save the machine state
	in	INT_CTLMASK	| mask out further keyboard interrupts
	orb	al,#KEYBOARD_MASK
	out	INT_CTLMASK
	sti			| allow unmasked, non-keyboard, interrupts
	call	_keyboard	| process a keyboard interrupt
	cli
	in	INT_CTLMASK	| unmask keyboard interrupt
	andb	al,#notop(KEYBOARD_MASK)
	out	INT_CTLMASK
	ret			| return to appropiate restart built by save()


#if C_RS232_INT_HANDLERS
|*===========================================================================*
|*				rs232_int				     *
|*===========================================================================*
_rs232_int:			| Interrupt routine for rs232 I/O.
_prs232_int:
	call	save
	in	INT_CTLMASK
	orb	al,#RS232_MASK
	out	INT_CTLMASK

| Don't enable interrupts, the handlers are not designed for it.
| The mask was set as usual so the handler can reenable interrupts as desired.

	call	_rs232_1handler	| process a serial line 1 interrupt
	in	INT_CTLMASK
	andb	al,#notop(RS232_MASK)
	out	INT_CTLMASK
	ret


|*===========================================================================*
|*				secondary_int				     *
|*===========================================================================*
_secondary_int:			| Interrupt routine for rs232 port 2
_psecondary_int:
	call	save
	in	INT_CTLMASK
	orb	al,#SECONDARY_MASK
	out	INT_CTLMASK
	call	_rs232_2handler	| process a serial line 2 interrupt
	in	INT_CTLMASK
	andb	al,#notop(SECONDARY_MASK)
	out	INT_CTLMASK
	ret
#endif /* C_RS232_INT_HANDLERS */


|*===========================================================================*
|*				lpr_int					     *
|*===========================================================================*
_lpr_int:			| Interrupt routine for printer output.
	call	save
	in	INT_CTLMASK
	orb	al,#PRINTER_MASK
	out	INT_CTLMASK
	sti
	call	_pr_char	| process a line printer interrupt
	cli
	in	INT_CTLMASK
#if ASLD
	andb	al,#notop(PRINTER_MASK)
#else
	andb	al,#notop(PRINTER_MASK) & 0xFF
#endif
	out	INT_CTLMASK
	ret


|*===========================================================================*
|*				disk_int				     *
|*===========================================================================*
_disk_int:			| Interrupt routine for the floppy disk.
	call	save
	cmp	_ps,#0		| check for 2nd int controller on ps (?)
	je	over_ps_disk
	movb	al,#ENABLE
	out	0x3C
over_ps_disk:

| The last doesn't make sense as an 8259 command, since the floppy vector
| seems to be the same on PS's so the usual 8259 must be controlling it.
| This used to be done at the start of the interrupt handler, in proc.c.
| Find out where it really goes, and if there are any mask bits in port
| 0x3D which have to be fiddled (here and in fdc_results()).

	in	INT_CTLMASK
	orb	al,#FLOPPY_MASK
	out	INT_CTLMASK
	sti
	mov	ax,#FLOPPY
	push	ax
	call	_interrupt	| interrupt(FLOPPY)
	add	sp,#2
	cli
	ret


|*===========================================================================*
|*				wini_int				     *
|*===========================================================================*
_wini_int:			| Interrupt routine for the winchester disk.
	call	save
	cmp	_pc_at,#0	| check for 2nd int controller on AT
	jnz	at_wini_int

xt_wini_int:
	in	INT_CTLMASK
	orb	al,#XT_WINI_MASK
	out	INT_CTLMASK
	sti
	mov	ax,#WINCHESTER
	push	ax
	call	_interrupt	| interrupt(WINCHESTER)
	add	sp,#2
	cli
	ret

at_wini_int:
	in	INT2_MASK
	orb	al,#AT_WINI_MASK
	out	INT2_MASK
	sti
	movb	al,#ENABLE	| reenable slave 8259
	out	INT2_CTL	| the master one was done in save() as usual
	mov	ax,#WINCHESTER
	push	ax
	call	_interrupt	| interrupt(WINCHESTER)
	add	sp,#2
	cli
	ret


|*===========================================================================*
|*				clock_int				     *
|*===========================================================================*
_clock_int:			| Interrupt routine for the clock.
	call	save
	cmp	_ps_mca, #0	| if not a PS/2, don't twiddle bits
	je	skip_mca_clock
	in	CLOCK_ACK_PORT	| PS/2 clock needs to be told to stop 
	orb	al,#CLOCK_ACK_BIT	| interrupting CPU, or we'll hang
	out	CLOCK_ACK_PORT
skip_mca_clock:
	in	INT_CTLMASK
	orb	al,#CLOCK_MASK
	out	INT_CTLMASK
	sti
	call	_clock_handler	| process a clock interrupt
				| This calls interrupt() only if necessary.
				| Very rarely.
	cli
	in	INT_CTLMASK
	andb	al,#notop(CLOCK_MASK)
	out	INT_CTLMASK
	ret


|*===========================================================================*
|*				eth_int					     *
|*===========================================================================*
_eth_int:			| Interrupt routine for ethernet input
	call	save
	in	INT_CTLMASK
	orb	al,#ETHER_MASK
	out	INT_CTLMASK
	sti
	call	_dp8390_int	| call the handler
	cli
	ret


|*===========================================================================*
|*				int00-16				     *
|*===========================================================================*
_int00:				| interrupt through vector 0
	push	ax
	movb	al,#0
	j	exception

_int01:				| interrupt through vector 1, etc
	push	ds
	seg	cs
	mov	ds,kernel_ds
	cmpb	_db_enabled,#0
	pop	ds
	jz	over_sstep
	seg	cs
	jmpi	@cs_sstep_vector

over_sstep:
	push	ax
	movb	al,#1
	j	exception

_int02:
	push	ax
	movb	al,#2
	j	exception

_int03:
	push	ds
	seg	cs
	mov	ds,kernel_ds
	cmpb	_db_enabled,#0
	pop	ds
	jz	over_breakpoint
	seg	cs
	jmpi	@cs_break_vector

over_breakpoint:
	push	ax
	movb	al,#3
	j	exception

_int04:
	push	ax
	movb	al,#4
	j	exception

_int05:
	push	ax
	movb	al,#5
	j	exception

_int06:
	push	ax
	movb	al,#6
	j	exception

_int07:
	push	ax
	movb	al,#7
	j	exception

_int08:
	push	ax
	movb	al,#8
	j	exception

_int09:
	push	ax
	movb	al,#9
	j	exception

_int10:
	push	ax
	movb	al,#10
	j	exception

_int11:
	push	ax
	movb	al,#11
	j	exception

_int12:
	push	ax
	movb	al,#12
	j	exception

_int13:
	push	ax
	movb	al,#13
	j	exception

_int14:
	push	ax
	movb	al,#14
	j	exception

_int15:
	push	ax
	movb	al,#15
	j	exception

_int16:
	push	ax
	movb	al,#16
	j	exception

_trp:
	push	ax
	movb	al,#17		| any vector above 17 becomes 17

exception:
	seg	cs
	movb	ex_number,al	| it's cumbersome to get this into dseg
	pop	ax
	call	save
	seg	cs
	push	ex_number	| high byte is constant 0
	call	_exception	| do whatever's necessary (sti only if safe)
	add	sp,#2
	cli
	ret


|*===========================================================================*
|*				save					     *
|*===========================================================================*
save:				| save the machine state in the proc table.
	cld			| set direction flag to a known value
	push	ds
	push	si
	seg	cs
	mov	ds,kernel_ds
	incb	_k_reenter	| from -1 if not reentering
	jnz	push_current_stack	| stack is already kernel's

	mov	si,_proc_ptr
	mov	AXREG(si),ax
	mov	BXREG(si),bx
	mov	CXREG(si),cx
	mov	DXREG(si),dx
	pop	SIREG(si)
	mov	DIREG(si),di
	mov	BPREG(si),bp
	mov	ESREG(si),es
	pop	DSREG(si)
	pop	bx		| return adr
	pop	PCREG(si)
	pop	CSREG(si)
	pop	PSWREG(si)
	mov	SPREG(si),sp
	mov	SSREG(si),ss

	mov	dx,ds
	mov	ss,dx
	mov	sp,#k_stktop
#if SPLIMITS
	mov	splimit,#k_stack+8
#endif
	mov	ax,#_restart	| build return address for interrupt handler
	push	ax

stack_switched:
	mov	es,dx
	movb	al,#ENABLE	| reenable int controller for everyone (early)
	out	INT_CTL
	jmp	(bx)

push_current_stack:
	push	es
	push	bp
	push	di
	push	dx
	push	cx
	push	bx
	push	ax
	mov	bp,sp
	mov	bx,18(bp)	| get the return adr; it becomes junk on stack
	mov	ax,#restart1
	push	ax
	mov	dx,ss
	mov	ds,dx
	j	stack_switched


|*===========================================================================*
|*				s_call					     *
|*===========================================================================*
_s_call:			| System calls are vectored here.
				| Do save routine inline for speed,
				| but don't save ax, bx, cx, dx,
				| since C doesn't require preservation,
				| and ax returns the result code anyway.
				| Regs bp, si, di get saved by sys_call as
				| well, but it is impractical not to preserve
				| them here, in case context gets switched.
				| Some special-case code in pick_proc()
				| could avoid this.
	cld			| set direction flag to a known value
	push	ds
	push	si
	seg	cs
	mov	ds,kernel_ds
	incb	_k_reenter
	mov	si,_proc_ptr
	pop	SIREG(si)
	mov	DIREG(si),di
	mov	BPREG(si),bp
	mov	ESREG(si),es
	pop	DSREG(si)
	pop	PCREG(si)
	pop	CSREG(si)
	pop	PSWREG(si)
	mov	SPREG(si),sp
	mov	SSREG(si),ss
	mov	dx,ds
	mov	es,dx
	mov	ss,dx		| interrupt handlers may not make system calls
	mov	sp,#k_stktop	| so stack is not already switched
#if SPLIMITS
	mov	splimit,#k_stack+8
#endif
				| end of inline save
				| now set up parameters for C routine sys_call
	push	bx		| pointer to user message
	push	ax		| src/dest
	push	cx		| SEND/RECEIVE/BOTH
	sti			| allow SWITCHER to be interrupted
	call	_sys_call	| sys_call(function, src_dest, m_ptr)
				| caller is now explicitly in proc_ptr
	mov	AXREG(si),ax	| sys_call MUST PRESERVE si
	cli

| Fall into code to restart proc/task running.


|*===========================================================================*
|*				restart					     *
|*===========================================================================*
_restart:

| Flush any held-up interrupts.
| This reenables interrupts, so the current interrupt handler may reenter.
| This doesn't matter, because the current handler is about to exit and no
| other handlers can reenter since flushing is only done when k_reenter == 0.

	cmp	_held_head,#0	| do fast test to usually avoid function call
	jz	over_call_unhold
	call	_unhold		| this is rare so overhead is acceptable
over_call_unhold:

	mov	si,_proc_ptr
#if SPLIMITS
	mov	ax,P_SPLIMIT(si)	| splimit = p_splimit
	mov	splimit,ax
#endif
	decb	_k_reenter
	mov	ax,AXREG(si)	| start restoring registers from proc table
				| could make AXREG == 0 to use lodw here
	mov	bx,BXREG(si)
	mov	cx,CXREG(si)
	mov	dx,DXREG(si)
	mov	di,DIREG(si)
	mov	bp,BPREG(si)
	mov	es,ESREG(si)
	mov	ss,SSREG(si)
	mov	sp,SPREG(si)
	push	PSWREG(si)	| fake interrupt stack frame
	push	CSREG(si)
	push	PCREG(si)
				| could put si:ds together to use
				| lds si,SIREG(si)
	push	DSREG(si)
	mov	si,SIREG(si)
	pop	ds
	nop			| helps debugger emulate iret - else pop skips
	iret			| return to user or task

restart1:
	decb	_k_reenter
	pop	ax
	pop	bx
	pop	cx
	pop	dx
	pop	di
	pop	bp
	pop	es
	pop	si
	pop	ds
	add	sp,#2		| skip return adr
	iret


|*===========================================================================*
|*				idle					     *
|*===========================================================================*
_idle_task:			| executed when there is no work 
	j	_idle_task	| a "hlt" before this fails in protected mode


|*===========================================================================*
|*				mpx_1hook				     *
|*===========================================================================*
| PUBLIC void mpx_1hook();
| Initialize klib from real mode for real or protected mode:
| Copy real mode debugger vectors to code segment.
| They are too hard to access at interrupt time.

_mpx_1hook:
	push	ds
	mov	ax,#VEC_TABLE_SEG
	mov	ds,ax
	mov	ax,4*BREAKPOINT_VECTOR
	seg	cs
	mov	cs_break_vector,ax
	mov	ax,4*BREAKPOINT_VECTOR+2
	seg	cs
	mov	cs_break_vector+2,ax
	mov	ax,4*DEBUG_VECTOR
	seg	cs
	mov	cs_sstep_vector,ax
	mov	ax,4*DEBUG_VECTOR+2
	seg	cs
	mov	cs_sstep_vector+2,ax
	pop	ds
	ret


|*===========================================================================*
|*				data					     *
|*===========================================================================*
| NB some variables are stored in code segment.

cs_break_vector:		| copy of real mode breakpoint vector
	.space	4
cs_sstep_vector:		| copy of real mode single-step vector
	.space	4
ex_number:			| exception number
	.space	2


|*===========================================================================*
|*			variants for 286 protected mode			     *
|*===========================================================================*

| Most routines are different in 286 protected mode.
| The only essential difference is that an interrupt in protected mode
| (usually) switches the stack, so there is less to do in software.

| These functions are reached along jumps patched in by klib286_init():

	.define		p_restart	| replaces _restart
	.define		p_save		| replaces save

| These exception and software-interrupt handlers are enabled by the new
| interrupt vector table set up in protect.c:

	.define		_divide_error		| _int00
	.define		_single_step_exception	| _int01
	.define		_nmi			| _int02
	.define		_breakpoint_exception	| _int03
	.define		_overflow		| _int04
	.define		_bounds_check		| _int05
	.define		_inval_opcode		| _int06
	.define		_copr_not_available	| _int07
	.define		_double_fault		| _int08
	.define		_copr_seg_overrun	| _int09
	.define		_inval_tss		| _int10
	.define		_segment_not_present	| _int11
	.define		_stack_exception	| _int12
	.define		_general_protection	| _int13
	.define		_p_s_call		| _s_call

| The hardware interrupt handlers need not be altered apart from putting
| them in the new table (save() handles the differences).
| Some of the intxx handlers (those for exceptions which don't push an
| error code) need not have been replaced, but the names here are better.

#include "protect.h"

/* Selected 286 tss offsets. */
#define TSS2_S_SP0	2

| imported variables

	.bss

	.extern		_tss

	.text
|*===========================================================================*
|*				exception handlers			     *
|*===========================================================================*
_divide_error:
	pushi8	(DIVIDE_VECTOR)
	j	p_exception

_single_step_exception:
	seg	ss
	cmpb	_db_enabled,#0
	jz	p_over_sstep
	seg	ss
	jmpi	@_sstep_vector

p_over_sstep:
	pushi8	(DEBUG_VECTOR)
	j	p_exception

_nmi:
	pushi8	(NMI_VECTOR)
	j	p_exception

_breakpoint_exception:
	seg	ss
	cmpb	_db_enabled,#0
	jz	p_over_breakpoint
	seg	ss
	jmpi	@_break_vector

p_over_breakpoint:
	pushi8	(BREAKPOINT_VECTOR)
	j	p_exception

_overflow:
	pushi8	(OVERFLOW_VECTOR)
	j	p_exception

_bounds_check:
	pushi8	(BOUNDS_VECTOR)
	j	p_exception

_inval_opcode:
	pushi8	(INVAL_OP_VECTOR)
	j	p_exception

_copr_not_available:
	pushi8	(COPROC_NOT_VECTOR)
	j	p_exception

_double_fault:
	pushi8	(DOUBLE_FAULT_VECTOR)
	j	errexception

_copr_seg_overrun:
	pushi8	(COPROC_SEG_VECTOR)
	j	p_exception

_inval_tss:
	pushi8	(INVAL_TSS_VECTOR)
	j	errexception

_segment_not_present:
	pushi8	(SEG_NOT_VECTOR)
	j	errexception

_stack_exception:
	pushi8	(STACK_FAULT_VECTOR)
	j	errexception

_general_protection:
	pushi8	(PROTECTION_VECTOR)
	j	errexception


|*===========================================================================*
|*				p_exception				     *
|*===========================================================================*
| This is called for all exceptions which don't push an error code.

p_exception:
	seg	ss
	pop	ds_ex_number
	call	p_save
	j	p1_exception


|*===========================================================================*
|*				errexception				     *
|*===========================================================================*
| This is called for all exceptions which push an error code.

errexception:
	seg	ss
	pop	ds_ex_number
	seg	ss
	pop	trap_errno
	call	p_save
p1_exception:			| Common for all exceptions.
	push	ds_ex_number
	call	_exception
	add	sp,#2
	cli
	ret


|*===========================================================================*
|*				p_save					     *
|*===========================================================================*
| Save for 286 protected mode.
| This is much simpler than for 8086 mode, because the stack already points
| into process table, or has already been switched to the kernel stack.

p_save:
	cld			| set direction flag to a known value
	pusha			| save "general" registers
	push	ds		| save ds
	push	es		| save es
	mov	dx,ss		| ss is kernel data segment
	mov	ds,dx		| load rest of kernel segments
	mov	es,dx
	movb	al,#ENABLE	| reenable int controller
	out	INT_CTL
	mov	bp,sp		| prepare to return
	incb	_k_reenter	| from -1 if not reentering
	jnz	set_p1_restart	| stack is already kernel's
	mov	sp,#k_stktop
#if SPLIMITS
	mov	splimit,#k_stack+8
#endif
	pushi16	(p_restart)	| build return address for interrupt handler
	jmp	@RETADR-P_STACKBASE(bp)

set_p1_restart:
	pushi16	(p1_restart)
	jmp	@RETADR-P_STACKBASE(bp)


|*===========================================================================*
|*				p_s_call				     *
|*===========================================================================*
_p_s_call:
	cld			| set direction flag to a known value
	sub	sp,#6*2		| skip RETADR, ax, cx, dx, bx, st
	push	bp		| stack already points into process table
	push	si
	push	di
	push	ds
	push	es
	mov	dx,ss
	mov	ds,dx
	mov	es,dx
	incb	_k_reenter
	mov	si,sp		| assumes P_STACKBASE == 0
	mov	sp,#k_stktop
#if SPLIMITS
	mov	splimit,#k_stack+8
#endif
				| end of inline save
	sti			| allow SWITCHER to be interrupted
				| now set up parameters for C routine sys_call
	push	bx		| pointer to user message
	push	ax		| src/dest
	push	cx		| SEND/RECEIVE/BOTH
	call	_sys_call	| sys_call(function, src_dest, m_ptr)
				| caller is now explicitly in proc_ptr
	mov	AXREG(si),ax	| sys_call MUST PRESERVE si
	cli

| Fall into code to restart proc/task running.

p_restart:

| Flush any held-up interrupts.
| This reenables interrupts, so the current interrupt handler may reenter.
| This doesn't matter, because the current handler is about to exit and no
| other handlers can reenter since flushing is only done when k_reenter == 0.

	cmp	_held_head,#0	| do fast test to usually avoid function call
	jz	p_over_call_unhold
	call	_unhold		| this is rare so overhead is acceptable
p_over_call_unhold:
	mov	si,_proc_ptr
#if SPLIMITS
	mov	ax,P_SPLIMIT(si)	| splimit = p_splimit
	mov	splimit,ax
#endif
	deflldt	(P_LDT_SEL(si))		| enable task's segment descriptors
	lea	ax,P_STACKTOP(si)	| arrange for next interrupt
	mov	_tss+TSS2_S_SP0,ax	| to save state in process table
	mov	sp,si		| assumes P_STACKBASE == 0
p1_restart:
	decb	_k_reenter
	pop	es
	pop	ds
	popa
	add	sp,#2		| skip RETADR
	iret			| continue process


|*===========================================================================*
|*				mpx_2hook				     *
|*===========================================================================*
| PUBLIC void mpx_2hook();
| Initialize klib from protected mode for real or protected mode:
| Nothing to do.

_mpx_2hook:
	ret


|*===========================================================================*
|*				data					     *
|*===========================================================================*
	.bss
ds_ex_number:
	.space	2
	.space	2		| try to align for 386 to take advantage of
trap_errno:
	.space	2
	.space	2		| align
