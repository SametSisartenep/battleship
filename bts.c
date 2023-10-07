#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <cursor.h>
#include <keyboard.h>
#include <geometry.h>
#include "dat.h"
#include "fns.h"
#include "mixer.h"

enum {
	PCBlack,
	PCWhite,
	PCRed,
	PCGreen,
	PCShip,
	PCYellow,
	PCBlue,
	PCWater,
	PCWaves,
	PCBrown,
	PCShadow,
	NCOLORS
};

enum {
	CMid,
	CMqueued,
	CMlayout,
	CMoid,
	CMwait,
	CMplay,
	CMwehit,
	CMwemiss,
	CMtheyhit,
	CMtheymiss,
	CMmatches,	/* list opening */
	CMmatch,	/* list entry */
	CMendmatches,	/* list closure */
	CMwatching,
	CMwin,
	CMlose,
	CMplayeroutlay,
	CMplayerhit,
	CMplayermiss,
	CMplayerplays,
	CMplayerwon,
};
Cmdtab svcmd[] = {
	CMid,		"id",		1,
	CMqueued,	"queued",	1,
	CMlayout, 	"layout",	1,
	CMoid, 		"oid",		2,
	CMwait, 	"wait",		1,
	CMplay,		"play",		1,
	CMwehit,	"hit",		1,
	CMwemiss,	"miss",		1,
	CMtheyhit, 	"hit",		2,
	CMtheymiss,	"miss",		2,
	CMmatches,	"matches",	1,
	CMmatch,	"m",		4,
	CMendmatches,	"endmatches",	1,
	CMwatching,	"watching",	4,
	CMwin,		"win",		1,
	CMlose,		"lose",		1,
	CMplayeroutlay,	"outlayed",	3,
	CMplayerhit,	"hit",		3,
	CMplayermiss,	"miss",		3,
	CMplayerplays,	"plays",	2,
	CMplayerwon,	"won",		2,
};

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
	{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	  0xff, 0xff, 0xf8, 0x1f, 0xf8, 0x1f, 0xf8, 0x1f,
	  0xf8, 0x1f, 0xf8, 0x1f, 0xf8, 0x1f, 0xff, 0xff,
	  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	},
	{ 0x00, 0x00, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0xfe,
	  0x70, 0x0e, 0x70, 0x0e, 0x70, 0x0e, 0x70, 0x0e,
	  0x70, 0x0e, 0x70, 0x0e, 0x70, 0x0e, 0x70, 0x0e,
	  0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x00, 0x00
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
char titlefontpath[] = "assets/font/gunmetal/gunmetal.48.font";
Font *titlefont;
char winspec[32];
char uid[8+1], oid[8+1];
Channel *drawchan;
Channel *ingress, *egress;
Mousectl *mctl; /* only used to update the cursor */
RFrame worldrf;
Image *pal[NCOLORS];
Image *screenb;
Image *tiletab[NTILES];
AudioSource *playlist[NSOUNDS];
Board alienboard;
Board localboard;
Ship armada[NSHIPS];
Ship *curship;
int layoutdone;
Point2 lastshot;
Menulist *matches;
MatchInfo match; /* of which we are an spectator */

struct {
	int state;
	int mode;
} game;
struct {
	Image *c; /* color */
	char *s; /* banner text */
	AudioSource *snd; /* victory or defeat bg sfx */
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
	np.x = floor(np.x);
	np.y = floor(np.y);
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

	assert(o == OH || o == OV);
	sv = o == OH? Vec2(1,0): Vec2(0,1);

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
	}
	curship = nil;
	layoutdone = 0;
	oid[0] = 0;
	game.state = Waiting0;
	conclusion.s = nil;
	csetcursor(mctl, nil);
	audio_stop(conclusion.snd);
	conclusion.snd = nil;
	audio_play(playlist[SBG0]);
}

