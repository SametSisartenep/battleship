#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include <geometry.h>
#include "dat.h"
#include "fns.h"

/* Nexus-9 technology from The Rosen Association */

static char *nametab[] = {
	"hannibal",
	"luba",
	"roy",
	"irmgard",
	"buster",
	"rachael",
	"phil",
	"pris",
	"polokov",
	"zhora",
	"kowalski",
	"luv",
	"sapper",
	"freysa",
	"mariette",
};
Point2 nwes[] = {
	{0,-1,0},
{-1,0,0},	{1,0,0},
	{0,1,0},
};

static char *
getaname(void)
{
	return nametab[getrand(nelem(nametab))];
}

static Point2
getnextfreecell(Map *m)
{
	Point2 p;
	int i, j;

	for(i = 0; i < MAPW; i++)
		for(j = 0; j < MAPH; j++)
			if(gettile(m, p = Pt2(i,j,1)) == Twater)
				return p;
	/*
	 * XXX getting here would mean that we shot every single cell and
	 * the game's still going, so something went wrong.
	 */
	abort();
	return Pt2(0,0,0);
}

static void
turnaround(Andy *a)
{
	if(--a->passes > 0){
		a->passdir = mulpt2(a->passdir, -1);
		a->lastshot = a->firsthit;
	}else
		a->disengage(a);
}

static int
lineXline(Point2 min0, Point2 max0, Point2 min1, Point2 max1)
{
	if(min0.x == max0.x)
		max0.x++;
	else if(min0.y == max0.y)
		max0.y++;

	if(min1.x == max1.x)
		max1.x++;
	else if(min1.y == max1.y)
		max1.y++;

	return min0.x < max1.x && min1.x < max0.x &&
	       min0.y < max1.y && min1.y < max0.y;
}

static void
andy_layout(Andy *a, Msg *m)
{
	Point2 cells[NSHIPS], sv[NSHIPS];
	char buf[NSHIPS*(1+3+1)+1];
	int i, j, o[NSHIPS], n;

	for(i = 0; i < NSHIPS; i++){
Retry:
		cells[i] = Pt2(getrand(MAPW-shiplen(i)), getrand(MAPH-shiplen(i)), 1);
		o[i] = i > 1 && o[i-1] != OH? OH: OV;
		sv[i] = o[i] == OH? Vec2(1,0): Vec2(0,1);
		for(j = 0; j < i; j++)
			if(lineXline(cells[i], addpt2(cells[i], mulpt2(sv[i], shiplen(i))),
					cells[j], addpt2(cells[j], mulpt2(sv[j], shiplen(j)))))
				goto Retry;
	}

	n = 0;
	for(i = 0; i < nelem(cells); i++){
		assert(sizeof buf - n > 1+3+1);
		if(i != 0)
			buf[n++] = ',';
		n += cell2coords(buf+n, sizeof buf - n, cells[i]);
		buf[n++] = o[i] == OH? 'h': 'v';
	}
	buf[n] = 0;

	m->body = smprint("layout %s", buf);
	sendp(a->ego->battle->data, m);
}

static void
andy_shoot(Andy *a, Msg *m)
{
	Point2 cell;
	char buf[3+1];
	int tries;

	tries = 0;

Retry:
	switch(a->state){
	case ASearching:
		do
			cell = ++tries > 100?
				getnextfreecell(a): Pt2(getrand(MAPW), getrand(MAPH), 1);
		while(gettile(a, cell) != Twater);
		break;
	case ACalibrating:
		do
			cell = addpt2(a->firsthit, nwes[--a->ntries&3]);
		while((gettile(a, cell) != Twater || isoob(cell)) && a->ntries > 1);
		if(gettile(a, cell) != Twater || isoob(cell)){
			a->disengage(a);
			goto Retry;
		}
		break;
	case ABombing:
		cell = addpt2(a->lastshot, a->passdir);
		if(gettile(a, cell) != Twater || isoob(cell)){
			turnaround(a);
			goto Retry;
		}
		break;
	}
	cell2coords(buf, sizeof buf, cell);
	m->body = smprint("shoot %s", buf);
	sendp(a->ego->battle->data, m);
	a->lastshot = cell;
}

static void
andy_engage(Andy *a)
{
	a->firsthit = a->lastshot;
	a->state = ACalibrating;
	a->ntries = nelem(nwes);
	a->passes = 2;
}

static void
andy_disengage(Andy *a)
{
	a->state = ASearching;
}

static void
andy_registerhit(Andy *a)
{
	settile(a, a->lastshot, Thit);
	if(a->state == ASearching)
		a->engage(a);
	else if(a->state == ACalibrating){
		a->passdir = subpt2(a->lastshot, a->firsthit);
		a->state = ABombing;
	}
}

static void
andy_registermiss(Andy *a)
{
	settile(a, a->lastshot, Tmiss);
	if(a->state == ACalibrating && a->ntries < 1)
		a->disengage(a);
	else if(a->state == ABombing)
		turnaround(a);
}

Andy *
newandy(Player *p)
{
	Andy *a;

	a = emalloc(sizeof *a);
	memset(a->map, Twater, MAPW*MAPH);
	a->ego = p;
	snprint(p->name, sizeof p->name, "%s", getaname());
	a->state = ASearching;
	a->layout = andy_layout;
	a->shoot = andy_shoot;
	a->engage = andy_engage;
	a->disengage = andy_disengage;
	a->registerhit = andy_registerhit;
	a->registermiss = andy_registermiss;
	return a;
}

void
freeandy(Andy *a)
{
	free(a);
}
