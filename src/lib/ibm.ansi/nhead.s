.extern _main, _stackpt, begtext, begdata, begbss, _data_org, _exit, .fat, .trp
.text
begtext:
	jmp L0
	.zerow 13		! stack for inital IRET when common I&D
				! also padding to make INIT_SP same as
				! for separate I&D
L0:	mov sp,_stackpt
	call _main
_exit:				! dummy for library functions - never executed
.fat:				! dummy
.trp:				! dummy
L1:	jmp L1			! this should never be executed either
.data
begdata:
_data_org:			! fs needs to know where build stuffed table
.data2 0xDADA			! magic number for build
.data2 8			! CLICK_SHIFT to check - must match h/const.h
.data2 0,0,0			! used by FS only for sizes of init
				! stack for separate I&D follows
.data2 0,0,0			! for ip:ss:f pushed by debugger traps
.data2 0,0,0			! for cs:ds:ret adr in save()
				! this was missing - a bug as late as V1.3c
				! for ds for new restart() as well
.data2 0,0,0			! for ip:ss:f built by restart()
				! so INIT_SP in const.h must be 0x1C
.bss
begbss:
