#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include <geometry.h>
#include "dat.h"
#include "fns.h"

int debug;


void
serveproc(void *arg)
{
	Biobuf *bin, *bout;
	NetConnInfo *nci;
	char *line;
	int fd, linelen;

	fd = *(int*)arg;
	nci = getnetconninfo(nil, fd);
	if(nci == nil)
		sysfatal("getnetconninfo: %r");
	threadsetname("serveproc %s", nci->raddr);
	freenetconninfo(nci);

	bin = Bfdopen(fd, OREAD);
	bout = Bfdopen(fd, OWRITE);
	if(bin == nil || bout == nil)
		sysfatal("Bfdopen: %r");

	while((line = Brdline(bin, '\n')) != nil){
		linelen = Blinelen(bin);
		Bwrite(bout, line, linelen);
		Bflush(bout);
		print("%.*s", linelen, line);
	}

	Bterm(bin);
	Bterm(bout);
}

void
listenthread(void *arg)
{
	char *addr, adir[40], ldir[40];
	int acfd, lcfd, dfd;

	addr = arg;

	acfd = announce(addr, adir);
	if(acfd < 0)
		sysfatal("announce: %r");

	if(debug)
		fprint(2, "listening on %s\n", addr);
	
	while((lcfd = listen(adir, ldir)) >= 0){
		if((dfd = accept(lcfd, ldir)) >= 0){
			proccreate(serveproc, &dfd, mainstacksize);
			close(dfd);
		}
		close(lcfd);
	}

	threadexitsall("listen: %r");
}

void
usage(void)
{
	fprint(2, "usage: %s [-d] [-a addr]\n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	char *addr;

	addr = "tcp!*!3047";
	ARGBEGIN{
	case 'd':
		debug++;
		break;
	case 'a':
		addr = EARGF(usage());
		break;
	}ARGEND
	if(argc != 0)
		usage();

	threadcreate(listenthread, addr, mainstacksize);
	yield();
}
