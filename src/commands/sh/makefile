CFLAGS=-F -T.
l=/lib

shobj = sh1.s sh2.s sh3.s sh4.s sh5.s sh6.s
sh:	$(shobj) sh.h
	@cc -i -o sh -T.  $(shobj)
	@chmem =13000 sh

