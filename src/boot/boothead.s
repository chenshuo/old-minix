!	Boothead.s - BIOS support for boot.c		Author: Kees J. Bot
!
!
! This file contains the startup and low level support for the secondary
! boot program.  It contains functions for disk, tty and keyboard I/O,
! copying memory to arbitrary locations, etc.
!
! The primary bootstrap code supplies the following parameters in registers:
!	dl	= Boot-device.
!	es:si	= Partition table entry if hard disk.
!

.define begtext, begdata, begbss
.data
begdata:
	.ascii	"(null)\0"
.bss
begbss:

	BOOTOFF	   =	0x7C00	! 0x0000:BOOTOFF load a bootstrap here
	LOADSEG    =	0x1000	! Where this code is loaded.
	BUFFER	   =	0x0600	! First free memory
	PENTRYSIZE =	    16	! Partition table entry size.
	DSKBASE    =	   120	! 120 = 4 * 0x1E = ptr to disk parameters
	DSKPARSIZE =	    11	! 11 bytes of floppy parameters
	SECTORS	   =	     4	! Offset into parameters to sectors per track
	a_flags	   =	     2	! From a.out.h, struct exec
	a_text	   =	     8
	a_data	   =	    12
	a_bss	   =	    16
	a_total	   =	    24
	A_SEP	   =	  0x20	! Separate I&D flag.

! Imported variables and functions:
.extern _caddr, _daddr, _runsize, _edata, _end	! Runtime environment
.extern _device, _dskpars, _heads, _sectors	! Boot disk parameters
.extern _rem_part				! To pass partition info

.text
begtext:
.extern _boot, _printk				! Boot Minix, kernel printf

! Set segment registers and stack pointer using the programs own header!
! The header is either 32 bytes (short form) or 48 bytes (long form).  The
! bootblock will jump to address 0x10030 in both cases, calling one of the
! two jmpf instructions below.

	jmpf	boot, LOADSEG+3	! Set cs right (skipping long a.out header)
	.space	11		! jmpf + 11 = 16 bytes
	jmpf	boot, LOADSEG+2	! Set cs right (skipping short a.out header)
boot:
	mov	ax, #LOADSEG
	mov	ds, ax		! ds = header

	movb	al, a_flags
	testb	al, #A_SEP	! Separate I&D?
	jnz	sepID
comID:	xor	ax, ax
	xchg	ax, a_text	! No text
	add	a_data, ax	! Treat all text as data
sepID:
	mov	ax, a_total	! Total nontext memory usage
	and	ax, #0xFFFE	! Round down to even
	mov	a_total, ax	! total - text = data + bss + heap + stack
	cli			! Ignore interrupts while stack in limbo
	mov	sp, ax		! Set sp at the top of all that

	mov	ax, a_text	! Determine offset of ds above cs
	movb	cl, #4
	shr	ax, cl
	mov	cx, cs
	add	ax, cx
	mov	ds, ax		! ds = cs + text / 16
	mov	ss, ax
	sti			! Stack ok now
	push	es		! Save es, we need it for the partition table
	mov	es, ax
	cld			! C compiler wants UP

! Clear bss
	xor	ax, ax		! Zero
	mov	di, #_edata	! Start of bss is at end of data
	mov	cx, #_end	! End of bss (begin of heap)
	sub	cx, di		! Number of bss bytes
	shr	cx, #1		! Number of words
	rep
	stos			! Clear bss

! Copy primary boot parameters to variables.  (Can do this now that bss is
! cleared and may be written into).
	xorb	dh, dh
	mov	_device, dx	! Boot device (probably 0x00 or 0x80)
	mov	_rem_part+0, si	! Remote partition table offset
	pop	_rem_part+2	! and segment (saved es)

! Give C code access to the code segment, data segment and the size of this
! process.
	xor	ax, ax
	mov	dx, cs
	call	seg2abs
	mov	_caddr+0, ax
	mov	_caddr+2, dx
	xor	ax, ax
	mov	dx, ds
	call	seg2abs
	mov	_daddr+0, ax
	mov	_daddr+2, dx
	push	ds
	mov	ax, #LOADSEG
	mov	ds, ax		! Back to the header once more
	mov	ax, a_total+0
	mov	dx, a_total+2	! dx:ax = data + bss + heap + stack
	add	ax, a_text+0
	adc	dx, a_text+2	! dx:ax = text + data + bss + heap + stack
	pop	ds
	add	ax, #0x000F
	adc	dx, #0
	and	ax, #0xFFF0	! Round up to a segment
	mov	_runsize+0, ax
	mov	_runsize+2, dx	! 32 bit size of this process

