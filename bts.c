#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <cursor.h>
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
Ship *curship;
Point2 lastshot;

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

int
rectXarmada(Rectangle r)
{
	int i;

	for(i = 0; i < nelem(armada); i++)
		if(curship != &armada[i] && rectXrect(r, armada[i].bbox))
			return 1;
	return 0;
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

	if(!rectinrect(s->bbox, localboard.bbox))
		return;

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
drawinfo(Image *dst)
{
	Point p;
	char *s;

	s = nil;
	switch(game.state){
	case Waiting0: s = "looking for players"; break;
	case Outlaying: s = "place the fleet"; break;
	case Waiting: s = "wait for your turn"; break;
	case Playing: s = "your turn"; break;
	}
	if(s == nil)
		return;
	p = Pt(SCRW/2 - stringwidth(font, s)/2, SCRH-Boardmargin);
	stringbg(dst, p, display->white, ZP, font, s, display->black, ZP);
}

void
redraw(void)
{
	lockdisplay(display);

	draw(screenb, screenb->r, display->black, nil, ZP);
	drawboard(screenb, &alienboard);
	drawboard(screenb, &localboard);
	drawships(screenb);
	drawinfo(screenb);

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
			brush = eallocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xAAAAAAFF);
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
		s->ncells = shiplen(i);
		s->orient = OV;
		s->hit = emalloc(s->ncells*sizeof(int));
		memset(s->hit, 0, s->ncells*sizeof(int));
		s->sunk = 0;
	}
}

int
confirmdone(Mousectl *mc)
{
	Cursor yousure = {
		{0, 0},
		{ 0xf7, 0xfe, 0x15, 0x54, 0x1d, 0x54, 0x09, 0x54,
		  0x09, 0xdc, 0x00, 0x00, 0x75, 0x77, 0x45, 0x54,
		  0x75, 0x66, 0x15, 0x54, 0x77, 0x57, 0x00, 0x00,
		  0x00, 0x02, 0x2a, 0x84, 0x11, 0x28, 0x2a, 0x90,
		},
		{ 0xea, 0x2b, 0xea, 0xab, 0xe2, 0xab, 0xf6, 0xab,
		  0xf6, 0x23, 0xff, 0xff, 0x8a, 0x88, 0xba, 0xab,
		  0x8a, 0x99, 0xea, 0xab, 0x88, 0xa8, 0xff, 0xff,
		  0xff, 0xfd, 0xd5, 0x7b, 0xee, 0xd7, 0xd5, 0x6f,
		}
	};
	setcursor(mc, &yousure);
	while(mc->buttons == 0)
		readmouse(mc);
	if(mc->buttons != 4){
		setcursor(mc, nil);
		return 0;
	}
	while(mc->buttons){
		if(mc->buttons != 4){
			setcursor(mc, nil);
			return 0;
		}
		readmouse(mc);
	}
	setcursor(mc, nil);
	return 1;
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
		if(b == &localboard)
			if(curship != nil && ++curship-armada >= nelem(armada))
				curship = nil;
		break;
	case Playing:
		if(b == &alienboard){
			chanprint(egress, "shoot %s\n", cell2coords(cell));
			lastshot = cell;
		}
		break;
	}
	send(drawchan, nil);
}

void
mmb(Mousectl *mc)
{
	enum {
		ROTATE,
	};
	static char *items[] = {
	 [ROTATE]	"rotate ship",
		nil
	};
	static Menu menu = { .item = items };

	if(game.state != Outlaying)
		return;

	mc->xy = addpt(mc->xy, screen->r.min);
	switch(menuhit(2, mc, &menu, _screen)){
	case ROTATE:
		if(curship != nil){
			curship->orient = curship->orient == OH? OV: OH;
			curship->bbox = mkshipbbox(curship->p, curship->orient, curship->ncells);

			/* steer it, captain! don't let it go off-board! */
			if(!rectinrect(curship->bbox, localboard.bbox))
				switch(curship->orient){
				case OH:
					curship->bbox.min.x -= curship->bbox.max.x-localboard.bbox.max.x;
					curship->bbox.max.x = localboard.bbox.max.x;
					break;
				case OV:
					curship->bbox.min.y -= curship->bbox.max.y-localboard.bbox.max.y;
					curship->bbox.max.y = localboard.bbox.max.y;
					break;
				}
				curship->p = toboard(&localboard, curship->bbox.min);
		}
		break;
	}
	send(drawchan, nil);
}

