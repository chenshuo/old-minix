# Generate binary keymaps.

.SUFFIXES:	.src .map

.src.map:
	$(CC) -DKEYSRC=\"$<\" genmap.c
	./a.out > $@
	@rm -f a.out

all:	\
	french.map \
	german.map \
	italian.map \
	olivetti.map \
	spanish.map \
	us-std.map \
	us-swap.map \

install:	\
	/usr/lib/keymaps \
	/usr/lib/keymaps/french.map \
	/usr/lib/keymaps/german.map \
	/usr/lib/keymaps/italian.map \
	/usr/lib/keymaps/olivetti.map \
	/usr/lib/keymaps/spanish.map \
	/usr/lib/keymaps/us-std.map \
	/usr/lib/keymaps/us-swap.map \

clean:
	rm -f a.out *.map

/usr/lib/keymaps:
	install -d $@

/usr/lib/keymaps/french.map:	french.map
	install -c $? $@

/usr/lib/keymaps/german.map:	german.map
	install -c $? $@

/usr/lib/keymaps/italian.map:	italian.map
	install -c $? $@

/usr/lib/keymaps/olivetti.map:	olivetti.map
	install -c $? $@

/usr/lib/keymaps/spanish.map:	spanish.map
	install -c $? $@

/usr/lib/keymaps/us-std.map:	us-std.map
	install -c $? $@

/usr/lib/keymaps/us-swap.map:	us-swap.map
	install -c $? $@
