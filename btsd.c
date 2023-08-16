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
		fprint(2, "pushed fd %d sfd %d o %p\n", p->fd, p->sfd, p->o);
}

Player *
popplayer(void)
{
	Player *p;

	p = nil;
	if(playerq.nplayers > 0){
		qlock(&playerq);
		p = playerq.players[--playerq.nplayers];
		qunlock(&playerq);
	}
	if(debug)
		fprint(2, "poppin fd %d sfd %d o %p\n", p->fd, p->sfd, p->o);
	return p;
}

void
netrecvthread(void *arg)
{
	Chanpipe *cp;
	Ioproc *io;
	char buf[256];
	int n;

	cp = arg;
	io = ioproc();

	while((n = ioread(io, cp->fd, buf, sizeof(buf)-1)) > 0){
		buf[n] = 0;
		chanprint(cp->c, "%s", buf);
	}
	chanclose(cp->c);
	if(debug)
		fprint(2, "[%d] lost connection\n", getpid());
}

void
serveproc(void *arg)
{
	NetConnInfo *nci[2];
	Player **m;
	Chanpipe cp[2];
	Alt a[3];
	int i, n0, tid[2];
	char *s;

	threadsetname("serveproc ");

	m = arg;
	s = nil;

	nci[0] = getnetconninfo(nil, m[0]->fd);
	nci[1] = getnetconninfo(nil, m[1]->fd);
	if(nci[0] == nil || nci[1] == nil)
		sysfatal("getnetconninfo: %r");
	threadsetname("serveproc %s ↔ %s", nci[0]->raddr, nci[1]->raddr);
	freenetconninfo(nci[0]);
	freenetconninfo(nci[1]);

	cp[0].c = chancreate(sizeof(char*), 1);
	cp[0].fd = m[0]->fd;
	cp[1].c = chancreate(sizeof(char*), 1);
	cp[1].fd = m[1]->fd;

	a[0].c = cp[0].c; a[0].v = &s; a[0].op = CHANRCV;
	a[1].c = cp[1].c; a[1].v = &s; a[1].op = CHANRCV;
	a[2].op = CHANEND;

	tid[0] = threadcreate(netrecvthread, &cp[0], mainstacksize);
	tid[1] = threadcreate(netrecvthread, &cp[1], mainstacksize);

	n0 = truerand();
	write(m[n0%2]->fd, "wait", 4);
	write(m[(n0+1)%2]->fd, "play", 4);

	while((i = alt(a)) >= 0){
		if(debug)
			fprint(2, "[%d] said '%s'\n", getpid(), s);
		if(a[i].err != nil){
			write(m[i^1]->fd, "won", 3);
			pushplayer(m[i^1]);
			free(m[i]);
			goto out;
		}
		if(write(m[i^1]->fd, s, strlen(s)) != strlen(s)){
			write(m[i]->fd, "won", 3);
			/* TODO free the player */
			pushplayer(m[i]);
			free(m[i^1]);
			goto out;
		}
		free(s);
	}
out:
	if(debug)
		fprint(2, "[%d] serveproc ending\n", getpid());
	chanclose(cp[0].c);
	chanclose(cp[1].c);
	close(cp[i].fd);
	/* TODO make sure this is the last thread to exit */
//	recv(cp[0].done)
//	recv(cp[1].done)
	free(m);
	yield();
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
				fprint(2, "reapin fd %d sfd %d o %p?",
						playerq.players[i]->fd, playerq.players[i]->sfd, playerq.players[i]->o);
			n = read(playerq.players[i]->sfd, buf, sizeof buf);
			if(n < 0 || strncmp(buf, "Closed", 6) == 0){
				if(debug)
					fprint(2, " yes\n");
				qlock(&playerq);
				close(playerq.players[i]->fd);
				close(playerq.players[i]->sfd);
				free(playerq.players[i]);
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
	Player **match;

	threadsetname("matchmaker");

	for(;;){
		if(playerq.nplayers < 2){
			sleep(100);
			continue;
		}

		match = emalloc(2*sizeof(Player*));
		match[0] = popplayer();
		match[1] = popplayer();
		match[1]->o = match[0];
		match[0]->o = match[1];

		proccreate(serveproc, match, mainstacksize);
	}
}

void
playerproc(void *arg)
{
	char buf[256];
	int fd, n;

	threadsetname("playerproc");

	fd = *(int*)arg;

	while((n = read(fd, buf, sizeof(buf)-1)) > 0){
		buf[n] = 0;
		if(debug)
			fprint(2, "[%d] rcvd '%s'\n", getpid(), buf);
	}
	if(debug)
		fprint(2, "[%d] lost connection\n", getpid());
}

void
listenthread(void *arg)
{
	char *addr, adir[40], ldir[40]/*, aux[128], *s*/;
	int acfd, lcfd, dfd/*, sfd*/;
//	Player *p;

	addr = arg;

	acfd = announce(addr, adir);
	if(acfd < 0)
		sysfatal("announce: %r");

	if(debug)
		fprint(2, "listening on %s\n", addr);

	while((lcfd = listen(adir, ldir)) >= 0){
		if((dfd = accept(lcfd, ldir)) >= 0){
//			fd2path(dfd, aux, sizeof aux);
//			s = strrchr(aux, '/');
//			*s = 0;
//			snprint(aux, sizeof aux, "%s/status", aux);
//			sfd = open(aux, OREAD);
//			if(sfd < 0)
//				sysfatal("open: %r");
//
//			p = emalloc(sizeof *p);
//			p->fd = dfd;
//			p->sfd = sfd;
//			p->mc = nil;
//			pushplayer(p);
			proccreate(playerproc, &dfd, mainstacksize);
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

//	proccreate(matchmaker, nil, mainstacksize);
//	proccreate(reaper, nil, mainstacksize);
	threadcreate(listenthread, addr, mainstacksize);
	yield();
}
