#!/bin/sh
#
#	instdist 1.6 - install a Minix distribution	Author: Kees J. Bot
#								20 Dec 1994
# (An external program can use the X_* hooks to add
# a few extra actions.  It needs to use a sed script to change
# them though, the shell doesn't get it otherwise.)

PATH=/bin:/usr/bin
export PATH

# Move out of /usr.
case "`pwd`" in
/usr*)	echo "Please type 'cd /', you are locking up /usr" >&2	
	exit 1
esac
case "$0" in
/tmp/*)	rm -f "$0"
	;;
*)	cp -p "$0" /tmp/instdist
	exec /tmp/instdist
esac

# Find out what we are running from.
exec 9<&0 </etc/mtab			# Mounted file table.
read thisroot rest			# Current root (/dev/ram or /dev/fd?)
read fdusr rest				# USR (/dev/fd? or /dev/fd?c)
exec 0<&9 9<&-

# What do we know about ROOT?
case $thisroot:$fdusr in
/dev/ram:/dev/fd0c)	fdroot=/dev/fd0		# Combined ROOT+USR in drive 0
			;;
/dev/ram:/dev/fd1c)	fdroot=/dev/fd1		# Combined ROOT+USR in drive 1
			;;
/dev/ram:*)		fdroot=unknown		# ROOT is some other floppy
			;;
*)			fdroot=$thisroot	# ROOT is mounted directly
esac

echo -n "\
This is the Minix installation script.

Note 1: If the screen blanks suddenly then hit F3 to select \"software
        scrolling\".

Note 2: If things go wrong then hit DEL and start over.

Note 3: The installation procedure is described in the manual page
        usage(8).  It will be hard without it.

Note 4: If you see a colon (:) then you should hit RETURN or ENTER to continue.
:"
read ret

echo "
What type of keyboard do you have?  You can choose one of:
"
ls -C /usr/lib/keymaps | sed -e 's/\.map//g' -e 's/^/    /'
echo -n "
Keyboard type? [us-std] "; read keymap
case "$keymap" in
?*)	loadkeys "/usr/lib/keymaps/$keymap.map"
esac

echo -n "
Minix needs one primary partition of at least 30 Mb (it fits in 20 Mb, but
it needs 30 Mb if fully recompiled.  Add more space to taste.)

If there is no free space on your disk then you have to back up one of the
other partitions, shrink, and reinstall.  See the appropriate manuals of the
the operating systems currently installed.  Restart your Minix installation
after you have made space.

To make this partition you will be put in the editor \"part\".  Follow the
advise under the '!' key to make a new partition of type MINIX.  Do not
touch an existing partition unless you know precisely what you are doing!
Please note the name of the partition (hd1, hd2, ..., hd9, sd1, sd2, ...
sd9) you make.  (See the devices section in usage(8) on Minix device names.)
:"
read ret

primary=
while [ -z "$primary" ]
do
	part || exit

	echo -n "
Please finish the name of the primary partition you have created:
(Just type RETURN if you want to rerun \"part\")                   /dev/"
	read primary
done

root=${primary}a
tmp=${primary}b
usr=${primary}c

echo -n "
You have created a partition named:	/dev/$primary
The following subpartitions are about to be created on /dev/$primary:

	Root subpartition:	/dev/$root	1440 kb
	Scratch subpartition:	/dev/$tmp	1440 kb
	/usr subpartition:	/dev/$usr	rest of $primary

Hit return if everything looks fine, or hit DEL to bail out if you want to
think it over.  The next step will destroy /dev/$primary.
:"
read ret
					# Secondary master bootstrap.
installboot -m /dev/$primary /usr/mdec/masterboot >/dev/null || exit

					# Partition the primary.
partition /dev/$primary 1 81:2880* 81:2880 81:0+ >/dev/null || exit

echo "
Migrating to disk...
"

mkfs /dev/$usr
echo "Scanning /dev/$usr for bad blocks.  (Hit DEL if there are no bad blocks)"
trap ': nothing' 2
readall -b /dev/$usr | sh
echo "Scan done"
sleep 2
trap 2
mount /dev/$usr /mnt || exit		# Mount the intended /usr.

cpdir -v /usr /mnt || exit		# Copy the usr floppy.

umount /dev/$usr || exit		# Unmount the intended /usr.

umount $fdusr				# Unmount the /usr floppy.

mount /dev/$usr /usr || exit		# A new /usr

if [ $fdroot = unknown ]
then
	echo "
By now the floppy /usr has been copied to /dev/$usr, and it is now in use as
/usr.  Please insert the installation root floppy in a floppy drive."

	drive=
	while [ -z "$drive" ]
	do
		echo -n "What floppy drive is it in? [0] "; read drive

		case $drive in
		'')	drive=0
			;;
		[01])
			;;
		*)	echo "It must be 0 or 1, not \"$drive\"."
			drive=
		esac
	done
	fdroot=/dev/fd$drive
fi

echo "
Copying $fdroot to /dev/$root
"

if [ `arch` = i86 ]; then size=720; else size=1024; fi
mkfs -i 512 /dev/$root $size || exit
mount /dev/$root /mnt || exit
if [ $thisroot = /dev/ram ]
then
	# Running from the RAM disk, root image is on a floppy.
	mount /dev/fd0 /fd0 || exit
	cpdir -v /fd0 /mnt || exit
	umount /dev/fd0 || exit
else
	# Running from the floppy itself.
	cpdir -vx / /mnt || exit
	chmod 555 /mnt/usr
fi
					# Change /etc/fstab.
echo >/mnt/etc/fstab "\
# Poor man's File System Table.

root=/dev/$root
tmp=/dev/$tmp
usr=/dev/$usr"

					# National keyboard map.
case "$keymap" in
?*)	cp -p "/usr/lib/keymaps/$keymap.map" /mnt/etc/keymap
esac

eval "$X_ROOT1"
umount /dev/$root || exit		# Unmount the new root.


					# Make bootable.
installboot -d /dev/$root /usr/mdec/bootblock /boot >/dev/null || exit
edparams /dev/$root "ramimagedev=$root; save" || exit
eval "$X_ROOT2"

if [ $thisroot != /dev/ram ]
then
	echo "
The root file system has been installed on /dev/$root.  The rest of /usr should
be installed now, but $thisroot is busy.  Please reboot Minix as described in
\"Testing\" and fill /usr as described at the end of \"Manual installation.\""
	exit
fi

echo "
The root file system has been installed on /dev/$root.  Please remove the
installation diskette, so that the rest of /usr may be installed.
"
size=
while [ -z "$size" ]
do
	case $fdusr in /dev/fd?c) defsize=1440;; *) defsize=720;; esac

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
The command that is run now is:

	cd /usr; vol $size /dev/fd$drive | uncompress | tar xvfp -

You may want to make a note of this, you can reboot and then run it again if
it fails.  (It may run out of memory on a small machine.)
"

cd /usr && vol -r $size /dev/fd$drive | uncompress | tar xvfp -

echo "
Please insert the installation root floppy and type 'halt' to exit Minix.
You can type 'boot $root' to try the newly installed Minix system.  See
\"Testing\" in the usage manual."
