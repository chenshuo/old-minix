| This file contains a number of assembly code utility routines needed by the
| kernel.  They are:
|
|   phys_copy:	copies data from anywhere to anywhere in memory
|   cp_mess:	copies messages from source to destination
|   lock:	disable interrupts
|   restore:	restore interrupts (enable/disabled) as they were before lock()
|   build_sig:	build 4 word structure pushed onto stack for signals
|   csv:	procedure prolog to save the registers
|   cret:	procedure epilog to restore the registers
|   get_chrome:	returns 0 if display is monochrome, 1 if it is color
|   get_ega:	returns 1 if display is EGA, 0 otherwise
|   vid_copy:	copy data to video ram (on color display during retrace only)
|   scr_up:	scroll screen a line up (in software, by copying)
|   scr_down:	scroll screen a line down (in software, by copying)
|   get_byte:	reads a byte from a user program and returns it as value
|   reboot:	reboot for CTRL-ALT-DEL
|   wreboot:	wait for character then reboot 
|   dma_read:	transfer data between HD controller and memory
|   dma_write:	transfer data between memory and HD controller
|   em_xfer:	read or write AT extended memory using the BIOS
|   wait_retrace: waits for retrace interval, and returns int disabled
|   ack_char:	acknowledge character from keyboard
|   save_tty_vec: save tty interrupt vector 0x71 for PS/2

| The following procedures are defined in this file and called from outside it.
.globl _phys_copy, _cp_mess, _lock, _restore
.globl _build_sig, csv, cret, _get_chrome, _vid_copy, _get_byte, _reboot
.globl _wreboot, _dma_read, _dma_write, _em_xfer, _scr_up, _scr_down
.globl _ack_char, _save_tty_vec, _get_ega, _wait_retrace


| The following external procedures are called in this file.
.globl _panic

| Variables and data structures
.globl _color, _cur_proc, _proc_ptr, splimit
.globl _port_65, _ps, _vec_table, _vid_mask, _vid_port

|*===========================================================================*
|*				phys_copy				     *
|*===========================================================================*
| This routine copies a block of physical memory.  It is called by:
|    phys_copy( (long) source, (long) destination, (long) bytecount)

_phys_copy:
	pushf			| save flags
|	cli			| disable interrupts
	cld			| clear direction flag
	push bp			| save the registers
	push ax			| save ax
	push bx			| save bx
	push cx			| save cx
	push dx			| save dx
	push si			| save si
	push di			| save di
	push ds			| save ds
	push es			| save es
	mov bp,sp		| set bp to point to saved es

  L0:	mov ax,28(bp)		| ax = high-order word of 32-bit destination
	mov di,26(bp)		| di = low-order word of 32-bit destination
	mov cx,*4		| start extracting click number from dest
  L1:	rcr ax,*1		| click number is destination address / 16
	rcr di,*1		| it is used in segment register for copy
	loop L1			| 4 bits of high-order word are used
	mov es,di		| es = destination click

	mov ax,24(bp)		| ax = high-order word of 32-bit source
	mov si,22(bp)		| si = low-order word of 32-bit source
	mov cx,*4		| start extracting click number from source
  L2:	rcr ax,*1		| click number is source address / 16
	rcr si,*1		| it is used in segment register for copy
	loop L2			| 4 bits of high-order word are used
	mov ds,si		| ds = source click

	mov di,26(bp)		| di = low-order word of dest address
	and di,*0x000F		| di = offset from paragraph # in es
	mov si,22(bp)		| si = low-order word of source address
	and si,*0x000F		| si = offset from paragraph # in ds

	mov dx,32(bp)		| dx = high-order word of byte count
	mov cx,30(bp)		| cx = low-order word of byte count

	test cx,#0x8000		| if bytes >= 32768, only do 32768 
	jnz L3			| per iteration
	test dx,#0xFFFF		| check high-order 17 bits to see if bytes
	jnz L3			| if bytes >= 32768 then go to L3
	jmp L4			| if bytes < 32768 then go to L4
  L3:	mov cx,#0x8000		| 0x8000 is unsigned 32768
  L4:	mov ax,cx		| save actual count used in ax; needed later

	test cx,*0x0001		| should we copy a byte or a word at a time?
	jz L5			| jump if even
	rep			| copy 1 byte at a time
	movb			| byte copy
	jmp L6			| check for more bytes

  L5:	shr cx,*1		| word copy
	rep			| copy 1 word at a time
	movw			| word copy

  L6:	mov dx,32(bp)		| decr count, incr src & dst, iterate if needed
	mov cx,30(bp)		| dx || cx is 32-bit byte count
	xor bx,bx		| bx || ax is 32-bit actual count used
	sub cx,ax		| compute bytes - actual count
	sbb dx,bx		| dx || cx is # bytes not yet processed
	or cx,cx		| see if it is 0
	jnz L7			| if more bytes then go to L7
	or dx,dx		| keep testing
	jnz L7			| if loop done, fall through

	pop es			| restore all the saved registers
	pop ds			| restore ds
	pop di			| restore di
	pop si			| restore si
	pop dx			| restore dx
	pop cx			| restore cx
	pop bx			| restore bx
	pop ax			| restore ax
	pop bp			| restore bp
	popf			| restore flags
	ret			| return to caller

