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

	switch(o){
	case OH: sv = Vec2(1,0); break;
	case OV: sv = Vec2(0,1); break;
	default: sysfatal("settiles: wrong ship orientation");
	}

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
shiplen(int stype)
{
	assert(stype >= 0 && stype < NSHIPS);
	return shiplentab[stype];
}
