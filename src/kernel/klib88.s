#
#include <minix/config.h>
#include <minix/const.h>
#include "const.h"
#include "sconst.h"
#include "protect.h"

! This file contains a number of assembly code utility routines needed by the
! kernel.  They are:

.define	_build_sig	! build 4 word structure pushed onto stack for signals
.define	_check_mem	! check a block of memory, return the valid size
.define	_cp_mess	! copies messages from source to destination
.define	_exit		! dummy for library routines
.define	__exit		! dummy for library routines
.define	___exit		! dummy for library routines
.define	.fat		! dummy for library routines
.define	_get_byte	! read a byte from anywhere and return it
.define	_in_byte	! read a byte from a port and return it
.define	_in_word	! read a word from a port and return it
.define	_klib_1hook	! init from real mode for real or protected mode
.define	_klib_2hook	! init from protected mode for real or protected mode
.define	_lock		! disable interrupts
.define	_unlock		! enable interrupts
.define	_enable_irq	! enable an irq at the 8259 controller
.define	_disable_irq	! disable an irq
.define	_mem_rdw	! copy one word from [segment:offset]
.define	_out_byte	! write a byte to a port
.define	_out_word	! write a word to a port
.define	_phys_copy	! copy data from anywhere to anywhere in memory
.define	_port_read	! transfer data from (disk controller) port to memory
.define	_port_write	! transfer data from memory to (disk controller) port
.define	_reset		! reset the system
.define	_scr_down	! scroll screen a line down (in software, by copying)
.define	_scr_up		! scroll screen a line up (in software, by copying)
.define	_test_and_set	! test and set locking primitive on a word in memory
.define	.trp		! dummy for library routines
.define	_vid_copy	! copy data to video ram (perhaps during retrace only)
.define	_wait_retrace	! wait for retrace interval
.define	_level0		! call a function at level 0

! The routines only guarantee to preserve the registers the 'C' compiler
! expects to be preserved (si, di, bp, sp, segment registers, and direction
! bit in the flags), though some of the older ones preserve bx, cx and dx.

#define DS_286_OFFSET	DS_286_INDEX*DESC_SIZE
#define ES_286_OFFSET	ES_286_INDEX*DESC_SIZE
#define EM_MASK		0xFFF0	/* extended memory mask for hi word */
#define EM_XFER_VEC	0x15	/* copy (normal or extended) memory */
#	define EM_XFER_FUNC	0x87
#define JMP_OPCODE	0xE9	/* opcode used for patching */
#define OFF_MASK	0x000F	/* offset mask for phys_b -> hclick:offset */

! imported functions

.extern	p_restart
.extern	p_save
.extern	_restart
.extern	save

! imported variables

.extern	_Ax, _Bx, _Cx, _Dx, _Es
.extern	_blank_color
.extern	_gdt
.extern	_protected_mode
.extern	_snow
.extern	_vec_table
.extern	_vid_mask
.extern	_vid_port
.extern	_level0_func

	.text
#if ENABLE_BIOS_WINI
!*===========================================================================*
!*				bios13					     *
!*===========================================================================*
! PUBLIC void bios13();
.define _bios13
_bios13:			! make a BIOS 0x13 call for disk I/O
	push ax			! save the registers
	push bx
	push cx
	push dx
	push es
	mov ax,_Ax		! load parameters
	mov bx,_Bx
	mov cx,_Cx
	mov dx,_Dx
	mov es,_Es
	int 0x13		! make the BIOS call
	mov _Ax,ax		! save results
	mov _Bx,bx
	mov _Cx,cx
	mov _Dx,dx
	pop es
	pop dx
	pop cx
	pop bx
	pop ax
	ret

#if NO_GOOD_YET
! BIOS call with real mode switch, crashes your system reliably!
p_bios13:
	pusha				! save the world
	pushf
	cli				! no time for interrupts
	inb	INT_CTLMASK
	push	ax			! save interrupt mask
	notb	al			! enable all disabled IRQ`s and vv.
	outb	INT_CTLMASK
	inb	INT2_CTLMASK		! same for slave controller
	push	ax
	notb	al
	outb	INT2_CTLMASK

! Shouldn`t one load 64 kb segment descriptors?  Where is the 386 book
! when you need it.
	mov	ax, _code_base+0
	mov	dx, _code_base+2	! phys address of kernel text
	mov	cx, #16
	div	cx
	push	ax			! code segment
	push	#csinit1		! code offset

	.data1	0x0F, 0x20, 0xC0   ! mov eax, cr0
	andb	al, #0xFE		! Clear the P(rotect)E(nable) bit
	.data1	0x0F, 0x22, 0xC0   ! mov cr0, eax

	retf				! set cs
