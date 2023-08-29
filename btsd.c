#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include <geometry.h>
#include "dat.h"
#include "fns.h"

int debug;

Playerq playerq;


void
pushplayer(Player *p)
{
	qlock(&playerq);
	if(++playerq.nplayers > playerq.cap){
		playerq.cap = playerq.nplayers;
		playerq.players = erealloc(playerq.players, playerq.cap * sizeof p);
	}
	playerq.players[playerq.nplayers-1] = p;
	qunlock(&playerq);
	if(debug)
		fprint(2, "pushed fd %d sfd %d state %d\n", p->fd, p->sfd, p->state);
}

Player *
popplayer(void)
{
	Player *p;

	p = nil;
	qlock(&playerq);
	if(playerq.nplayers > 0)
		p = playerq.players[--playerq.nplayers];
	qunlock(&playerq);
	if(debug)
		fprint(2, "poppin fd %d sfd %d state %d\n", p->fd, p->sfd, p->state);
	return p;
}

void
freeplayer(Player *p)
{
	close(p->sfd);
	close(p->fd);
	free(p);
}

void
netrecvthread(void *arg)
{
	Chanpipe *cp;
	Ioproc *io;
	char buf[256], *e;
	int n, tot;

	cp = arg;
	io = ioproc();

	tot = 0;
	while((n = ioread(io, cp->fd, buf+tot, sizeof(buf)-1-tot)) > 0){
		tot += n;
		buf[tot] = 0;
		while((e = strchr(buf, '\n')) != nil){
			*e++ = 0;
			chanprint(cp->c, "%s", buf);
			tot -= e-buf;
			memmove(buf, e, tot);
		}
		if(tot >= sizeof(buf)-1)
			tot = 0;
	}
	if(debug)
		fprint(2, "[%d] lost connection\n", getpid());
	closeioproc(io);
	chanclose(cp->c);
	threadexits(nil);
}

void
serveproc(void *arg)
{
	NetConnInfo *nci[2];
	Match *m;
	Player *p, *op;
	Chanpipe cp[2];
	Alt a[3];
	int i;
	uint n0;
	char *s;

	Point2 cell;
	char *coords[5];
	int j, orient;

	m = arg;
	s = nil;

	nci[0] = getnetconninfo(nil, m->pl[0]->fd);
	nci[1] = getnetconninfo(nil, m->pl[1]->fd);
	if(nci[0] == nil || nci[1] == nil)
		sysfatal("getnetconninfo: %r");
	threadsetname("serveproc %s ↔ %s", nci[0]->raddr, nci[1]->raddr);
	freenetconninfo(nci[0]);
	freenetconninfo(nci[1]);

	cp[0].c = chancreate(sizeof(char*), 1);
	cp[0].fd = m->pl[0]->fd;
	cp[1].c = chancreate(sizeof(char*), 1);
	cp[1].fd = m->pl[1]->fd;

	a[0].c = cp[0].c; a[0].v = &s; a[0].op = CHANRCV;
	a[1].c = cp[1].c; a[1].v = &s; a[1].op = CHANRCV;
	a[2].op = CHANEND;

	threadsetgrp(truerand());
	threadcreate(netrecvthread, &cp[0], mainstacksize);
	threadcreate(netrecvthread, &cp[1], mainstacksize);

	write(m->pl[0]->fd, "layout\n", 7);
	write(m->pl[1]->fd, "layout\n", 7);
	m->pl[0]->state = Outlaying;
	m->pl[1]->state = Outlaying;

	while((i = alt(a)) >= 0){
		p = m->pl[i];
		op = m->pl[i^1];

		if(a[i].err != nil){
			if(debug)
				fprint(2, "[%d] alt: %s\n", getpid(), a[i].err);
			write(op->fd, "win\n", 4);
			pushplayer(op);
			freeplayer(p);
			break;
		}
		if(debug)
			fprint(2, "[%d] said '%s'\n", i, s);

		switch(p->state){
		case Outlaying:
			if(strncmp(s, "layout", 6) == 0)
				if(gettokens(s+7, coords, nelem(coords), ",") == nelem(coords)){
					if(debug)
						fprint(2, "rcvd layout from %d\n", i);
					for(j = 0; j < nelem(coords); j++){
						cell = coords2cell(coords[j]);
						orient = coords[j][strlen(coords[j])-1] == 'h'? OH: OV;
						settiles(p, cell, orient, shiplen(j), Tship);
					}
					p->state = Waiting;
					if(debug)
						fprint(2, "curstates [%d] %d / [%d] %d\n", i, p->state, i^1, op->state);
					if(op->state == Waiting){
						if(debug){
							fprint(2, "map0:\n");
							fprintmap(2, p);
							fprint(2, "map1:\n");
							fprintmap(2, op);
						}
						n0 = truerand();
						if(debug)
							fprint(2, "let the game begin: %d plays, %d waits\n", n0%2, (n0+1)%2);
						write(m->pl[n0%2]->fd, "play\n", 5);
						m->pl[n0%2]->state = Playing;
						write(m->pl[(n0+1)%2]->fd, "wait\n", 5);
					}
				}
			break;
		case Playing:
			if(strncmp(s, "shoot", 5) == 0){
				cell = coords2cell(s+6);
				switch(gettile(op, cell)){
				case Tship:
					settile(op, cell, Thit);
					write(p->fd, "hit\n", 4);
					fprint(op->fd, "hit %s\n", cell2coords(cell));
					if(countshipcells(op) < 1){
						write(p->fd, "win\n", 4);
						write(op->fd, "lose\n", 5);
						pushplayer(p);
						pushplayer(op);
						goto Finish;
					}
					goto Swapturn;
				case Twater:
					settile(op, cell, Tmiss);
					write(p->fd, "miss\n", 5);
					fprint(op->fd, "miss %s\n", cell2coords(cell));
Swapturn:
					write(p->fd, "wait\n", 5);
					write(op->fd, "play\n", 5);
					p->state = Waiting;
					op->state = Playing;
					break;
				}
				if(debug)
					fprint(2, "%d plays, %d waits\n", i^1, i);
			}
			break;
		}
		free(s);
	}
Finish:
	if(debug)
		fprint(2, "[%d] serveproc ending\n", getpid());
	free(m);
	chanfree(cp[0].c);
	chanfree(cp[1].c);
	threadkillgrp(threadgetgrp());
	threadexits(nil);
}

