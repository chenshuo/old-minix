| This file contains the assembler startup code for Minix.
| It cooperates with cstart.c to set up a good environment for main().
| It contains the following routines:

.define		_db		| trap to external debugger
.define		_get_chrome	| returns nonzero if display is color
.define		_get_ega	| returns nonzero if display is ega
.define		_get_ext_memsize  | returns amount of extended memory in K
.define		_get_low_memsize  | returns amount of low memory in K
.define		_get_processor	| returns processor type (86, 186, 286, 386)
.define		_get_word	| returns word at given segment:offset
.define		_put_word	| writes given word to given segment:offset

| All except db() only work in real mode, so must not be called by main() or
| later.

| Imported functions.

.extern		_cstart
.extern		_main

| Exported variables.

.define		kernel_ds

	.bss
.define		begbss
.define		begdata
.define		k_stktop
.define		_sizes

| Imported variables.

.extern		_gdt

#include <minix/config.h>
#include <minix/const.h>
#include <minix/com.h>
#include "const.h"
#include "protect.h"
#include "sconst.h"

/* BIOS software interrupts, subfunctions and return values. */
#define EQUIPMENT_CHECK		0x11
#	define EDISPLAY_MASK	0x30	/* mask for video display equipment */
#	define EMONO_DISPLAY	0x30	/* value for mono disply */
#define GET_EXTMEM_VEC		0x15	/* get extended memory size */
#	define GET_EXTMEM_FUNC	0x88
#define GET_MEM_VEC		0x12	/* get memory size */
#define SET_PROTECT_VEC		0x15	/* set protected mode */
#	define SET_PROTECT_FUNC 0x89
#define VIDEO			0x10
#	define ALTERNATE_SELECT	0x12
#	define A_S_INFO		0x10

	.text
|*===========================================================================*
|*				MINIX					     *
|*===========================================================================*
MINIX:				| this is the entry point for the MINIX kernel
	j	over_kernel_ds	| skip over the next few bytes
	.word	CLICK_SHIFT	| for build, later used by db for syms offset
kernel_ds:
	.word	0		| build puts kernel DS here at fixed address 4
over_kernel_ds:
	cli			| disable interrupts
	cld			| C compiler needs es = ds and direction = up

| Set up kernel segment registers and stack.
| The old fsck1.s sets up an invalid stack, so the above cli is beside the
| point, and a debugger trap here would crash.

	seg	cs
	mov	dx,kernel_ds	| dx is the only free register
	mov	ds,dx
	mov	es,dx
	mov	ss,dx
	mov	sp,#k_stktop	| set sp to point to the top of kernel stack

| Call C startup code to prepare for switching modes.

	push	ds
	push	cs
	push	di
	push	si
	push	dx
	push	cx
	push	bx
	push	ax
	call	_cstart
	add	sp,#8*2

| After switching to protected mode, the CPU may be in 32-bit mode, which has
| different instruction decoding, so "jmp _main" would fail.
| Fake a 32-bit return address to get around this ("ret" is the same in 32-bit
| mode apart from popping 32 bits).

	sub	ax,ax
	push	ax		| becomes harmless junk in 16-bit mode
	mov	ax,#_main
	push	ax

| Call the BIOS to switch to protected mode.
| This is just to do any cleanup necessary, typically to disable a hardware
| kludge which holds the A20 address line low.

| The call requires the gdt as set up by prot_init():
|	gdt pointer in gdt[1]
|	ldt pointer in gdt[2]
|	new ds in gdt[3]
|	new es in gdt[4]
|	new ss in gdt[5]
|	new cs in gdt[6]
|	nothing in gdt[7] (overwritten with BIOS cs)
|	ICW2 for master 8259 in bh
|	ICW2 for slave 8259 in bl
| The BIOS enables interrupts briefly - this is OK since the BIOS vectors
| are still valid.
| Most registers are destroyed.
| The 8259's are reinitialised.

	mov	si,#_gdt
	mov	bx,#IRQ0_VECTOR * 256 + IRQ8_VECTOR
	movb	ah,#SET_PROTECT_FUNC
	int	SET_PROTECT_VEC

| Now the processor is in protected mode.
| There is a little more protected mode initialization to do, but leave it
| to main(), to avoid using incompatible instructions here.

	ret			| "return" to _main


|*===========================================================================*
|*				db					     *
|*===========================================================================*

| PUBLIC void db();
| Trap to external debugger.
| This may be called from all modes (real or protected, 16 or 32-bit).

_db:
	int	3
	ret


|*===========================================================================*
|*				get_chrome				     *
|*===========================================================================*

| PUBLIC u16_t get_chrome();
| Call the BIOS to find out if the display is color.