Point
vstring(Image *dst, Point p, Image *src, Point sp, Font *f, char *s)
{
	char buf[2];
	buf[1] = 0;
	while(*s){
		buf[0] = *s++;
		string(dst, p, src, sp, f, buf);
		p.y += font->height;
	}
	return p;
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
	assert(s->orient == OH || s->orient == OV);
	sv = s->orient == OH? Vec2(1,0): Vec2(0,1);

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

	border(dst, b->bbox, -Borderwidth, pal[PCBrown], ZP);
	for(i = 0; i < MAPW; i++)
		for(j = 0; j < MAPH; j++)
			drawtile(dst, b, Pt2(i,j,1), b->map[i][j]);
}

void
drawtitle(Image *dst)
{
	static char s[] = "BATTLESHIP";

	string(dst, Pt(SCRW/2 - stringwidth(titlefont, s)/2, 0), pal[PCWhite], ZP, titlefont, s);
}

void
drawgameoptions(Image *dst)
{
	static char s[] = "press p to play, w to watch";

	string(dst, Pt(SCRW/2 - stringwidth(font, s)/2, 10*font->height+5), pal[PCWhite], ZP, font, s);
}

void
drawinfo(Image *dst)
{
	Point p;
	char *s, aux[32], aux2[32];
	int i;

	s = "";
	switch(game.state){
	case Watching:
		snprint(aux, sizeof aux, "watching %s vs. %s", match.pl[0].uid, match.pl[1].uid);
		s = aux;
		break;
	case Ready: s = "looking for players"; break;
	case Outlaying: s = "place the fleet"; break;
	case Waiting: s = "wait for your turn"; break;
	case Playing: s = "your turn"; break;
	}
	p = Pt(SCRW/2 - stringwidth(font, s)/2, 0);
	string(dst, p, pal[PCWhite], ZP, font, s);

	s = "TARGET";
	p = subpt(alienboard.bbox.min, Pt(font->width+2+Borderwidth,0));
	vstring(dst, p, pal[PCWhite], ZP, font, s);
	s = "LOCAL";
	p = Pt(localboard.bbox.max.x+2+Borderwidth, localboard.bbox.min.y);
	vstring(dst, p, pal[PCWhite], ZP, font, s);

	p = Pt(alienboard.bbox.max.x+2+Borderwidth, alienboard.bbox.min.y);
	vstring(dst, p, pal[PCWhite], ZP, font, game.state == Watching? match.pl[1].uid: oid);
	p = subpt(localboard.bbox.min, Pt(font->width+2+Borderwidth,0));
	vstring(dst, p, pal[PCWhite], ZP, font, game.state == Watching? match.pl[0].uid: uid);

	/* TODO make this an info panel and show errors from bad transactions. */
	if(game.state == Outlaying){
		if(curship != nil){
			snprint(aux, sizeof aux, "%s (%d)", shipname(curship-armada), curship->ncells);
			p = Pt(SCRW/2 - stringwidth(font, aux)/2, SCRH-Boardmargin);
			string(dst, p, pal[PCYellow], ZP, font, aux);
		}else{
			s = "done with the layout?";
			p = Pt(SCRW/2 - stringwidth(font, s)/2, SCRH-Boardmargin);
			string(dst, p, pal[PCYellow], ZP, font, s);
		}
	}else if(game.state == Watching){
		snprint(aux, sizeof aux, "waiting for players to");
		snprint(aux2, sizeof aux2, "lay out their fleet");
		for(i = 0; i < nelem(match.pl); i++)
			if(match.pl[i].state == Playing){
				snprint(aux, sizeof aux, "it's %s's turn", match.pl[i].uid);
				aux2[0] = 0;
			}
		p = Pt(SCRW/2 - stringwidth(font, aux)/2, SCRH-Boardmargin);
		string(dst, p, pal[PCBlue], ZP, font, aux);
		p = Pt(SCRW/2 - stringwidth(font, aux2)/2, SCRH-Boardmargin+font->height);
		string(dst, p, pal[PCBlue], ZP, font, aux2);
	}
}

