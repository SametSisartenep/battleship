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

Channel *playerq;
Channel *msgq;
Channel *mmctl; /* matchmaker's */
Match theater;
RWLock theaterlk;


Player *
newplayer(int fd)
{
	Player *p;

	p = emalloc(sizeof *p);
	p->name[0] = 0;
	p->state = Waiting0;
	p->battle = nil;
	p->nci = getnetconninfo(nil, fd);
	p->io.fd = fd;
	p->io.in = chancreate(sizeof(char*), 8);
	p->io.out = chancreate(sizeof(char*), 8);
	p->ctl = chancreate(sizeof(char*), 1);
	p->io.ctl = p->ctl;
	return p;
}

void
freeplayer(Player *p)
{
	chanfree(p->io.out);
	chanfree(p->io.in);
	chanfree(p->ctl);
	close(p->io.fd);
	freenetconninfo(p->nci);
	free(p);
}

Msg *
newmsg(Player *p, char *s)
{
	Msg *m;

	m = emalloc(sizeof *m);
	m->from = p;
	m->body = s; /* must be malloc'ed */
	return m;
}

void
freemsg(Msg *m)
{
	free(m->body);
	free(m);
}

Match *
newmatch(Player *p0, Player *p1)
{
	Match *m;

	m = emalloc(sizeof *m);
	m->id = p0->io.fd + p1->io.fd;
	m->data = chancreate(sizeof(Msg*), 8);
	m->ctl = chancreate(sizeof(Msg*), 1);
	m->pl[0] = p0;
	m->pl[1] = p1;
	return m;
}

void
addmatch(Match *m)
{
	wlock(&theaterlk);
	m->next = &theater;
	m->prev = theater.prev;
	theater.prev->next = m;
	theater.prev = m;
	wunlock(&theaterlk);
}

Match *
getmatch(int id)
{
	Match *m;

	rlock(&theaterlk);
	for(m = theater.next; m != &theater; m = m->next)
		if(m->id == id){
			runlock(&theaterlk);
			return m;
		}
	runlock(&theaterlk);
	return nil;
}

void
rmmatch(Match *m)
{
	wlock(&theaterlk);
	m->prev->next = m->next;
	m->next->prev = m->prev;
	m->prev = nil;
	m->next = nil;
	wunlock(&theaterlk);
}

void
freematch(Match *m)
{
	chanfree(m->data);
	chanfree(m->ctl);
	free(m);
}

void
takeseat(Stands *s, Player *p)
{
	if(++s->nused > s->cap){
		s->cap = s->nused;
		s->seats = erealloc(s->seats, s->nused * sizeof p);
	}
	s->seats[s->nused-1] = p;
}

void
leaveseat(Stands *s, Player *p)
{
	int i;

	for(i = 0; i < s->nused; i++)
		if(s->seats[i] == p)
			memmove(&s->seats[i], &s->seats[i+1], --s->nused * sizeof p);
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
			chanprint(cp->in, "%s", buf);
			tot -= e-buf;
			memmove(buf, e, tot);
		}
		if(tot >= sizeof(buf)-1)
			tot = 0;
	}
	closeioproc(io);
	sendp(cp->ctl, nil);
	threadexits(nil);
}

void
netsendthread(void *arg)
{
	Chanpipe *cp;
	char *s;

	cp = arg;

	while((s = recvp(cp->out)) != nil){
		if(write(cp->fd, s, strlen(s)) != strlen(s))
			sendp(cp->ctl, nil);
		else if(debug)
			fprint(2, "[%d] sent '%s'\n", getpid(), s);
		free(s);
	}
	sendp(cp->ctl, nil);
	threadexits(nil);
}

