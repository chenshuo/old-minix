# Makefile for inet

# Directories
u = /usr
i = $u/include
s = $i/sys
h = $i/minix
n = $i/net
l = $u/lib
g = generic

# Programs, flags, etc.
CC =	exec cc
LD =	$(CC) -.o
CFLAGS = -m -I. -I.. -I$i
LDFLAGS = -i
HEAD =	$l/head.o

OBJ = 	buf.o clock.o mnx_eth.o mq.o nw_task.o sr.o \
	$g/arp.o $g/eth.o $g/icmp.o $g/io.o \
	$g/ip.o $g/ip_ioctl.o $g/ip_lib.o \
	$g/ip_read.o $g/ip_write.o $g/ipr.o \
	$g/tcp.o $g/tcp_lib.o $g/tcp_recv.o \
	$g/tcp_send.o $g/udp.o

inet:	$(HEAD) inet.a putk.o
	$(LD) -o $@ $(LDFLAGS) $(HEAD) inet.a putk.o

all install:	# Nothing

inet.a:	$(OBJ)
	@rm -f $@
	aal cr $@ $(OBJ)

clean:
	rm -f $(OBJ) inet.a putk.o inet *.bak

# Depencencies

nw_task.h:	const.h
nw_task.h:	$s/ioctl.h
nw_task.h:	$n/gen/udp_io.h
nw_task.h:	$n/gen/udp_hdr.h
nw_task.h:	$n/gen/udp.h
nw_task.h:	$n/gen/tcp_io.h
nw_task.h:	$n/gen/tcp_hdr.h
nw_task.h:	$n/gen/tcp.h
nw_task.h:	$n/gen/route.h
nw_task.h:	$n/gen/oneCsum.h
nw_task.h:	$n/gen/icmp_hdr.h
nw_task.h:	$n/gen/icmp.h
nw_task.h:	$n/gen/ip_io.h
nw_task.h:	$n/gen/ip_hdr.h
nw_task.h:	$n/gen/in.h
nw_task.h:	$n/gen/eth_io.h
nw_task.h:	$n/gen/eth_hdr.h
nw_task.h:	$n/gen/ether.h
nw_task.h:	$n/hton.h
nw_task.h:	$h/syslib.h
nw_task.h:	$h/const.h
nw_task.h:	$h/com.h
nw_task.h:	$h/type.h
nw_task.h:	$h/config.h
nw_task.h:	$i/string.h
nw_task.h:	$i/stdlib.h
nw_task.h:	$i/stddef.h
nw_task.h:	$i/errno.h
nw_task.h:	$i/ansi.h
nw_task.h:	$s/types.h
	touch nw_task.h  # Trick to avoid many dependencies (that overload make)

buf.o:	$g/type.h
buf.o:	$g/buf.h
buf.o:	$g/assert.h
buf.o:	$i/string.h
buf.o:	$i/stdlib.h
buf.o:	nw_task.h

clock.o:	$g/type.h
clock.o:	$g/clock.h
clock.o:	$g/buf.h
clock.o:	$g/assert.h
clock.o:	proto.h
clock.o:	nw_task.h

mnx_eth.o:	$g/sr.h
mnx_eth.o:	$g/eth_int.h
mnx_eth.o:	$g/eth.h
mnx_eth.o:	$g/clock.h
mnx_eth.o:	osdep_eth.h
mnx_eth.o:	$g/buf.h
mnx_eth.o:	$g/assert.h
mnx_eth.o:	proto.h
mnx_eth.o:	nw_task.h

mq.o:	mq.h
mq.o:	nw_task.h

nw_task.o:	$g/udp.h
nw_task.o:	$g/type.h
nw_task.o:	$g/tcp.h
nw_task.o:	$g/sr.h
nw_task.o:	$g/ip.h
nw_task.o:	$g/eth.h
nw_task.o:	$g/clock.h
nw_task.o:	$g/buf.h
nw_task.o:	$g/arp.h
nw_task.o:	$g/assert.h
nw_task.o:	proto.h
nw_task.o:	mq.h
nw_task.o:	$h/callnr.h
nw_task.o:	$i/limits.h
nw_task.o:	$i/lib.h
nw_task.o:	config.h
nw_task.o:	nw_task.h