void
drawconclusion(Image *dst)
{
	static char s[] = "press any key to continue";

	if(conclusion.s == nil)
		return;

	draw(dst, dst->r, pal[PCShadow], nil, ZP);
	string(dst, Pt(SCRW/2 - stringwidth(font, conclusion.s)/2, font->height+5), conclusion.c, ZP, font, conclusion.s);
	string(dst, Pt(SCRW/2 - stringwidth(font, s)/2, 10*font->height+5), pal[PCWhite], ZP, font, s);
}

void
redraw(void)
{
	lockdisplay(display);

	draw(screenb, screenb->r, pal[PCBlack], nil, ZP);
	switch(game.state){
	case Waiting0:
		drawtitle(screenb);
		drawgameoptions(screenb);
		matches->draw(matches, screenb);
		break;
	default:
		drawboard(screenb, &alienboard);
		drawboard(screenb, &localboard);
		drawships(screenb);
		drawinfo(screenb);
		break;
	}
	drawconclusion(screenb);

	draw(screen, screen->r, screenb, nil, ZP);

	flushimage(display, 1);
	unlockdisplay(display);
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

	nbsend(drawchan, nil);
}

void
initpalette(void)
{
	pal[PCBlack] = display->black;
	pal[PCWhite] = display->white;
	pal[PCRed] = eallocimage(display, Rect(0,0,1,1), screen->chan, 1, DRed);
	pal[PCGreen] = eallocimage(display, Rect(0,0,1,1), screen->chan, 1, DGreen);
	pal[PCShip] = eallocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xAAAAAAFF);
	pal[PCYellow] = eallocimage(display, Rect(0,0,1,1), screen->chan, 1, DDarkyellow);
	pal[PCWater] = eallocimage(display, Rect(0,0,1,1), screen->chan, 1, DPalegreyblue);
	pal[PCWaves] = eallocimage(display, Rect(0,0,1,1), screen->chan, 1, DPalebluegreen);
	pal[PCBlue] = pal[PCWaves];
	pal[PCBrown] = eallocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x806000FF);
	pal[PCShadow] = eallocimage(display, Rect(0,0,1,1), RGBA32, 1, 0x0000007f);
}

void
inittiles(void)
{
	int i, x, y;
	Point pts[2];

	for(i = 0; i < nelem(tiletab); i++){
		tiletab[i] = eallocimage(display, Rect(0,0,TW,TH), screen->chan, 0, DNofill);
		switch(i){
		case Twater:
			draw(tiletab[i], tiletab[i]->r, pal[PCWater], nil, ZP);
			for(pts[0] = ZP, x = 0; x < TW; x++){
				y = sin(x)*TH/2;
				pts[1] = Pt(x,y+TH/2);
				line(tiletab[i], pts[0], pts[1], Endsquare, Endsquare, 0, pal[PCWaves], ZP);
				pts[0] = pts[1];
			}
			break;
		case Tship:
			draw(tiletab[i], tiletab[i]->r, pal[PCShip], nil, ZP);
			fillellipse(tiletab[i], Pt(TW/2,TH/2), 2, 2, pal[PCBlack], ZP);
			break;
		case Thit:
			draw(tiletab[i], tiletab[i]->r, pal[PCRed], nil, ZP);
			pts[0] = Pt(TW/2-TW/4,TH/2-TH/4);
			pts[1] = Pt(TW/2+TW/4,TH/2+TH/4);
			line(tiletab[i], pts[0], pts[1], Endsquare, Endsquare, 1, pal[PCBlack], ZP);
			pts[0].y += TH/2;
			pts[1].y -= TH/2;
			line(tiletab[i], pts[0], pts[1], Endsquare, Endsquare, 1, pal[PCBlack], ZP);
			break;
		case Tmiss:
			draw(tiletab[i], tiletab[i]->r, tiletab[Twater], nil, ZP);
			ellipse(tiletab[i], Pt(TW/2,TH/2), 6, 6, 1, pal[PCWhite], ZP);
			break;
		}
	}
}