void
playerproc(void *arg)
{
	Player *my;
	char *s, *cmd[2];
	int nc;

	my = arg;

	threadsetname("player %s", my->nci->raddr);

	threadsetgrp(my->io.fd);
	threadcreate(netrecvthread, &my->io, mainstacksize);
	threadcreate(netsendthread, &my->io, mainstacksize);

	chanprint(my->io.out, "id\n");

	enum { NETIN, CTL, NONE };
	Alt a[] = {
	 [NETIN]	{my->io.in, &s, CHANRCV},
	 [CTL]		{my->ctl, &s, CHANRCV},
	 [NONE]		{nil, nil, CHANEND}
	};
	for(;;)
		switch(alt(a)){
		case NETIN:
			if(debug)
				fprint(2, "[%d] rcvd '%s'\n", getpid(), s);
			if(my->name[0] == 0){
				nc = tokenize(s, cmd, nelem(cmd));
				if(nc == 2 && strcmp(cmd[0], "id") == 0 && strlen(cmd[1]) > 0)
					snprint(my->name, sizeof my->name, "%s", cmd[1]);
				else
					chanprint(my->io.out, "id\n");
				free(s);
			}else
				sendp(msgq, newmsg(my, s));
			break;
		case CTL:
			if(s == nil) /* cable cut */
				goto End;
			free(s);
			break;
		}
End:
	if(debug)
		fprint(2, "[%d] lost connection\n", getpid());
	sendp(msgq, newmsg(my, nil));
	threadkillgrp(threadgetgrp());
	threadexits(nil);
}

void
operator(void *)
{
	Msg *msg;
	Match *m;
	char *cmd[2];
	int nc, mid;

	threadsetname("operator");

	while((msg = recvp(msgq)) != nil){
		if(debug)
			fprint(2, "operator got '%s' from p(fd=%d)\n", msg->body, msg->from->io.fd);

		if(msg->body == nil){ /* player left */
			if(msg->from->state != Waiting0)
				sendp(mmctl, newmsg(msg->from, estrdup("player left")));
			else
				freeplayer(msg->from);
		}else{
			switch(msg->from->state){
			case Waiting0:
				nc = tokenize(msg->body, cmd, nelem(cmd));
				if(nc == 1 && strcmp(cmd[0], "play") == 0)
					sendp(playerq, msg->from);
				else if(nc == 1 && strcmp(cmd[0], "watch") == 0){
					if(theater.next == &theater)
						chanprint(msg->from->io.out, "no matches\n");
					else for(m = theater.next; m != &theater; m = m->next)
						chanprint(msg->from->io.out, "%d %s vs. %s\n", m->id, m->pl[0]->name, m->pl[1]->name);
				}else if(nc == 2 && strcmp(cmd[0], "watch") == 0){
					mid = strtoul(cmd[1], nil, 10);
					m = getmatch(mid);
					if(m == nil)
						chanprint(msg->from->io.out, "no such match\n");
					else
						sendp(m->ctl, newmsg(msg->from, estrdup("take seat")));
				}
				break;
			case Watching:
				nc = tokenize(msg->body, cmd, nelem(cmd));
				if(nc == 1 && strcmp(cmd[0], "leave") == 0)
					sendp(msg->from->battle->ctl, newmsg(msg->from, estrdup("leave seat")));
				break;
			default:
				if(msg->from->battle != nil)
					sendp(msg->from->battle->data, newmsg(msg->from, estrdup(msg->body)));
			}
		}
		freemsg(msg);
	}
	threadexitsall("operator was KIA");
}

