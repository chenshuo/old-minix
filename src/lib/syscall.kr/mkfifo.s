.define	_mkfifo
.extern	__mkfifo
.align 2
_mkfifo: 
j __mkfifo
