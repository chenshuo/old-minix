#
!*===========================================================================*
!*		rs232 interrupt handlers for real and protected modes	     *
!*===========================================================================*
! This is a fairly direct translation of the interrupt handlers in rs232.c.
! See the comments there.
! It is about 5 times as efficient, by avoiding save/restart and slow function
! calls for port i/o as well as the compiler!

#include <minix/config.h>
#include <minix/const.h>
#include "const.h"
#include "protect.h"
#include "sconst.h"

#if !C_RS232_INT_HANDLERS /* otherwise, don't use anything in this file */

#undef MINOR

#define add1_and_align(n)	[[[n]+SIZEOF_INT] & [-[SIZEOF_INT-1]-1]]

! These constants are defined in tty.h. That has C stuff so can`t be included.
EVENT_THRESHOLD		=	64
RS_IBUFSIZE		=	256

! These constants are defined in rs232.c.
IS_LINE_STATUS_CHANGE	=	6
IS_MODEM_STATUS_CHANGE	=	0
IS_NO_INTERRUPT		=	1
IS_RECEIVER_READY	=	4
IS_TRANSMITTER_READY	=	2
LS_OVERRUN_ERR		=	2
LS_PARITY_ERR		=	4
LS_FRAMING_ERR		=	8
LS_BREAK_INTERRUPT	=	0x10
LS_TRANSMITTER_READY	=	0x20
MC_DTR			=	1
MC_OUT2			=	8
MS_CTS			=	0x10
MS_RLSD			=	0x80
MS_DRLSD		=	0x08
ODEVHUP			=	0x80
ODEVREADY		=	MS_CTS
ODONE			=	1
OQUEUED			=	0x20
ORAW			=	2
OSWREADY		=	0x40
OWAKEUP			=	4
RS_IHIGHWATER		=	3*RS_IBUFSIZE/4

! These port offsets are hard-coded in rs232.c.
XMIT_OFFSET		=	0
RECV_OFFSET		=	0
INT_ID_OFFSET		=	2
MODEM_CTL_OFFSET	=	4
LINE_STATUS_OFFSET	=	5
MODEM_STATUS_OFFSET	=	6

! Offsets in struct rs232_s. They must match rs232.c
MINOR			=	0
IDEVREADY		=	MINOR+SIZEOF_INT
ITTYREADY		=	IDEVREADY+1
IBUF			=	add1_and_align(ITTYREADY)
IBUFEND			=	IBUF+SIZEOF_INT
IHIGHWATER		=	IBUFEND+SIZEOF_INT
IPTR			=	IHIGHWATER+SIZEOF_INT
OSTATE			=	IPTR+SIZEOF_INT
OXOFF			=	OSTATE+1
OBUFEND			=	add1_and_align(OXOFF)
OPTR			=	OBUFEND+SIZEOF_INT
XMIT_PORT		=	OPTR+SIZEOF_INT
RECV_PORT		=	XMIT_PORT+SIZEOF_INT
DIV_LOW_PORT		=	RECV_PORT+SIZEOF_INT
DIV_HI_PORT		=	DIV_LOW_PORT+SIZEOF_INT
INT_ENAB_PORT		=	DIV_HI_PORT+SIZEOF_INT
INT_ID_PORT		=	INT_ENAB_PORT+SIZEOF_INT
LINE_CTL_PORT		=	INT_ID_PORT+SIZEOF_INT
MODEMCTL_PORT		=	LINE_CTL_PORT+SIZEOF_INT
LINESTATUS_PORT		=	MODEMCTL_PORT+SIZEOF_INT
MODEMSTATUS_PORT	=	LINESTATUS_PORT+SIZEOF_INT
LSTATUS			=	MODEMSTATUS_PORT+SIZEOF_INT
FRAMING_ERRORS		=	add1_and_align(LSTATUS)
OVERRUN_ERRORS		=	FRAMING_ERRORS+SIZEOF_INT
PARITY_ERRORS		=	OVERRUN_ERRORS+SIZEOF_INT
BREAK_INTERRUPTS	=	PARITY_ERRORS+SIZEOF_INT
IBUF1			=	BREAK_INTERRUPTS+SIZEOF_INT
IBUF2			=	IBUF1+RS_IBUFSIZE+1
SIZEOF_STRUCT_RS232_S	=	add1_and_align(IBUF2+RS_IBUFSIZE)

#if _WORD_SIZE == 4
!*===========================================================================*
!*				386 code				     *
!*===========================================================================*

! sections
.sect .text; .sect .rom; .sect .data; .sect .bss

! exported functions

.sect .text
.define		_rs232_1handler
.define		_rs232_2handler

! imported functions

.extern		save
.extern		_tty_wakeup

! imported variables

.sect .bss
.extern		_rs_lines
.extern		_tty_events

.sect .text

! input interrupt

inint:
	addb	dl, RECV_OFFSET-INT_ID_OFFSET
	inb	dx
	mov	ebx, IPTR(esi)
	movb	(ebx), al
	cmp	ebx, IBUFEND(esi)
	jge	checkxoff
	inc	(_tty_events)
	inc	ebx
	mov	IPTR(esi), ebx
	cmp	ebx, IHIGHWATER(esi)
	jne	checkxoff
	addb	dl, MODEM_CTL_OFFSET-RECV_OFFSET
	movb	al, MC_OUT2+MC_DTR
	outb	dx
	movb	IDEVREADY(esi), FALSE
checkxoff:
	testb	ah, ORAW
	jne	rsnext
	cmpb	al, OXOFF(esi)
	je	gotxoff
	testb	ah, OSWREADY
	jne	rsnext
	orb	ah, OSWREADY
	mov	edx, LINESTATUS_PORT(esi)
	inb	dx
	testb	al, LS_TRANSMITTER_READY
	je	rsnext
	addb	dl, XMIT_OFFSET-LINE_STATUS_OFFSET
	jmp	outint1

gotxoff:
	andb	ah, ~OSWREADY
	jmp	rsnext

! PUBLIC void int _rs232_2handler(int irq);

_rs232_2handler:
	mov	esi, _rs_lines+SIZEOF_STRUCT_RS232_S
	jmp	common

! PUBLIC void int _rs232_1handler(int irq);

_rs232_1handler:
	mov	esi, _rs_lines

common:
	cli
	movb	ah, OSTATE(esi)
	mov	edx, INT_ID_PORT(esi)
	inb	dx
rsmore:
	cmpb	al, IS_RECEIVER_READY
	je	inint
	cmpb	al, IS_TRANSMITTER_READY
	je	outint
	cmpb	al, IS_MODEM_STATUS_CHANGE
	je	modemint
	cmpb	al, IS_LINE_STATUS_CHANGE
	jne	rsdone		! fishy

! line status change interrupt

	addb	dl, LINE_STATUS_OFFSET-INT_ID_OFFSET
	inb	dx
	testb	al, LS_FRAMING_ERR
	je	over_framing_error
	inc	FRAMING_ERRORS(esi)	
over_framing_error:
	testb	al, LS_OVERRUN_ERR
	je	over_overrun_error
	inc	OVERRUN_ERRORS(esi)	
over_overrun_error:
	testb	al, LS_PARITY_ERR
	je	over_parity_error
	inc	PARITY_ERRORS(esi)
over_parity_error:
	testb	al, LS_BREAK_INTERRUPT
	je	over_break_interrupt
	inc	BREAK_INTERRUPTS(esi)
over_break_interrupt:

rsnext:
	mov	edx, INT_ID_PORT(esi)
	inb	dx
	cmpb	al, IS_NO_INTERRUPT
	jne	rsmore
rsdone:
	testb	ah, OWAKEUP
	jne	owakeup
	movb	OSTATE(esi), ah
	mov	eax, 1		! reenable RS232 interrupt
	ret

! output interrupt

outint:
	addb	dl, XMIT_OFFSET-INT_ID_OFFSET
outint1:
	cmpb	ah, ODEVREADY+OQUEUED+OSWREADY
	jb	rsnext		! not all are set
	mov	ebx, OPTR(esi)
	movb	al, (ebx)
	outb	dx
	inc	ebx
	mov	OPTR(esi), ebx
	cmp	ebx, OBUFEND(esi)
	jb	rsnext
	add	(_tty_events), EVENT_THRESHOLD
	xorb	ah, ODONE+OQUEUED+OWAKEUP	! OQUEUED off, others on
	jmp	rsnext		! direct exit might lose interrupt

! modem status change interrupt

modemint:
	addb	dl, MODEM_STATUS_OFFSET-INT_ID_OFFSET
	inb	dx

	testb	al, MS_RLSD	! hungup = MS_RLSD false and MS_DRLSD true
	jne	m_handshake
	testb	al, MS_DRLSD
	je	m_handshake
	orb	ah, ODEVHUP
	inc	(_tty_events)

m_handshake:
#if NO_HANDSHAKE
	orb	al, MS_CTS
#endif
	testb	al, MS_CTS
	jne	m_devready
	andb	ah, ~ODEVREADY
	jmp	rsnext

m_devready:
	testb	ah, ODEVREADY
	jne	rsnext
	orb	ah, ODEVREADY
	addb	dl, LINE_STATUS_OFFSET-MODEM_STATUS_OFFSET
	inb	dx
	testb	al, LS_TRANSMITTER_READY
	je	rsnext
	addb	dl, XMIT_OFFSET-LINE_STATUS_OFFSET
	jmp	outint1

! special exit for output just completed

owakeup:
	andb	ah, ~OWAKEUP
	movb	OSTATE(esi), ah

	sti			! enable interrupts
	call	_tty_wakeup
	mov	eax, 1		! reenable RS232 interrupt
	ret

#else
!*===========================================================================*
!*				8086 code				     *
!*===========================================================================*

! exported functions

	.text
.define		_rs232_1handler
.define		_rs232_2handler

! imported functions

.extern		save
.extern		_tty_wakeup

! imported variables

	.bss
.extern		_rs_lines
.extern		_tty_events

	.text

! input interrupt

inint:
	addb	dl,#RECV_OFFSET-INT_ID_OFFSET
	inb
	mov	bx,IPTR(si)
	movb	(bx),al
	cmp	bx,IBUFEND(si)
	jae	checkxoff
	inc	_tty_events
	inc	bx
	mov	IPTR(si),bx
	cmp	bx,IHIGHWATER(si)
	jne	checkxoff
	addb	dl,#MODEM_CTL_OFFSET-RECV_OFFSET
	movb	al,#MC_OUT2+MC_DTR
	outb
	movb	IDEVREADY(si),#FALSE
checkxoff:
	testb	ah,#ORAW
	jne	rsnext
	cmpb	al,OXOFF(si)
	je	gotxoff
	testb	ah,#OSWREADY
	jne	rsnext
	orb	ah,#OSWREADY
	mov	dx,LINESTATUS_PORT(si)
	inb
	testb	al,#LS_TRANSMITTER_READY
	je	rsnext
	addb	dl,#XMIT_OFFSET-LINE_STATUS_OFFSET
	jmp	outint1

gotxoff:
	andb	ah,#~OSWREADY
	jmp	rsnext

! PUBLIC void int _rs232_2handler(int irq);

_rs232_2handler:
	mov	si,#_rs_lines+SIZEOF_STRUCT_RS232_S
	jmp	common

! PUBLIC void int _rs232_1handler(int irq);

_rs232_1handler:
	mov	si,#_rs_lines

common:
	cli
	movb	ah,OSTATE(si)
	mov	dx,INT_ID_PORT(si)
	inb
rsmore:
	cmpb	al,#IS_RECEIVER_READY
	je	inint
	cmpb	al,#IS_TRANSMITTER_READY
	je	outint
	cmpb	al,#IS_MODEM_STATUS_CHANGE
	je	modemint
	cmpb	al,#IS_LINE_STATUS_CHANGE
	jne	rsdone		! fishy

! line status change interrupt

	addb	dl,#LINE_STATUS_OFFSET-INT_ID_OFFSET
	inb
	testb	al,#LS_FRAMING_ERR
	je	over_framing_error
	inc	FRAMING_ERRORS(si)	
over_framing_error:
	testb	al,#LS_OVERRUN_ERR
	je	over_overrun_error
	inc	OVERRUN_ERRORS(si)	
over_overrun_error:
	testb	al,#LS_PARITY_ERR
	je	over_parity_error
	inc	PARITY_ERRORS(si)
over_parity_error:
	testb	al,#LS_BREAK_INTERRUPT
	je	over_break_interrupt
	inc	BREAK_INTERRUPTS(si)
over_break_interrupt:

rsnext:
	mov	dx,INT_ID_PORT(si)
	inb
	cmpb	al,#IS_NO_INTERRUPT
	jne	rsmore
rsdone:
	testb	ah,#OWAKEUP
	jne	owakeup
	movb	OSTATE(si),ah
	mov	ax,#1		! reenable RS232 interrupt
	ret

! output interrupt

outint:
	addb	dl,#XMIT_OFFSET-INT_ID_OFFSET
outint1:
	cmpb	ah,#ODEVREADY+OQUEUED+OSWREADY
	jb	rsnext		! not all are set
	mov	bx,OPTR(si)
	movb	al,(bx)
	outb
	inc	bx
	mov	OPTR(si),bx
	cmp	bx,OBUFEND(si)
	jb	rsnext
	add	_tty_events,#EVENT_THRESHOLD
	xorb	ah,#ODONE+OQUEUED+OWAKEUP	! OQUEUED off, others on
	jmp	rsnext		! direct exit might lose interrupt

! modem status change interrupt

modemint:
	addb	dl,#MODEM_STATUS_OFFSET-INT_ID_OFFSET
	inb

	testb	al, #MS_RLSD	! hungup = MS_RLSD false and MS_DRLSD true
	jne	m_handshake
	testb	al, #MS_DRLSD
	je	m_handshake
	orb	ah, #ODEVHUP
	inc	_tty_events

m_handshake:
#if NO_HANDSHAKE
	orb	al,#MS_CTS
#endif
	testb	al,#MS_CTS
	jne	m_devready
	andb	ah,#~ODEVREADY
	jmp	rsnext

m_devready:
	testb	ah,#ODEVREADY
	jne	rsnext
	orb	ah,#ODEVREADY
	addb	dl,#LINE_STATUS_OFFSET-MODEM_STATUS_OFFSET
	inb
	testb	al,#LS_TRANSMITTER_READY
	je	rsnext
	addb	dl,#XMIT_OFFSET-LINE_STATUS_OFFSET
	jmp	outint1

! special exit for output just completed

owakeup:
	andb	ah,#~OWAKEUP
	movb	OSTATE(si),ah

	sti			! enable interrupts
	call	_tty_wakeup
	mov	ax,#1		! reenable RS232 interrupt
	ret

#endif /* 386 or 8086 */
#endif /* !C_RS232_INT_HANDLERS */