_get_chrome:
	int	EQUIPMENT_CHECK		| get bit pattern for equipment
	and	ax,#EDISPLAY_MASK	| isolate color/mono field
	sub	ax,#EMONO_DISPLAY	| nonzero means it is color
	ret


|*===========================================================================*
|*				get_ega  				     *
|*===========================================================================*

| PUBLIC u16_t get_ega();
| Call the BIOS to find out if the display is EGA.
| (Actually this only tells if the BIOS supports EGA, not whether the
| screen is EGA. Doing it right is far more complicated.)

_get_ega:
	movb	bl,#A_S_INFO	| get info about
	movb	ah,#ALTERNATE_SELECT	| alternative display types
	int	VIDEO		| this will fail for non-EGA BIOS
	movb	al,bl		| success is determined by knowing
	movb	ah,#0		| a successful call will change bl
	sub	ax,#A_S_INFO	| nonzero means it is EGA
	ret


|*===========================================================================*
|*				get_ext_memsize				     *
|*===========================================================================*

| PUBLIC u16_t get_ext_memsize();
| Ask the BIOS how much extended memory there is.

_get_ext_memsize:
	pushf			| gak!, PC's set the interrupt enable flag
	movb	ah,#GET_EXTMEM_FUNC
	clc			| carry will stay clear if call exists
	int	GET_EXTMEM_VEC	| returns size (in K) in ax for AT's
	jnc	got_ext_memsize	| error, probably a PC
	sub	ax,ax
got_ext_memsize:
	popf
	ret


|*===========================================================================*
|*				get_low_memsize				     *
|*===========================================================================*

| PUBLIC u16_t get_low_memsize();
| Ask the BIOS how much normal memory there is.

_get_low_memsize:
	int	GET_MEM_VEC	| returns the size (in K) in ax
	ret


|*===========================================================================*
|*				get_processor				     *
|*===========================================================================*

| PUBLIC u16_t get_processor();
| Decide processor type among 8088=8086, 80188=80186, 80286, 80386.
| Return 86, 186, 286 or 386.
| Preserves all registers except the flags and the return register ax.

| Method:
| 8088=8086 and 80188=80186 push sp as new sp, 80286 and 80386 as old sp.
| All but 8088=8086 do shifts mod 32 or 16.
| 386 stores 0 for the upper 8 bits of the GDT pointer in 16 bit mode,
| while 286 stores 0xFF.

_get_processor:
	push	sp		| see if pushed sp == sp
	pop	ax
	cmp	ax,sp
	jz	new_processor
	push	cx		| see if shifts are mod 32
	mov	cx,#0x0120
	shlb	ch,cl		| zero tells if 86
	pop	cx
	mov	ax,#86
	jz	got_processor
	mov	ax,#186
	ret

new_processor:
	push	bp		| see if high bits are set in saved GDT
	mov	bp,sp
	sub	sp,#6		| space for GDT ptr
	defsgdt	(-6(bp))	| save 3 word GDT ptr
	add	sp,#4		| discard 2 words of GDT ptr
	pop	ax		| top word of GDT ptr
	pop	bp
	cmpb	ah,#0		| zero only for 386
	mov	ax,#286
	jnz	got_processor
	mov	ax,#386
got_processor:
	ret


|*===========================================================================*
|*				get_word				     *
|*===========================================================================*

| PUBLIC u16_t get_word(u16_t segment, u16_t *offset);
| Load and return the word at the far pointer  segment:offset.

_get_word:
	mov	cx,ds		| save ds
	pop	dx		| return adr
	pop	ds		| segment
	pop	bx		| offset
	sub	sp,#2+2		| adjust for parameters popped
	mov	ax,(bx)		| load the word to return
	mov	ds,cx		| restore ds
	jmp	(dx)		| return


|*===========================================================================*
|*				put_word				     *
|*===========================================================================*

| PUBLIC void put_word(u16_t segment, u16_t *offset, u16_t value);
| Store the word  value  at the far pointer  segment:offset.

_put_word:
	mov	cx,ds		| save ds
	pop	dx		| return adr
	pop	ds		| segment
	pop	bx		| offset
	pop	ax		| value
	sub	sp,#2+2+2	| adjust for parameters popped
	mov	(bx),ax		| store the word
	mov	ds,cx		| restore ds
	jmp	(dx)		| return


|*===========================================================================*
|*				data					     *
|*===========================================================================*

	.data
begdata:
_sizes:				| sizes of kernel, mm, fs filled in by build
	.word	0x526F		| this must be the first data entry (magic #)
	.word	CLICK_SHIFT	| consistency check for build
	.zerow	6		| build uses prevous 2 words and this space
	.space	K_STACK_BYTES	| kernel stack
k_stktop:			| top of kernel stack

	.bss
begbss:
