.define	_getuid
.extern	__getuid
.align 2
_getuid: 
j __getuid
