Title mpx88
page,132


; This file is part of the lowest layer	of the Minix kernel.  All process
; switching and	message	handling is done here and in file "proc.c".  This file
; is entered on	every transition to the	kernel,	both for sending/receiving
; messages and for all interrupts.  In all cases, the trap or interrupt
; routine first	calls save() to	store the machine state	in the proc table.
; Then the stack is switched to	k_stack.  Finally, the real trap or interrupt
; handler (in C) is called.  When it returns, the interrupt routine jumps to
; restart, to run the process or task whose number is in 'cur_proc'.
;
; The external entry points into this file are:
;   s_call:	process	or task	wants to send or receive a message
;   tty_int:	interrupt routine for each key depression and release
;   lpr_int:	interrupt routine for each line	printer	interrupt
;   disk_int:	disk interrupt routine
;   clock_int:	clock interrupt	routine	[HZ times per second]
;   surprise:	all other interrupts < 16 are vectored here
;   trp:	all traps with vector >= 16 are	vectored here
;   divide:	divide overflow traps are vectored here
;   restart:	start running a	task or	process


; include the following	from const.h:
K_STACK_BYTES	EQU  256
IDLE		EQU -999

; include the following	from ../h/com.h
DISKINT		EQU  1
CLOCK		EQU -3
CLOCK_TICK	EQU  2
FLOPPY		EQU -5


; The following	procedures are defined in this file and	called from outside it.
PUBLIC $main, tty_int, lpr_int,	clock_in, disk_int
PUBLIC s_call, surprise, trp, divide, restart


; The following	external procedures are	called in this file.
EXTRN main:NEAR,  sys_call:NEAR, interrup:NEAR,	keyboard:NEAR
EXTRN panic:NEAR, unexpect:NEAR, trap:NEAR, div_trap:NEAR, pr_char:NEAR


; Variables and	data structures.
PUBLIC sizes, brksize, splimit,	@end
EXTRN  cur_proc:WORD, proc_ptr:WORD, int_mess:WORD
EXTRN  scan_cod:WORD, k_stack:WORD


; The following	constants are offsets into the proc table.
esreg =	14
dsreg =	16
csreg =	18
ssreg =	20
spreg =	22
PC    =	24
PSW   =	28
SPLIM =	50
OFF   =	18
ROFF  =	12


INCLUDE	..\lib\prologue.h


@CODE	SEGMENT
	assume	cs:@code,ds:dgroup

;===========================================================================
;				Minix
;===========================================================================
$main:
Minix:				; this is the entry point for the Minix	kernel.
	jmp short M0		; skip over the	next word(s)
	ORG 4			; build	writes the ds value at text address 4
ker_ds	DW  dgroup		; this word will contain kernel's ds value
				; and it forces	relocation for dos2out
     M0:cli			; disable interrupts
	mov ax,cs		; set up segment registers
	mov ds,ax		; set up ds
	mov ax,cs:ker_ds	; build	has loaded this	word with ds value
	mov ds,ax		; ds now contains proper value
	mov ss,ax		; ss now contains proper value
	mov dgroup:scan_cod,bx		; save scan code for '=' key from bootstrap
	mov sp,offset dgroup:k_stack	; set sp to point to the top of	the
	add sp,K_STACK_BYTES		; kernel stack
	call main		; start	the main program of Minix

     M1:jmp M1			; this should never be executed


;===========================================================================
;				s_call
;===========================================================================
s_call:				; System calls are vectored here.
	call save		; save the machine state
	mov bp,dgroup:proc_ptr	; use bp to access sys call parameters
	push 2[bp]		; push(pointer to user message)	(was bx)
	push [bp]		; push(src/dest) (was ax)
	push dgroup:cur_proc	; push caller
	push 4[bp]		; push(SEND/RECEIVE/BOTH) (was cx)
	call sys_call		; sys_call(function, caller, src_dest, m_ptr)
	jmp restart		; jump to code to restart proc/task running


;===========================================================================
;				tty_int
;===========================================================================
tty_int:			; Interrupt routine for	terminal input.
	call save		; save the machine state
	call keyboard		; process a keyboard interrupt
	jmp  restart		; continue execution


;===========================================================================
;				lpr_int
;===========================================================================
lpr_int:			; Interrupt routine for	terminal input.
	call save		; save the machine state
	call pr_char		; process a line printer interrupt
	jmp  restart		; continue execution


;===========================================================================
;				disk_int
;===========================================================================
disk_int:			; Interrupt routine for	the floppy disk.
	call save			; save the machine state
	mov  dgroup:int_mess+2,DISKINT	; build	message	for disk task
	mov  ax,offset dgroup:int_mess	; prepare to call interrupt[FLOPPY, &intmess]
	push ax				; push second parameter
	mov  ax,FLOPPY		; prepare to push first	parameter
	push ax			; push first parameter
	call interrup		; this is the call
	jmp  restart		; continue execution


;===========================================================================
;				clock_int
;===========================================================================
clock_in:			; Interrupt routine for	the clock.
	call save			; save the machine state
	mov dgroup:int_mess+2,CLOCK_TICK; build	message	for clock task
	mov ax,offset dgroup:int_mess	; prepare to call interrupt(CLOCK,&intmess)
	push ax				; push second parameter
	mov  ax,CLOCK		; prepare to push first	parameter
	push ax			; push first parameter
	call interrup		; this is the call
	jmp  restart		; continue execution


