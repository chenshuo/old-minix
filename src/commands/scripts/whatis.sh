# whatis - look up name in a data base		Author: Andy Tanenbaum

DATABASE=/usr/etc/whatis

case $# in
0)	echo Usage: whatis name ...
	;;
esac

for i
do grep "^$i[ 	]" $DATABASE
done