! Time to switch to a higher level language (not much higher)
	call	_boot

.define	_exit, __exit, ___exit, _main	! Make various compilers happy
.define _reboot				! Normal reboot entry point
_exit:
__exit:
___exit:
_main:
_reboot:
	mov	ax, #any_key
	push	ax
	call	_printk
	call	_getchar
	int	0x19
.data
any_key:
	.ascii	"\nHit any key to reboot\n\0"
.text

! Alas we need some low level support
!
! u32_t mon2abs(void *ptr)
!	Address in monitor data to absolute address.
.define _mon2abs
_mon2abs:
	mov	bx, sp
	mov	ax, 2(bx)	! ptr
	mov	dx, ds		! Monitor data segment
	jmp	seg2abs

! u32_t vec2abs(vector *vec)
!	8086 interrupt vector to absolute address.
.define _vec2abs
_vec2abs:
	mov	bx, sp
	mov	bx, 2(bx)
	mov	ax, (bx)
	mov	dx, 2(bx)	! dx:ax vector
	!jmp	seg2abs		! Translate

seg2abs:			! Translate dx:ax to the 32 bit address dx-ax
	push	bx
	push	cx
	mov	bx, ax
	mov	ax, dx
	xor	dx, dx		! Segment number in dx-ax
	mov	cx, #4
shld4:	shl	ax, #1
	rcl	dx, #1		! dx-ax <<= 4
	loop	shld4
	add	ax, bx		! Add offset
	adc	dx, #0
	pop	cx
	pop	bx
	ret

abs2seg:			! Translate the 32 bit address dx-ax to dx:ax
	push	bx
	push	cx
	mov	bx, ax
	and	ax, #0x000F	! Offset in ax
	mov	cx, #4
shrd4:	shr	dx, #1
	rcr	bx, #1		! dx-bx >>= 4
	loop	shrd4
	mov	dx, bx		! Segment in dx
	pop	cx
	pop	bx
	ret

! void raw_copy(u32_t dstaddr, u32_t srcaddr, u32_t count)
!	Copy count bytes from srcaddr to dstaddr.  Don't do overlaps.
!	Also handles copying words to extended memory.
.define _raw_copy
_raw_copy:
	push	bp
	mov	bp, sp
	push	si
	push	di		! Save C variable registers
copy:
	cmp	14(bp), #0
	jnz	bigcopy
	mov	cx, 12(bp)
	jcxz	copydone	! Count is zero, end copy
	cmp	cx, #0xFFF0
	jb	smallcopy
bigcopy:mov	cx, #0xFFF0	! Don't copy more than about 64K at once
smallcopy:
	push	cx		! Save copying count
	mov	ax, 4(bp)
	mov	dx, 6(bp)
	cmp	dx, #0x0010	! Copy to extended memory?
	jae	ext_copy
	call	abs2seg
	mov	di, ax
	mov	es, dx		! es:di = dstaddr
	mov	ax, 8(bp)
	mov	dx, 10(bp)
	call	abs2seg
	mov	si, ax
	mov	ds, dx		! ds:si = srcaddr
	mov	ax, cx		! Save copying count
	shr	cx, #1		! Words to move
	rep
	movs			! Do the word copy
	adc	cx, cx		! One more byte?
	rep
	movsb			! Do the byte copy
	jmp	copyadjust
ext_copy:
	mov	x_dst_desc+2, ax
	movb	x_dst_desc+4, dl ! Set base of destination segment
	mov	ax, 8(bp)
	mov	dx, 10(bp)
	mov	x_src_desc+2, ax
	movb	x_src_desc+4, dl ! Set base of source segment
	mov	si, #x_gdt	! es:si = global descriptor table
	shr	cx, #1		! Words to move
	movb	ah, #0x87	! Code for extended memory move
	int	0x15
copyadjust:
	pop	cx		! Restore count
	add	4(bp), cx
	adc	6(bp), #0	! srcaddr += copycount
	add	8(bp), cx
	adc	10(bp), #0	! dstaddr += copycount
	sub	12(bp), cx
	sbb	14(bp), #0	! count -= copycount
	jmp	copy		! and repeat
