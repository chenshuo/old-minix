#!/bin/sh
#
#	mkdist 3.4 - Make a Minix distribution		Author: Kees J. Bot
#								20 Dec 1994

PATH=/bin:/usr/bin
export PATH

# Move out of /usr.
case "$0" in
/tmp/*)	rm -f "$0"
	;;
*)	cp -p "$0" /tmp/mkdist
	exec /tmp/mkdist
esac

usrlist="
bin
bin/MAKEDEV
bin/arch
bin/badblocks
bin/chmod
bin/clone
bin/compress
bin/cp
bin/cpdir
bin/df
`test -f /usr/bin/mtools || echo bin/dos*`
`test -f /usr/bin/mtools && echo bin/mtools`
bin/edparams
bin/getty
bin/grep
bin/installboot
bin/isodir
bin/isoinfo
bin/isoread
bin/kill
bin/ln
bin/login
bin/ls
bin/mined
bin/mkdir
bin/mkfs
bin/mknod
bin/mkswap
bin/mv
bin/od
bin/part
bin/partition
bin/readall
bin/repartition
bin/rm
bin/rmdir
bin/sed
bin/setup
bin/shutdown
bin/sleep
bin/sort
bin/stty
bin/sysenv
bin/tar
bin/uname
bin/uncompress
bin/update
bin/vol
bin/zcat
etc
etc/rc
lib
lib/keymaps
`cd /usr && echo lib/keymaps/*`
lib/pwdauth
mdec
mdec/boot
mdec/bootblock
mdec/jumpboot
mdec/masterboot
tmp
"

# Find the root device, and the real root device.
. /etc/fstab
realroot=`printroot -r`
if [ $realroot = $root ]
then
	rootdir=/
else
	umount $root >/dev/null 2>&1
	mount $root /root || exit
	rootdir=/root
fi

echo -n "
The installation root and /usr can be put on either one diskette of at least
1.2 Mb, or on two diskettes of at least 720 kb.

Do you want to use a single diskette of at least 1.2 Mb? [y] "; read single

case $single in
''|[yY]*|sure)	single=true
	;;
*)		single=
esac

echo -n "Which drive to use? [0] "; read drive

case $drive in
'')	drive=0
	;;
[01])	;;
*)	echo "Please type '0' or '1'" >&2; exit 1
esac

if [ "$single" ]
then
	echo -n "Insert the root+usr diskette in drive $drive and hit RETURN"
else
	echo -n "Insert the root diskette in drive $drive and hit RETURN"
fi

read ret
umount /dev/fd$drive 2>/dev/null
umount /dev/fd${drive}p1 2>/dev/null
mkfs -1 -i 272 /dev/fd$drive 480 || exit
partition -mf /dev/fd$drive 0 81:960 81:240 81:240 >/dev/null || exit
repartition /dev/fd$drive >/dev/null || exit
mkfs -1 /dev/fd${drive}p1 || exit
mkfs -1 /dev/fd${drive}p2 || exit
mount /dev/fd$drive /mnt || exit
mount /dev/fd${drive}p1 $rootdir/minix || exit	# Hide /minix and /etc
mount /dev/fd${drive}p2 $rootdir/etc 2>/dev/null # (complains about /etc/mtab)
cpdir -vx $rootdir /mnt || exit
install -d -o 0 -g 0 -m 755 /mnt || exit
install -d -o 0 -g 0 -m 555 /mnt/root || exit
install -d -o 0 -g 0 -m 555 /mnt/mnt || exit
install -d -o 0 -g 0 -m 555 /mnt/usr || exit
umount /dev/fd${drive}p2 || exit		# Unhide /etc
umount /dev/fd${drive}p1 || exit		# Unhide /minix
install -d -o 2 -g 0 -m 755 /mnt/minix || exit
install -d -o 2 -g 0 -m 755 /mnt/etc || exit
set `ls -t $rootdir/minix`			# Install the latest kernel
install -c $rootdir/minix/$1 /mnt/minix/`uname -r`.`uname -v` || exit
cpdir -v /usr/src/etc /mnt/etc || exit		# Install a fresh /etc
chown -R 0:0 /mnt/etc				# Patch up owner and mode
chmod 600 /mnt/etc/shadow

# Change /etc/fstab.
echo >/mnt/etc/fstab "\
# Poor man's File System Table.

root=unknown
usr=unknown"

# How to install?
echo >/mnt/etc/issue "\

Login as root and run 'setup' to install Minix."

umount /dev/fd$drive || exit
umount $root 2>/dev/null
installboot -d /dev/fd$drive /usr/mdec/bootblock boot >/dev/null

# Partition the root floppy whether necessary or not.  (Two images can be
# concatenated, or a combined image can be split later.)
partition -mf /dev/fd$drive 0 81:960 0:0 81:1440 81:480 >/dev/null || exit

if [ "$single" ]
then
	repartition /dev/fd$drive >/dev/null
	part=p2
else
	echo -n "Insert the usr diskette in drive $drive and hit RETURN"
	read ret
	part=
fi

mkfs -1 -i 96 /dev/fd$drive$part 720 || exit
mount /dev/fd$drive$part /mnt || exit
install -d -o 0 -g 0 -m 755 /mnt || exit
(cd /usr && exec tar cfD - $usrlist) | (cd /mnt && exec tar xvfp -) || exit
umount /dev/fd$drive$part || exit

# Put a "boot the other drive" bootblock on the /usr floppy.
installboot -m /dev/fd$drive$part /usr/mdec/masterboot >/dev/null

# Guess the size of /usr in compressed form.  Assume compression down to 60%
# of the original size.  Use "disk megabytes" of 1000 kb.
set -$- `df | grep "^$usr"`
size=`expr \\( $6 \\* 6 / 10 + 999 \\) / 1000`

echo -n "
You now need enough diskettes to hold /usr in compressed form, close to
$size Mb total.  "

size=
while [ -z "$size" ]
do
	if [ "$single" ]; then defsize=1440; else defsize=720; fi

	echo -n "What is the size of the diskettes? [$defsize] "; read size

	case $size in
	'')	size=$defsize
		;;
	360|720|1200|1440)
		;;
	*)	echo "Sorry, I don't believe \"$size\", try again." >&2
		size=
	esac
done

drive=
while [ -z "$drive" ]
do
	echo -n "What floppy drive to use? [0] "; read drive

	case $drive in
	'')	drive=0
		;;
	[01])
		;;
	*)	echo "It must be 0 or 1, not \"$drive\"."
		drive=
	esac
done

echo "
Enter the floppies in drive $drive when asked to.  Mark them with the volume
numbers!
"
sleep 2

if [ `arch` = i86 ]; then bits=13; else bits=16; fi

>/tmp/LAST
cd /usr && tar cvf - . /tmp/LAST \
	| compress -b$bits | vol -w $size /dev/fd$drive &&
echo Done.