L7:	mov 32(bp),dx		| store decremented byte count back in mem
	mov 30(bp),cx		| as a long
	add 26(bp),ax		| increment destination
	adc 28(bp),bx		| carry from low-order word
	add 22(bp),ax		| increment source
	adc 24(bp),bx		| carry from low-order word
	jmp L0			| start next iteration


|*===========================================================================*
|*				cp_mess					     *
|*===========================================================================*
| This routine is makes a fast copy of a message from anywhere in the address
| space to anywhere else.  It also copies the source address provided as a
| parameter to the call into the first word of the destination message.
| It is called by:
|    cp_mess(src, src_clicks, src_offset, dst_clicks, dst_offset)
| where all 5 parameters are shorts (16-bits).
|
| Note that the message size, 'Msize' is in WORDS (not bytes) and must be set
| correctly.  Changing the definition of message the type file and not changing
| it here will lead to total disaster.
| 
| This routine only preserves the registers the 'C' compiler
| expects to be preserved (es, ds, si, di, sp, bp).

Msize = 12			| size of a message in 16-bit words
_cp_mess:
	push es			| save es
	push ds			| save ds
	mov bx,sp		| index off bx because machine can't use sp
	pushf			| save flags
	cli			| disable interrupts
	push si			| save si
	push di			| save di
	mov di,14(bx)		| di = offset of destination buffer
	les si,10(bx)		| use 32 bit load(ds is our base)
				| si = offset of source message
				| es = clicks of destination 
	lds ax,6(bx)		| use 32 bit load ....
				| ax = process number of sender
				| ds = clicks of source message
	seg es			| segment override prefix
  	mov (di),ax		| copy sender's process number to dest message
	add si,*2		| don't copy first word
	add di,*2		| don't copy first word
	mov cx,*Msize-1		| remember, first word doesn't count
	cld			| clear direction flag
	rep			| iterate cx times to copy 11 words
	movw			| copy the message
	pop di			| restore di
	pop si			| restore si
	popf			| restore flags (resets interrupts to old state)
	pop ds			| restore ds
	pop es			| restore es	
	ret			| that's all folks!


|*===========================================================================*
|*				lock					     *
|*===========================================================================*
| Disable CPU interrupts.  Return old psw as function value.
_lock:  
	pushf			| save flags on stack
	cli			| disable interrupts
	pop ax	 		| return flags for restoration later
	ret			| return to caller


|*===========================================================================*
|*				restore					     *
|*===========================================================================*
| restore enable/disable bit to the value it had before last lock.
_restore:
	push bp			| save it
	mov bp,sp		| set up base for indexing
	push 4(bp)		| bp is the psw to be restored
	popf			| restore flags
	pop bp			| restore bp
	ret			| return to caller


