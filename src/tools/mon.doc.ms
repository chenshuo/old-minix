.SH
Doing more with the Minix Boot Monitor.
.PP
This text describes the menu interface of the Minix Boot Monitor, and
the commands that may be used to customize it.
.LP
First of all, the monitor mode as distributed normally hides some of the
functionality, but shows you an explanation of the environment
variables instead.  If you add -DEXTENDED_LIST to CFLAGS in the Makefile
and recompile, then you will no longer see the long explanation (you
should know it by now), but you will see all the commands the monitor
knows about.
.SH
The commands.
.PP
The boot command has two functions, one is to load and start Minix, the
other is to boot a different operating system.  If the first partition
on your hard disk contains MS-DOS, then
.DS
.B
boot hd1
.R
.DE
will boot MS-DOS.  (Not all operating systems like to be called this
way, some insist on being on the active partition.)
.LP
The delay, ls, and other simple commands are not too difficult to
understand, just try them out.  The trap command may be used to execute
a function after a delay.  You can show a menu first and boot Minix
after 5 seconds of inactivity like this:
.DS
.B
trap 5000 boot; menu
.R
.DE
(This must be typed on one line, traps are cancelled when the prompt is
printed.)
.SH
Functions.
.PP
Functions are used to bundle commands, or to build menu items.  The best
example of a simple function is 'main', the function executed by the
monitor on startup.  Main is by default defined as:
.DS
.B
main() { menu }
.R
.DE
So that's why you see a menu at the start.  The example with 'trap'
above could be executed by main if you type:
.DS
.B
main() { trap 5000 boot; menu }
save
.R
.DE
The save command will save the changed environment of the monitor to the
second half of the boot block, the "boot parameters sector".
.LP
Functions may have one or two arguments, the first is a key to be
pressed from the menu to execute the function, the optional second
argument is the text that is to be displayed on the menu.  The single
argument functions should only be produced by construct, like this
one:
.DS
.B
AT(a) {label=AT;image=42:626;echo AT kernel selected;menu}
.R
.DE
It invites you to choose one of many kernels on a special boot floppy.
.LP
The two argument functions are used to customize the menu, once you
define one the default option disappears, so your first function will
probably be one to start Minix.  Example:
.DS
.B
minix(=,Start Minix) { boot }
dos(d,Boot MS-DOS) { boot hd1 }
save
menu
.R
.DE
Now you can type '=' or 'd' to choose between Minix and DOS.
