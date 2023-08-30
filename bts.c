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

Cursor patrolcursor = {
	{0, 0},
	{ 0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x03, 0x80,
	  0x06, 0x80, 0x06, 0xc0, 0x0e, 0xe0, 0x1e, 0xf0,
	  0x1f, 0xf8, 0x3f, 0x00, 0x01, 0x00, 0x41, 0x02,
	  0x7f, 0xfe, 0x7f, 0xfc, 0x3f, 0xf8, 0x00, 0x00,
	},
	{ 0x03, 0x80, 0x06, 0x80, 0x04, 0xc0, 0x0c, 0x40,
	  0x09, 0x60, 0x19, 0x30, 0x31, 0x18, 0x21, 0x0c,
	  0x60, 0x04, 0x40, 0xfc, 0xfe, 0x87, 0xbe, 0xfd,
	  0x80, 0x01, 0x80, 0x03, 0xc0, 0x06, 0x7f, 0xfc,
	},
};
Cursor waitcursor = {
	{0, 0},
	{ 0x01, 0x80, 0x03, 0xc0, 0x07, 0xe0, 0x07, 0xe0,
	  0x07, 0xe0, 0x07, 0xe0, 0x03, 0xc0, 0x0f, 0xf0,
	  0x1f, 0xf8, 0x1f, 0xf8, 0x1f, 0xf8, 0x1f, 0xf8,
	  0x0f, 0xf0, 0x1f, 0xf8, 0x3f, 0xfc, 0x3f, 0xfc,
	},
	{ 0x01, 0x80, 0x03, 0xc0, 0x07, 0xe0, 0x04, 0x20,
	  0x04, 0x20, 0x06, 0x60, 0x02, 0x40, 0x0c, 0x30,
	  0x10, 0x08, 0x14, 0x08, 0x14, 0x28, 0x12, 0x28,
	  0x0a, 0x50, 0x16, 0x68, 0x20, 0x04, 0x3f, 0xfc,
	}
};
Cursor boxcursor = {
	{-7, -7},
	{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	  0xFF, 0xFF, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F,
	  0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xFF, 0xFF,
	  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	},
	{ 0x00, 0x00, 0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE,
	  0x70, 0x0E, 0x70, 0x0E, 0x70, 0x0E, 0x70, 0x0E,
	  0x70, 0x0E, 0x70, 0x0E, 0x70, 0x0E, 0x70, 0x0E,
	  0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x00, 0x00
	}
};
Cursor aimcursor = {
	{-7, -7},
	{ 0x1f, 0xf8, 0x3f, 0xfc, 0x7f, 0xfe, 0xfb, 0xdf,
	  0xf3, 0xcf, 0xe3, 0xc7, 0xff, 0xff, 0xff, 0xff,
	  0xff, 0xff, 0xff, 0xff, 0xe3, 0xc7, 0xf3, 0xcf,
	  0x7b, 0xdf, 0x7f, 0xfe, 0x3f, 0xfc, 0x1f, 0xf8,
	},
	{ 0x00, 0x00, 0x0f, 0xf0, 0x31, 0x8c, 0x21, 0x84,
	  0x41, 0x82, 0x41, 0x82, 0x41, 0x82, 0x7f, 0xfe,
	  0x7f, 0xfe, 0x41, 0x82, 0x41, 0x82, 0x41, 0x82,
	  0x21, 0x84, 0x31, 0x8c, 0x0f, 0xf0, 0x00, 0x00,
	}
};

char deffont[] = "/lib/font/bit/pelm/unicode.9.font";
char winspec[32];
Channel *drawchan;
Channel *ingress, *egress;
Mousectl *mctl; /* only used to update the cursor */
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
	int layoutdone;
} game;

struct {
	Image *c; /* color */
	char *s; /* banner text */
} conclusion;


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

void
csetcursor(Mousectl *mc, Cursor *c)
{
	static Cursor *oc;

	if(c == oc)
		return;
	setcursor(mc, c);
	oc = c;
}