copydone:
	mov	ax, ss		! Restore ds and es from the remaining ss
	mov	ds, ax
	mov	es, ax
	pop	di
	pop	si		! Restore C variable registers
	pop	bp
	ret

! u16_t get_word(u32_t addr);
! void put_word(u32_t addr, u16_t word);
!	Read or write a 16 bits word at an arbitrary location.
.define	_get_word, _put_word
_get_word:
	mov	bx, sp
	call	gp_getaddr
	mov	ax, (bx)	! Word to get from addr
	jmp	gp_ret
_put_word:
	mov	bx, sp
	push	6(bx)		! Word to store at addr
	call	gp_getaddr
	pop	(bx)		! Store the word
	jmp	gp_ret
gp_getaddr:
	mov	ax, 2(bx)
	mov	dx, 4(bx)
	call	abs2seg
	mov	bx, ax
	mov	ds, dx		! ds:bx = addr
	ret
gp_ret:
	push	es
	pop	ds		! Restore ds
	ret

! void relocate(void);
!	After the program has copied itself to a safer place, it needs to change
!	the segment registers.  Caddr has already been set to the new location.
.define _relocate
_relocate:
	pop	bx		! Return address
	mov	ax, _caddr+0
	mov	dx, _caddr+2
	call	abs2seg
	mov	cx, dx		! cx = new code segment
	mov	ax, cs		! Old code segment
	sub	ax, cx		! ax = -(new - old) = -Moving offset
	mov	dx, ds
	sub	dx, ax
	mov	ds, dx		! ds += (new - old)
	mov	es, dx
	mov	ss, dx
	xor	ax, ax
	call	seg2abs
	mov	_daddr+0, ax
	mov	_daddr+2, dx	! New data address
	push	cx		! New text segment
	push	bx		! Return offset of this function
	retf			! Relocate

! void *brk(void *addr)
! void *sbrk(size_t incr)
!	Cannot fail implementations of brk(2) and sbrk(3), so we can use
!	malloc(3).  They reboot on stack collision instead of returning -1.
.data
	.align	2
break:	.data2	_end		! A fake heap pointer
.text
.define _brk, __brk, _sbrk, __sbrk
_brk:
__brk:				! __brk is for the standard C compiler
	xor	ax, ax
	jmp	sbrk		! break= 0; return sbrk(addr);
_sbrk:
__sbrk:
	mov	ax, break	! ax= current break
sbrk:	push	ax		! save it as future return value
	mov	bx, sp		! Stack is now: (retval, retaddr, incr, ...)
	add	ax, 4(bx)	! ax= break + increment
	mov	break, ax	! Set new break
	lea	dx, -1024(bx)	! sp minus a bit of breathing space
	cmp	dx, ax		! Compare with the new break
	jb	heaperr		! Suffocating noises
	lea	dx, -4096(bx)	! A warning when heap+stack goes < 4K
	cmp	dx, ax
	jae	plenty		! No reason to complain
	mov	ax, #memwarn
	push	ax
	call	_printk		! Warn about memory running low
	pop	ax
	movb	memwarn, #0	! No more warnings
plenty:	pop	ax		! Return old break (0 for brk)
	ret
heaperr:mov	ax, #chmem
	push	ax
	mov	ax, #nomem
	push	ax
	call	_printk
	call	_reboot
.data
nomem:	.ascii	"\nOut of%s\0"
memwarn:.ascii	"\nLow on"
chmem:	.ascii	" memory, use chmem to increase the heap\n\0"
.text

! int dev_geometry(void);
!	Given the device "_device" and floppy disk parameters "_dskpars",
!	set the number of heads and sectors.  It returns true iff the device
!	exists.
.define	_dev_geometry
_dev_geometry:
	movb	dl, _device	! The default device
	cmpb	dl, #0x80	! Floppy < 0x80, winchester >= 0x80
	jae	winchester
floppy:
	int	0x11		! Get equipment configuration
	testb	al, #0x01	! Bit 0 set if floppies available
	jz	geoerr		! No floppy drives on this box
	shl	ax, #1		! Highest floppy drive # in bits 6-7
	shl	ax, #1		! Now in bits 0-1 of ah
	andb	ah, #0x03	! Extract bits
	cmpb	dl, ah		! Must be dl <= ah for drive to exist
	ja	geoerr		! Alas no drive dl.
	movb	dh, #2
	movb	_heads, dh	! Number of heads for this device
	mov	bx, #_dskpars	! bx = disk parameters
	movb	cl, SECTORS(bx)
	movb	_sectors, cl	! Sectors per track
	xor	ax, ax
	mov	es, ax		! es = 0 = vector segment
	eseg
	mov	DSKBASE+0, bx
	eseg
	mov	DSKBASE+2, ds	! DSKBASE+2:DSKBASE+0 = ds:bx = floppy parms
	jmp	geoboth