void
initboards(void)
{
	memset(alienboard.map, Twater, MAPW*MAPH);
	alienboard.p = Pt2(Boardmargin+Borderwidth,Boardmargin+Borderwidth,1);
	alienboard.bx = Vec2(TW,0);
	alienboard.by = Vec2(0,TH);
	alienboard.bbox = Rpt(fromworld(alienboard.p), fromworld(addpt2(alienboard.p, Pt2(TW*MAPW,TH*MAPH,1))));

	memset(localboard.map, Twater, MAPW*MAPH);
	localboard.p = addpt2(alienboard.p, Vec2(0,MAPH*TH+Borderwidth+TH+Borderwidth));
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
	}
}

void
initsound(void)
{
	struct {
		char *path;
		double gain;
		int loops;
	} sndtab[NSOUNDS] = {
	 [SBG0]		{"assets/sfx/bg0.mp3", 1.0, 1},
	 [SBG1]		{"assets/sfx/bg1.mp3", 1.0, 1},
	 [SBG2]		{"assets/sfx/bg2.mp3", 1.0, 1},
	 [SCANNON]	{"assets/sfx/cannon.mp3", 5.0, 0},
	 [SWATER]	{"assets/sfx/water.mp3", 3.0, 0},
	 [SVICTORY]	{"assets/sfx/victory.mp3", 1.0, 1},
	 [SDEFEAT]	{"assets/sfx/defeat.mp3", 1.0, 1},
	};
	int i;

	audio_init(44100);
	audio_set_master_gain(0.5);

	for(i = 0; i < NSOUNDS; i++){
		playlist[i] = audio_new_source_from_file(sndtab[i].path);
		if(playlist[i] == nil)
			sysfatal("audio_new_source_from_file: %r");
		audio_set_gain(playlist[i], sndtab[i].gain);
		audio_set_loop(playlist[i], sndtab[i].loops);
	}

	audio_play(playlist[SBG0]);
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
	Point2 cell;
	char buf[3+1];

	if(conclusion.s != nil)
		return;

	switch(game.state){
	case Outlaying:
		if(!ptinrect(mc->xy, localboard.bbox))
			break;

		if(curship != nil && rectinrect(curship->bbox, localboard.bbox)){
			if(++curship-armada >= nelem(armada))
				curship = nil;
			else if(curship != &armada[0])
				curship->orient = (curship-1)->orient;
			nbsend(drawchan, nil);
		}
		break;
	case Playing:
		if(!ptinrect(mc->xy, alienboard.bbox))
			break;

		audio_play(playlist[SCANNON]);
		cell = toboard(&alienboard, mc->xy);
		/* TODO check if we already shot at that cell */
		cell2coords(buf, sizeof buf, cell);
		if(gettile(&alienboard, cell) == Twater){
			chanprint(egress, "shoot %s\n", buf);
			lastshot = cell;
		}
		break;
	}
}

void
mmb(Mousectl *mc)
{
	if(game.state != Outlaying)
		return;

	if(curship != nil){
		curship->orient = curship->orient == OH? OV: OH;
		curship->bbox = mkshipbbox(curship->p, curship->orient, curship->ncells);

		if(debug)
			fprint(2, "curship orient %c\n", curship->orient == OH? 'h': 'v');

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
			readmouse(mc); /* ignore mmb click triggered by moveto */
		}
		/* …nor ram allied ships! */
		if(rectXarmada(curship->bbox))
			curship->bbox = ZR;

		nbsend(drawchan, nil);
	}
}

