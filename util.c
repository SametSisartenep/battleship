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


char *
cell2coords(Point2 cell)
{
	static char s[3+1];

	assert(cell.x < 17 && cell.x >= 0
		&& cell.y < 17 && cell.y >= 0);

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

	assert(cell.x < 17 && cell.x >= 0);

	return cell;
}
