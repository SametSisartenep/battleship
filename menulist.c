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

enum {
	Menuborder = 2,
	Vspace = 2,
	Scrollwidth = 10,
	Maxvisitems = 5,
};
static char none[] = "none";


static void
menulist_add(Menulist *ml, int id, char *title)
{
	Mentry e = {id, title};
	int ew;

	ml->entries = erealloc(ml->entries, ++ml->nentries * sizeof *ml->entries);
	ml->entries[ml->nentries-1] = e;

	if((ew = stringwidth(font, e.title)) > Dx(ml->r)){
		ml->r.min.x = SCRW/2 - ew/2;
		ml->r.max.x = ml->r.min.x + ew;
	}
	if(ml->nentries > 1)
		ml->r.max.y += font->height+Vspace;
}

static void
menulist_clear(Menulist *ml)
{
	int i, w;

	if(ml->entries == nil)
		return;

	for(i = 0; i < ml->nentries; i++)
		free(ml->entries[i].title);
	free(ml->entries);
	ml->entries = nil;
	ml->nentries = 0;
	ml->filling = 0;

	w = max(stringwidth(font, ml->title), stringwidth(font, none));
	ml->r.min.x = SCRW/2 - w/2;
	ml->r.max = addpt(ml->r.min, Pt(w, font->height+Vspace));
	ml->sr = ZR;
	ml->high = -1;
}

static void
menulist_update(Menulist *ml, Mousectl *mc)
{
	if(ptinrect(mc->xy, ml->r)){
		/* item highlighting and selection */
	}else if(ptinrect(mc->xy, ml->sr)){
		/* scrolling */
	}
}

static void
menulist_draw(Menulist *ml, Image *dst)
{
	static Image *bc;
	Rectangle tr, er; /* title and per-entry */
	int i;

	if(ml->filling)
		return;

	if(bc == nil)
		bc = eallocimage(display, Rect(0,0,1,1), screen->chan, 1, DPurpleblue);

	/* draw title */
	tr.min = subpt(ml->r.min, Pt(0,Menuborder + font->height+Vspace));
	tr.max = Pt(ml->r.max.x, ml->r.min.y - Menuborder);
	draw(dst, tr, display->black, nil, ZP);
	string(dst, tr.min, display->white, ZP, font, ml->title);

	/* draw content */
	border(dst, ml->r, -Menuborder, bc, ZP);
	er.min = ml->r.min;
	er.max = Pt(ml->r.max.x, er.min.y + font->height+Vspace);
	for(i = 0; i < ml->nentries; i++){
		draw(dst, er, display->white, nil, ZP);
		string(dst, er.min, display->black, ZP, font, ml->entries[i].title);
		er.min.y += font->height+Vspace;
		er.max.y = er.min.y + font->height+Vspace;
	}
	if(i == 0){
		draw(dst, er, display->white, nil, ZP);
		string(dst, er.min, display->black, ZP, font, none);
	}
}

Menulist *
newmenulist(int topmargin, char *title)
{
	Menulist *ml;
	int w;

	ml = emalloc(sizeof *ml);
	memset(ml, 0, sizeof *ml);
	ml->title = estrdup(title);
	w = max(stringwidth(font, title), stringwidth(font, none));
	ml->r.min = Pt(SCRW/2 - w/2, topmargin);
	ml->r.max = addpt(ml->r.min, Pt(w, font->height+Vspace));
	ml->high = -1;
	ml->add = menulist_add;
	ml->clear = menulist_clear;
	ml->update = menulist_update;
	ml->draw = menulist_draw;
	return ml;
}

void
delmenulist(Menulist *ml)
{
	ml->clear(ml);
	free(ml->title);
	free(ml);
}
