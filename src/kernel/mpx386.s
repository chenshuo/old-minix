#
! This file contains the assembler startup code for Minix and the 32-bit
! interrupt handlers.  It cooperates with cstart.c to set up a good
! environment for main().

! This file is part of the lowest layer of the MINIX kernel.  The other part
! is "proc.c".  The lowest layer does process switching and message handling.

! Every transition to the kernel goes through this file.  Transitions are
! caused by sending/receiving messages and by most interrupts.  (RS232
! interrupts may be handled in the file "rs2.s" and then they rarely enter
! the kernel.)

! Transitions to the kernel may be nested.  The initial entry may be with a
! system call, exception or hardware interrupt; reentries may only be made
! by hardware interrupts.  The count of reentries is kept in `k_reenter`.
! It is important for deciding whether to switch to the kernel stack and
! for protecting the message passing code in "proc.c".

! For the message passing trap, most of the machine state is saved in the
! proc table.  (Some of the registers need not be saved.)  Then the stack is
! switched to `k_stack`, and interrupts are reenabled.  Finally, the system
! call handler (in C) is called.  When it returns, interrupts are disabled
! again and the code falls into the restart routine, to finish off held-up
! interrupts and run the process or task whose pointer is in `proc_ptr`.

! Hardware interrupt handlers do the same, except  (1) The entire state must
! be saved.  (2) There are too many handlers to do this inline, so the save
! routine is called.  A few cycles are saved by pushing the address of the
! appropiate restart routine for a return later.  (3) A stack switch is
! avoided when the stack is already switched.  (4) The (master) 8259 interrupt
! controller is reenabled centrally in save().  (5) Each interrupt handler
! masks its interrupt line using the 8259 before enabling (other unmasked)
! interrupts, and unmasks it after servicing the interrupt.  This limits the
! nest level to the number of lines and protects the handler from itself.


! sections

.sect .text
begtext:
.sect .rom
begrom:
.sect .data
begdata:
.sect .bss
begbss:

#include <minix/config.h>
#include <minix/const.h>
#include <minix/com.h>
#include "const.h"
#include "protect.h"
#include "sconst.h"

/* Selected 386 tss offsets. */
#define TSS3_S_SP0	4

! Exported functions

.sect .text
.define	_db		! trap to external debugger
.define	_get_word	! returns word at given segment:offset
.define	_put_word	! writes given word to given segment:offset
.define	_idle_task
.define	_mpx_1hook
.define	_mpx_2hook
.define	_restart
.define	save

.define	_divide_error		
.define	_single_step_exception
.define	_nmi
.define	_breakpoint_exception
.define	_overflow
.define	_bounds_check
.define	_inval_opcode
.define	_copr_not_available
.define	_double_fault
.define	_copr_seg_overrun
.define	_inval_tss
.define	_segment_not_present
.define	_stack_exception
.define	_general_protection
.define	_page_fault
.define	_copr_error

.define	_hwint00	! handlers for hardware interrupts
.define	_hwint01
.define	_hwint02
.define	_hwint03
.define	_hwint04
.define	_hwint05
.define	_hwint06
.define	_hwint07
.define	_hwint08
.define	_hwint09
.define	_hwint10
.define	_hwint11
.define	_hwint12
.define	_hwint13
.define	_hwint14
.define	_hwint15

.define	_s_call			
.define	_p_s_call
.define	_level0_call

! Imported functions.

.extern	_cstart
.extern	_main
.extern	_exception
.extern	_interrupt
.extern	_sys_call
.extern	_unhold

! Exported variables.

.sect .bss
.define	begbss
.define	begdata
.define	_sizes

! Imported variables.

.sect .bss
.extern	_gdt
.extern	_code_base
.extern	_data_base
.extern	_break_vector
.extern	_db_enabled
.extern	_held_head
.extern	_k_reenter
.extern	_pc_at
.extern	_proc_ptr
.extern	_ps_mca
.extern	_sstep_vector
.extern	_tss
.extern	_level0_func

