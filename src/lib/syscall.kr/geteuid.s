.define	_geteuid
.extern	__geteuid
.align 2
_geteuid: 
j __geteuid