void
rmb(Mousectl *mc)
{
	enum {
		PLACESHIP,
		DONE,
	};
	static char *items[] = {
	 [PLACESHIP]	"place ships",
	 [DONE]		"done",
		nil
	};
	static Menu menu = { .item = items };
	char buf[5*(1+3+1)+1];
	int i, n;

	if(game.state != Outlaying)
		return;

	mc->xy = addpt(mc->xy, screen->r.min);
	switch(menuhit(3, mc, &menu, _screen)){
	case PLACESHIP:
		curship = &armada[0];
		break;
	case DONE:
		if(curship != nil)
			break;

		if(!confirmdone(mc))
			break;

		n = 0;
		for(i = 0; i < nelem(armada); i++){
			assert(sizeof buf - n > 1+3+1);
			if(i != 0)
				buf[n++] = ',';
			n += snprint(buf+n, sizeof buf - n, "%s%c",
				cell2coords(armada[i].p), armada[i].orient == OH? 'h': 'v');
		}
		chanprint(egress, "layout %s\n", buf);
		break;
	}
	send(drawchan, nil);
}

void
mouse(Mousectl *mc)
{
	Rectangle newbbox;

	mc->xy = subpt(mc->xy, screen->r.min);

	if(game.state == Outlaying && curship != nil){
		newbbox = mkshipbbox(toboard(&localboard, mc->xy), curship->orient, curship->ncells);

		if(rectinrect(newbbox, localboard.bbox) && !rectXarmada(newbbox)){
			curship->p = toboard(&localboard, mc->xy);
			curship->bbox = newbbox;
		}
		send(drawchan, nil);
	}

	switch(mc->buttons){
	case 1:
		lmb(mc);
		break;
	case 2:
		mmb(mc);
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
bobross(void *)
{
	while(recv(drawchan, nil) > 0)
		redraw();
	sysfatal("painter died");
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
processcmd(char *cmd)
{
	char *coords[2];

	if(debug)
		fprint(2, "rcvd '%s'\n", cmd);

	if(strcmp(cmd, "win") == 0){
//		celebrate();
		game.state = Waiting0;
	}else if(strcmp(cmd, "lose") == 0){
//		keelhaul();
		game.state = Waiting0;
	}

	switch(game.state){
	case Waiting0:
		if(strcmp(cmd, "layout") == 0){
			game.state = Outlaying;
			curship = &armada[0];
		}
		break;
	case Outlaying:
		if(strcmp(cmd, "wait") == 0)
			game.state = Waiting;
		else if(strcmp(cmd, "play") == 0)
			game.state = Playing;
		break;
	case Playing:
		if(strcmp(cmd, "wait") == 0)
			game.state = Waiting;
		else if(strcmp(cmd, "hit") == 0)
			settile(&alienboard, lastshot, Thit);
		else if(strcmp(cmd, "miss") == 0)
			settile(&alienboard, lastshot, Tmiss);
		break;
	case Waiting:
		if(strcmp(cmd, "play") == 0)
			game.state = Playing;
		else if(strncmp(cmd, "hit", 3) == 0){
			if(gettokens(cmd+4, coords, nelem(coords), "-") == nelem(coords))
				settile(&localboard, Pt2(strtoul(coords[0], nil, 10), strtoul(coords[1], nil, 10), 1), Thit);
		}else if(strncmp(cmd, "miss", 4) == 0){
			if(gettokens(cmd+5, coords, nelem(coords), "-") == nelem(coords))
				settile(&localboard, Pt2(strtoul(coords[0], nil, 10), strtoul(coords[1], nil, 10), 1), Tmiss);
		}
		break;
	}
	send(drawchan, nil);
}

void
netrecvthread(void *arg)
{
	Ioproc *io;
	char buf[256], *e;
	int n, tot, fd;

	fd = *(int*)arg;
	io = ioproc();

	tot = 0;
	while((n = ioread(io, fd, buf+tot, sizeof(buf)-1-tot)) > 0){
		tot += n;
		buf[tot] = 0;
		while((e = strchr(buf, '\n')) != nil){
			*e++ = 0;
			processcmd(buf);
			tot -= e-buf;
			memmove(buf, e, tot);
		}
		if(tot >= sizeof(buf)-1)
			tot = 0;
	}
	closeioproc(io);
	threadexitsall("connection lost");
}

void
netsendthread(void *arg)
{
	char *s;
	int fd;

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
	ingress = chancreate(sizeof(char*), 1);
	egress = chancreate(sizeof(char*), 1);
	threadcreate(bobross, nil, mainstacksize);
	threadcreate(inputthread, &in, mainstacksize);
	threadcreate(netrecvthread, &fd, mainstacksize);
	threadcreate(netsendthread, &fd, mainstacksize);
	send(drawchan, nil);
	yield();
}
