.sect .text
.extern	__stty
.define	_stty
.align 2

_stty:
	jmp	__stty