void
resetgame(void)
{
	int i;

	memset(localboard.map, Twater, MAPW*MAPH);
	memset(alienboard.map, Twater, MAPW*MAPH);
	for(i = 0; i < nelem(armada); i++){
		armada[i].bbox = ZR;
		memset(armada[i].hit, 0, armada[i].ncells*sizeof(int));
		armada[i].sunk = 0;
	}
	curship = nil;
	game.state = Waiting0;
	game.layoutdone = 0;
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
	static Image *c;
	Point p;
	char *s, aux[32];

	s = nil;
	switch(game.state){
	case Waiting0: s = "looking for players"; break;
	case Outlaying: s = "place the fleet"; break;
	case Waiting: s = "wait for your turn"; break;
	case Playing: s = "your turn"; break;
	}
	if(s == nil)
		return;
	p = Pt(SCRW/2 - stringwidth(font, s)/2, 0);
	string(dst, p, display->white, ZP, font, s);

	if(game.state == Outlaying){
		if(c == nil)
			c = eallocimage(display, Rect(0,0,1,1), screen->chan, 1, DYellow);
		if(curship != nil){
			snprint(aux, sizeof aux, "%s (%d)", shipname(curship-armada), curship->ncells);
			p = Pt(SCRW/2 - stringwidth(font, aux)/2, SCRH-Boardmargin);
			string(dst, p, c, ZP, font, aux);
		}else{
			s = "done with the layout?";
			p = Pt(SCRW/2 - stringwidth(font, s)/2, SCRH-Boardmargin);
			string(dst, p, c, ZP, font, s);
		}
	}
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

	if(conclusion.s != nil)
		string(screenb, Pt(SCRW/2 - stringwidth(font, conclusion.s)/2, font->height+5), conclusion.c, ZP, font, conclusion.s);

	draw(screen, screen->r, screenb, nil, ZP);

	flushimage(display, 1);
	unlockdisplay(display);

	if(conclusion.s != nil){
		resetgame();
		conclusion.s = nil;
		sleep(5000);
		redraw();
	}
}

void
resize(void)
{
	int fd;

	lockdisplay(display);
	if(getwindow(display, Refnone) < 0)
		sysfatal("resize failed");
	unlockdisplay(display);

	/* ignore move events */
	if(Dx(screen->r) != SCRW || Dy(screen->r) != SCRH){
		fd = open("/dev/wctl", OWRITE);
		if(fd >= 0){
			fprint(fd, "resize %s", winspec);
			close(fd);
		}
	}

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
	/* thanks sigrid! */
	Cursor anchor = {
		{0, 0},
		{ 0x00, 0x00, 0x00, 0x1e, 0x01, 0x92, 0x30, 0xd2,
		  0x70, 0x7e, 0x60, 0x70, 0x40, 0xf8, 0x41, 0xcc,
		  0x43, 0x84, 0x47, 0x00, 0x4e, 0x00, 0x5c, 0x00,
		  0x78, 0x18, 0x70, 0x38, 0x7f, 0xf0, 0x00, 0x00,
		},
		{ 0x00, 0x3f, 0x03, 0xe1, 0x7a, 0x6d, 0xcb, 0x2d,
		  0x89, 0x81, 0x99, 0x8f, 0xb3, 0x07, 0xa6, 0x33,
		  0xac, 0x7a, 0xb8, 0xce, 0xb1, 0x80, 0xa3, 0x3c,
		  0x86, 0x64, 0x8f, 0xc4, 0x80, 0x0c, 0xff, 0xf8,
		},
	};
	csetcursor(mc, &anchor);
	while(mc->buttons == 0)
		readmouse(mc);
	if(mc->buttons != 4){
		csetcursor(mc, nil);
		return 0;
	}
	while(mc->buttons){
		if(mc->buttons != 4){
			csetcursor(mc, nil);
			return 0;
		}
		readmouse(mc);
	}
	csetcursor(mc, nil);
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
			if(curship != nil && rectinrect(curship->bbox, localboard.bbox))
				if(++curship-armada >= nelem(armada))
					curship = nil;
				else if(curship != &armada[0])
					curship->orient = (curship-1)->orient;
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
			if(!rectinrect(curship->bbox, localboard.bbox)){
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
				moveto(mc, addpt(screen->r.min, curship->bbox.min));
			}
			/* …nor ram allied ships! */
			if(rectXarmada(curship->bbox))
				curship->bbox = ZR;
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
		if(!game.layoutdone)
			curship = &armada[0];
		break;
	case DONE:
		if(curship != nil || game.layoutdone)
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
		game.layoutdone++;
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

		if(ptinrect(mc->xy, localboard.bbox))
			csetcursor(mctl, &boxcursor);
		else
			csetcursor(mctl, nil);

		if(rectinrect(newbbox, localboard.bbox) && !rectXarmada(newbbox)){
			curship->p = toboard(&localboard, mc->xy);
			curship->bbox = newbbox;
			send(drawchan, nil);
		}
	}

	if(game.state == Playing)
		if(ptinrect(mc->xy, alienboard.bbox))
			csetcursor(mctl, &aimcursor);
		else
			csetcursor(mctl, nil);

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
celebrate(void)
{
	static Image *c;
	static char s[] = "YOU WON!";

	if(c == nil)
		c = eallocimage(display, Rect(0,0,1,1), screen->chan, 1, DGreen);
	conclusion.c = c;
	conclusion.s = s;
}