winchester:
	movb	ah, #0x08	! Code for drive parameters
	int	0x13		! dl still contains drive
	jb	geoerr		! No such drive?
	andb	cl, #0x3F	! cl = max sector number (1-origin)
	movb	_sectors, cl	! Number of sectors per track
	incb	dh		! dh = 1 + max head number (0-origin)
	movb	_heads, dh
geoboth:
	mov	ax, #1		! Code for success
	push	ax
	movb	al, cl		! al = sectors per track
	mulb	dh		! ax = heads * sectors
	mov	secspcyl, ax	! Sectors per cylinder = heads * sectors
geodone:push	ds
	pop	es		! Restore es register
	pop	ax		! Return code
	ret
geoerr:	xor	ax, ax
	push	ax
	jmp	geodone
.bss
secspcyl:	.space 1*2
.text

! int readsectors(u32_t bufaddr, u32_t sector, u8_t count)
! int writesectors(u32_t bufaddr, u32_t sector, u8_t count)
!	Read/write several sectors from/to disk or floppy.  The buffer must
!	be between 64K boundaries!  Count must fit in a byte.  The external
!	variables _device, _sectors and _heads describe the disk and its
!	geometry.  Returns 0 for success, otherwise the BIOS error code.
!
.define _readsectors, _writesectors
_writesectors:
	push	bp
	mov	bp, sp
	movb	13(bp), #3	! Code for a disk write
	jmp	rwsec
_readsectors:
	push	bp
	mov	bp, sp
	movb	13(bp), #2	! Code for a disk read
rwsec:	push	di
	push	es
	mov	ax, 4(bp)
	mov	dx, 6(bp)
	call	abs2seg
	mov	bx, ax
	mov	es, dx		! es:bx = bufaddr
	mov	di, #3		! Execute 3 resets on floppy error
	cmpb	_device, #0x80
	jb	nohd
	mov	di, #1		! But only 1 reset on hard disk error
nohd:	cmpb	12(bp), #0	! count equals zero?
	jz	done
more:	mov	ax, 8(bp)
	mov	dx, 10(bp)	! dx:ax = abs sector.  Divide it by sectors/cyl
	div	secspcyl	! ax = cylinder, dx = sector within cylinder
	xchg	ax, dx		! ax = sector within cylinder, dx = cylinder
	movb	ch, dl		! ch = low 8 bits of cylinder
	divb	_sectors	! al = head, ah = sector (0-origin)
	xorb	dl, dl		! About to shift bits 8-9 of cylinder into dl
	shr	dx, #1
	shr	dx, #1		! dl[6..7] = high cylinder
	orb	dl, ah		! dl[0..5] = sector (0-origin)
	movb	cl, dl		! cl[0..5] = sector, cl[6..7] = high cyl
	incb	cl		! cl[0..5] = sector (1-origin)
	movb	dh, al		! dh = head
	movb	dl, _device	! dl = device to use
	movb	al, _sectors	! Sectors per track - Sector number (0-origin)
	subb	al, ah		! = Sectors left on this track
	cmpb	al, 12(bp)	! Compare with # sectors to transfer
	jbe	doit		! Can't go past the end of a cylinder?
	movb	al, 12(bp)	! 12(bp) < sectors left on this track
doit:	movb	ah, 13(bp)	! Code for disk read (2) or write (3)
	push	ax		! Save al = sectors to read
	int	0x13		! call the BIOS to do the transfer
	pop	cx		! Restore al in cl
	jb	ioerr		! I/O error
	movb	al, cl		! Restore al = sectors read
	addb	bh, al		! bx += 2 * al * 256 (add bytes transferred)
	addb	bh, al		! es:bx = where next sector is located
	add	8(bp), ax	! Update address by sectors transferred
	adc	10(bp), #0	! Don't forget high word
	subb	12(bp), al	! Decrement sector count by sectors transferred
	jnz	more		! Not all sectors have been transferred