csinit1:
  cseg	mov	ax, kernel_ds		! Reload all segment registers
	mov	ds, ax
	mov	es, ax
	.data1	0x8E, 0xE0	   ! mov fs, ax
	.data1	0x8E, 0xE8	   ! mov gs, ax
	mov	ss, ax

	lidt	idt_vectors		! Real mode interrupt vectors

	! Everything back to "normal".  We can now make BIOS calls, unless
	! the BIOS can`t live with the A20 line enabled.  In that case one
	! has to run Minix in real mode.

	mov	ax, _Ax			! load parameters
	mov	bx, _Bx
	mov	cx, _Cx
	mov	dx, _Dx
	mov	es, _Es
!	sti
!	int	0x13			! make the BIOS call
!	cli
movb	ah, #1
	mov	_Ax, ax			! save results
	mov	_Bx, bx
	mov	_Cx, cx
	mov	_Dx, dx

	! The BIOS call has been made, step back up to protected mode.

	.data1	0x0F, 0x20, 0xC0   ! mov eax, cr0
	or	ax, #0x0001		! Set the PE bit
	.data1	0x0F, 0x22, 0xC0   ! mov cr0, eax

	lgdt	_gdt+BIOS_GDT_SELECTOR
	lidt    _gdt+BIOS_IDT_SELECTOR

	jmpf	csinit2, CS_SELECTOR
csinit2:
	mov	ax, #DS_SELECTOR
	mov	ds, ax
	mov	es, ax
	.data1	0x8E, 0xE0	   ! mov fs, ax
	.data1	0x8E, 0xE8	   ! mov gs, ax
	mov	ss, ax

	pop	ax			! restore the world
	outb	INT2_CTLMASK
	pop	ax
	outb	INT_CTLMASK
	popf
	popa
	ret
#endif
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
	push bp			! save bp
	mov bp,sp		! set bp to sp for accessing params
	push bx			! save bx
	push si			! save si
	mov bx,4(bp)		! bx points to sig_stuff
	mov si,6(bp)		! si points to proc table entry
	mov ax,8(bp)		! ax = signal number
	mov (bx),ax		! put signal number in sig_stuff
	mov ax,PCREG(si)	! ax = signalled process' PC
	mov 2(bx),ax		! put pc in sig_stuff
	mov ax,CSREG(si)	! ax = signalled process' cs
	mov 4(bx),ax		! put cs in sig_stuff
	mov ax,PSWREG(si)	! ax = signalled process' PSW
	mov 6(bx),ax		! put psw in sig_stuff
	pop si			! restore si
	pop bx			! restore bx
	pop bp			! restore bp
	ret			! return to caller


!*===========================================================================*
!*				check_mem				     *
!*===========================================================================*
! PUBLIC phys_bytes check_mem(phys_bytes base, phys_bytes size);
! Check a block of memory, return the amount valid.
! Only every 16th byte is checked.
! This only works in protected mode.
! An initial size of 0 means everything.
! This really should do some alias checks.

PCM_DENSITY	=	256	! resolution of check
				! the shift logic depends on this being 256
_check_mem:
	pop	bx
	pop	_gdt+DS_286_OFFSET+DESC_BASE
	pop	ax		! pop base into base of source descriptor
	movb	_gdt+DS_286_OFFSET+DESC_BASE_MIDDLE,al
	pop	cx		! byte count in dx:cx
	pop	dx
	sub	sp,#4+4
	push	bx
	push	ds

	sub	ax,ax		! prepare for early exit
	test	dx,#0xFF00
	jnz	cm_1exit	! can't handle bases above 16M
	movb	cl,ch		! divide size by 256 and discard high byte
	movb	ch,dl
	push	cx		! save divided size
	sub	bx,bx		! test bytes at bases of segments
cm_loop:
	mov	ax,#DS_286_SELECTOR
	mov	ds,ax
	movb	dl,#TEST1PATTERN
	xchgb	dl,(bx)		! write test pattern, remember original value
	xchgb	dl,(bx)		! restore original value, read test pattern
	cmpb	dl,#TEST1PATTERN	! must agree if good real memory
	jnz	cm_exit		! if different, memory is unusable
	movb	dl,#TEST2PATTERN
	xchgb	dl,(bx)
	xchgb	dl,(bx)
	cmpb	dl,#TEST2PATTERN
	jnz	cm_exit
	eseg			! next segement, test for wraparound at 16M
	add	_gdt+DS_286_OFFSET+DESC_BASE,#PCM_DENSITY
	eseg			! assuming es == old ds
	adcb	_gdt+DS_286_OFFSET+DESC_BASE_MIDDLE,#0
	loopnz	cm_loop

cm_exit:
	pop	ax
	sub	ax,cx		! verified size in multiples of PCM_DENSITY
cm_1exit:
	movb	dl,ah		! convert to phys_bytes in dx:ax
	subb	dh,dh
	movb	ah,al
	movb	al,dh
	pop	ds
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
! Note that the message size, 'Msize' is in WORDS (not bytes) and must be set
! correctly.  Changing the definition of message in the type file and not
! changing it here will lead to total disaster.

_cp_mess:
	cld
	push es			! save es
	push ds			! save ds
	mov bx,sp		! index off bx because machine can't use sp
	push si			! save si
	push di			! save di

	mov	ax,12(bx)	! destination click
#if HCLICK_SHIFT > CLICK_SHIFT
#error /* Small click sizes are not supported (right shift will lose bits). */
#endif
#if HCLICK_SHIFT < CLICK_SHIFT
	movb	cl,#CLICK_SHIFT-HCLICK_SHIFT
	shl	ax,cl		! destination segment
#endif
	mov	es,ax
	mov	di,14(bx)	! offset of destination message

! Be careful not to destroy ds before we're finished with the bx pointer.
! We're using bx and not the more natural bp to save pushing bp.

	mov	ax,6(bx)	! process number of sender
	mov	si,10(bx)	! offset of source message
	mov	bx,8(bx)	! source click (finished with bx as a pointer)
#if HCLICK_SHIFT < CLICK_SHIFT
	shl	bx,cl		! source segment
#endif
	mov	ds,bx

	stos			! copy sender's process number to dest message
	add si,*2		! don't copy first word
	mov cx,*Msize-1		! remember, first word doesn't count
	rep			! iterate cx times to copy 11 words
	movs			! copy the message
	pop di			! restore di
	pop si			! restore si
	pop ds			! restore ds
	pop es			! restore es	
	ret			! that's all folks!


!*===========================================================================*
!*				exit					     *
!*===========================================================================*
! PUBLIC void exit();
! Some library routines use exit, so provide a dummy version.
! Actual calls to exit cannot occur in the kernel.
! Same for .fat
! Same for .trp.

_exit:
__exit:
___exit:
.fat:
.trp:
	sti
	jmp __exit

!*===========================================================================*
!*				get_byte				     *
!*===========================================================================*
! PUBLIC u16_t get_byte(u16_t segment, u8_t *offset);
! Load and return the byte at the far pointer  segment:offset.

_get_byte:
	mov	cx,ds		! save ds
	pop	dx		! return adr
	pop	ds		! segment
	pop	bx		! offset
	sub	sp,#2+2		! adjust for parameters popped
	movb	al,(bx)		! load the byte to return
	xorb	ah,ah		! extend the u8_t to a u16_t
	mov	ds,cx		! restore ds
	jmp	(dx)		! return


!*===========================================================================*
!*				in_byte					     *
!*===========================================================================*
! PUBLIC unsigned in_byte(port_t port);
! Read an (unsigned) byte from the i/o port  port  and return it.

_in_byte:
	pop	bx
	pop	dx		! port
	dec	sp
	dec	sp
	inb			! input 1 byte
	subb	ah,ah		! unsign extend
	jmp	(bx)
!*===========================================================================*
!*				in_word					     *
!*===========================================================================*
! PUBLIC unsigned short in_word(port_t port);
! Read an (unsigned) word from the i/o port and return it.

_in_word:
	pop	bx
	pop	dx		! port
	dec	sp
	dec	sp		! added to new klib.x 3/21/91 d.e.c.
	inw			! input 1 word no sign extend needed
	jmp	(bx)


!*===========================================================================*
!*				klib_1hook				     *
!*===========================================================================*
! PUBLIC void klib_1hook();
! Initialize klib from real mode for real or protected mode,
! Do nothing for real mode.
! For protected mode, patch some real mode functions at their starts to jump
! to their protected mode equivalents, according to the patch table.

_klib_1hook:
	cmpb	_protected_mode,#0
	jz	hook1_done
	mov	si,#patch_table
	lods			! original function
patch1:
	mov	bx,ax
	cseg
	movb	(bx),#JMP_OPCODE
	lods			! new function - target of jump
	sub	ax,bx		! relative jump
	sub	ax,#3		! adjust by length of jump instruction
	cseg
	mov	1(bx),ax
	lods			! next original function
	test	ax,ax
	jnz	patch1
hook1_done:
	ret


!*===========================================================================*
!*				klib_2hook				     *
!*===========================================================================*
! PUBLIC void klib_prot_mode_init();
! Initialize klib protected real mode for real or protected mode,
! Do nothing for real mode.
! For protected mode, load idt, task reg and flags.

_klib_2hook:
	cmpb	_protected_mode,#0
	jz	hook2_done
	lidt	_gdt+BIOS_IDT_SELECTOR	! loaded by BIOS, but in wrong mode!
	mov	ax,#TSS_SELECTOR	! no other TSS is used
	ltr	ax
	sub	ax,ax		! zero
	push	ax		! set flags to known good state
	popf			! especially, clear nested task and int enable
hook2_done:
	ret


!*===========================================================================*
!*				lock					     *
!*===========================================================================*
! PUBLIC void lock();
! Disable CPU interrupts.

_lock:
	cli				! disable interrupts
	ret


!*===========================================================================*
!*				unlock					     *
!*===========================================================================*
! PUBLIC void unlock();
! Enable CPU interrupts.

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

_enable_irq:
	mov	bx, sp
	mov	cx, 2(bx)		! irq
	pushf
	cli
	movb	ah, #~1
	rolb	ah, cl			! ah = ~(1 << (irq % 8))
	cmpb	cl, #8
	jae	enable_8		! enable irq >= 8 at the slave 8259
enable_0:
	inb	INT_CTLMASK
	andb	al, ah
	outb	INT_CTLMASK		! clear bit at master 8259
	popf
	ret
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

_disable_irq:
	mov	bx, sp
	mov	cx, 2(bx)		! irq
	pushf
	cli
	movb	ah, #1
	rolb	ah, cl			! ah = (1 << (irq % 8))
	cmpb	cl, #8
	jae	disable_8		! disable irq >= 8 at the slave 8259
disable_0:
	inb	INT_CTLMASK
	testb	al, ah
	jnz	dis_already		! already disabled?
	orb	al, ah
	outb	INT_CTLMASK		! set bit at master 8259
	popf
	mov	ax, #1			! disabled by this function
	ret
disable_8:
	inb	INT2_CTLMASK
	testb	al, ah
	jnz	dis_already		! already disabled?
	orb	al, ah
	outb	INT2_CTLMASK		! set bit at slave 8259
	popf
	mov	ax, #1			! disabled by this function
	ret
dis_already:
	popf
	xor	ax, ax			! already disabled
	ret


!*===========================================================================*
!*				phys_copy				     *
!*===========================================================================*
! PUBLIC void phys_copy(phys_bytes source, phys_bytes destination,
!			phys_bytes bytecount);
! Copy a block of physical memory.

SRCLO	=	4
SRCHI	=	6
DESTLO	=	8
DESTHI	=	10
COUNTLO	=	12
COUNTHI	=	14

_phys_copy:
	push	bp		! save only registers required by C
	mov	bp,sp		! set bp to point to source arg less 4

! check for extended memory

	mov	ax,SRCHI(bp)
	or	ax,DESTHI(bp)
	test	ax,#EM_MASK
	jnz	to_em_xfer

	push	si		! save si
	push	di		! save di
	push	ds		! save ds
	push	es		! save es

	mov	ax,SRCLO(bp)	! dx:ax = source address (dx is NOT segment)
	mov	dx,SRCHI(bp)
	mov	si,ax		! si = source offset = address % 16
	and	si,#OFF_MASK
!	andb	dl,#HCHIGH_MASK	| ds = source segment = address / 16 % 0x10000
				! mask is unnecessary because of EM_MASK test
	andb	al,#HCLOW_MASK
	orb	al,dl		! now bottom 4 bits of dx are in ax
	movb	cl,#HCLICK_SHIFT ! rotate them to the top 4
	ror	ax,cl
	mov	ds,ax

	mov	ax,DESTLO(bp)	! dx:ax = destination addr (dx is NOT segment)
	mov	dx,DESTHI(bp)
	mov	di,ax		! di = dest offset = address % 16
	and	di,#OFF_MASK
!	andb	dl,#HCHIGH_MASK	| es = dest segment = address / 16 % 0x10000
	andb	al,#HCLOW_MASK
	orb	al,dl
	ror	ax,cl
	mov	es,ax

	mov	ax,COUNTLO(bp)	! dx:ax = remaining count
	mov	dx,COUNTHI(bp)

! copy upwards (can't handle overlapped copy)

pc_loop:
	mov	cx,ax		! provisional count for this iteration
	test	ax,ax		! if count >= 0x8000, only do 0x8000 per iter
	js	pc_bigcount	! low byte already >= 0x8000
	test	dx,dx
	jz	pc_upcount	! less than 0x8000
pc_bigcount:
	mov	cx,#0x8000	! use maximum count per iteration
pc_upcount:
	sub	ax,cx		! update count
	sbb	dx,#0		! can't underflow, so carry clear now for rcr
	rcr	cx,#1		! count in words, carry remembers if byte
	jnb	pc_even		! no odd byte
	movb			! copy odd byte
pc_even:
	rep			! copy 1 word at a time
	movs			! word copy

	mov	cx,ax		! test if remaining count is 0
	or	cx,dx
	jnz	pc_more		! more to do

	pop	es		! restore es
	pop	ds		! restore ds
	pop	di		! restore di
	pop	si		! restore si
	pop	bp		! restore bp
	ret			! return to caller

pc_more:
	sub	si,#0x8000	! adjust pointers so the offset doesn't
	mov	cx,ds		! overflow in the next 0x8000 bytes
	add	cx,#0x800	! pointers end up same physical location
	mov	ds,cx		! the current offsets are known >= 0x8000
	sub	di,#0x8000	! since we just copied that many
	mov	cx,es
	add	cx,#0x800
	mov	es,cx
	jmp	pc_loop		! start next iteration

! When source or target is above 1M, join em_xfer.
! Rely on MM and MEMTASK not to provide bad parameters, and omit checking the
! following:
!	count must be even
!	count must be < 64K
!	machine must be AT-compatible
! which are not required for phys_copy.

to_em_xfer:
	shr	COUNTLO(bp),#1	! convert count to words
	pop	bp		! stack frame now agrees with em_xfer's
	jmp	_em_xfer


!*===========================================================================*
!*				em_xfer					     *
!*===========================================================================*
!  This file contains one routine which transfers words between user memory
!  and extended memory on an AT or clone.  A BIOS call (INT 15h, Func 87h)
!  is used to accomplish the transfer.  The BIOS call is "faked" by pushing
!  the processor flags on the stack and then doing a far call through the
!  saved vector table to the actual BIOS location.  An actual INT 15h would
!  get a MINIX complaint from an unexpected trap.

!  This particular BIOS routine runs with interrupts off since the 80286
!  must be placed in protected mode to access the memory above 1 Mbyte.
!  So there should be no problems using the BIOS call, except it may take
!  too long and cause interrupts to be lost.
!
	.text
gdt:				! Begin global descriptor table
					! Dummy descriptor
	.data2 0		! segment length (limit)
	.data2 0		! bits 15-0 of physical address
	.data1 0		! bits 23-16 of physical address
	.data1 0		! access rights byte
	.data2 0		! reserved
					! descriptor for GDT itself
	.data2 0		! segment length (limit)
	.data2 0		! bits 15-0 of physical address
	.data1 0		! bits 23-16 of physical address
	.data1 0		! access rights byte
	.data2 0		! reserved
src:					! source descriptor
srcsz:	.data2 0		! segment length (limit)
srcl:	.data2 0		! bits 15-0 of physical address
srch:	.data1 0		! bits 23-16 of physical address
	.data1 0x93	! access rights byte
	.data2 0		! reserved
tgt:					! target descriptor
tgtsz:	.data2 0		! segment length (limit)
tgtl:	.data2 0		! bits 15-0 of physical address
tgth:	.data1 0		! bits 23-16 of physical address
	.data1 0x93	! access rights byte
	.data2 0		! reserved
					! BIOS CS descriptor
	.data2 0		! segment length (limit)
	.data2 0		! bits 15-0 of physical address
	.data1 0		! bits 23-16 of physical address
	.data1 0		! access rights byte
	.data2 0		! reserved
					! stack segment descriptor
	.data2 0		! segment length (limit)
	.data2 0		! bits 15-0 of physical address
	.data1 0		! bits 23-16 of physical address
	.data1 0		! access rights byte
	.data2 0		! reserved

!
!
!  Execute a transfer between user memory and extended memory.
!
!  status = em_xfer(source, dest, count);
!
!    Where:
!       status => return code (0 => OK)
!       source => Physical source address (32-bit)
!       dest   => Physical destination address (32-bit)
!       count  => Number of words to transfer
!
!
!
_em_xfer:
	push	bp		! Save registers
	mov	bp,sp
	push	si
	push	es
	push	cx
!
!  Pick up source and destination addresses and update descriptor tables
!
	mov ax,4(bp)
	cseg
	mov srcl,ax
	mov ax,6(bp)
	cseg
	movb srch,al
	mov ax,8(bp)
	cseg
	mov tgtl,ax
	mov ax,10(bp)
	cseg
	movb tgth,al
!
!  Update descriptor table segment limits
!
	mov cx,12(bp)
	mov ax,cx
	add ax,ax
	cseg
	mov tgtsz,ax
	cseg
	mov srcsz,ax

!
!  Now do actual DOS call
!
	push cs
	pop es
	mov si,#gdt
	movb ah,#EM_XFER_FUNC
	pushf			! fake interrupt
	callf	@_vec_table+4*EM_XFER_VEC

!
!  All done, return to caller.
!

	pop	cx		! restore registers
	pop	es
	pop	si
	mov	sp,bp
	pop	bp
	ret


!*===========================================================================*
!*				mem_rdw					     *
!*===========================================================================*
! PUBLIC u16_t mem_rdw(u16_t segment, u16_t *offset);
! Load and return the word at the far pointer  segment:offset.

_mem_rdw:
	mov	cx,ds		! save ds
	pop	dx		! return adr
	pop	ds		! segment
	pop	bx		! offset
	sub	sp,#2+2		! adjust for parameters popped
	mov	ax,(bx)		! load the word to return
	mov	ds,cx		! restore ds
	jmp	(dx)		! return


!*===========================================================================*
!*				out_byte				     *
!*===========================================================================*
! PUBLIC void out_byte(port_t port, int value);
! Write  value  (cast to a byte)  to the I/O port  port.

_out_byte:
	pop	bx
	pop	dx		! port
	pop	ax		! value
	sub	sp,#2+2
	outb			! output 1 byte
	jmp	(bx)


!*===========================================================================*
!*				out_word				     *
!*===========================================================================*
! PUBLIC void out_word(port_t port, int value);
! Write  value  (cast to a word)  to the I/O port  port.

_out_word:
	pop	bx
	pop	dx		! port
	pop	ax		! value
	sub	sp,#2+2
	outw			! output 1 word
	jmp	(bx)


!*===========================================================================*
!*				port_read				     *
!*===========================================================================*
! PUBLIC void port_read(port_t port, phys_bytes destination,unsigned bytcount);
! Transfer data from (hard disk controller) port to memory.

_port_read:
	cld
	push	bp
	mov	bp,sp
	push	cx
	push	dx
	push	di
	push	es
	mov	ax,4+2(bp)	! destination addr in dx:ax
	mov	dx,4+2+2(bp)
	mov	di,ax		! di = dest offset = address % 16
	and	di,#OFF_MASK
	andb	dl,#HCHIGH_MASK	! es = dest segment = address / 16 % 0x10000
	andb	al,#HCLOW_MASK
	orb	al,dl
	movb	cl,#HCLICK_SHIFT
	ror	ax,cl
	mov	es,ax

	mov	cx,4+2+4(bp)	! count in bytes
	shr	cx,#1		! count in words
	mov	dx,4(bp)	! port to read from
	rep
	ins
	pop	es
	pop	di
	pop	dx
	pop	cx
	mov	sp,bp
	pop	bp
	ret


!*===========================================================================*
!*				port_write				     *
!*===========================================================================*
! PUBLIC void port_write(port_t port, phys_bytes source, unsigned bytcount);
! Transfer data from memory to (hard disk controller) port.

_port_write:
	cld
	push	bp
	mov	bp,sp
	push	cx
	push	dx
	push	si
	push	ds
	mov	ax,4+2(bp)	! source addr in dx:ax
	mov	dx,4+2+2(bp)
	mov	si,ax		! si = source offset = address % 16
	and	si,#OFF_MASK
	andb	dl,#HCHIGH_MASK	! ds = source segment = address / 16 % 0x10000
	andb	al,#HCLOW_MASK
	orb	al,dl
	movb	cl,#HCLICK_SHIFT
	ror	ax,cl
	mov	ds,ax
	mov	cx,4+2+4(bp)	! count in bytes
	shr	cx,#1		! count in words
	mov	dx,4(bp)	! port to read from
	rep
	outs
	pop	ds
	pop	si
	pop	dx
	pop	cx
	mov	sp,bp
	pop	bp
	ret


!*===========================================================================*
!*				reset					     *
!*===========================================================================*
! PUBLIC void reset();
! Reset the system.
! In real mode we simply jump to the reset address.

_reset:
	jmpf	0,0xFFFF


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

_scr_down:
	std
_scr_up:
	push	bp
	mov	bp,sp
	push	si
	push	di
	push	ds
	push	es
	mov	ax,4(bp)	! videoseg (selector for video ram)
	mov	si,6(bp)	! source (offset within video ram)
	mov	di,8(bp)	! dest (offset within video ram)
	mov	cx,10(bp)	! count (in words)
	mov	ds,ax		! set source and dest segs to videoseg
	mov	es,ax
	rep			! do the copy
	movs
	pop	es
	pop	ds
	pop	di
	pop	si
	pop	bp
	cld			! restore (unnecessarily for scr_up)
	ret


!*===========================================================================*
!*				test_and_set				     *
!*===========================================================================*
! PUBLIC int test_and_set(int *flag);
! Set the flag to TRUE, indivisibly with getting its old value.
! Return old flag.

_test_and_set:
	pop	dx
	pop	bx
	sub	sp,#2
	mov	ax,#1
	xchg	ax,(bx)
	jmp	(dx)


!*===========================================================================*
!*				vid_copy				     *
!*===========================================================================*
! PUBLIC void vid_copy(char *buffer, unsigned videobase, int offset,
!		       int words);
! where
!     'buffer'    is a pointer to the (character, attribute) pairs
!     'videobase' is 0xB800 for color and 0xB000 for monochrome displays
!     'offset'    tells where within video ram to copy the data
!     'words'     tells how many words to copy
! if buffer is zero, the fill char (blank_color) is used
!
! This routine takes a string of (character, attribute) pairs and writes them
! onto the screen.  For a snowy display, the writing only takes places during
! the vertical retrace interval, to avoid displaying garbage on the screen.

_vid_copy:
	push bp			! we need bp to access the parameters
	mov bp,sp		! set bp to sp for indexing
	push si			! save the registers
	push di			! save di
	push bx			! save bx
	push cx			! save cx
	push dx			! save dx
	push es			! save es
vid.0:	mov si,4(bp)		! si = pointer to data to be copied
	mov es,6(bp)		! load es NOW: int routines may NOT ruin it
	mov di,8(bp)		! di = offset within video ram
	and di,_vid_mask	! only 4K or 16K counts
	mov cx,10(bp)		! cx = word count for copy loop
	mov dx,#0x3DA		! prepare to see if color display is retracing

	mov bx,di		! see if copy will run off end of video ram
	add bx,cx		! compute where copy ends
	add bx,cx		! bx = last character copied + 1
	sub bx,_vid_mask	! bx = # characters beyond end of video ram
	sub bx,#1		! note: dec bx doesn't set flags properly
				! it DOES for jle!!
	jle vid.1		! jump if no overrun
	sar bx,#1		! bx = # words that don't fit in video ram
	sub cx,bx		! reduce count by overrun

vid.1:	push cx			! save actual count used for later
	cmpb _snow,#0		! skip vertical retrace test if no snow
	jz vid.4

!vid.2:	in			| with a color display, you can only copy to
!	test al,*010		| the video ram during vertical retrace, so
!	jnz vid.2		| wait for start of retrace period.  Bit 3 of
vid.3:	in			! 0x3DA is set during retrace.  First wait
	testb al,*010		! until it is off (no retrace), then wait
	jz vid.3		! until it comes on (start of retrace)

vid.4:	cmp si,#0		! si = 0 means blank the screen
	je vid.7		! jump for blanking
	lock			! this is a trick for the IBM PC simulator only
	inc vidlock		! 'lock' indicates a video ram access
	rep			! this is the copy loop
	movs			! ditto

vid.5:	pop si			! si = count of words copied
	cmp bx,#0		! if bx < 0, then no overrun and we are done
	jle vid.6		! jump if everything fit
	mov 10(bp),bx		! set up residual count
	mov 8(bp),#0		! start copying at base of video ram
	cmp 4(bp),#0		! NIL_PTR means store blanks
	je vid.0		! go do it
	add si,si		! si = count of bytes copied
	add 4(bp),si		! increment buffer pointer
	jmp vid.0		! go copy some more

vid.6:	pop es			! restore registers
	pop dx			! restore dx
	pop cx			! restore cx
	pop bx			! restore bx
	pop di			! restore di
	pop si			! restore si
	pop bp			! restore bp
	ret			! return to caller

vid.7:	mov ax,_blank_color	! ax = blanking character
	rep			! copy loop
	stos			! blank screen
	jmp vid.5		! done


!*===========================================================================*
!*			      wait_retrace				     *
!*===========================================================================*
! PUBLIC void wait_retrace();
! Wait for the *start* of retrace period.
! The VERTICAL_RETRACE_MASK of the color vid_port is set during retrace.
! First wait until it is off (no retrace).
! Then wait until it comes on (start of retrace).
! We can't afford to worry about interrupts.

_wait_retrace:
	mov	dx,_vid_port
	orb	dl,#COLOR_STATUS_PORT & 0xFF
wait_no_retrace:
	inb
	testb	al,#VERTICAL_RETRACE_MASK
	jnz	wait_no_retrace
wait_retrace:
	inb
	testb	al,#VERTICAL_RETRACE_MASK
	jz	wait_retrace
	ret


!*===========================================================================*
!*			      level0					     *
!*===========================================================================*
! PUBLIC void level0(void (*func)(void))
! Not very interesting in real mode, see p_level0.
!
_level0:
	mov	bx, sp
	jmp	@2(bx)


!*===========================================================================*
!*			variants for protected mode			     *
!*===========================================================================*
! Some routines are different in protected mode.
! The only essential difference is the handling of segment registers.
! One complication is that the method of building segment descriptors is not
! reentrant, so the protected mode versions must not be called by interrupt
! handlers.

!*===========================================================================*
!*				p_cp_mess				     *
!*===========================================================================*
! The real mode version attempts to be efficient by passing raw segments but
! that just gets in the way here.

p_cp_mess:
	cld
	pop	dx
	pop	bx		! proc
	pop	cx		! source clicks
	pop	ax		! source offset
#if CLICK_SHIFT != HCLICK_SHIFT + 4
#error /* the only click size supported is 256, to avoid slow shifts here */
#endif
	addb	ah,cl		! calculate source offset
	adcb	ch,#0 		! and put in base of source descriptor
	mov	_gdt+DS_286_OFFSET+DESC_BASE,ax
	movb	_gdt+DS_286_OFFSET+DESC_BASE_MIDDLE,ch
	pop	cx		! destination clicks
	pop	ax		! destination offset
	addb	ah,cl		! calculate destination offset
	adcb	ch,#0 		! and put in base of destination descriptor
	mov	_gdt+ES_286_OFFSET+DESC_BASE,ax
	movb	_gdt+ES_286_OFFSET+DESC_BASE_MIDDLE,ch
	sub	sp,#2+2+2+2+2

	push	ds
	push	es
	mov	ax,#DS_286_SELECTOR
	mov	ds,ax
	mov	ax,#ES_286_SELECTOR
	mov	es,ax

	eseg
	mov	0,bx		! sender's proc no. from arg, not msg
	mov	ax,si
	mov	bx,di
	mov	si,#2		! src offset is now 2 relative to start of seg
	mov	di,si		! and destination offset
	mov	cx,#Msize-1	! word count
	rep
	movs
	mov	di,bx
	mov	si,ax
	pop	es
	pop	ds
	jmp	(dx)


!*===========================================================================*
!*				p_phys_copy				     *
!*===========================================================================*
p_phys_copy:
	cld
	pop	dx
	pop	_gdt+DS_286_OFFSET+DESC_BASE
	pop	ax		! pop source into base of source descriptor
	movb	_gdt+DS_286_OFFSET+DESC_BASE_MIDDLE,al
	pop	_gdt+ES_286_OFFSET+DESC_BASE
	pop	ax		! pop destination into base of dst descriptor
	movb	_gdt+ES_286_OFFSET+DESC_BASE_MIDDLE,al
	pop	cx		! byte count in bx:cx
	pop	bx
	sub	sp,#4+4+4

	push	di
	push	si
	push	es
	push	ds
	sub	si,si		! src offset is now 0 relative to start of seg
	mov	di,si		! and destination offset
	jmp	ppc_next

! It is too much trouble to align the segment bases, so word alignment is hard.
! Avoiding the book-keeping for alignment may be good anyway.

ppc_large:
	push	cx
	mov	cx,#0x8000	! copy a large chunk of this many words
	rep
	movs
	pop	cx
	dec	bx
	pop	ds		! update the descriptors
	incb	_gdt+DS_286_OFFSET+DESC_BASE_MIDDLE
	incb	_gdt+ES_286_OFFSET+DESC_BASE_MIDDLE
	push	ds
ppc_next:
	mov	ax,#DS_286_SELECTOR	! (re)load the selectors
	mov	ds,ax
	mov	ax,#ES_286_SELECTOR
	mov	es,ax
	test	bx,bx
	jnz	ppc_large

	shr	cx,#1		! word count
	rep
	movs			! move any leftover words
	rcl	cx,#1		! restore old bit 0
	rep
	movb			! move any leftover byte
	pop	ds
	pop	es
	pop	si
	pop	di
	jmp	(dx)


!*===========================================================================*
!*				p_port_read				     *
!*===========================================================================*
p_port_read:
	cld
	pop	bx
	pop	dx		! port
	pop	_gdt+ES_286_OFFSET+DESC_BASE
	pop	ax		! pop destination into base of dst descriptor
	movb	_gdt+ES_286_OFFSET+DESC_BASE_MIDDLE,al
	pop	cx		! byte count
	sub	sp,#2+4+2
	push	es
	mov	ax,#ES_286_SELECTOR
	mov	es,ax
	mov	ax,di
	sub	di,di		! dst offset is now 0 relative to start of seg
	shr	cx,#1		! word count
	rep
	ins			! read everything
	mov	di,ax
	pop	es
	jmp	(bx)


!*===========================================================================*
!*				p_port_write				     *
!*===========================================================================*
p_port_write:
	cld
	pop	bx
	pop	dx		! port
	pop	_gdt+DS_286_OFFSET+DESC_BASE
	pop	ax		! pop offset into base of source descriptor
	movb	_gdt+DS_286_OFFSET+DESC_BASE_MIDDLE,al
	pop	cx		! byte count, discard high word
	sub	sp,#2+4+2
	push	ds
	mov	ax,#DS_286_SELECTOR
	mov	ds,ax
	mov	ax,si
	sub	si,si		! src offset is now 0 relative to start of seg
	shr	cx,#1		! word count
	rep
	outs			! write everything
	mov	si,ax
	pop	ds
	jmp	(bx)


!*===========================================================================*
!*				p_reset					     *
!*===========================================================================*
! Reset the system by loading IDT with offset 0 and interrupting.

p_reset:
	lidt	idt_zero
	int	3			! anything goes, the 286 won`t like it


!*===========================================================================*
!*				p_level0				     *
!*===========================================================================*
! PUBLIC void level0(void (*func)(void))
! Call a function at permission level 0.  This allows kernel tasks to do
! things that are only possible at the most privileged CPU level.
!
p_level0:
	mov	bx, sp
	mov	ax, 2(bx)
	mov	_level0_func, ax
	int	LEVEL0_VECTOR
	ret


!*===========================================================================*
!*				data					     *
!*===========================================================================*
	.data
patch_table:			! pairs (old function, new function)
	.data2	_cp_mess, p_cp_mess
	.data2	_phys_copy, p_phys_copy
	.data2	_port_read, p_port_read
	.data2	_port_write, p_port_write
	.data2	_reset, p_reset
	.data2	_level0, p_level0
	.data2	_restart, p_restart	! in mpx file
	.data2	save, p_save	! in mpx file
	.data2	0		! end of table
vidlock:			! dummy variable for use with lock prefix
	.data2 0
idt_vectors:
	.data2	0x3FF		! limit and base of real mode interrupt vectors
	.data4	0
	.data2	0
idt_zero:
	.data4	0, 0
