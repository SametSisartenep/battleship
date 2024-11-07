</$objtype/mkfile

MAN=/sys/man/1
BIN=/$objtype/bin/games
TARG=\
	bts\
	btsd\

OFILES=\
	alloc.$O\
	parse.$O\
	util.$O\
	andy.$O\
	menulist.$O\
	mixer.$O\

HFILES=\
	dat.h\
	fns.h\
	mixer.h\

ASSETDIR=/sys/games/lib/battleship

</sys/src/cmd/mkmany

ohman:V:
	cp bts.man $MAN/battleship

install:V: ohman
	for(i in $TARG)
		mk $MKFLAGS $i.install
	mkdir -p $ASSETDIR
	dircp assets $ASSETDIR

uninstall:V:
	for(i in $TARG)
		rm -f $BIN/$i
	rm -f $MAN/battleship
	rm -rf $ASSETDIR
