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

char deffont[] = "/lib/font/bit/pelm/unicode.9.font";
char winspec[32];
Channel *drawchan;
Channel *ingress, *egress;
RFrame worldrf;
Image *screenb;
Image *tiletab[NTILES];
Board alienboard;
Board localboard;
Ship armada[NSHIPS];

struct {
	int state;
} game;


Point
fromworld(Point2 p)
{
	p = invrframexform(p, worldrf);
	return Pt(p.x,p.y);
}

Point2
toworld(Point p)
{
	return rframexform(Pt2(p.x,p.y,1), worldrf);
}

Point
fromboard(Board *b, Point2 p)
{
	p = invrframexform(invrframexform(p, *b), worldrf);
	return Pt(p.x,p.y);
}

Point2
toboard(Board *b, Point p)
{
	Point2 np;

	np = rframexform(rframexform(Pt2(p.x,p.y,1), worldrf), *b);
	np.x = (int)np.x;
	np.y = (int)np.y;
	return np;
}

Rectangle
mkshipbbox(Point2 p, int o, int ncells)
{
	Point2 sv;

	switch(o){
	case OH: sv = Vec2(1,0); break;
	case OV: sv = Vec2(0,1); break;
	default: sysfatal("mkshipbbox: wrong ship orientation");
	}

	return Rpt(
		fromboard(&localboard, p),
		fromboard(&localboard, addpt2(addpt2(p, mulpt2(sv, ncells)), Vec2(sv.y,sv.x)))
	);
}

Image *
gettileimage(int type)
{
	if(type < 0 || type > nelem(tiletab))
		return nil;
	return tiletab[type];
}

void
settile(Board *b, Point2 cell, int type)
{
	Point p;

	p.x = cell.x;
	p.y = cell.y;
	b->map[p.x][p.y] = type;
}

void
drawtile(Image *dst, Board *b, Point2 cell, int type)
{
	Point p;
	Image *ti;

	p = fromboard(b, cell);
	ti = gettileimage(type);
	if(ti == nil)
		return;

	draw(dst, Rpt(p, addpt(p, Pt(TW,TH))), ti, nil, ZP);
}

void
drawship(Image *dst, Ship *s)
{
	Point2 p, sv;
	int i;

	p = s->p;
	switch(s->orient){
	case OH: sv = Vec2(1,0); break;
	case OV: sv = Vec2(0,1); break;
	default: return;
	}

	for(i = 0; i < s->ncells; i++){
		drawtile(dst, &localboard, p, s->hit[i]? Thit: Tship);
		p = addpt2(p, sv);
	}
}

void
drawships(Image *dst)
{
	int i;

	for(i = 0; i < nelem(armada); i++)
		drawship(dst, &armada[i]);
}

void
drawboard(Image *dst, Board *b)
{
	int i, j;

	for(i = 0; i < MAPW; i++)
		for(j = 0; j < MAPH; j++)
			drawtile(dst, b, Pt2(i,j,1), b->map[i][j]);
}

void
redraw(void)
{
	lockdisplay(display);

	draw(screenb, screenb->r, display->black, nil, ZP);
	drawboard(screenb, &alienboard);
	drawboard(screenb, &localboard);
	drawships(screenb);

	draw(screen, screen->r, screenb, nil, ZP);

	flushimage(display, 1);
	unlockdisplay(display);
}

void
resize(void)
{
	lockdisplay(display);
	if(getwindow(display, Refnone) < 0)
		sysfatal("resize failed");
	unlockdisplay(display);

	freeimage(screenb);
	screenb = eallocimage(display, rectsubpt(screen->r, screen->r.min), screen->chan, 0, DNofill);
	send(drawchan, nil);
}

