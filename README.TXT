The installation guide can be found in INSTALL.TXT

More manual pages are in MAN\CAT*\*
If a manual page refers to, say boot(8), then the manual page for boot
can be found in MAN\CAT8\BOOT.8.  Under Minix you can type 'man 8 boot'
if you have installed the SYS.TAZ set.

Making floppies for the i386 on 1.44M diskettes:

	vol 1440 A: i386\ROOT i386\USR	  (Combined boot floppy)
	vol 1440 A: i386\USR.TAZ	  (Base system on 3 floppies)
	vol 1440 A: SYS.TAZ		  (System sources on 2 floppies)
	vol 1440 A: CMD.TAZ		  (Commands sources on 3 floppies)

Use 1200 to specify 1.2M floppies.  Likewise for 720k floppies, except
that you have to put ROOT and USR installation images on two separate
floppies:

	vol 720 A: i386\ROOT		  (Separate ROOT and USR)
	vol 720 A: i386\USR
	vol 720 A: i386\USR.TAZ		  (*.TAZ files as above)
	...

The i86 directory contains the installation and base system material to
install the 16-bits version of Minix for old XTs and 286 based ATs.  If
the intended machine has little memory or a 360k drive then read
XT/README.TXT and use the installation floppy images in the XT directory
instead of the regular ROOT and USR.