|*===========================================================================*
|*				build_sig				     *
|*===========================================================================*
|* Build a structure that is pushed onto the stack for signals.  It contains
|* pc, psw, etc., and is machine dependent. The format is the same as generated
|* by hardware interrupts, except that after the "interrupt", the signal number
|* is also pushed.  The signal processing routine within the user space first
|* pops the signal number, to see which function to call.  Then it calls the
|* function.  Finally, when the function returns to the low-level signal
|* handling routine, control is passed back to where it was prior to the signal
|* by executing a return-from-interrupt instruction, hence the need for using
|* the hardware generated interrupt format on the stack.  The call is:
|*     build_sig(sig_stuff, rp, sig)

| Offsets within proc table
PC    = 24
csreg = 18
PSW   = 28

_build_sig:
	push bp			| save bp
	mov bp,sp		| set bp to sp for accessing params
	push bx			| save bx
	push si			| save si
	mov bx,4(bp)		| bx points to sig_stuff
	mov si,6(bp)		| si points to proc table entry
	mov ax,8(bp)		| ax = signal number
	mov (bx),ax		| put signal number in sig_stuff
	mov ax,PC(si)		| ax = signalled process' PC
	mov 2(bx),ax		| put pc in sig_stuff
	mov ax,csreg(si)	| ax = signalled process' cs
	mov 4(bx),ax		| put cs in sig_stuff
	mov ax,PSW(si)		| ax = signalled process' PSW
	mov 6(bx),ax		| put psw in sig_stuff
	pop si			| restore si
	pop bx			| restore bx
	pop bp			| restore bp
	ret			| return to caller


|*===========================================================================*
|*				csv & cret				     *
|*===========================================================================*
| This version of csv replaces the standard one.  It checks for stack overflow
| within the kernel in a simpler way than is usually done. cret is standard.
csv:
	pop bx			| bx = return address
	push bp			| stack old frame pointer
	mov bp,sp		| set new frame pointer to sp
	push di			| save di
	push si			| save si
	sub sp,ax		| ax = # bytes of local variables
	cmp sp,splimit		| has kernel stack grown too large
	jbe csv.1		| if sp is too low, panic
	jmp (bx)		| normal return: copy bx to program counter

csv.1:
	mov splimit,#0		| prevent call to panic from aborting in csv
	mov bx,_proc_ptr	| update rp->p_splimit
	mov 50(bx),#0		| rp->sp_limit = 0
	push _cur_proc		| task number
	mov ax,#stkoverrun	| stack overran the kernel stack area
	push ax			| push first parameter
	call _panic		| call is: panic(stkoverrun, cur_proc)
	jmp csv.1		| this should not be necessary


cret:
	lea	sp,*-4(bp)	| set sp to point to saved si
	pop	si		| restore saved si
	pop	di		| restore saved di
	pop	bp		| restore bp
	ret			| end of procedure

|*===========================================================================*
|*				get_chrome				     *
|*===========================================================================*
| This routine calls the BIOS to find out if the display is monochrome or 
| color.  The drivers are different, as are the video ram addresses, so we
| need to know.
_get_chrome:
	int 0x11		| call the BIOS to get equipment type
	andb al,#0x30		| isolate color/mono field
	cmpb al,*0x30		| 0x30 is monochrome
	je getchr1		| if monochrome then go to getchr1
	mov ax,#1		| color = 1
	ret			| color return
getchr1: xor ax,ax		| mono = 0
	ret			| monochrome return

|*===========================================================================*
|*				get_ega  				     *
|*===========================================================================*
| This routine calls the BIOS to find out if the display is ega.  This
| is needed because scrolling is different.
_get_ega:
	movb bl,*0x10
	movb ah,*0x12
	int 0x10		| call the BIOS to get equipment type

	cmpb bl,*0x10		| if reg is unchanged, it failed
	je notega
	mov ax,#1		| color = 1
	ret			| color return
notega: xor ax,ax		| mono = 0
	ret			| monochrome return

