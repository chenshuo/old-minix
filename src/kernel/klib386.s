#
! sections

.sect .text; .sect .rom; .sect .data; .sect .bss

#include <minix/config.h>
#include <minix/const.h>
#include "const.h"
#include "sconst.h"
#include "protect.h"

! This file contains a number of assembly code utility routines needed by the
! kernel.  They are:

.define	_monitor	! exit Minix and return to the monitor
.define	_build_sig	! build 4 word structure pushed onto stack for signals
.define	_check_mem	! check a block of memory, return the valid size
.define	_cp_mess	! copies messages from source to destination
.define	_exit		! dummy for library routines
.define	__exit		! dummy for library routines
.define	___exit		! dummy for library routines
.define	___main		! dummy for GCC
.define	_in_byte	! read a byte from a port and return it
.define	_in_word	! read a word from a port and return it
.define	_lock		! disable interrupts
.define	_unlock		! enable interrupts
.define	_enable_irq	! enable an irq at the 8259 controller
.define	_disable_irq	! disable an irq
.define	_mem_rdw	! copy one word from [segment:offset]
.define	_out_byte	! write a byte to a port
.define	_out_word	! write a word to a port
.define	_phys_copy	! copy data from anywhere to anywhere in memory
.define	_phys_zero	! zero memory anywhere in memory
.define	_port_read	! transfer data from (disk controller) port to memory
.define	_port_read_byte	! likewise byte by byte
.define	_port_write	! transfer data from memory to (disk controller) port
.define	_port_write_byte ! likewise byte by byte
.define	_reset		! reset the system
.define	_scr_down	! scroll screen a line down (in software, by copying)
.define	_scr_up		! scroll screen a line up (in software, by copying)
.define	_test_and_set	! test and set locking primitive on a word in memory
.define	_vid_copy	! copy data to video ram (perhaps during retrace only)
.define	_wait_retrace	! wait for retrace interval
.define	_level0		! call a function at level 0

! The routines only guarantee to preserve the registers the `C` compiler
! expects to be preserved (ebx, esi, edi, ebp, esp, segment registers, and
! direction bit in the flags).

! imported variables

.sect .bss
.extern	_Ax, _Bx, _Cx, _Dx, _Es
.extern	_mon_return, _mon_sp
.extern _irq_use
.extern	_blank_color
.extern	_ext_memsize
.extern	_gdt
.extern	_low_memsize
.extern	_sizes
.extern	_snow
.extern	_vid_mask
.extern	_vid_port
.extern	_level0_func

.sect .text
!*===========================================================================*
!*				monitor					     *
!*===========================================================================*
! PUBLIC void monitor();
! Return to the monitor.

_monitor:
	mov	eax, (_reboot_code)	! address of new parameters
	mov	esp, (_mon_sp)		! restore monitor stack pointer
    o16 mov	dx, SS_SELECTOR		! monitor data segment
	mov	ds, dx
	mov	es, dx
	mov	fs, dx
	mov	gs, dx
	mov	ss, dx
	pop	edi
	pop	esi
	pop	ebp
    o16 retf				! return to the monitor


#if ENABLE_BIOS_WINI
!*===========================================================================*
!*				bios13					     *
!*===========================================================================*
! PUBLIC void bios13();
.define	_bios13	
_bios13:
	cmpb	(_mon_return), 0	! is the monitor there?
	jnz	0f
	movb	(_Ax+1), 0x01		! "invalid command"
	ret
0:	push	ebp			! save C registers
	push	esi
	push	edi
	push	ebx
	pushf				! save flags
	cli				! no interruptions

	inb	INT2_CTLMASK
	movb	ah, al
	inb	INT_CTLMASK
	push	eax			! save interrupt masks
	mov	eax, (_irq_use)		! map of in-use IRQ`s
	and	eax, ~[1<<CLOCK_IRQ]	! there is a special clock handler
	outb	INT_CTLMASK		! enable all unused IRQ`s and vv.
	movb	al, ah
	outb	INT2_CTLMASK

	mov	eax, cr0
	push	eax			! save machine status word

	mov	eax, SS_SELECTOR	! monitor data segment
	mov	ss, ax
	xchg	esp, (_mon_sp)		! switch stacks
    o16	push	(_Es)			! parameters used in bios 13 call
    o16	push	(_Dx)
    o16	push	(_Cx)
    o16	push	(_Bx)
    o16	push	(_Ax)
	mov	ds, ax			! remaining data selectors
	mov	es, ax
	mov	fs, ax
	mov	gs, ax
	push	cs
	push	return			! kernel return address and selector
    o16	jmpf	16+2*4+5*2+2*4(esp)	! make the call