done:	xorb	ah, ah		! No error here!
	jmp	finish
ioerr:	dec	di		! Do we allow another reset?
	jl	finish		! No, report the error
	xorb	ah, ah		! Code for a reset (0)
	int	0x13
	jae	more		! Succesful reset, try request again
finish:	movb	al, ah
	xorb	ah, ah		! ax = error number
	pop	es
	pop	di
	pop	bp
	ret

! int getchar(void), peekchar(void);
!	Read a character from the keyboard, or just look if there is one.
!	A carriage return is changed into a linefeed for UNIX compatibility.
.define _getchar, _peekchar
_peekchar:
	movb	ah, #1		! Keyboard status
	int	0x16
	jnz	getc		! Keypress?
	xor	ax, ax		! No key
	ret
_getchar:
	movb	ah, #0		! Read character from keyboard
	int	0x16
getc:	cmpb	al, #0x0D	! Carriage return?
	jnz	nocr
	movb	al, #0x0A	! Change to linefeed
nocr:	xorb	ah, ah		! ax = al
	ret

! int putchar(int c);
!	Write a character in teletype mode.  The putc and putk synonyms
!	are for the kernel printk function that uses one of them.
!	Newlines are automatically prepended by a carriage return.
!
.define _putchar, _putc, _putk
_putchar:
_putc:
_putk:	mov	bx, sp
	movb	al, 2(bx)	! al = character to be printed
	testb	al, al		! 1.6.* printk prints null characters
	jz	nulch		! that appear as blanks, so don't do it.
	cmpb	al, #0x0A	! al = newline?
	jnz	putc		! No
	movb	al, #0x0D	! putc('\r')
	call	putc
	movb	al, #0x0A	! Restore the '\n' and print it
putc:	movb	ah, #14		! 14 = print character in teletype mode
	mov	bx, #0x0001	! Page 0, foreground color
	int	0x10		! Call BIOS VIDEO_IO
nulch:	ret

! void reset_video(unsigned mode);
!	Reset and clear the screen.
.define _reset_video
_reset_video:
	mov	bx, sp
	mov	ax, 2(bx)	! Video mode
	testb	ah, ah
	jnz	xvesa		! VESA extended mode?
	int	0x10		! Reset video (ah = 0)
	jmp	setcur
xvesa:	mov	bx, ax		! bx = extended mode
	mov	ax, #0x4F02	! Reset video
	int	0x10
setcur:	xor	dx, dx		! dl = column = 0, dh = row = 0
	xorb	bh, bh		! Page 0
	movb	ah, #0x02	! Set cursor position
	int	0x10
	ret

! u32_t get_tick(void);
!	Return the current value of the clock tick counter.  This counter
!	increments 18.2 times per second.  Poll it to do delays.  Does not
!	work on the original PC, but works on the PC/XT.
.define _get_tick
_get_tick:
	xorb	ah, ah		! Code for get tick count
	int	0x1A
	mov	ax, dx
	mov	dx, cx		! dx:ax = cx:dx = tick count
	ret


! Functions used to obtain info about the hardware, like the type of video
! and amount of memory.  Boot uses this information itself, but will also
! pass them on to a pure 386 kernel, because one can't make BIOS calls from
! protected mode.  The video type could probably be determined by the kernel
! too by looking at the hardware, but there is a small chance on errors that
! the monitor allows you to correct by setting variables.

.define		_get_video	! returns type of display
.define		_get_memsize	! returns amount of low memory in K
.define		_get_ext_memsize  ! returns amount of extended memory in K

! u16_t get_video(void)
!	Return type of display, in order: MDA, CGA, mono EGA, color EGA,
!	mono VGA, color VGA.

_get_video:
	mov	ax, #0x1A00	! Function 1A returns display code
	int	0x10		! al = 1A if supported
	cmpb	al, #0x1A
	jnz	no_dc		! No display code function supported

	mov	ax, #2
	cmpb	bl, #5		! Is it a monochrome EGA?
	jz	got_video
	inc	ax
	cmpb	bl, #4		! Is it a color EGA?
	jz	got_video
	inc	ax
	cmpb	bl, #7		! Is it a monochrome VGA?
	jz	got_video
	inc	ax
	cmpb	bl, #8		! Is it a color VGA?
	jz	got_video