;===========================================================================
;				surprise
;===========================================================================
surprise:			; This is where	unexpected interrupts come.
	call save		; save the machine state
	call unexpect		; go panic
	jmp  restart		; never	executed


;===========================================================================
;				trp
;===========================================================================
trp:				; This is where	unexpected traps come.
	call save		; save the machine state
	call trap		; print	a message
	jmp restart		; this error is	not fatal


;===========================================================================
;				divide					     
;===========================================================================
divide:				; This is where divide overflow traps come.
	call save		; save the machine state
	call div_trap		; print a message
	jmp restart		; this error is not fatal


;===========================================================================
;				save
;===========================================================================
save:				; save the machine state in the	proc table.
	push ds			; stack: psw/cs/pc/ret addr/ds
	push cs			; prepare to restore ds
	pop ds			; ds has now been set to cs
	mov ds,ker_ds		; word 4 in kernel text	space contains ds value
	pop ds_save		; stack: psw/cs/pc/ret addr
	pop ret_save		; stack: psw/cs/pc
	mov bx_save,bx		; save bx for later ; we need a	free register
	mov bx,dgroup:proc_ptr	; start	save set up; make bx point to save area
	add bx,OFF		; bx points to place to	store cs
	pop PC-OFF[bx]		; store	pc in proc table
	pop csreg-OFF[bx]	; store	cs in proc table
	pop PSW-OFF[bx]		; store	psw
	mov ssreg-OFF[bx],ss	; store	ss
	mov spreg-OFF[bx],sp	; sp as	it was prior to	interrupt
	mov sp,bx		; now use sp to	point into proc	table/task save
	mov bx,ds		; about	to set ss
	mov ss,bx		; set ss
	push ds_save		; start	saving all the registers, sp first
	push es			; save es between sp and bp
	mov es,bx		; es now references kernel memory too
	push bp			; save bp
	push di			; save di
	push si			; save si
	push dx			; save dx
	push cx			; save cx
	push bx_save		; save original	bx
	push ax				    ; all registers now	saved
	mov sp,offset dgroup:k_stack	    ; temporary	stack for interrupts
	add sp,K_STACK_BYTES		    ; set sp to	top of temporary stack
	mov splimit,offset dgroup:k_stack   ; limit for	temporary stack
	add splimit,8			    ; splimit checks for stack overflow
	mov ax,ret_save		; ax = address to return to
	jmp ax			; return to caller; Note: sp points to saved ax


;===========================================================================
;				restart
;===========================================================================
restart:			; This routine sets up and runs	a proc or task.
	cmp dgroup:cur_proc,IDLE; restart user; if cur_proc = IDLE, go	idle
	je _idle		; no user is runnable, jump to idle routine
	cli			; disable interrupts
	mov sp,dgroup:proc_ptr	; return to user, fetch	regs from proc table
	pop ax			; start	restoring registers
	pop bx			; restore bx
	pop cx			; restore cx
	pop dx			; restore dx
	pop si			; restore si
	pop di			; restore di
	mov lds_low,bx		; lds_low contains bx
	mov bx,sp		; bx points to saved bp	register
	mov bp,SPLIM-ROFF[bx]	; splimit = p_splimit
	mov splimit,bp		; ditto
	mov bp,dsreg-ROFF[bx]	; bp = ds
	mov lds_low+2,bp	; lds_low+2 contains ds
	pop bp			; restore bp
	pop es			; restore es
	mov sp,spreg-ROFF[bx]	; restore sp
	mov ss,ssreg-ROFF[bx]	; restore ss using the value of	ds
	push PSW-ROFF[bx]	; push psw (flags)
	push csreg-ROFF[bx]	; push cs
	push PC-ROFF[bx]	; push pc
	lds  bx,DWORD PTR lds_low   ; restore ds and bx	in one fell swoop
	iret			; return to user or task

;===========================================================================
;				idle
;===========================================================================
_idle:				; executed when	there is no work
	sti			; enable interrupts
L3:	wait			; just idle while waiting for interrupt
	jmp L3			; loop until interrupt


@CODE	ENDS


;===========================================================================
;				data
;===========================================================================

@DATAB	SEGMENT			; datab	ensures	it is the beginning of all data
sizes	 DW	526Fh		; this must be the first data entry (magic nr)
	 DW	7 dup(0)	; space	for build table	- total	16b
splimit	 DW	0		; stack	limit for current task (kernel only)
bx_save	 DW	0		; storage for bx
ds_save	 DW	0		; storage for ds
ret_save DW	0		; storage for return address
lds_low	 DW	0,0		; storage used for restoring ds:bx
brksize	 DW	offset dgroup:@END+2	; first	free memory in kernel
ttyomess DB	"RS232 interrupt",0
@DATAB	ENDS


@DATAV	SEGMENT			; DATAV	holds nothing. The label in
@end	label	byte		; the segment just tells us where
@DATAV	ENDS			; the data+bss ends.

@STACK	SEGMENT	BYTE STACK 'STACK'	; Satisfy DOS-linker (dummy stack)
@STACK	ENDS

	END	; end of assembly-file