.sect .text
!*===========================================================================*
!*				MINIX					     *
!*===========================================================================*
MINIX:				! this is the entry point for the MINIX kernel
	jmp	over_flags	! skip over the next few bytes
	.data2	CLICK_SHIFT	! for the monitor: memory granularity
flags:
	.data2	0x0009		! boot monitor flags: call in 386 mode,
				!	load high
	nop			! extra byte to sync up disassembler
over_flags:

! Set up kernel segment registers and stack.
! The monitor sets cs and ds right.  ss still points to the monitor data,
! because the boot parameters offset and size are on its stack.

! Copy the monitor gdt to the kernel`s address space for cstart.  (There is
! no other reason for this than to get it into our address space.)
	sgdt	(_gdt)		! Get the monitor gdtr
	mov	esi, (_gdt+2)	! Absolute address of global descriptor table
	mov	ebx, _gdt	! Address of kernel`s global descriptor table
	mov	ecx, 8*8	! Copying eight descriptors
copygdt:
 eseg	movb	al, (esi)
	movb	(ebx), al
	inc	esi
	inc	ebx
	loop	copygdt

! Locate boot parameters and set segment registers
	pop	ebx		! boot parameters offset
	pop	ecx		! boot parameters length
	mov	dx, ss		! monitor data
	mov	ax, ds		! kernel data
	mov	es, ax
	mov	fs, ax
	mov	gs, ax
	mov	ss, ax
	mov	esp, k_stktop	! set sp to point to the top of kernel stack

! Call C startup code to set up a proper gdt and idt

	push	ecx
	movzx	edx, dx
	push	edx
	push	ebx
	mov	ax, ds
	movzx	eax, ax
	push	eax
	mov	ax, cs
	movzx	eax, ax
	push	eax
	call	_cstart		! cstart(cs, ds, parmoff, parmseg, parmlen)
	add	esp, 5*4

! prot_init() has set up a gdt compatible with the BIOS interrupt to switch
! modes.  The boot monitor has done this already however, but without a proper
! idt.  So we reload gdtr, idtr and the segment registers to this better table.
! This is what the BIOS call would have needed:
!	gdt pointer in gdt[1]
!	ldt pointer in gdt[2]
!	new ds in gdt[3]
!	new es in gdt[4]
!	new ss in gdt[5]
!	new cs in gdt[6]
!	nothing in gdt[7] (overwritten with BIOS cs)
!	ICW2 for master 8259 in bh
!	ICW2 for slave 8259 in bl

	lgdt	(_gdt+BIOS_GDT_SELECTOR)
	lidt	(_gdt+BIOS_IDT_SELECTOR)

	jmpf	CS_SELECTOR:csinit
csinit:
    o16	mov	ax, DS_SELECTOR
	mov	ds, ax
	mov	es, ax
	mov	fs, ax
	mov	gs, ax
	mov	ss, ax

! There is a little more protected mode initialization to do, but leave it
! to main().

	call	_main


!*===========================================================================*
!*				db					     *
!*===========================================================================*

! PUBLIC void db();
! Trap to external debugger.
! This may be called from all modes (real or protected, 16 or 32-bit).

_db:
	int	3
	ret


!*===========================================================================*
!*				get_word				     *
!*===========================================================================*

! PUBLIC u16_t get_word(u16_t segment, u16_t *offset);
! Load and return the word at the far pointer  segment:offset.

_get_word:
	push	ds			! save ds
#if __ACK__
	mov	ds, 4+4(esp)		! segment
#else
	.data1	0x8E,0x5C,0x24, 4+4	! segment
#endif
	mov	eax, 4+4+4(esp)		! offset
	movzx	eax, (eax)		! load the word to return
	pop	ds			! restore ds
	ret


!*===========================================================================*
!*				put_word				     *
!*===========================================================================*

! PUBLIC void put_word(u16_t segment, u16_t *offset, u16_t value);
! Store the word  value  at the far pointer  segment:offset.

_put_word:
	push	ds			! save ds
