#include <u.h>
#include <libc.h>
#include <mp.h>
#include <libsec.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include <geometry.h>
#include "dat.h"
#include "fns.h"

static char rowtab[] = "abcdefghijklmnopq";
static int shiplentab[] = {
 [Scarrier]	5,
 [Sbattleship]	4,
 [Scruiser]	3,
 [Ssubmarine]	3,
 [Sdestroyer]	2,
};
static char *shipnametab[] = {
 [Scarrier]	"carrier",
 [Sbattleship]	"battleship",
 [Scruiser]	"cruiser",
 [Ssubmarine]	"submarine",
 [Sdestroyer]	"destroyer",
};
static char *statenametab[] = {
 [Waiting0]	"Waiting0",
 [Watching]	"Watching",
 [Ready]	"Ready",
 [Outlaying]	"Outlaying",
 [Waiting]	"Waiting",
 [Playing]	"Playing",
};


int
isoob(Point2 cell)
{
	return cell.x < 0 || cell.x >= MAPW ||
		cell.y < 0 || cell.y >= MAPH;
}

char *
cell2coords(Point2 cell)
{
	static char s[3+1];

	assert(!isoob(cell));

	snprint(s, sizeof s, "%c%d", rowtab[(int)cell.y], (int)cell.x);
	return s;
}

Point2
coords2cell(char *s)
{
	Point2 cell;
	char *p;

	assert(s[0] >= 'a' && s[0] <= 'q');

	cell = Pt2(0,0,1);
	p = strchr(rowtab, s[0]);
	cell.y = p-rowtab;
	cell.x = strtol(s+1, nil, 10);

	assert(cell.x >= 0 && cell.x < MAPW);

	return cell;
}

int
gettile(Map *m, Point2 cell)
{
	return m->map[(int)cell.x][(int)cell.y];
}

void
settile(Map *m, Point2 cell, int type)
{
	assert(!isoob(cell));
	m->map[(int)cell.x][(int)cell.y] = type;
}

void
settiles(Map *m, Point2 cell, int o, int ncells, int type)
{
	Point2 sv;

	assert(o == OH || o == OV);
	sv = o == OH? Vec2(1,0): Vec2(0,1);

	while(ncells-- > 0){
		assert(!isoob(cell));
		settile(m, cell, type);
		cell = addpt2(cell, sv);
	}
}

void
fprintmap(int fd, Map *m)
{
	int i, j;

	for(i = 0; i < MAPH; i++){
		fprint(fd, "\t");
		for(j = 0; j < MAPW; j++)
			switch(m->map[j][i]){
			case Twater: fprint(fd, "W"); break;
			case Tship: fprint(fd, "S"); break;
			case Thit: fprint(fd, "X"); break;
			case Tmiss: fprint(fd, "O"); break;
			default: fprint(fd, "?"); break;
			}
		fprint(fd, "\n");
	}
}

int
countshipcells(Map *m)
{
	int i, j, n;

	n = 0;
	for(i = 0; i < MAPW; i++)
		for(j = 0; j < MAPH; j++)
			if(gettile(m, Pt2(i,j,1)) == Tship)
				n++;
	return n;
}

int
shiplen(int stype)
{
	if(stype < 0 || stype >= nelem(shiplentab))
		return -1;
	return shiplentab[stype];
}

char *
shipname(int stype)
{
	if(stype < 0 || stype >= nelem(shipnametab))
		return nil;
	return shipnametab[stype];
}

char *
statename(int state)
{
	if(state < 0 || state > nelem(statenametab))
		return nil;
	return statenametab[state];
}

int
min(int a, int b)
{
	return a < b? a: b;
}

int
max(int a, int b)
{
	return a > b? a: b;
}

int
bitpackmap(uchar *buf, ulong len, Map *m)
{
	int i, j, off, n;

	assert(len >= BY2MAP);

	off = n = 0;
	*buf = 0;
	for(i = 0; i < MAPW; i++)
		for(j = 0; j < MAPH; j++){
			if(off >= 8){
				buf[++n] = 0;
				off = 0;
			}
			buf[n] |= (m->map[i][j] & TMASK) << off;
			off += TBITS;
		}
	return n+1;
}

int
bitunpackmap(Map *m, uchar *buf, ulong len)
{
	int i, j, off, n;

	assert(len >= BY2MAP);

	off = n = 0;
	for(i = 0; i < MAPW; i++)
		for(j = 0; j < MAPH; j++){
			if(off >= 8){
				n++;
				off = 0;
			}
			m->map[i][j] = buf[n] >> off & TMASK;
			off += TBITS;
		}
	return n+1;
}

int
chanvprint(Channel *c, char *fmt, va_list arg)
{
	char *p;
	int n;

	p = vsmprint(fmt, arg);
	if(p == nil)
		sysfatal("vsmprint failed: %r");
	n = sendp(c, p);
	yield();	/* let recipient handle message immediately */
	return n;
}

ulong
getrand(ulong max)
{
	mpint *n, *r;
	ulong c;

	n = uitomp(max, nil);
	r = mpnrand(n, genrandom, nil);
	c = mptoui(r);
	mpfree(n);
	mpfree(r);
	return c;
}