void
inittiles(void)
{
	Image *brush;
	int i, x, y;
	Point pts[2];

	for(i = 0; i < nelem(tiletab); i++){
		tiletab[i] = eallocimage(display, Rect(0,0,TW,TH), screen->chan, 0, DNofill);
		switch(i){
		case Twater:
			brush = eallocimage(display, Rect(0,0,1,1), screen->chan, 1, DPalegreyblue);
			draw(tiletab[i], tiletab[i]->r, brush, nil, ZP);
			freeimage(brush);
			brush = eallocimage(display, Rect(0,0,1,1), screen->chan, 1, DPalebluegreen);
			for(pts[0] = ZP, x = 0; x < TW; x++){
				y = sin(x)*TH/2;
				pts[1] = Pt(x,y+TH/2);
				line(tiletab[i], pts[0], pts[1], Endsquare, Endsquare, 0, brush, ZP);
				pts[0] = pts[1];
			}
			freeimage(brush);
			break;
		case Tship:
			brush = eallocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x333333FF);
			draw(tiletab[i], tiletab[i]->r, brush, nil, ZP);
			freeimage(brush);
			brush = eallocimage(display, Rect(0,0,1,1), screen->chan, 1, DBlack);
			fillellipse(tiletab[i], Pt(TW/2,TH/2), 2, 2, brush, ZP);
			freeimage(brush);
			break;
		case Thit:
			brush = eallocimage(display, Rect(0,0,1,1), screen->chan, 1, DRed);
			draw(tiletab[i], tiletab[i]->r, brush, nil, ZP);
			freeimage(brush);
			brush = eallocimage(display, Rect(0,0,1,1), screen->chan, 1, DBlack);
			pts[0] = Pt(TW/2-TW/4,TH/2-TH/4);
			pts[1] = Pt(TW/2+TW/4,TH/2+TH/4);
			line(tiletab[i], pts[0], pts[1], Endsquare, Endsquare, 1, brush, ZP);
			pts[0].y += TH/2;
			pts[1].y -= TH/2;
			line(tiletab[i], pts[0], pts[1], Endsquare, Endsquare, 1, brush, ZP);
			freeimage(brush);
			break;
		case Tmiss:
			draw(tiletab[i], tiletab[i]->r, tiletab[Twater], nil, ZP);
			brush = eallocimage(display, Rect(0,0,1,1), screen->chan, 1, DWhite);
			ellipse(tiletab[i], Pt(TW/2,TH/2), 6, 6, 1, brush, ZP);
			freeimage(brush);
			break;
		}
	}
}

void
initboards(void)
{
	memset(alienboard.map, Twater, MAPW*MAPH);
	alienboard.p = Pt2(Boardmargin,Boardmargin,1);
	alienboard.bx = Vec2(TW,0);
	alienboard.by = Vec2(0,TH);
	alienboard.bbox = Rpt(fromworld(alienboard.p), fromworld(addpt2(alienboard.p, Pt2(TW*MAPW,TH*MAPH,1))));

	memset(localboard.map, Twater, MAPW*MAPH);
	localboard.p = addpt2(alienboard.p, Vec2(0,MAPH*TH+TH));
	localboard.bx = Vec2(TW,0);
	localboard.by = Vec2(0,TH);
	localboard.bbox = Rpt(fromworld(localboard.p), fromworld(addpt2(localboard.p, Pt2(TW*MAPW,TH*MAPH,1))));
}

void
initarmada(void)
{
	Ship *s;
	int i;

	for(i = 0; i < nelem(armada); i++){
		s = &armada[i];
		switch(i){
		case Scarrier: s->ncells = 5; break;
		case Sbattleship: s->ncells = 4; break;
		case Scruiser: /* fallthrough */
		case Ssubmarine: s->ncells = 3; break;
		case Sdestroyer: s->ncells = 2; break;
		default: sysfatal("initships: unknown ship: %d", i);
		}
		s->orient = OH;
		s->hit = emalloc(s->ncells*sizeof(int));
		memset(s->hit, 0, s->ncells*sizeof(int));
		s->sunk = 0;
	}
}

void
placeship(Mousectl *mc, Ship *s)
{
	Rectangle newbbox;

	for(;;){
		if(readmouse(mc) < 0)
			break;

		mc->xy = subpt(mc->xy, screen->r.min);
		newbbox = mkshipbbox(toboard(&localboard, mc->xy), s->orient, s->ncells);

		if(rectinrect(newbbox, localboard.bbox)){
			s->p = toboard(&localboard, mc->xy);
			s->bbox = newbbox;
		}
		if(mc->buttons == 1 && ptinrect(mc->xy, localboard.bbox))
			break;
		send(drawchan, nil);
	}
}

void
placeships(Mousectl *mc)
{
	int i;

	for(i = 0; i < nelem(armada); i++)
		placeship(mc, &armada[i]);
}

void
lmb(Mousectl *mc)
{
	Board *b;
	Point2 cell;

	b = nil;
	if(ptinrect(mc->xy, alienboard.bbox))
		b = &alienboard;
	else if(ptinrect(mc->xy, localboard.bbox))
		b = &localboard;

	if(b == nil)
		return;

	cell = toboard(b, mc->xy);
	switch(game.state){
	case Outlaying:
		settile(b, cell, Tship);
		break;
	case Playing:
		chanprint(egress, "shoot %d-%d", (int)cell.x, (int)cell.y);
		break;
	}
	send(drawchan, nil);
}

void
rmb(Mousectl *mc)
{
	enum {
		PLACESHIP,
	};
	static char *items[] = {
	 [PLACESHIP]	"place ship",
		nil
	};
	static Menu menu = { .item = items };

	switch(menuhit(3, mc, &menu, _screen)){
	case PLACESHIP:
		placeships(mc);
		break;
	}
}

void
mouse(Mousectl *mc)
{
	mc->xy = subpt(mc->xy, screen->r.min);

	switch(mc->buttons){
	case 1:
		lmb(mc);
		break;
	case 2:
//		mmb(mc);
		break;
	case 4:
		rmb(mc);
		break;
	}
}