|*===========================================================================*
|*				dma_read				     *
|*===========================================================================*
_dma_read:
	push	bp
	mov	bp,sp
	push	cx
	push	dx
	push	di
	push	es
	mov	cx,#256		| transfer 256 words
	mov	dx,#0x1F0	| from/to port 1f0
	cld
	mov	es,4(bp)	| segment in es
	mov	di,6(bp)	| offset in di
	.byte	0xF3, 0x6D	| opcode for 'rep insw'
	pop	es
	pop	di
	pop	dx
	pop	cx
	mov	sp,bp
	pop	bp
	ret

|*===========================================================================*
|*				dma_write				     *
|*===========================================================================*
_dma_write:
	push	bp
	mov	bp,sp
	push	cx
	push	dx
	push	si
	push	ds
	mov	cx,#256		| transfer 256 words
	mov	dx,#0x1F0	| from/to port 1f0
	cld
	mov	ds,4(bp)	| segment in ds
	mov	si,6(bp)	| offset in si
	.byte	0xF3, 0x6F	| opcode for 'rep outsw'
	pop	ds
	pop	si
	pop	dx
	pop	cx
	mov	sp,bp
	pop	bp
	ret

|*===========================================================================*
|*				vid_copy				     *
|*===========================================================================*
| This routine takes a string of (character, attribute) pairs and writes them
| onto the screen.  For a color display, the writing only takes places during
| the vertical retrace interval, to avoid displaying garbage on the screen.
| The call is:
|     vid_copy(buffer, videobase, offset, words)
| where
|     'buffer'    is a pointer to the (character, attribute) pairs
|     'videobase' is 0xB800 for color and 0xB000 for monochrome displays
|     'offset'    tells where within video ram to copy the data
|     'words'     tells how many words to copy
| if buffer is zero, the fill char (blank_color) is used

_vid_copy:
	push bp			| we need bp to access the parameters
	mov bp,sp		| set bp to sp for indexing
	push si			| save the registers
	push di			| save di
	push bx			| save bx
	push cx			| save cx
	push dx			| save dx
	push es			| save es
vid.0:	mov si,4(bp)		| si = pointer to data to be copied
	mov di,8(bp)		| di = offset within video ram
	and di,_vid_mask	| only 4K or 16K counts
	mov cx,10(bp)		| cx = word count for copy loop
	mov dx,#0x3DA		| prepare to see if color display is retracing

	mov bx,di		| see if copy will run off end of video ram
	add bx,cx		| compute where copy ends
	add bx,cx		| bx = last character copied + 1
	sub bx,_vid_mask	| bx = # characters beyond end of video ram
	sub bx,#1		| note: dec bx doesn't set flags properly
	jle vid.1		| jump if no overrun
	sar bx,#1		| bx = # words that don't fit in video ram
	sub cx,bx		| reduce count by overrun
	mov tmp,cx		| save actual count used for later

vid.1:	test _color,*1		| skip vertical retrace test if display is mono
	jz vid.4		| if monochrome then go to vid.2
	test _ega,*1		| if ega also don't need to wait
	jnz vid.4

|vid.2:	in			| with a color display, you can only copy to
|	test al,*010		| the video ram during vertical retrace, so
|	jnz vid.2		| wait for start of retrace period.  Bit 3 of
vid.3:	in			| 0x3DA is set during retrace.  First wait
	testb al,*010		| until it is off (no retrace), then wait
	jz vid.3		| until it comes on (start of retrace)

vid.4:	pushf			| copying may now start; save flags
|	cli			| interrupts just get in the way: disable them
	cld			| clear direction flag
	mov es,6(bp)		| load es now: int routines may ruin it

	cmp si,#0		| si = 0 means blank the screen
	je vid.7		| jump for blanking
	lock			| this is a trick for the IBM PC simulator only
	inc vidlock		| 'lock' indicates a video ram access
	rep			| this is the copy loop
	movw			| ditto