putk.o:	nw_task.h

sr.o:	$g/type.h
sr.o:	$g/sr.h
sr.o:	$g/buf.h
sr.o:	$g/assert.h
sr.o:	proto.h
sr.o:	mq.h
sr.o:	$h/callnr.h
sr.o:	nw_task.h

$g/arp.o:	$g/type.h
$g/arp.o:	$g/sr.h
$g/arp.o:	$g/io.h
$g/arp.o:	$g/eth.h
$g/arp.o:	$g/clock.h
$g/arp.o:	$g/buf.h
$g/arp.o:	$g/assert.h
$g/arp.o:	$g/arp.h
$g/arp.o:	nw_task.h
$g/arp.o:	$g/arp.c
	cd generic; $(CC) -c $(CFLAGS) arp.c

$g/eth.o:	$g/type.h
$g/eth.o:	$g/sr.h
$g/eth.o:	$g/io.h
$g/eth.o:	$g/eth_int.h
$g/eth.o:	$g/eth.h
$g/eth.o:	$g/assert.h
$g/eth.o:	osdep_eth.h
$g/eth.o:	$g/clock.h
$g/eth.o:	$g/buf.h
$g/eth.o:	nw_task.h
$g/eth.o:	$g/eth.c
	cd generic; $(CC) -c $(CFLAGS) eth.c

$g/icmp.o:	$g/ipr.h
$g/icmp.o:	$g/ip_int.h
$g/icmp.o:	$g/ip.h
$g/icmp.o:	$g/io.h
$g/icmp.o:	$g/icmp_lib.h
$g/icmp.o:	$g/icmp.h
$g/icmp.o:	$g/clock.h
$g/icmp.o:	$g/assert.h
$g/icmp.o:	$g/type.h
$g/icmp.o:	$g/buf.h
$g/icmp.o:	nw_task.h
$g/icmp.o:	$g/icmp.c
	cd generic; $(CC) -c $(CFLAGS) icmp.c

$g/io.o:	$g/io.h
$g/io.o:	nw_task.h
$g/io.o:	$g/io.c
	cd generic; $(CC) -c $(CFLAGS) io.c

$g/ip.o:	$g/sr.h
$g/ip.o:	$g/ipr.h
$g/ip.o:	$g/ip_int.h
$g/ip.o:	$g/ip.h
$g/ip.o:	$g/io.h
$g/ip.o:	$g/icmp_lib.h
$g/ip.o:	$g/icmp.h
$g/ip.o:	$g/eth.h
$g/ip.o:	$g/clock.h
$g/ip.o:	$g/assert.h
$g/ip.o:	$g/arp.h
$g/ip.o:	$g/type.h
$g/ip.o:	$g/buf.h
$g/ip.o:	nw_task.h
$g/ip.o:	$g/ip.c
	cd generic; $(CC) -c $(CFLAGS) ip.c

$g/ip_ioctl.o:	$g/ipr.h
$g/ip_ioctl.o:	$g/ip_int.h
$g/ip_ioctl.o:	$g/ip.h
$g/ip_ioctl.o:	$g/icmp_lib.h
$g/ip_ioctl.o:	$g/clock.h
$g/ip_ioctl.o:	$g/assert.h
$g/ip_ioctl.o:	$g/arp.h
$g/ip_ioctl.o:	$g/type.h
$g/ip_ioctl.o:	$g/buf.h
$g/ip_ioctl.o:	nw_task.h
$g/ip_ioctl.o:	$g/ip_ioctl.c
	cd generic; $(CC) -c $(CFLAGS) ip_ioctl.c

$g/ip_lib.o:	$g/ip_int.h
$g/ip_lib.o:	$g/io.h
$g/ip_lib.o:	$g/assert.h
$g/ip_lib.o:	$g/type.h
$g/ip_lib.o:	$g/buf.h
$g/ip_lib.o:	nw_task.h
$g/ip_lib.o:	$g/ip_lib.c
	cd generic; $(CC) -c $(CFLAGS) ip_lib.c

