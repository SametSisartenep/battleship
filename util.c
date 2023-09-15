#include <u.h>
#include <libc.h>
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


char *
cell2coords(Point2 cell)
{
	static char s[3+1];

	assert(cell.x >= 0 && cell.x < MAPW
		&& cell.y >= 0 && cell.y < MAPH);

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
	m->map[(int)cell.x][(int)cell.y] = type;
}

void
settiles(Map *m, Point2 cell, int o, int ncells, int type)
{
	Point2 sv;

	assert(o == OH || o == OV);
	sv = o == OH? Vec2(1,0): Vec2(0,1);

	while(ncells-- > 0){
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