void
rmb(Mousectl *mc)
{
	enum {
		PLACESHIP,
		DONE,
	};
	static char *items[] = {
	 [PLACESHIP]	"relocate ships",
	 [DONE]		"done",
		nil
	};
	static Menu menu = { .item = items };
	char buf[NSHIPS*(1+3+1)+1];
	int i, n;

	if(game.state != Outlaying)
		return;

	mc->xy = addpt(mc->xy, screen->r.min);
	switch(menuhit(3, mc, &menu, _screen)){
	case PLACESHIP:
		if(!layoutdone)
			curship = &armada[0];
		break;
	case DONE:
		if(curship != nil || layoutdone)
			break;

		if(!confirmdone(mc))
			break;

		n = 0;
		for(i = 0; i < nelem(armada); i++){
			assert(sizeof buf - n > 1+3+1);
			if(i != 0)
				buf[n++] = ',';
			n += cell2coords(buf+n, sizeof buf - n, armada[i].p);
			buf[n++] = armada[i].orient == OH? 'h': 'v';
		}
		chanprint(egress, "layout %s\n", buf);
		layoutdone++;
		break;
	}
	nbsend(drawchan, nil);
}

void
mouse(Mousectl *mc)
{
	Rectangle newbbox;
	static Mouse oldm;
	int selmatch;

	mc->xy = subpt(mc->xy, screen->r.min);

	if(game.state == Waiting0)
		if((selmatch = matches->update(matches, mc, drawchan)) >= 0){
			if(debug) fprint(2, "selected match id %d title %s\n", matches->entries[selmatch].id, matches->entries[selmatch].title);
			chanprint(egress, "watch %d\n", matches->entries[selmatch].id);
		}

	if(game.state == Outlaying && curship != nil){
		newbbox = mkshipbbox(toboard(&localboard, mc->xy), curship->orient, curship->ncells);

		if(ptinrect(mc->xy, localboard.bbox))
			csetcursor(mctl, &boxcursor);
		else
			csetcursor(mctl, nil);

		if(rectinrect(newbbox, localboard.bbox) && !rectXarmada(newbbox)){
			curship->p = toboard(&localboard, mc->xy);
			curship->bbox = newbbox;
			nbsend(drawchan, nil);
		}
	}

	if(game.state == Playing && conclusion.s == nil)
		if(ptinrect(mc->xy, alienboard.bbox))
			csetcursor(mctl, &aimcursor);
		else
			csetcursor(mctl, nil);

	if(oldm.buttons != mc->buttons)
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

	oldm = mc->Mouse;
}

void
key(Rune r)
{
	if(conclusion.s != nil){
		resetgame();
		nbsend(drawchan, nil);
		return;
	}

	switch(r){
	case Kdel:
	case 'q':
		threadexitsall(nil);
	case 'p':
		if(game.state != Waiting0)
			break;
		chanprint(egress, "play %d\n", game.mode);
		break;
	case 'w':
		if(game.state != Waiting0)
			break;
		chanprint(egress, "watch\n");
		break;
	}
}

void
celebrate(void)
{
	static char s[] = "YOU WON!";

	conclusion.c = pal[PCGreen];
	conclusion.s = s;
	conclusion.snd = playlist[SVICTORY];

	audio_stop(playlist[SBG2]);
	audio_play(conclusion.snd);
}

void
keelhaul(void)
{
	static char s[] = "…YOU LOST";

	conclusion.c = pal[PCRed];
	conclusion.s = s;
	conclusion.snd = playlist[SDEFEAT];

	audio_stop(playlist[SBG2]);
	audio_play(conclusion.snd);
}

void
announcewinner(char *winner)
{
	static char s[16];

	if(winner == nil)
		return;

	snprint(s, sizeof s, "%s WON", winner);
	conclusion.c = pal[PCGreen];
	conclusion.s = s;
	conclusion.snd = playlist[SVICTORY];

	audio_stop(playlist[SBG2]);
	audio_play(conclusion.snd);
}