$g/ip_read.o:	$g/ip_int.h
$g/ip_read.o:	$g/ip.h
$g/ip_read.o:	$g/io.h
$g/ip_read.o:	$g/icmp_lib.h
$g/ip_read.o:	$g/assert.h
$g/ip_read.o:	$g/type.h
$g/ip_read.o:	$g/clock.h
$g/ip_read.o:	$g/buf.h
$g/ip_read.o:	nw_task.h
$g/ip_read.o:	$g/ip_read.c
	cd generic; $(CC) -c $(CFLAGS) ip_read.c

$g/ip_write.o:	$g/ipr.h
$g/ip_write.o:	$g/ip_int.h
$g/ip_write.o:	$g/ip.h
$g/ip_write.o:	$g/io.h
$g/ip_write.o:	$g/icmp_lib.h
$g/ip_write.o:	$g/eth.h
$g/ip_write.o:	$g/clock.h
$g/ip_write.o:	$g/assert.h
$g/ip_write.o:	$g/arp.h
$g/ip_write.o:	$g/type.h
$g/ip_write.o:	$g/buf.h
$g/ip_write.o:	nw_task.h
$g/ip_write.o:	$g/ip_write.c
	cd generic; $(CC) -c $(CFLAGS) ip_write.c

$g/ipr.o:	$g/ipr.h
$g/ipr.o:	$g/io.h
$g/ipr.o:	$g/assert.h
$g/ipr.o:	$g/clock.h
$g/ipr.o:	nw_task.h
$g/ipr.o:	$g/ipr.c
	cd generic; $(CC) -c $(CFLAGS) ipr.c

$g/tcp.o:	$g/tcp_int.h
$g/tcp.o:	$g/tcp.h
$g/tcp.o:	$g/assert.h
$g/tcp.o:	$g/sr.h
$g/tcp.o:	$g/ip.h
$g/tcp.o:	$g/io.h
$g/tcp.o:	$g/type.h
$g/tcp.o:	$g/clock.h
$g/tcp.o:	$g/buf.h
$g/tcp.o:	nw_task.h
$g/tcp.o:	$g/tcp.c
	cd generic; $(CC) -c $(CFLAGS) tcp.c

$g/tcp_lib.o:	$g/tcp_int.h
$g/tcp_lib.o:	$g/assert.h
$g/tcp_lib.o:	$g/type.h
$g/tcp_lib.o:	$g/io.h
$g/tcp_lib.o:	$g/clock.h
$g/tcp_lib.o:	$g/buf.h
$g/tcp_lib.o:	nw_task.h
$g/tcp_lib.o:	$g/tcp_lib.c
	cd generic; $(CC) -c $(CFLAGS) tcp_lib.c

$g/tcp_recv.o:	$g/assert.h
$g/tcp_recv.o:	$g/tcp.h
$g/tcp_recv.o:	$g/tcp_int.h
$g/tcp_recv.o:	$g/type.h
$g/tcp_recv.o:	$g/clock.h
$g/tcp_recv.o:	$g/buf.h
$g/tcp_recv.o:	nw_task.h
$g/tcp_recv.o:	$g/tcp_recv.c
	cd generic; $(CC) -c $(CFLAGS) tcp_recv.c

$g/tcp_send.o:	$g/tcp_int.h
$g/tcp_send.o:	$g/tcp.h
$g/tcp_send.o:	$g/clock.h
$g/tcp_send.o:	$g/assert.h
$g/tcp_send.o:	$g/type.h
$g/tcp_send.o:	$g/buf.h
$g/tcp_send.o:	$g/ip.h
$g/tcp_send.o:	nw_task.h
$g/tcp_send.o:	$g/tcp_send.c
	cd generic; $(CC) -c $(CFLAGS) tcp_send.c

$g/udp.o:	$g/assert.h
$g/udp.o:	$g/buf.h
$g/udp.o:	$g/clock.h
$g/udp.o:	$g/io.h
$g/udp.o:	$g/ip.h
$g/udp.o:	$g/sr.h
$g/udp.o:	$g/type.h
$g/udp.o:	$g/udp.h
$g/udp.o:	nw_task.h
$g/udp.o:	$g/udp.c
	cd generic; $(CC) -c $(CFLAGS) udp.c
