#!/bin/sh
#
# DESCRIBE 1.13 - Describe the given devices.		Author: Kees J. Bot
#
# BUGS
# - Arguments may not contain shell metacharacters.

case $# in
0)	flag=; set -$- /dev ;;
*)	flag=d ;;
esac

ls -l$flag $* | \
sed	-e '/^total/d' \
	-e '/^[^bc]/s/.* /BAD BAD /' \
	-e '/^[bc]/s/.* \([0-9][0-9]*\), *\([0-9][0-9]*\).* /\1 \2 /' \
| {
ex=true

while read major minor path
do
	case $path in
	/*)	name=`expr $path : '.*/\(.*\)$'`
		;;
	*)	name=$path
	esac

	case $major,$minor in
	1,0)	des="RAM disk" dev=ram
		;;
	1,1)	des="memory" dev=mem
		;;
	1,2)	des="kernel memory" dev=kmem
		;;
	1,3)	des="null device, data sink" dev=null
		;;
	1,4)	des="I/O ports (obsolete)" dev=port
		;;
	2,*)	drive=`expr $minor % 4`
		case `expr $minor - $drive` in
		0)	des='auto density' dev="fd$drive"
			;;
		4)	des='360k, 5.25"' dev="pc$drive"
			;;
		8)	des='1.2M, 5.25"' dev="at$drive"
			;;
		12)	des='360k in 720k, 5.25"' dev="qd$drive"
			;;
		16)	des='720k, 3.5"' dev="ps$drive"
			;;
		20)	des='360k in 1.2M, 5.25"' dev="pat$drive"
			;;
		24)	des='720k in 1.2M, 5.25"' dev="qh$drive"
			;;
		28)	des='1.44M, 3.5"' dev="PS$drive"
			;;
		112)	des='auto partition 0' dev="fd${drive}a"
			;;
		116)	des='auto partition 1' dev="fd${drive}b"
			;;
		120)	des='auto partition 2' dev="fd${drive}c"
			;;
		124)	des='auto partition 3' dev="fd${drive}d"
			;;
		*)	dev=BAD
		esac
		des="floppy drive $drive ($des)"
		;;
	3,[05]|3,[123][05])
		drive=`expr $minor / 5`
		des="hard disk drive $drive" dev=hd$minor
		;;
	3,?|3,[123]?)
		drive=`expr $minor / 5`
		par=`expr $minor % 5`
		des="hard disk $drive, partition $par" dev=hd$minor
		;;
	3,128|3,129|3,1[3-9]?|3,2??)
		drive=`expr \\( $minor - 128 \\) / 16`
		par=`expr \\( \\( $minor - 128 \\) / 4 \\) % 4 + 1`
		sub=`expr \\( $minor - 128 \\) % 4 + 1`
		des="hard disk $drive, partition $par, subpartition $sub"
		par=`expr $drive '*' 5 + $par`
		case $sub in
		1)	dev=hd${par}a ;;
		2)	dev=hd${par}b ;;
		3)	dev=hd${par}c ;;
		4)	dev=hd${par}d ;;
		esac
		;;
	4,0)	des="console device"
		case $name in
		tty0)	dev=$name ;;
		*)	dev=console ;;
		esac
		;;
	4,?)
		des="serial line $minor" dev=tty$minor
		;;
	5,0)	des="anonymous tty" dev=tty
		;;
	6,0)	des="line printer, parallel port" dev=lp
		;;
	7,1)	des="raw ethernet" dev=eth
		;;
	7,2)	des="raw IP" dev=ip
		;;
	7,3)	des="TCP/IP" dev=tcp
		;;
	7,4)	des="UDP" dev=udp
		;;
	10,[05]|10,[123][05])
		drive=`expr $minor / 5`
		des="scsi disk drive $drive" dev=sd$minor
		;;
	10,?|10,[123]?)
		drive=`expr $minor / 5`
		par=`expr $minor % 5`
		des="scsi disk $drive, partition $par" dev=sd$minor
		;;
	10,128|10,129|10,1[3-9]?|10,2??)
		drive=`expr \\( $minor - 128 \\) / 16`
		par=`expr \\( \\( $minor - 128 \\) / 4 \\) % 4 + 1`
		sub=`expr \\( $minor - 128 \\) % 4 + 1`
		des="scsi disk $drive, partition $par, subpartition $sub"
		par=`expr $drive '*' 5 + $par`
		case $sub in
		1)	dev=sd${par}a ;;
		2)	dev=sd${par}b ;;
		3)	dev=sd${par}c ;;
		4)	dev=sd${par}d ;;
		esac
		;;
	10,6[4-9]|10,7?)
		tape=`expr \\( $minor - 64 \\) / 2`
		case $minor in
		*[02468])
			des="scsi tape $tape (non-rewinding)" dev=nrst$tape
			;;
		*[13579])
			des="scsi tape $tape (rewinding)" dev=rst$tape
		esac
		;;
	BAD,BAD)
		des= dev=
		;;
	*)	dev=BAD
	esac

	case $name:$dev in
	*:)
		echo "$path: not a device" >&2
		ex=false
		;;
	*:*BAD*)
		echo "$path: cannot describe: major=$major, minor=$minor" >&2
		ex=false
		;;
	$dev:*)
		echo "$path: $des"
		;;
	*:*)	echo "$path: nonstandard name for $dev: $des"
	esac
done

$ex
}