#if __ACK__
	mov	ds, 4+4(esp)		! segment
#else
	.data1	0x8E,0x5C,0x24, 4+4	! segment
#endif
	mov	eax, 4+4+4(esp)		! offset
	mov	edx, 4+4+4+4(esp) 	! value
    o16	mov	(eax), dx		! store the word
	pop	ds			! restore ds
	ret


!*===========================================================================*
!*		interrupt handlers for 386 32-bit protected mode	     *
!*===========================================================================*


.sect .text
!*===========================================================================*
!*				exception handlers			     *
!*===========================================================================*
_divide_error:
	push	DIVIDE_VECTOR
	jmp	exception

_single_step_exception:
	sseg
	cmpb	(_db_enabled), 0
	jz	over_sstep
	sseg
	jmpf	(_sstep_vector)

over_sstep:
	push	DEBUG_VECTOR
	jmp	exception

_nmi:
	push	NMI_VECTOR
	jmp	exception

_breakpoint_exception:
	sseg
	cmpb	(_db_enabled), 0
	jz	over_breakpoint
	sseg
	jmpf	(_break_vector)

over_breakpoint:
	push	BREAKPOINT_VECTOR
	jmp	exception

_overflow:
	push	OVERFLOW_VECTOR
	jmp	exception

_bounds_check:
	push	BOUNDS_VECTOR
	jmp	exception

_inval_opcode:
	push	INVAL_OP_VECTOR
	jmp	exception

_copr_not_available:
	push	COPROC_NOT_VECTOR
	jmp	exception

_double_fault:
	push	DOUBLE_FAULT_VECTOR
	jmp	errexception

_copr_seg_overrun:
	push	COPROC_SEG_VECTOR
	jmp	exception

_inval_tss:
	push	INVAL_TSS_VECTOR
	jmp	errexception

_segment_not_present:
	push	SEG_NOT_VECTOR
	jmp	errexception

_stack_exception:
	push	STACK_FAULT_VECTOR
	jmp	errexception

_general_protection:
	push	PROTECTION_VECTOR
	jmp	errexception

_page_fault:
	push	PAGE_FAULT_VECTOR
	jmp	errexception
  
_copr_error:
	push	COPROC_ERR_VECTOR
	jmp	exception

_level0_call:
	call	save
	jmp	(_level0_func)
  