void
processcmd(char *cmd)
{
	Cmdbuf *cb;
	Cmdtab *ct;
	Point2 cell;
	uchar buf[BY2MAP];
	int i, idx;

	if(debug)
		fprint(2, "rcvd '%s'\n", cmd);

	cb = parsecmd(cmd, strlen(cmd));
	ct = lookupcmd(cb, svcmd, nelem(svcmd));
	if(ct == nil){
		free(cb);
		return;
	}

	if(ct->index == CMwin)
		celebrate();
	else if(ct->index == CMlose)
		keelhaul();

	switch(game.state){
	case Waiting0:
		if(ct->index == CMid)
			chanprint(egress, "id %s\n", uid);
		else if(ct->index == CMqueued){
			game.state = Ready;
			csetcursor(mctl, &patrolcursor);
		}else if(ct->index == CMmatches && !matches->filling){
			matches->clear(matches);
			matches->filling = 1;
		}else if(ct->index == CMmatch && matches->filling)
			matches->add(matches, strtoul(cb->f[1], nil, 10), smprint("%s vs %s", cb->f[2], cb->f[3]));
		else if(ct->index == CMendmatches && matches->filling)
			matches->filling = 0;
		else if(ct->index == CMwatching){
			match.id = strtoul(cb->f[1], nil, 10);
			snprint(match.pl[0].uid, sizeof match.pl[0].uid, "%s", cb->f[2]);
			snprint(match.pl[1].uid, sizeof match.pl[1].uid, "%s", cb->f[3]);
			match.pl[0].state = Outlaying;
			match.pl[1].state = Outlaying;
			match.bl[0] = &localboard;
			match.bl[1] = &alienboard;
			game.state = Watching;
			audio_stop(playlist[SBG0]);
			audio_play(playlist[SBG2]);
		}
		break;
	case Ready:
		if(ct->index == CMlayout){
			game.state = Outlaying;
			curship = &armada[0];
			audio_stop(playlist[SBG0]);
			audio_play(playlist[SBG2]);
		}else if(ct->index == CMoid)
			snprint(oid, sizeof oid, "%s", cb->f[1]);
		break;
	case Watching:
		if(ct->index == CMplayeroutlay){
			idx = strtoul(cb->f[1], nil, 10);
			if(dec64(buf, sizeof buf, cb->f[2], strlen(cb->f[2])) < 0)
				sysfatal("dec64 failed");
			bitunpackmap(match.bl[idx], buf, sizeof buf);
			match.pl[idx].state = Waiting;
		}else if(ct->index == CMplayerhit){
			idx = strtoul(cb->f[1], nil, 10);
			cell = coords2cell(cb->f[2]);
			settile(match.bl[idx^1], cell, Thit);
		}else if(ct->index == CMplayermiss){
			idx = strtoul(cb->f[1], nil, 10);
			cell = coords2cell(cb->f[2]);
			settile(match.bl[idx^1], cell, Tmiss);
		}else if(ct->index == CMplayerplays){
			idx = strtoul(cb->f[1], nil, 10);
			match.pl[idx].state = Playing;
			match.pl[idx^1].state = Waiting;
		}else if(ct->index == CMplayerwon){
			idx = strtoul(cb->f[1], nil, 10);
			announcewinner(match.pl[idx].uid);
		}
		break;
	case Outlaying:
		if(ct->index == CMwait){
			game.state = Waiting;
			csetcursor(mctl, &waitcursor);
		}else if(ct->index == CMplay)
			game.state = Playing;
		break;
	case Playing:
		if(ct->index == CMwait){
			game.state = Waiting;
			csetcursor(mctl, &waitcursor);
		}else if(ct->index == CMwehit)
			settile(&alienboard, lastshot, Thit);
		else if(ct->index == CMwemiss){
			audio_play(playlist[SWATER]);
			settile(&alienboard, lastshot, Tmiss);
		}
		break;
	case Waiting:
		if(ct->index == CMplay){
			game.state = Playing;
			csetcursor(mctl, nil);
		}else if(ct->index == CMtheyhit){
			cell = coords2cell(cb->f[1]);
			for(i = 0; i < nelem(armada); i++)
				if(ptinrect(fromboard(&localboard, cell), armada[i].bbox)){
					cell = subpt2(cell, armada[i].p);
					armada[i].hit[(int)vec2len(cell)] = 1;
					break;
				}
		}else if(ct->index == CMtheymiss){
			cell = coords2cell(cb->f[1]);
			settile(&localboard, cell, Tmiss);
		}
		break;
	}
	free(cb);
	nbsend(drawchan, nil);
}