return:
    o16	pop	(_Ax)
    o16	pop	(_Bx)
    o16	pop	(_Cx)
    o16	pop	(_Dx)
    o16	pop	(_Es)
	lgdt	(_gdt+GDT_SELECTOR)	! reload global descriptor table
	jmpf	CS_SELECTOR:csinit	! restore everything
csinit:	mov	eax, DS_SELECTOR
	mov	ds, ax
	mov	es, ax
	mov	fs, ax
	mov	gs, ax
	mov	ss, ax
	xchg	esp, (_mon_sp)		! unswitch stacks
	lidt	(_gdt+IDT_SELECTOR)	! reload interrupt descriptor table
	andb	(_gdt+TSS_SELECTOR+DESC_ACCESS), ~0x02  ! clear TSS busy bit
	mov	ax, TSS_SELECTOR
	ltr	ax			! set TSS register

	pop	eax
	mov	cr0, eax		! restore machine status word

	pop	eax
	outb	INT_CTLMASK		! restore interrupt masks
	movb	al, ah
	outb	INT2_CTLMASK

	popf				! restore flags
	pop	ebx			! restore C registers
	pop	edi
	pop	esi
	pop	ebp
	ret
#endif /* ENABLE_BIOS_WINI */


!*===========================================================================*
!*				build_sig				     *
!*===========================================================================*
! PUBLIC void build_sig(char *sig_stuff, struct proc *rp, int sig)
! Build a structure that is pushed onto the stack for signals.  It contains
! pc, psw, etc., and is machine dependent. The format is the same as generated
! by hardware interrupts, except that after the "interrupt", the signal number
! is also pushed.  The signal processing routine within the user space first
! pops the signal number, to see which function to call.  Then it calls the
! function.  Finally, when the function returns to the low-level signal
! handling routine, control is passed back to where it was prior to the signal
! by executing a return-from-interrupt instruction, hence the need for using
! the hardware generated interrupt format on the stack.

_build_sig:
	mov	ecx, 4(esp)		! sig_stuff
	mov	edx, 4+4(esp)		! rp
	mov	eax, 4+4+4(esp)		! sig
	mov	(ecx), eax		! put signal number in sig_stuff
	mov	eax, EPCREG(edx)	! signalled process` PC
	mov	4(ecx), eax		! put pc in sig_stuff
	mov	eax, ECSREG(edx)	! signalled process` cs
	mov	4+4(ecx), eax		! put cs in sig_stuff
	mov	eax, EPSWREG(edx)	! signalled process` PSW
	mov	4+4+4(ecx), eax		! put psw in sig_stuff
	ret


!*===========================================================================*
!*				check_mem				     *
!*===========================================================================*
! PUBLIC phys_bytes check_mem(phys_bytes base, phys_bytes size);
! Check a block of memory, return the amount valid.
! Only every 16th byte is checked.
! An initial size of 0 means everything.
! This really should do some alias checks.

CM_DENSITY	=	16
CM_LOG_DENSITY	=	4
CHKM_ARGS	=	4 + 4 + 4	! 4 + 4
!			ds ebx eip	base size

_check_mem:
	push	ebx
	push	ds
    o16	mov	ax, FLAT_DS_SELECTOR
	mov	ds, ax
	mov	eax, CHKM_ARGS(esp)
	mov	ebx, eax
	mov	ecx, CHKM_ARGS+4(esp)
	shr	ecx, CM_LOG_DENSITY
cm_loop:
	movb	dl, TEST1PATTERN
	xchgb	dl, (eax)		! write test pattern, remember original
	xchgb	dl, (eax)		! restore original, read test pattern
	cmpb	dl, TEST1PATTERN	! must agree if good real memory
	jnz	cm_exit			! if different, memory is unusable
	movb	dl, TEST2PATTERN
	xchgb	dl, (eax)
	xchgb	dl, (eax)
	add	eax, CM_DENSITY
	cmpb	dl, TEST2PATTERN
	loopz	cm_loop
cm_exit:
	sub	eax, ebx
	pop	ds
	pop	ebx
	ret


!*===========================================================================*
!*				cp_mess					     *
!*===========================================================================*
! PUBLIC void cp_mess(int src, phys_clicks src_clicks, vir_bytes src_offset,
!		      phys_clicks dst_clicks, vir_bytes dst_offset);
! This routine makes a fast copy of a message from anywhere in the address
! space to anywhere else.  It also copies the source address provided as a
! parameter to the call into the first word of the destination message.
!
! Note that the message size, `Msize` is in WORDS (not bytes) and must be set
! correctly.  Changing the definition of message in the type file and not
! changing it here will lead to total disaster.

CM_ARGS	=	4 + 4 + 4 + 4 + 4	! 4 + 4 + 4 + 4 + 4
!		es  ds edi esi eip 	proc scl sof dcl dof

	.align	16
_cp_mess:
	cld
	push	esi
	push	edi
	push	ds
	push	es

	mov	eax, FLAT_DS_SELECTOR
	mov	ds, ax
	mov	es, ax

	mov	esi, CM_ARGS+4(esp)		! src clicks
	shl	esi, CLICK_SHIFT
	add	esi, CM_ARGS+4+4(esp)		! src offset
	mov	edi, CM_ARGS+4+4+4(esp)		! dst clicks
	shl	edi, CLICK_SHIFT
	add	edi, CM_ARGS+4+4+4+4(esp)	! dst offset

	mov	eax, CM_ARGS(esp)	! process number of sender
	stos				! copy sender`s number to dest message
	add	esi, 4			! don`t copy first word
	mov	ecx, [Msize>>1] - 1	! Msize assumed even, dword count now
	rep
	movs				! copy the message

	pop	es
	pop	ds
	pop	edi
	pop	esi
	ret				! that`s all folks!