no_dc:	movb	ah, #0x12	! Get information about the EGA
	movb	bl, #0x10
	int	0x10
	cmpb	bl, #0x10	! Did it come back as 0x10? (No EGA)
	jz	no_ega

	mov	ax, #2
	cmpb	bh, #1		! Is it monochrome?
	jz	got_video
	inc	ax
	jmp	got_video

no_ega:	int	0x11		! Get bit pattern for equipment
	and	ax, #0x30	! Isolate color/mono field
	sub	ax, #0x30
	jz	got_video	! Is it an MDA?
	mov	ax, #1		! No it's CGA

got_video:
	ret

! u16_t get_memsize(void);
!	Ask the BIOS how much normal memory there is.

_get_memsize:
	int	0x12		! Returns the size (in K) in ax
	ret

! u16_t get_ext_memsize(void);
!	Ask the BIOS how much extended memory there is.

_get_ext_memsize:
	call	_getprocessor	! Is this an old crate?
	cmp	ax, #186
	jbe	no_ext		! Don't try the next function, it crashed an XT
	movb	ah, #0x88
	clc			! Carry will stay clear if call exists
	int	0x15		! Returns size (in K) in ax for AT's
	jnb	got_ext_memsize
no_ext:	sub	ax, ax		! Error, probably a PC
got_ext_memsize:
	ret

! Functions to leave the boot monitor by calling a bootstrap, Minix in 16 bit
! mode, or Minix in 32 bit mode.

.define		_bootstrap	! Call another bootstrap
.define		_minix86	! Call 16 bit Minix
.define		_minix386	! Call 32 bit Minix

! void _bootstrap(int device, struct part_entry *entry)
!	Call another bootstrap routine to boot MS-DOS for instance.  (No real
!	need for that anymore, now that you can format floppies under Minix).
!	The bootstrap must have been loaded at BOOTSEG from "device".
_bootstrap:
	mov	bx, sp
	movb	dl, 2(bx)	! Device to boot from
	mov	si, 4(bx)	! ds:si = partition table entry
	xor	ax, ax
	mov	es, ax		! Vector segment
	mov	di, #BUFFER	! es:di = buffer in low core
	mov	cx, #PENTRYSIZE	! cx = size of partition table entry
	rep
	movsb			! Copy the entry to low core
	mov	si, #BUFFER	! es:si = partition table entry
	mov	ds, ax		! Some bootstraps need zero segment registers
	cli
	mov	ss, ax
	mov	sp, #BOOTOFF	! This should do it
	sti
	jmpf	BOOTOFF, 0	! Back to where the BIOS loads the boot code

! To my surprise this code is so fast that floppy drive 0 was still running
! (tries to boot floppy first), when Minix was started after a hard disk boot.
stop_motor:
	mov	dx, #0x03F2	! Motor drive control bits
	movb	al, #0x0C	! Bits 4-7 for floppy 0-3 are off
	outb	dx		! Kill the motors
	ret

! void minix86(u32_t koff, u32_t kcs, u32_t kds,
!					char *bootparams, size_t paramsize);
!	Call 8086 Minix.
_minix86:
	call	stop_motor	! Turn off floppy drives
	mov	bp, sp		! Pointer to arguments
	push	16(bp)		! # bytes of boot parameters
	push	14(bp)		! address of boot parameters
	mov	ax, 6(bp)
	mov	dx, 8(bp)
	call	abs2seg
	push	dx		! Kernel code segment
	push	2(bp)		! Kernel code offset
	mov	ax, 10(bp)
	mov	dx, 12(bp)
	call	abs2seg
	mov	ds, dx		! Kernel data segment
	mov	es, dx		! Set es to kernel data too
	cli			! Disable interrupts
	retf			! Finally out of this mess!