vid.5:	popf			| restore flags
	cmp bx,#0		| if bx < 0, then no overrun and we are done
	jle vid.6		| jump if everything fit
	mov 10(bp),bx		| set up residual count
	mov 8(bp),#0		| start copying at base of video ram
	cmp 4(bp),#0		| NIL_PTR means store blanks
	je vid.0		| go do it
	mov si,tmp		| si = count of words copied
	add si,si		| si = count of bytes copied
	add 4(bp),si		| increment buffer pointer
	jmp vid.0		| go copy some more

vid.6:	pop es			| restore registers
	pop dx			| restore dx
	pop cx			| restore cx
	pop bx			| restore bx
	pop di			| restore di
	pop si			| restore si
	pop bp			| restore bp
	ret			| return to caller

vid.7:	mov ax,_blank_color	| ax = blanking character
	rep			| copy loop
	stow			| blank screen
	jmp vid.5		| done

|*===========================================================================*
|*			      wait_retrace				     *
|*===========================================================================*
| Wait until we're in the retrace interval.  Return locked (ints off).
| But enable them during the wait.

_wait_ret: push dx
	pushf
	mov dx,_vid_port
	or dx,#0x000A
wtre.3:	sti
	nop
	nop
	cli	
	in			| 0x3DA bit 3 is set during retrace.
	testb al,*010		| Wait until it is on.
	jz wtre.3

	pop ax	 		| return flags for restoration later
	pop dx
	ret			| return to caller

|*===========================================================================*
|*				scr_up  				     *
|*===========================================================================*
| This routine scrolls the screen up one line on an EGA display 
| 
| The call is:
|     scr_up(org,source,dest,count)
| where
|     'org'       is the video segment origin of the desired page

_scr_up:
	push bp			| we need bp to access the parameters
	mov bp,sp		| set bp to sp for indexing
	push si			| save the registers
	push di			| save di
	push cx			| save cx
	push es			| save es
	push ds			| save ds
	mov si,6(bp)		| si = pointer to data to be copied
	mov di,8(bp)		| di = offset within video ram
	mov cx,10(bp)		| cx = word count for copy loop

	pushf			| copying may now start; save flags
|	cli			| interrupts just get in the way: disable them
	cld			| clear diretion flag
	mov ax,4(bp)
	mov es,ax		| load es now: int routines may ruin it
	mov ds,ax

	rep			| this is the copy loop
	movw			| ditto

	popf			| restore flags
	pop ds			| restore ds
	pop es			| restore es
	pop cx			| restore cx
	pop di			| restore di
	pop si			| restore si
	pop bp			| restore bp
	ret			| return to caller

|*===========================================================================*
|*				  scr_down				     *
|*===========================================================================*
| This routine scrolls the screen down one line on an EGA display 
| 
| The call is:
|     scr_down(org)
| where
|     'org'       is the video segment origin of the desired page

_scr_down:
	push bp			| we need bp to access the parameters
	mov bp,sp		| set bp to sp for indexing
	push si			| save the registers
	push di			| save di
	push cx			| save cx
	push es			| save es
	push ds			| save ds
	mov si,6(bp)		| si = pointer to data to be copied
	mov di,8(bp)		| di = offset within video ram
	mov cx,10(bp)		| cx = word count for copy loop

	pushf			| copying may now start; save flags
|	cli			| interrupts just get in the way: disable them
	mov ax,4(bp)
	mov es,ax		| load es now: int routines may ruin it
	mov ds,ax

	std
	rep			| this is the copy loop
	movw			| ditto

	popf			| restore flags
	pop ds			| restore ds
	pop es			| restore es
	pop cx			| restore cx
	pop di			| restore di
	pop si			| restore si
	pop bp			| restore bp
	ret			| return to caller

|*===========================================================================*
|*				get_byte				     *
|*===========================================================================*
| This routine is used to fetch a byte from anywhere in memory.
| The call is:
|     c = get_byte(seg, off)
| where
|     'seg' is the value to put in es
|     'off' is the offset from the es value
_get_byte:
	push bp			| save bp
	mov bp,sp		| we need to access parameters
	push es			| save es
	mov es,4(bp)		| load es with segment value
	mov bx,6(bp)		| load bx with offset from segment
	seg es			| go get the byte
	movb al,(bx)		| al = byte
	xorb ah,ah		| ax = byte
	pop es			| restore es
	pop bp			| restore bp
	ret			| return to caller

