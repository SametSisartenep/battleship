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

</sys/src/cmd/mkmany

ohman:V:
	cp bts.man $MAN/battleship

install:V: ohman

uninstall:V:
	for(i in $TARG)
		rm -f $BIN/$i
	rm -f $MAN/battleship