void
battleproc(void *arg)
{
	Msg *msg;
	Match *m;
	Player *p, *op;
	Stands stands;
	char *cmd[2];
	uint n0;
	int nc;

	Point2 cell;
	char *coords[5];
	int i, orient;

	m = arg;
	memset(&stands, 0, sizeof stands);

	threadsetname("battleproc [%d] %s ↔ %s", m->id, m->pl[0]->nci->raddr, m->pl[1]->nci->raddr);

	chanprint(m->pl[0]->io.out, "oid %s\n", m->pl[1]->name);
	chanprint(m->pl[1]->io.out, "oid %s\n", m->pl[0]->name);
	chanprint(m->pl[0]->io.out, "layout\n");
	chanprint(m->pl[1]->io.out, "layout\n");
	m->pl[0]->state = Outlaying;
	m->pl[1]->state = Outlaying;

	enum { DATA, CTL, NONE };
	Alt a [] = {
	 [DATA]	{m->data, &msg, CHANRCV},
	 [CTL]	{m->ctl, &msg, CHANRCV},
	 [NONE]	{nil, nil, CHANEND}
	};
	for(;;){
		switch(alt(a)){
		case DATA:
			if(debug) fprint(2, "[%d] battleproc rcvd '%s' from p(fd=%d) on data\n", getpid(), msg->body, msg->from->io.fd);

			p = msg->from;
			op = p == m->pl[0]? m->pl[1]: m->pl[0];

			nc = tokenize(msg->body, cmd, nelem(cmd));

			switch(p->state){
			case Outlaying:
				if(nc == 2 && strcmp(cmd[0], "layout") == 0)
					if(gettokens(cmd[1], coords, nelem(coords), ",") == nelem(coords)){
						if(debug)
							fprint(2, "rcvd layout from %s @ %s\n", p->name, p->nci->raddr);
						for(i = 0; i < nelem(coords); i++){
							cell = coords2cell(coords[i]);
							orient = coords[i][strlen(coords[i])-1] == 'h'? OH: OV;
							settiles(p, cell, orient, shiplen(i), Tship);
						}
						p->state = Waiting;
						if(op->state == Waiting){
							if(debug){
								fprint(2, "%s's map:\n", p->name);
								fprintmap(2, p);
								fprint(2, "%s's map:\n", op->name);
								fprintmap(2, op);
							}
							n0 = truerand();
							if(debug)
								fprint(2, "let the game begin: %s plays, %s waits\n", m->pl[n0%2]->name, m->pl[(n0+1)%2]->name);
							chanprint(m->pl[n0%2]->io.out, "play\n");
							m->pl[n0%2]->state = Playing;
							chanprint(m->pl[(n0+1)%2]->io.out, "wait\n");
						}
					}
				break;
			case Playing:
				if(nc == 2 && strcmp(cmd[0], "shoot") == 0){
					cell = coords2cell(cmd[1]);
					switch(gettile(op, cell)){
					case Tship:
						settile(op, cell, Thit);
						chanprint(p->io.out, "hit\n");
						chanprint(op->io.out, "hit %s\n", cell2coords(cell));
						if(countshipcells(op) < (debug? 17: 1)){
							chanprint(p->io.out, "win\n");
							chanprint(op->io.out, "lose\n");
							p->state = Waiting0;
							p->battle = nil;
							op->state = Waiting0;
							op->battle = nil;
							freemsg(msg);
							goto Finish;
						}
						goto Swapturn;
					case Twater:
						settile(op, cell, Tmiss);
						chanprint(p->io.out, "miss\n");
						chanprint(op->io.out, "miss %s\n", cell2coords(cell));
Swapturn:
						chanprint(p->io.out, "wait\n");
						chanprint(op->io.out, "play\n");
						p->state = Waiting;
						op->state = Playing;
						break;
					}
					if(debug)
						fprint(2, "%s plays, %s waits\n", op->name, p->name);
				}
				break;
			}

			freemsg(msg);
			break;
		case CTL:
			if(debug) fprint(2, "[%d] battleproc rcvd '%s' from p(fd=%d) on ctl\n", getpid(), msg->body, msg->from->io.fd);

			p = msg->from;
			if(strcmp(msg->body, "player left") == 0){
				if(p->state == Watching){
					leaveseat(&stands, p);
					freeplayer(p);
				}else{
					op = p == m->pl[0]? m->pl[1]: m->pl[0];
					chanprint(op->io.out, "win\n");
					op->state = Waiting0;
					op->battle = nil;
					freeplayer(p);
					freemsg(msg);
					goto Finish;
				}
			}else if(strcmp(msg->body, "take seat") == 0){
				takeseat(&stands, p);
				p->battle = m;
				p->state = Watching;
			}else if(strcmp(msg->body, "leave seat") == 0){
				leaveseat(&stands, p);
				p->battle = nil;
				p->state = Waiting0;
			}

			freemsg(msg);
			break;
		}
	}
Finish:
	if(debug)
		fprint(2, "[%d] battleproc ending\n", getpid());
	free(stands.seats);
	rmmatch(m);
	freematch(m);
	threadexits(nil);
}

void
matchmaker(void *)
{
	Msg *msg;
	Match *m;
	Player *pl[2];
	int i;

	threadsetname("matchmaker");

	i = 0;

	enum { QUE, CTL, NONE };
	Alt a[] = {
	 [QUE]	{playerq, &pl[0], CHANRCV},
	 [CTL]	{mmctl, &msg, CHANRCV},
	 [NONE]	{nil, nil, CHANEND}
	};
	for(;;)
		switch(alt(a)){
		case QUE:
			if(debug) fprint(2, "matchmaker got %d%s player fd %d\n", i+1, i == 0? "st": "nd", pl[i]->io.fd);

			pl[i]->state = Ready;
			memset(pl[i]->map, Twater, MAPW*MAPH);

			if(++i > 1){
				m = newmatch(pl[0], pl[1]);
				addmatch(m);
				pl[0]->battle = m;
				pl[1]->battle = m;
				i = 0;

				proccreate(battleproc, m, mainstacksize);
			}
			a[QUE].v = &pl[i];
			break;
		case CTL:
			if(debug) fprint(2, "matchmaker rcvd '%s' from p(fd=%d)\n", msg->body, msg->from->io.fd);

			if(strcmp(msg->body, "player left") == 0){
				if(i == 1 && pl[0] == msg->from){
					i = 0;
					a[QUE].v = &pl[0];
					freeplayer(pl[0]);
					freemsg(msg);
				}else
					sendp(msg->from->battle->ctl, msg);
			}
			break;
		}
}