|===========================================================================
|                		em_xfer
|===========================================================================
|
|  This file contains one routine which transfers words between user memory
|  and extended memory on an AT or clone.  A BIOS call (INT 15h, Func 87h)
|  is used to accomplish the transfer.  The BIOS call is "faked" by pushing
|  the processor flags on the stack and then doing a far call to the actual
|  BIOS location.  An actual INT 15h would get a MINIX complaint from an
|  unexpected trap.
|
|  NOTE:  WARNING:  CAUTION: ...
|  Before using this routine, you must find your BIOS address for INT 15h.
|  The debug command "d 0:54 57" will give you the segment and address of
|  the BIOS call.  On my machine this generates:
|      0000:0050      59 F8 00 F0                          Y...
|  These values are then plugged into the two strange ".word xxxx" lines
|  near the end of this routine.  They correspond to offset=0xf859 and
|  seg=0xf000.  The offset is the first two bytes and the segment is the
|  last two bytes (Note the byte swap).
|
|  This particular BIOS routine runs with interrupts off since the 80286
|  must be placed in protected mode to access the memory above 1 Mbyte.
|  So there should be no problems using the BIOS call.
|
	.text
gdt:				| Begin global descriptor table
					| Dummy descriptor
	.word 0		| segment length (limit)
	.word 0		| bits 15-0 of physical address
	.byte 0		| bits 23-16 of physical address
	.byte 0		| access rights byte
	.word 0		| reserved
					| descriptor for GDT itself
	.word 0		| segment length (limit)
	.word 0		| bits 15-0 of physical address
	.byte 0		| bits 23-16 of physical address
	.byte 0		| access rights byte
	.word 0		| reserved
src:					| source descriptor
srcsz:	.word 0		| segment length (limit)
srcl:	.word 0		| bits 15-0 of physical address
srch:	.byte 0		| bits 23-16 of physical address
	.byte 0x93	| access rights byte
	.word 0		| reserved
tgt:					| target descriptor
tgtsz:	.word 0		| segment length (limit)
tgtl:	.word 0		| bits 15-0 of physical address
tgth:	.byte 0		| bits 23-16 of physical address
	.byte 0x93	| access rights byte
	.word 0		| reserved
					| BIOS CS descriptor
	.word 0		| segment length (limit)
	.word 0		| bits 15-0 of physical address
	.byte 0		| bits 23-16 of physical address
	.byte 0		| access rights byte
	.word 0		| reserved
					| stack segment descriptor
	.word 0		| segment length (limit)
	.word 0		| bits 15-0 of physical address
	.byte 0		| bits 23-16 of physical address
	.byte 0		| access rights byte
	.word 0		| reserved

|
|
|  Execute a transfer between user memory and extended memory.
|
|  status = em_xfer(source, dest, count);
|
|    Where:
|       status => return code (0 => OK)
|       source => Physical source address (32-bit)
|       dest   => Physical destination address (32-bit)
|       count  => Number of words to transfer
|
|
|
_em_xfer:

	push	bp		| Save registers
	mov	bp,sp
	push	si
	push	es
	push	cx
|
|  Pick up source and destination addresses and update descriptor tables
|
	mov ax,4(bp)
	seg cs
	mov srcl,ax
	mov ax,6(bp)
	seg cs
	movb srch,al
	mov ax,8(bp)
	seg cs
	mov tgtl,ax
	mov ax,10(bp)
	seg cs
	movb tgth,al
|
|  Update descriptor table segment limits
|
	mov cx,12(bp)
	mov ax,cx
	add ax,ax
	seg cs
	mov tgtsz,ax
	seg cs
	mov srcsz,ax
