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
	return nametab[ntruerand(nelem(nametab))];
}

static void
doanotherpass(Andy *a)
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
			cell = Pt2(ntruerand(MAPW), ntruerand(MAPH), 1);
		while(gettile(a, cell) != Twater);
		fprint(2, "[%d] search shot\n", getpid());
		break;
	case ACalibrating:
		do
			cell = addpt2(a->firsthit, nwes[--a->ntries&3]);
		while(gettile(a, cell) != Twater && a->ntries > 1);
		if(a->ntries < 1 && gettile(a, cell) != Twater){
			fprint(2, "[%d] neverland\n", getpid());
			a->disengage(a);
			goto Retry;
		}
		fprint(2, "[%d] calibrating shot\n", getpid());
		break;
	case ABombing:
		cell = addpt2(a->lastshot, a->passdir);
		if(gettile(a, cell) != Twater){
			doanotherpass(a);
			goto Retry;
		}
		fprint(2, "[%d] bombing shot\n", getpid());
		break;
	}
	m->body = smprint("shoot %s", cell2coords(cell));
	sendp(a->ego->battle->data, m);
	a->lastshot = cell;
	fprint(2, "[%d] shot enemy\n", getpid());
}

static void
andy_engage(Andy *a)
{
	a->firsthit = a->lastshot;
	a->state = ACalibrating;
	a->ntries = nelem(nwes);
	a->passes = 2;
	fprint(2, "[%d] enemy engaged\n", getpid());
}

static void
andy_disengage(Andy *a)
{
	a->state = ASearching;
	fprint(2, "[%d] enemy disengaged\n", getpid());
}

static void
andy_registerhit(Andy *a)
{
	fprint(2, "[%d] hit enemy\n", getpid());
	settile(a, a->lastshot, Thit);
	if(a->state == ASearching)
		a->engage(a);
	else if(a->state == ACalibrating){
		a->passdir = subpt2(a->lastshot, a->firsthit);
		a->state = ABombing;
		fprint(2, "[%d] began bombing\n", getpid());
	}
}

static void
andy_registermiss(Andy *a)
{
	fprint(2, "[%d] missed enemy\n", getpid());
	settile(a, a->lastshot, Tmiss);
	if(a->state == ACalibrating && a->ntries < 1)
		a->disengage(a);
	else if(a->state == ABombing){
		doanotherpass(a);
		fprint(2, "[%d] bombing pass #%d dir %v\n", getpid(), a->passes, a->passdir);
	}
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