!*===========================================================================*
!*				exit					     *
!*===========================================================================*
! PUBLIC void exit();
! Some library routines use exit, so provide a dummy version.
! Actual calls to exit cannot occur in the kernel.
! GNU CC likes to call ___main from main() for nonobvious reasons.

_exit:
__exit:
___exit:
	sti
	jmp	___exit

___main:
	ret


!*===========================================================================*
!*				in_byte					     *
!*===========================================================================*
! PUBLIC unsigned in_byte(port_t port);
! Read an (unsigned) byte from the i/o port  port  and return it.

	.align	16
_in_byte:
	mov	edx, 4(esp)		! port
	sub	eax, eax
	inb	dx			! read 1 byte
	ret


!*===========================================================================*
!*				in_word					     *
!*===========================================================================*
! PUBLIC unsigned in_word(port_t port);
! Read an (unsigned) word from the i/o port  port  and return it.

	.align	16
_in_word:
	mov	edx, 4(esp)		! port
	sub	eax, eax
    o16	in	dx			! read 1 word
	ret


!*===========================================================================*
!*				lock					     *
!*===========================================================================*
! PUBLIC void lock();
! Disable CPU interrupts.

	.align	16
_lock:
	cli				! disable interrupts
	ret


!*===========================================================================*
!*				unlock					     *
!*===========================================================================*
! PUBLIC void unlock();
! Enable CPU interrupts.

	.align	16
_unlock:
	sti
	ret


!*==========================================================================*
!*				enable_irq				    *
!*==========================================================================*/
! PUBLIC void enable_irq(unsigned irq)
! Enable an interrupt request line by clearing an 8259 bit.
! Equivalent code for irq < 8:
!	out_byte(INT_CTLMASK, in_byte(INT_CTLMASK) & ~(1 << irq));

	.align	16
_enable_irq:
	mov	ecx, 4(esp)		! irq
	pushf
	cli
	movb	ah, ~1
	rolb	ah, cl			! ah = ~(1 << (irq % 8))
	cmpb	cl, 8
	jae	enable_8		! enable irq >= 8 at the slave 8259
enable_0:
	inb	INT_CTLMASK
	andb	al, ah
	outb	INT_CTLMASK		! clear bit at master 8259
	popf
	ret
	.align	4
enable_8:
	inb	INT2_CTLMASK
	andb	al, ah
	outb	INT2_CTLMASK		! clear bit at slave 8259
	popf
	ret


!*==========================================================================*
!*				disable_irq				    *
!*==========================================================================*/
! PUBLIC int disable_irq(unsigned irq)
! Disable an interrupt request line by setting an 8259 bit.
! Equivalent code for irq < 8:
!	out_byte(INT_CTLMASK, in_byte(INT_CTLMASK) | (1 << irq));
! Returns true iff the interrupt was not already disabled.

	.align	16
_disable_irq:
	mov	ecx, 4(esp)		! irq
	pushf
	cli
	movb	ah, 1
	rolb	ah, cl			! ah = (1 << (irq % 8))
	cmpb	cl, 8
	jae	disable_8		! disable irq >= 8 at the slave 8259