void
keelhaul(void)
{
	static Image *c;
	static char s[] = "…YOU LOST";

	if(c == nil)
		c = eallocimage(display, Rect(0,0,1,1), screen->chan, 1, DRed);
	conclusion.c = c;
	conclusion.s = s;
}

void
processcmd(char *cmd)
{
	Point2 cell;
	int i;

	if(debug)
		fprint(2, "rcvd '%s'\n", cmd);

	if(strcmp(cmd, "win") == 0)
		celebrate();
	else if(strcmp(cmd, "lose") == 0)
		keelhaul();

	switch(game.state){
	case Waiting0:
		if(strcmp(cmd, "layout") == 0){
			game.state = Outlaying;
			curship = &armada[0];
		}
		csetcursor(mctl, &patrolcursor);
		break;
	case Outlaying:
		if(strcmp(cmd, "wait") == 0){
			game.state = Waiting;
			csetcursor(mctl, &waitcursor);
		}else if(strcmp(cmd, "play") == 0)
			game.state = Playing;
		break;
	case Playing:
		if(strcmp(cmd, "wait") == 0){
			game.state = Waiting;
			csetcursor(mctl, &waitcursor);
		}else if(strcmp(cmd, "hit") == 0)
			settile(&alienboard, lastshot, Thit);
		else if(strcmp(cmd, "miss") == 0)
			settile(&alienboard, lastshot, Tmiss);
		break;
	case Waiting:
		if(strcmp(cmd, "play") == 0){
			game.state = Playing;
			csetcursor(mctl, nil);
		}else if(strncmp(cmd, "hit", 3) == 0){
			cell = coords2cell(cmd+4);
			for(i = 0; i < nelem(armada); i++)
				if(ptinrect(fromboard(&localboard, cell), armada[i].bbox)){
					cell = subpt2(cell, armada[i].p);
					armada[i].hit[(int)vec2len(cell)] = 1;
					break;
				}
		}else if(strncmp(cmd, "miss", 4) == 0){
			cell = coords2cell(cmd+5);
			settile(&localboard, cell, Tmiss);
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

	mctl = in.mc;

	screenb = eallocimage(display, rectsubpt(screen->r, screen->r.min), screen->chan, 0, DNofill);
	worldrf.p = Pt2(0,0,1);
	worldrf.bx = Vec2(1,0);
	worldrf.by = Vec2(0,1);

	inittiles();
	initboards();
	initarmada();
	game.state = Waiting0;
	csetcursor(mctl, &patrolcursor);

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