int
fprintmatches(int fd)
{
	Match *m;
	int n;

	n = 0;
	if(theater.next == &theater)
		n += fprint(fd, "let there be peace\n");
	else for(n = 0, m = theater.next; m != &theater; m = m->next)
		n += fprint(fd, "%d\t%s vs. %s\n", m->id, m->pl[0]->name, m->pl[1]->name);
	return n;
}

int
fprintmatch(int fd, Match *m)
{
	int n, i;

	n = 0;
	if(m == nil)
		n += fprint(fd, "no such match\n");
	else{
		n += fprint(fd, "id %d\n", m->id);
		n += fprint(fd, "players\n");
		for(i = 0; i < nelem(m->pl); i++)
			n += fprint(fd, "\t%d\n"
					"\t\tname %s\n"
					"\t\tstate %s\n"
					"\t\taddr %s\n"
					"\t\tfd %d\n",
				i, m->pl[i]->name, statename(m->pl[i]->state), m->pl[i]->nci->raddr, m->pl[i]->io.fd);
	}
	return n;
}

/*
 * Command & Control
 *
 *	- show matches: prints ongoing matches
 *	- show match [mid]: prints info about a given match
 *	- debug [on|off]: toggles debug mode
 */
void
c2proc(void *)
{
	char buf[256], *user, *cmd[3];
	int fd, pfd[2], n, nc, mid;

	threadsetname("c2proc");

	if(pipe(pfd) < 0)
		sysfatal("pipe: %r");

	user = getenv("user");
	snprint(buf, sizeof buf, "/srv/btsctl.%s.%d", user, getppid());
	free(user);

	fd = create(buf, OWRITE|ORCLOSE|OCEXEC, 0600);
	if(fd < 0)
		sysfatal("open: %r");
	fprint(fd, "%d", pfd[0]);
	close(pfd[0]);

	while((n = read(pfd[1], buf, sizeof(buf)-1)) > 0){
		buf[n] = 0;

		nc = tokenize(buf, cmd, nelem(cmd));
		if((nc == 2 || nc == 3) && strcmp(cmd[0], "show") == 0){
			if(nc == 2 && strcmp(cmd[1], "matches") == 0)
				fprintmatches(pfd[1]);
			else if(nc == 3 && strcmp(cmd[1], "match") == 0){
				mid = strtoul(cmd[2], nil, 10);
				fprintmatch(pfd[1], getmatch(mid));
			}
		}else if(nc == 2 && strcmp(cmd[0], "debug") == 0){
			if(strcmp(cmd[1], "on") == 0)
				debug = 1;
			else if(strcmp(cmd[1], "off") == 0)
				debug = 0;
		}
	}

	threadexitsall("fleet admiral drowned");
}

void
dolisten(char *addr)
{
	char adir[40], ldir[40];
	int acfd, lcfd, dfd;
	Player *p;

	acfd = announce(addr, adir);
	if(acfd < 0)
		sysfatal("announce: %r");

	if(debug)
		fprint(2, "listening on %s\n", addr);

	while((lcfd = listen(adir, ldir)) >= 0){
		if((dfd = accept(lcfd, ldir)) >= 0){
			p = newplayer(dfd);
			proccreate(playerproc, p, mainstacksize);
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
	default: usage();
	}ARGEND
	if(argc != 0)
		usage();

	playerq = chancreate(sizeof(Player*), 8);
	msgq = chancreate(sizeof(Msg*), 8);
	mmctl = chancreate(sizeof(Msg*), 8);
	theater.next = theater.prev = &theater;
	proccreate(c2proc, nil, mainstacksize);
	proccreate(matchmaker, nil, mainstacksize);
	proccreate(operator, nil, mainstacksize);
	dolisten(addr);
}