disable_0:
	inb	INT_CTLMASK
	testb	al, ah
	jnz	dis_already		! already disabled?
	orb	al, ah
	outb	INT_CTLMASK		! set bit at master 8259
	popf
	mov	eax, 1			! disabled by this function
	ret
disable_8:
	inb	INT2_CTLMASK
	testb	al, ah
	jnz	dis_already		! already disabled?
	orb	al, ah
	outb	INT2_CTLMASK		! set bit at slave 8259
	popf
	mov	eax, 1			! disabled by this function
	ret
dis_already:
	popf
	xor	eax, eax		! already disabled
	ret


!*===========================================================================*
!*				phys_copy				     *
!*===========================================================================*
! PUBLIC void phys_copy(phys_bytes source, phys_bytes destination,
!			phys_bytes bytecount);
! Copy a block of physical memory.

PC_ARGS	=	4 + 4 + 4 + 4 + 4	! 4 + 4 + 4
!		es  ds edi esi eip 	src dst len

	.align	16
_phys_copy:
	cld
	push	esi
	push	edi
	push	ds
	push	es

	mov	eax, FLAT_DS_SELECTOR
	mov	ds, ax
	mov	es, ax

	mov	esi, PC_ARGS(esp)
	mov	edi, PC_ARGS+4(esp)
	mov	eax, PC_ARGS+4+4(esp)

	cmp	eax, 10			! avoid align overhead for small counts
	jb	pc_small
	mov	ecx, esi		! align source, hope target is too
	neg	ecx
	and	ecx, 3			! count for alignment
	sub	eax, ecx
	rep
	movsb
	mov	ecx, eax
	shr	ecx, 2			! count of dwords
	rep
	movs
	and	eax, 3
pc_small:
	xchg	ecx, eax		! remainder
	rep
	movsb

	pop	es
	pop	ds
	pop	edi
	pop	esi
	ret



!*===========================================================================*
!*				phys_zero				     *
!*===========================================================================*
! PUBLIC void phys_zero(phys_bytes destination, phys_bytes bytecount);
! Zero a block of physical memory.

PZ_ARGS	=	4 + 4 + 4	! 4 + 4
!		es  edi eip 	dst len

	.align	16
_phys_zero:
	cld
	push	edi
	push	es

	mov	eax, FLAT_DS_SELECTOR
	mov	es, ax

	xor	eax, eax		! a zero

	mov	edi, PZ_ARGS(esp)
	mov	edx, PZ_ARGS+4(esp)

	cmp	edx, 10			! avoid align overhead for small counts
	jb	pz_small
	mov	ecx, edi		! align dest
	neg	ecx
	and	ecx, 3			! count for alignment
	sub	edx, ecx
	rep
	stosb
	mov	ecx, edx
	shr	ecx, 2			! count of dwords
	rep
	stos
	and	edx, 3
pz_small:
	xchg	ecx, edx		! remainder
	rep
	stosb

	pop	es
	pop	edi
	ret



!*===========================================================================*
!*				mem_rdw					     *
!*===========================================================================*
! PUBLIC u16_t mem_rdw(U16_t segment, u16_t *offset);
! Load and return word at far pointer segment:offset.

	.align	16
_mem_rdw:
	mov	cx, ds
	mov	ds, 4(esp)		! segment
	mov	eax, 4+4(esp)		! offset
	movzx	eax, (eax)		! byte to return
	mov	ds, cx
	ret

!*===========================================================================*
!*				out_byte				     *
!*===========================================================================*
! PUBLIC void out_byte(port_t port, u8_t value);
! Write  value  (cast to a byte)  to the I/O port  port.

	.align	16
_out_byte:
	mov	edx, 4(esp)		! port
	movzxb	eax, 4+4(esp)		! truncated value
	outb	dx			! output 1 byte
	ret

!*===========================================================================*
!*				out_word				     *
!*===========================================================================*
! PUBLIC void out_word(Port_t port, U16_t value);
! Write  value  (cast to a word)  to the I/O port  port.

	.align	16
_out_word:
	mov	edx, 4(esp)		! port
	movzx	eax, 4+4(esp)		! truncated value
    o16	out	dx			! output 1 word
	ret

!*===========================================================================*
!*				port_read				     *
!*===========================================================================*
! PUBLIC void port_read(port_t port, phys_bytes destination, unsigned bytcount);
! Transfer data from (hard disk controller) port to memory.