|
|  Now do actual DOS call
|
	push cs
	pop es
	seg cs
	mov si,#gdt
	movb ah,#0x87
	pushf
	int 0x15		| Do a far call to BIOS routine
|
|  All done, return to caller.
|

	pop	cx		| restore registers
	pop	es
	pop	si
	mov	sp,bp
	pop	bp
	ret

|*===========================================================================
|*				ack_char
|*===========================================================================
| Acknowledge character from keyboard for PS/2

_ack_char:
	push dx
	mov dx,#0x69
	in
	xor ax,#0x10
	out
	xor ax,#0x10
	out

	mov dx,#0x66
	movb ah,#0x10
	in
	notb ah
	andb al,ah
	out
	jmp frw1
frw1:	notb ah
	orb al,ah
	out
	jmp frw2
frw2:	notb ah
	andb al,ah
	out
	
	pop dx
	ret


|*===========================================================================*
|*				save_tty_vec				     *
|*===========================================================================*
| Save the tty vector 0x71 (PS/2)
_save_tty_vec:
	push es
	xor ax,ax
	mov es,ax
	seg es
	mov ax,452
	mov tty_vec1,ax
	seg es
	mov ax,454
	mov tty_vec2,ax
	pop es
	ret

|*===========================================================================*
|*				reboot & wreboot			     *
|*===========================================================================*
| This code reboots the PC

_reboot:
	cli			| disable interrupts
	mov ax,#0x20		| re-enable interrupt controller
	out 0x20
	call _eth_stp		| stop the ethernet chip
	call resvec		| restore the vectors in low core
	mov ax,#0x40
	push ds
	mov ds,ax
	mov ax,#0x1234
	mov 0x72,ax
	pop ds
	test _ps,#0xFFFF
	jnz r.1
	mov ax,#0xFFFF
	mov ds,ax
	mov ax,3
	push ax
	mov ax,1
	push ax
	reti
r.1:
	mov ax,_port_65		| restore port 0x65
	mov dx,#0x65
	out
	mov dx,#0x21		| restore interrupt mask port
	mov ax,#0xBC
	out
	sti			| enable interrupts
	int 0x19		| for PS/2 call bios to reboot

_wreboot:
	cli			| disable interrupts
	mov ax,#0x20		| re-enable interrupt controller
	out 0x20
	call _eth_stp		| stop the ethernet chip
	call resvec		| restore the vectors in low core
	xor ax,ax		| wait for character before continuing
	int 0x16		| get char
	mov ax,#0x40
	push ds
	mov ds,ax
	mov ax,#0x1234
	mov 0x72,ax
	pop ds
	test _ps,#0xFFFF
	jnz wr.1
	mov ax,#0xFFFF
	mov ds,ax
	mov ax,3
	push ax
	mov ax,1
	push ax
	reti
wr.1:
	mov ax,_port_65		| restore port 0x65
	mov dx,#0x65
	out
	mov dx,#0x21		| restore interrupt mask port
	mov ax,#0xBC
	out
	sti			| enable interrupts
	int 0x19		| for PS/2 call bios to reboot
		

| Restore the interrupt vectors in low core.
resvec:	cld
	mov cx,#2*71
	mov si,#_vec_table
	xor di,di
	mov es,di
	rep
	movw

	mov ax,tty_vec1		| Restore keyboard interrupt vector for PS/2
	seg es
	mov 452,ax
	mov ax,tty_vec2
	seg es
	mov 454,ax

	ret

| Some library routines use exit, so this label is needed.
| Actual calls to exit cannot occur in the kernel.
.globl _exit
_exit:	sti
	jmp _exit

.data
vidlock:	.word 0		| dummy variable for use with lock prefix
splimit:	.word 0		| stack limit for current task (kernel only)
tmp:		.word 0		| count of bytes already copied
stkoverrun:	.asciz "Kernel stack overrun, task = "
_vec_table:	.zerow 142	| storage for interrupt vectors
tty_vec1:	.word 0		| sorage for vector 0x71 (offset)
tty_vec2:	.word 0		| sorage for vector 0x71 (segment)