void
soundproc(void *)
{
	Biobuf *aout;
	uchar adata[512];

	threadsetname("soundproc");

	aout = Bopen("/dev/audio", OWRITE);
	if(aout == nil)
		sysfatal("Bopen: %r");

	for(;;){
		audio_process((void*)adata, sizeof(adata)/2);
		Bwrite(aout, adata, sizeof adata);
	}
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
	fprint(2, "usage: %s [-da] addr\n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	char *addr;
	char *user;
	int fd;
	Mousectl *mc;
	Keyboardctl *kc;
	Rune r;

	GEOMfmtinstall();
	ARGBEGIN{
	case 'd':
		debug++;
		break;
	case 'a':
		game.mode = GMPvAI;
		break;
	default: usage();
	}ARGEND
	if(argc != 1)
		usage();

	snprint(winspec, sizeof winspec, "-dx %d -dy %d", SCRW, SCRH);
	if(newwindow(winspec) < 0)
		sysfatal("newwindow: %r");
	if(initdraw(nil, deffont, "bts") < 0)
		sysfatal("initdraw: %r");
	if((mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	if((kc = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");

	display->locking = 1;
	unlockdisplay(display);

	mctl = mc;
	if((user = getenv("user")) == nil)
		user = getuser();
	snprint(uid, sizeof uid, "%s", user);

	screenb = eallocimage(display, rectsubpt(screen->r, screen->r.min), screen->chan, 0, DNofill);
	worldrf.p = Pt2(0,0,1);
	worldrf.bx = Vec2(1,0);
	worldrf.by = Vec2(0,1);

	titlefont = openfont(display, titlefontpath);
	if(titlefont == nil)
		sysfatal("openfont: %r");

	initpalette();
	inittiles();
	initboards();
	initarmada();
	matches = newmenulist(14*font->height, "ongoing matches");
	game.state = Waiting0;

	initsound();
	proccreate(soundproc, nil, mainstacksize);

	addr = netmkaddr(argv[0], "tcp", "3047");
	if(debug)
		fprint(2, "connecting to %s\n", addr);

	fd = dial(addr, nil, nil, nil);
	if(fd < 0)
		sysfatal("dial: %r");
	else if(debug)
		fprint(2, "line established\n");

	drawchan = chancreate(sizeof(void*), 1);
	ingress = chancreate(sizeof(char*), 1);
	egress = chancreate(sizeof(char*), 1);
	threadcreate(netrecvthread, &fd, mainstacksize);
	threadcreate(netsendthread, &fd, mainstacksize);
	nbsend(drawchan, nil);

	enum { MOUSE, RESIZE, KEYS, DRAW, NONE };
	Alt a[] = {
	 [MOUSE]	{mc->c, &mc->Mouse, CHANRCV},
	 [RESIZE]	{mc->resizec, nil, CHANRCV},
	 [KEYS]		{kc->c, &r, CHANRCV},
	 [DRAW]		{drawchan, nil, CHANRCV},
	 [NONE]		{nil, nil, CHANEND}
	};
	for(;;)
		switch(alt(a)){
		case MOUSE:
			mouse(mc);
			break;
		case RESIZE:
			resize();
			break;
		case KEYS:
			key(r);
			break;
		case DRAW:
			redraw();
			break;
		default:
			sysfatal("input thread interrupted");
		}
}