PR_ARGS	=	4 + 4 + 4		! 4 + 4 + 4
!		es edi eip		port dst len

	.align	16
_port_read:
	cld
	push	edi
	push	es
	mov	ecx, FLAT_DS_SELECTOR
	mov	es, cx
	mov	edx, PR_ARGS(esp)	! port to read from
	mov	edi, PR_ARGS+4(esp)	! destination addr
	mov	ecx, PR_ARGS+4+4(esp)	! byte count
	shr	ecx, 1			! word count
	rep				! (hardware can`t handle dwords)
    o16	ins				! read everything
	pop	es
	pop	edi
	ret


!*===========================================================================*
!*				port_read_byte				     *
!*===========================================================================*
! PUBLIC void port_read_byte(port_t port, phys_bytes destination,
!						unsigned bytcount);
! Transfer data from port to memory.

PR_ARGS_B =	4 + 4 + 4		! 4 + 4 + 4
!		es edi eip		port dst len

_port_read_byte:
	cld
	push	edi
	push	es
	mov	ecx, FLAT_DS_SELECTOR
	mov	es, cx
	mov	edx, PR_ARGS_B(esp)
	mov	edi, PR_ARGS_B+4(esp)
	mov	ecx, PR_ARGS_B+4+4(esp)
	rep	
	insb
	pop	es
	pop	edi
	ret


!*===========================================================================*
!*				port_write				     *
!*===========================================================================*
! PUBLIC void port_write(port_t port, phys_bytes source, unsigned bytcount);
! Transfer data from memory to (hard disk controller) port.

PW_ARGS	=	4 + 4 + 4		! 4 + 4 + 4
!		es edi eip		port src len

	.align	16
_port_write:
	cld
	push	esi
	push	ds
	mov	ecx, FLAT_DS_SELECTOR
	mov	ds, cx
	mov	edx, PW_ARGS(esp)	! port to write to
	mov	esi, PW_ARGS+4(esp)	! source addr
	mov	ecx, PW_ARGS+4+4(esp)	! byte count
	shr	ecx, 1			! word count
	rep				! (hardware can`t handle dwords)
    o16	outs				! write everything
	pop	ds
	pop	esi
	ret


!*===========================================================================*
!*				port_write_byte				     *
!*===========================================================================*
! PUBLIC void port_write_byte(port_t port, phys_bytes source,
!						unsigned bytcount);
! Transfer data from memory to port.

PW_ARGS_B =	4 + 4 + 4		! 4 + 4 + 4
!		es edi eip		port src len

_port_write_byte:
	cld
	push	esi
	push	ds
	mov	ecx, FLAT_DS_SELECTOR
	mov	ds, cx
	mov	edx, PW_ARGS_B(esp)
	mov	esi, PW_ARGS_B+4(esp)
	mov	ecx, PW_ARGS_B+4+4(esp)
	rep
	outsb
	pop	ds
	pop	esi
	ret


!*===========================================================================*
!*				reset					     *
!*===========================================================================*
! PUBLIC void reset();
! Reset the system by loading IDT with offset 0 and interrupting.