void
key(Rune r)
{
	switch(r){
	case Kdel:
	case 'q':
		threadexitsall(nil);
	}
}

void
showproc(void *)
{
	threadsetname("showproc");

	while(recv(drawchan, nil) > 0)
		redraw();

	sysfatal("showproc died");
}

void
inputthread(void *arg)
{
	Input *in;
	Rune r;
	Alt a[4];

	in = arg;

	a[0].c = in->mc->c; a[0].v = &in->mc->Mouse; a[0].op = CHANRCV;
	a[1].c = in->mc->resizec; a[1].v = nil; a[1].op = CHANRCV;
	a[2].c = in->kc->c; a[2].v = &r; a[2].op = CHANRCV;
	a[3].op = CHANEND;

	for(;;)
		switch(alt(a)){
		case -1:
			sysfatal("input thread interrupted");
		case 0:
			mouse(in->mc);
			break;
		case 1:
			resize();
			break;
		case 2:
			key(r);
			break;
		}
}

void
netrecvthread(void *arg)
{
	Ioproc *io;
	char buf[256], *coords[2];
	int n, fd;

	threadsetname("netrecvthread");

	fd = *(int*)arg;
	io = ioproc();

	while((n = ioread(io, fd, buf, sizeof(buf)-1)) > 0){
		buf[n] = 0;
		if(debug)
			fprint(2, "rcvd '%s'\n", buf);
		switch(game.state){
		case Waiting0:
			if(strcmp(buf, "layout") == 0)
				game.state = Outlaying;
			break;
		case Outlaying:
			if(strcmp(buf, "wait") == 0)
				game.state = Waiting;
			else if(strcmp(buf, "play") == 0)
				game.state = Playing;
			break;
		case Playing:
			if(strcmp(buf, "wait") == 0)
				game.state = Waiting;
			break;
		case Waiting:
			if(strcmp(buf, "play") == 0)
				game.state = Playing;
			else if(strncmp(buf, "hit", 3) == 0){
				if(gettokens(buf+4, coords, nelem(coords), "-") == nelem(coords))
					settile(&localboard, Pt2(strtoul(coords[0], nil, 10), strtoul(coords[1], nil, 10), 1), Thit);
			}else if(strncmp(buf, "miss", 4) == 0){
				if(gettokens(buf+5, coords, nelem(coords), "-") == nelem(coords))
					settile(&localboard, Pt2(strtoul(coords[0], nil, 10), strtoul(coords[1], nil, 10), 1), Tmiss);
			}
			break;
		}
	}
	closeioproc(io);
	threadexitsall("connection lost");
}

void
netsendthread(void *arg)
{
	char *s;
	int fd;

	threadsetname("netsendthread");

	fd = *(int*)arg;

	while(recv(egress, &s) > 0){
		if(write(fd, s, strlen(s)) != strlen(s))
			break;
		if(debug)
			fprint(2, "sent '%s'\n", s);
		free(s);
	}
	threadexitsall("connection lost");
}

void
usage(void)
{
	fprint(2, "usage: %s [-d] addr\n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	char *addr;
	int fd;
	Input in;

	GEOMfmtinstall();
	ARGBEGIN{
	case 'd':
		debug++;
		break;
	default: usage();
	}ARGEND
	if(argc != 1)
		usage();

	addr = netmkaddr(argv[0], "tcp", "3047");
	if(debug)
		fprint(2, "connecting to %s\n", addr);

	fd = dial(addr, nil, nil, nil);
	if(fd < 0)
		sysfatal("dial: %r");
	else if(debug)
		fprint(2, "line established\n");

	snprint(winspec, sizeof winspec, "-dx %d -dy %d", SCRW, SCRH);
	if(newwindow(winspec) < 0)
		sysfatal("newwindow: %r");
	if(initdraw(nil, deffont, "bts") < 0)
		sysfatal("initdraw: %r");
	if((in.mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	if((in.kc = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");

	display->locking = 1;
	unlockdisplay(display);

	screenb = eallocimage(display, rectsubpt(screen->r, screen->r.min), screen->chan, 0, DNofill);
	worldrf.p = Pt2(0,0,1);
	worldrf.bx = Vec2(1,0);
	worldrf.by = Vec2(0,1);

	inittiles();
	initboards();
	initarmada();
	game.state = Waiting0;

	drawchan = chancreate(sizeof(void*), 0);
	ingress = chancreate(sizeof(char*), 16);
	egress = chancreate(sizeof(char*), 16);
	proccreate(showproc, nil, mainstacksize);
	threadcreate(inputthread, &in, mainstacksize);
	threadcreate(netrecvthread, &fd, mainstacksize);
	threadcreate(netsendthread, &fd, mainstacksize);
	send(drawchan, nil);
	yield();
}
