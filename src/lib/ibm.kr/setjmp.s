.define _setjmp, _longjmp, __longjmp
.extern _setjmp, _longjmp, __longjmp
.extern _sigprocmask
.extern __longjerr
.extern __sigjmp
.extern .sti, .gto
.text
|
| Warning:  this code depends on the C language declaration of
| jmp_buf in <setjmp.h>, and on the definitions of the flags
| SC_ONSTACK, SC_SIGCONTEXT, and SC_NOREGLOCALS in <sys/sigcontext.h>.
SC_SIGCONTEXT=2
SC_NOREGLOCALS=4
|
| _setjmp is called with two or three arguments. If called with three
| arguments, the second argument should be 0 or 1, and if it is 0,
| the signal context should not be restored on the longjmp.
| The last argument is added by the compiler, | which recognizes the
| "setjmp" identifier and adds an extra parameter: the | return address
| (there is no portable way to obtain this in the compiler intermediate code).
|
| _setjmp(jmp_buf, 0/1, retaddr)
| or
| _setjmp(jmp_buf, retaddr)
|
_setjmp:
	push	bp
	mov	bp,sp

	push	0(bp)		| frame pointer
	lea	ax, 4(bp)
	push	ax		| stack pointer
	push	2(bp)		| program counter; take it from the return
				| area. We could also get it from either
				| 6(bp) or 8(bp), but this is faster.
	xor	ax, ax
	push	ax		| signal mask high
	push	ax		| signal mask low
	mov	ax,*SC_NOREGLOCALS	| flags (4 is SC_NOREGLOCALS)
	push	ax

	mov	ax, 6(bp)	| get the savemask arg
	cmp	ax, *0
	je	nosigstate	| don't save signal state

	or	-12(bp), *SC_SIGCONTEXT		| flags |= SC_SIGCONTEXT

	lea	ax, -10(bp)
	push	ax
	xor	ax, ax
	push	ax
	push	ax
	call	_sigprocmask	| fill in signal mask
	add	sp, *6

nosigstate:
	mov	bx, 4(bp)	| jmp_buf
	mov	cx, *12		| sizeof(jmp_buf)
	call	.sti		| sp = src addr, bx = dest addr, cx = count
	xor	ax, ax
	mov	sp,bp
	pop	bp
	ret

|
| _longjmp() should be called for a setjmp() call that did not save the
| signal context.
|

__longjmp:
	push	bp
	mov	bp, sp

	mov	bx,4(bp)

| Check that this jmp_buf has no saved registers.
	mov 	ax, (bx)		| get flags
	test 	ax, *4
	je	__longjerr

| Set up the value to return in ax.
	mov	ax, 6(bp)		| value to return
	or	ax, ax
	jne	nonzero
	mov	ax, *1
nonzero:
	add	bx, *6
	jmp	.gto

|
| longjmp() should be called for a setjmp() call that DID save the
| signal context.
|

_longjmp:
	push	bp
	mov	bp, sp

| Check that this is a jmp_buf with no saved regs and with signal context info.
	mov	bx, 4(bp)		| pointer to jmp_buf
	mov	ax, (bx)		| get the flags
	test 	ax, *4			| check for no saved registers
	je	__longjerr
	test	ax, *2			| check for signal context
	je	__longjerr
	
| Compute the value to return
	mov	ax, 6(bp)		| proposed value to return
	or	ax, ax
	jne	nonzero1
	mov	ax, *1
nonzero1:

| Call _sigjmp to restore the old signal context.
	push	ax
	push	bx
	call	__sigjmp
	add	sp, *4