_reset:
	lidt	(idt_zero)
	int	3			! anything goes, the 386 won`t like it
.sect .data
idt_zero:	.data4	0, 0
.sect .text


!*===========================================================================*
!*				scr_down & scr_up			     *
!*===========================================================================*
! PUBLIC void scr_down(unsigned videoseg, int source, int dest, int count);
! Scroll the screen down one line.
!
! PUBLIC void scr_up(unsigned videoseg, int source, int dest, int count);
! Scroll the screen up one line.
!
! These are identical except scr_down() must reverse the direction flag
! during the copy to avoid problems with overlap.

SDU_ARGS	=	4 + 4 + 4 + 4 + 4	! 4 + 4 + 4 + 4
!			es  ds edi esi eip 	 seg src dst ct

_scr_down:
	std
_scr_up:
	push	esi
	push	edi
	push	ds
	push	es
	mov	eax, SDU_ARGS(esp)	! videoseg (selector for video ram)
	mov	esi, SDU_ARGS+4(esp)	! source (offset within video ram)
	mov	edi, SDU_ARGS+4+4(esp)	! dest (offset within video ram)
	mov	ecx, SDU_ARGS+4+4+4(esp) ! count (in words)
	mov	ds, ax			! set source and dest segs to videoseg
	mov	es, ax
	rep				! do the copy
    o16	movs
	pop	es
	pop	ds
	pop	edi
	pop	esi
	cld				! restore (unnecessarily for scr_up)
	ret


!*===========================================================================*
!*				test_and_set				     *
!*===========================================================================*
! PUBLIC int test_and_set(int *flag);
! Set the flag to TRUE, indivisibly with getting its old value.
! Return old flag.

	.align	16
_test_and_set:
	mov	ecx, 4(esp)
	mov	eax, 1
	xchg	eax, (ecx)
	ret


!*===========================================================================*
!*				vid_copy				     *
!*===========================================================================*
! PUBLIC void vid_copy(char *buffer, unsigned videobase, int offset,
!		       int words);
! where
!     `buffer`    is a pointer to the (character, attribute) pairs
!     `videobase` is 0xB800 for color and 0xB000 for monochrome displays
!     `offset`    tells where within video ram to copy the data
!     `words`     tells how many words to copy
! if buffer is zero, the fill char (blank_color) is used
!
! This routine takes a string of (character, attribute) pairs and writes them
! onto the screen.  For a snowy display, the writing only takes places during
! the vertical retrace interval, to avoid displaying garbage on the screen.

VC_ARGS	=	4 + 4 + 4 + 4 + 4	! 4 + 4 + 4 + 4
!		es edi esi ebx eip 	 buf bas off words

_vid_copy:
	push	ebx
	push	esi
	push	edi
	push	es
vid0:
	mov	esi, VC_ARGS(esp)	! buffer
	mov	es, VC_ARGS+4(esp)	! video_base
	mov	edi, VC_ARGS+4+4(esp)	! offset
	and	edi, (_vid_mask)	! only 4K or 16K counts
	mov	ecx, VC_ARGS+4+4+4(esp)	! word count for copy loop

	lea	ebx, -1(edi)(ecx*2)	! point at last char in target
	sub	ebx, (_vid_mask)	! # characters beyond end of video ram
	jle	vid1			! copy does not run off end of vram
	sar	ebx, 1			! # words that don`t fit
	sub	ecx, ebx		! reduce count by overrun

vid1:
	push	ecx			! save actual count used for later

! With a snowy (color, most CGA`s) display, you can avoid snow by only copying
! to video ram during vertical retrace.

	cmp	(_snow), 0
	jz	over_wait_for_retrace
	call	_wait_retrace

over_wait_for_retrace:
	test	esi, esi		! buffer == 0 means blank the screen
	je	vid7			! jump for blanking
	rep				! this is the copy loop
    o16	movs


vid5:
	pop	esi			! count of words copied
	test	ebx, ebx		! if no overrun, we are done
	jle	vc_exit			! everything fit

	mov	VC_ARGS+4+4+4(esp), ebx	! set up residual count
	mov	VC_ARGS+4+4(esp), 0	! start copying at base of video ram
	cmp	VC_ARGS(esp), 0		! NIL_PTR means store blanks
	je	vid0			! go do it
	add	esi, esi		! count of bytes copied
	add	VC_ARGS(esp), esi	! increment buffer pointer
	jmp	vid0			! go copy some more

vc_exit:
	pop	es
	pop	edi
	pop	esi
	pop	ebx
	ret

vid7:
	mov	eax, (_blank_color)	! ax = blanking character
	rep				! copy loop
    o16	stos				! blank screen
	jmp	vid5			! done


!*===========================================================================*
!*			      wait_retrace				     *
!*===========================================================================*
! PUBLIC void wait_retrace();
! Wait for the *start* of retrace period.
! The VERTICAL_RETRACE_MASK of the color vid_port is set during retrace.
! First wait until it is off (no retrace).
! Then wait until it comes on (start of retrace).
! We can`t afford to worry about interrupts.

_wait_retrace:
	mov	edx, (_vid_port)
	orb	dl, COLOR_STATUS_PORT & 0xFF
wait_no_retrace:
	inb	dx
	testb	al, VERTICAL_RETRACE_MASK
	jnz	wait_no_retrace
wait_retrace:
	inb	dx
	testb	al, VERTICAL_RETRACE_MASK
	jz	wait_retrace
	ret


!*===========================================================================*
!*			      level0					     *
!*===========================================================================*
! PUBLIC void level0(void (*func)(void))
! Call a function at permission level 0.  This allows kernel tasks to do
! things that are only possible at the most privileged CPU level.
!
_level0:
	mov	eax, 4(esp)
	mov	(_level0_func), eax
	int	LEVEL0_VECTOR
	ret