!*===========================================================================*
!*				exception				     *
!*===========================================================================*
! This is called for all exceptions which don`t push an error code.

	.align	16
exception:
 sseg	mov	(trap_errno), 0		! clear trap_errno
 sseg	pop	(ex_number)
	jmp	exception1


!*===========================================================================*
!*				errexception				     *
!*===========================================================================*
! This is called for all exceptions which push an error code.

	.align	16
errexception:
 sseg	pop	(ex_number)
 sseg	pop	(trap_errno)
exception1:				! Common for all exceptions.

	push	eax			! eax is scratch register
	mov	eax, 0+4(esp)		! old eip
 sseg	mov	(old_eip), eax
	movzx	eax, 4+4(esp)		! old cs
 sseg	mov	(old_cs), eax
	mov	eax, 8+4(esp)		! old eflags
 sseg	mov	(old_eflags), eax
 	pop	eax
 
 	call	save
	push	(old_eflags)
	push	(old_cs)
	push	(old_eip)
	push	(trap_errno)
	push	(ex_number)
	call	_exception		! (ex_number, trap_errno, old_eip,
					!	old_cs, old_eflags)
	add	esp, 5*4
	cli
	ret


!*===========================================================================*
!*				interrupt handlers			     *
!*===========================================================================*


!*===========================================================================*
!*				hwint00 - 07				     *
!*===========================================================================*
#define hwint_master(irq)	\
	call	save			/* save interrupted process state */;\
	inb	INT_CTLMASK						    ;\
	orb	al, [1<<irq]						    ;\
	outb	INT_CTLMASK		/* disable the irq		  */;\
	movb	al, ENABLE						    ;\
	outb	INT_CTL			/* reenable master 8259		  */;\
	sti				/* enable interrupts		  */;\
	push	irq			/* irq				  */;\
	call	(_irq_table + 4*irq)	/* eax = (*irq_table[irq])(irq)   */;\
	pop	ecx							    ;\
	cli				/* disable interrupts		  */;\
	test	eax, eax		/* need to reenable irq?	  */;\
	jz	0f							    ;\
	inb	INT_CTLMASK						    ;\
	andb	al, ~[1<<irq]						    ;\
	outb	INT_CTLMASK		/* enable the irq		  */;\
0:	ret				/* restart (another) process      */


	.align	16
_hwint00:		! Interrupt routine for irq 0 (the clock).
	hwint_master(0)


	.align	16
_hwint01:		! Interrupt routine for irq 1 (keyboard)
	hwint_master(1)


	.align	16
_hwint02:		! Interrupt routine for irq 2 (cascade!)
	hwint_master(2)


	.align	16
_hwint03:		! Interrupt routine for irq 3 (second serial)
	hwint_master(3)


	.align	16
_hwint04:		! Interrupt routine for irq 4 (first serial)
	hwint_master(4)


	.align	16
_hwint05:		! Interrupt routine for irq 5 (XT winchester)
	hwint_master(5)


	.align	16
_hwint06:		! Interrupt routine for irq 6 (floppy)
	hwint_master(6)


	.align	16
_hwint07:		! Interrupt routine for irq 7 (printer)
	hwint_master(7)


!*===========================================================================*
!*				hwint08 - 15				     *
!*===========================================================================*
#define hwint_slave(irq)	\
	call	save			/* save interrupted process state */;\
	inb	INT2_CTLMASK						    ;\
	orb	al, [1<<[irq-8]]					    ;\
	outb	INT2_CTLMASK		/* disable the irq		  */;\
	movb	al, ENABLE						    ;\
	outb	INT_CTL			/* reenable master 8259		  */;\
	jmp	.+2			/* delay			  */;\
	outb	INT2_CTL		/* reenable slave 8259		  */;\
	sti				/* enable interrupts		  */;\
	push	irq			/* irq				  */;\
	call	(_irq_table + 4*irq)	/* eax = (*irq_table[irq])(irq)   */;\
	pop	ecx							    ;\
	cli				/* disable interrupts		  */;\
	test	eax, eax		/* need to reenable irq?	  */;\
	jz	0f							    ;\
	inb	INT2_CTLMASK						    ;\
	andb	al, ~[1<<[irq-8]]					    ;\
	outb	INT2_CTLMASK		/* enable the irq		  */;\
0:	ret				/* restart (another) process      */


	.align	16
_hwint08:		! Interrupt routine for irq 8 (realtime clock)
	hwint_slave(8)


	.align	16
_hwint09:		! Interrupt routine for irq 9 (irq 2 redirected)
	hwint_slave(9)


	.align	16
_hwint10:		! Interrupt routine for irq 10
	hwint_slave(10)


	.align	16
_hwint11:		! Interrupt routine for irq 11
	hwint_slave(11)


	.align	16
_hwint12:		! Interrupt routine for irq 12
	hwint_slave(12)


	.align	16
_hwint13:		! Interrupt routine for irq 13 (FPU exception)
	hwint_slave(13)


	.align	16
_hwint14:		! Interrupt routine for irq 14 (AT winchester)
	hwint_slave(14)


	.align	16
_hwint15:		! Interrupt routine for irq 15
	hwint_slave(15)


!*===========================================================================*
!*				save					     *
!*===========================================================================*
! Save for protected mode.
! This is much simpler than for 8086 mode, because the stack already points
! into the process table, or has already been switched to the kernel stack.

	.align	16
save:
	cld			! set direction flag to a known value
	pushad			! save "general" registers
    o16	push	ds		! save ds
    o16	push	es		! save es
    o16	push	fs		! save fs
    o16	push	gs		! save gs
	mov	dx, ss		! ss is kernel data segment
	mov	ds, dx		! load rest of kernel segments
	mov	es, dx		! kernel doesn`t use fs, gs
	mov	eax, esp	! prepare to return
	incb	(_k_reenter)	! from -1 if not reentering
	jnz	set_restart1	! stack is already kernel`s
	mov	esp, k_stktop
	push	_restart	! build return address for int handler
	xor	ebp, ebp	! for stacktrace
	jmp	ERETADR-P_STACKBASE(eax)

	.align	4
set_restart1:
	push	restart1
	jmp	ERETADR-P_STACKBASE(eax)


!*===========================================================================*
!*				_s_call					     *
!*===========================================================================*
	.align	16
_s_call:
_p_s_call:
	cld			! set direction flag to a known value
	sub	esp, 6*4	! skip RETADR, eax, ecx, edx, ebx, est
	push	ebp		! stack already points into proc table
	push	esi
	push	edi
    o16	push	ds
    o16	push	es
    o16	push	fs
    o16	push	gs
	mov	dx, ss
	mov	ds, dx
	mov	es, dx
	incb	(_k_reenter)
	mov	esi, esp	! assumes P_STACKBASE == 0
	mov	esp, k_stktop
	xor	ebp, ebp	! for stacktrace
				! end of inline save
	sti			! allow SWITCHER to be interrupted
				! now set up parameters for sys_call()
	push	ebx		! pointer to user message
	push	eax		! src/dest
	push	ecx		! SEND/RECEIVE/BOTH
	call	_sys_call	! sys_call(function, src_dest, m_ptr)
				! caller is now explicitly in proc_ptr
	mov	EAXREG(esi), eax	! sys_call MUST PRESERVE si
	cli

! Fall into code to restart proc/task running.

_restart:

! Flush any held-up interrupts.
! This reenables interrupts, so the current interrupt handler may reenter.
! This doesn`t matter, because the current handler is about to exit and no
! other handlers can reenter since flushing is only done when k_reenter == 0.

	cmp	(_held_head), 0	! do fast test to usually avoid function call
	jz	over_call_unhold
	call	_unhold		! this is rare so overhead acceptable