void
reaper(void *)
{
	char buf[8];
	ulong i;
	int n;

	threadsetname("reaper");

	for(;;){
		for(i = 0; i < playerq.nplayers; i++){
			if(debug)
				fprint(2, "reapin fd %d sfd %d state %d?",
						playerq.players[i]->fd, playerq.players[i]->sfd, playerq.players[i]->state);
			n = pread(playerq.players[i]->sfd, buf, sizeof buf, 0);
			if(n < 0 || strncmp(buf, "Close", 5) == 0){
				if(debug)
					fprint(2, " yes\n");
				qlock(&playerq);
				freeplayer(playerq.players[i]);
				memmove(&playerq.players[i], &playerq.players[i+1], (--playerq.nplayers-i)*sizeof(Player*));
				qunlock(&playerq);
			}else if(debug)
					fprint(2, " no\n");
		}
		sleep(HZ2MS(1));
	}
}

void
matchmaker(void *)
{
	Match *m;

	threadsetname("matchmaker");

	for(;;){
		if(playerq.nplayers < 2){
			sleep(100);
			continue;
		}

		m = emalloc(sizeof *m);
		m->pl[0] = popplayer();
		m->pl[1] = popplayer();
		m->pl[0]->state = Waiting0;
		m->pl[1]->state = Waiting0;
		memset(m->pl[0]->map, Twater, MAPW*MAPH);
		memset(m->pl[1]->map, Twater, MAPW*MAPH);

		proccreate(serveproc, m, mainstacksize);
	}
}

void
listenthread(void *arg)
{
	char *addr, adir[40], ldir[40], aux[128], *s;
	int acfd, lcfd, dfd, sfd;
	Player *p;

	addr = arg;

	acfd = announce(addr, adir);
	if(acfd < 0)
		sysfatal("announce: %r");

	if(debug)
		fprint(2, "listening on %s\n", addr);

	while((lcfd = listen(adir, ldir)) >= 0){
		if((dfd = accept(lcfd, ldir)) >= 0){
			fd2path(dfd, aux, sizeof aux);
			s = strrchr(aux, '/');
			*s = 0;
			snprint(aux, sizeof aux, "%s/status", aux);
			sfd = open(aux, OREAD);
			if(sfd < 0)
				sysfatal("open: %r");

			p = emalloc(sizeof *p);
			p->fd = dfd;
			p->sfd = sfd;
			p->state = Waiting0;
			pushplayer(p);
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

	GEOMfmtinstall();
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

	proccreate(matchmaker, nil, mainstacksize);
	proccreate(reaper, nil, mainstacksize);
	threadcreate(listenthread, addr, mainstacksize);
	yield();
}
