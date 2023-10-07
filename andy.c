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

static void
turnaround(Andy *a)
{
	if(--a->passes > 0){
		a->passdir = mulpt2(a->passdir, -1);
		a->lastshot = a->firsthit;
	}else
		a->disengage(a);
}

static void
andy_layout(Andy *a, Msg *m)
{
	/* TODO write a real layout algorithm */
	m->body = estrdup("layout f9v,g6v,b12v,c15v,l14v");
	sendp(a->ego->battle->data, m);
}

static void
andy_shoot(Andy *a, Msg *m)
{
	Point2 cell;

Retry:
	switch(a->state){
	case ASearching:
		do
			cell = Pt2(getrand(MAPW), getrand(MAPH), 1);
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
	m->body = smprint("shoot %s", cell2coords(cell));
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