over_call_unhold:
	mov	esp, (_proc_ptr)	! will assume P_STACKBASE == 0
	lldt	P_LDT_SEL(esp)		! enable task`s segment descriptors
	lea	eax, P3_STACKTOP(esp)	! arrange for next interrupt
	mov	(_tss+TSS3_S_SP0), eax	! to save state in process table
restart1:
	decb	(_k_reenter)
    o16	pop	gs
    o16	pop	fs
    o16	pop	es
    o16	pop	ds
	popad
	add	esp, 4		! skip return adr
	iretd			! continue process


!*===========================================================================*
!*				idle					     *
!*===========================================================================*
_idle_task:			! executed when there is no work 
	jmp	_idle_task	! a "hlt" before this fails in protected mode


!*===========================================================================*
!*				mpx_1hook				     *
!*				mpx_2hook				     *
!*===========================================================================*
! PUBLIC void mpx_1hook();
! Initialize mpx from real mode for protected mode (no real mode).
! Nothing to do.

! PUBLIC void mpx_2hook();
! Initialize mpx from protected mode for protected mode (no real mode).
! Nothing to do.

_mpx_1hook:
_mpx_2hook:
	ret


!*===========================================================================*
!*				data					     *
!*===========================================================================*

.sect .rom	! Before the string table please
_sizes:				! sizes of kernel, mm, fs filled in by build
	.data2	0x526F		! this must be the first data entry (magic #)
	.data2	CLICK_SHIFT	! consistency check for build
	.space	16*2*4-4	! monitor uses prevous 2 words and this space

.sect .bss
k_stack:
	.space	K_STACK_BYTES	! kernel stack
k_stktop:			! top of kernel stack

	.comm	ex_number, 4
	.comm	trap_errno, 4
	.comm	old_eip, 4
	.comm	old_cs, 4
	.comm	old_eflags, 4