! void minix386(u32_t koff, u32_t kcs, u32_t kds,
!					char *bootparams, size_t paramsize);
!	Call 386 Minix with a 386 mode switch.  Code inspired by the Amoeba
!	386 bootstrap by Leendert van Doorn.
_minix386:
	call	stop_motor	! Turn off floppy drives
	mov	bp, sp		! Pointer to arguments

	mov	dx, ds		! Monitor ds
	mov	ax, #p_gdt	! dx:ax = Global descriptor table
	call	seg2abs
	mov	p_gdt_desc+2, ax
	movb	p_gdt_desc+4, dl ! Set base of kernel data segment

	mov	ax, 10(bp)
	mov	dx, 12(bp)	! Kernel ds (absolute address)
	mov	p_ds_desc+2, ax
	movb	p_ds_desc+4, dl	! Set base of the kernel data segment

	mov	dx, ss		! Monitor ss
	xor	ax, ax		! dx:ax = Monitor stack segment
	call	seg2abs		! Minix starts with the stack of the monitor
	mov	p_ss_desc+2, ax
	movb	p_ss_desc+4, dl

	mov	ax, 6(bp)
	mov	dx, 8(bp)	! Kernel cs (absolute address)
	mov	p_cs_desc+2, ax
	movb	p_cs_desc+4, dl

	xor	ax, ax
	push	ax
	push	16(bp)		! 32 bit size of parameters on stack
	push	ax
	push	14(bp)		! 32 bit address of parameters (ss relative)

! Use the BIOS to kick us into protected mode.  This is the most portable
! way to enable the A20 address line.  A real programmer would use cr0.

	mov	ax, 6(bp)
	mov	dx, 8(bp)
	call	abs2seg		! dx = kernel cs
	mov	si, #p_gdt	! es:si = global descriptor table
	xor	bx, bx		! 8259's must be initialized by the kernel
	movb	ah, #0x89	! Protected mode function code
! Fake an interrupt stack frame as if called from the kernel entry point
	cli			! No more interruptions
	pushf			! Flags
	push	dx		! Kernel cs
	push	2(bp)		! Kernel entry point
	mov	ds, bx		! ds = vector segment
	jmpf	@4*0x15		! "int 0x15"

! The "interrupt" will return directly to the Minix kernel in 386 mode.  The
! split is clean:  No 386 code here, and no 8086 code in the kernel.  The
! boot parameters address and size are on the stack.  They may be retrieved
! using the ss descriptor.


.data
	.align	2

! Global descriptor tables.
	UNSET	= 0		! Must be computed

! For "Extended Memory Block Move".
x_gdt:
x_null_desc:
	! Null descriptor
	.data2	0x0000, 0x0000
	.data1	0x00, 0x00, 0x00, 0x00
x_gdt_desc:
	! Descriptor for this descriptor table
	.data2	6*8-1, UNSET
	.data1	UNSET, 0x00, 0x00, 0x00
x_src_desc:
	! Source segment descriptor
	.data2	0xFFFF, UNSET
	.data1	UNSET, 0x92, 0x00, 0x00
x_dst_desc:
	! Destination segment descriptor
	.data2	0xFFFF, UNSET
	.data1	UNSET, 0x92, 0x00, 0x00
x_bios_desc:
	! BIOS segment descriptor (scratch for int 0x15)
	.data2	UNSET, UNSET
	.data1	UNSET, UNSET, UNSET, UNSET
x_ss_desc:
	! BIOS stack segment descriptor (scratch for int 0x15)
	.data2	UNSET, UNSET
	.data1	UNSET, UNSET, UNSET, UNSET

! For "Switch Processor to Protected Mode".
p_gdt:
p_null_desc:
	! Null descriptor
	.data2	0x0000, 0x0000
	.data1	0x00, 0x00, 0x00, 0x00
p_gdt_desc:
	! Descriptor for this descriptor table
	.data2	8*8-1, UNSET
	.data1	UNSET, 0x00, 0x00, 0x00
p_idt_desc:
	! Interrupt descriptor table descriptor (no interrupts allowed)
	.data2	0x0000, 0x0000
	.data1	0x00, 0x00, 0x00, 0x00
p_ds_desc:
	! Kernel data segment descriptor (4Gb flat)
	.data2	0xFFFF, UNSET
	.data1	UNSET, 0x92, 0xCF, 0x00
p_es_desc:
	! Physical memory descriptor (4Gb flat)
	.data2	0xFFFF, 0x0000
	.data1	0x00, 0x92, 0xCF, 0x00
p_ss_desc:
	! Monitor data segment descriptor (64Kb flat)
	.data2	0xFFFF, UNSET
	.data1	UNSET, 0x92, 0x40, 0x00
p_cs_desc:
	! Kernel code segment descriptor (4Gb flat)
	.data2	0xFFFF, UNSET
	.data1	UNSET, 0x9A, 0xCF, 0x00
p_bios_desc:
	! BIOS segment descriptor (scratch for int 0x15)
	.data2	UNSET, UNSET
	.data1	UNSET, UNSET, UNSET, UNSET
