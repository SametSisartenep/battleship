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
	Maxvisitems = 8,
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

	if(ml->nentries > Maxvisitems){
		ml->sr.min = subpt(ml->r.min, Pt(Scrollwidth+2*Menuborder,0));
		ml->sr.max = Pt(ml->r.min.x-2*Menuborder, ml->r.max.y);
	}else if(ml->nentries > 1)
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
	ml->off = 0;
}

static int
menulist_update(Menulist *ml, Mousectl *mc, Channel *drawchan)
{
	/* redundant from bts.c:/^mouse\(, but it's necessary to avoid overdrawing */
	static Mouse oldm;
	static ulong lastlmbms;
	Rectangle r;
	int selected;

	if(ml->nentries < 1)
		return -1;

	r = ml->nentries > Maxvisitems? Rpt(ml->sr.min, ml->r.max): ml->r;

	selected = -1;
	if(ptinrect(mc->xy, r)){
		if(ptinrect(mc->xy, ml->r)){
			/* item highlighting and selection */
			ml->high = ml->off + (mc->xy.y - ml->r.min.y)/(font->height+Vspace);
			if(oldm.buttons != mc->buttons && mc->buttons == 1){
				if(mc->msec-lastlmbms < 500)
					selected = ml->high;
				else
					lastlmbms = mc->msec;
			}
		}
		if(mc->buttons != oldm.buttons && ml->nentries > Maxvisitems)
			/* scrolling */
			switch(mc->buttons){
			case 1: if(!ptinrect(mc->xy, ml->sr)) break;
			case 8:
				ml->off = max(0, ml->off - (mc->xy.y - ml->sr.min.y)/(font->height+Vspace));
				break;
			case 4: if(!ptinrect(mc->xy, ml->sr)) break;
			case 16:
				ml->off = min(ml->off + (mc->xy.y - ml->sr.min.y)/(font->height+Vspace), ml->nentries-Maxvisitems);
				break;
			}
	}
	nbsendp(drawchan, nil);
	oldm = mc->Mouse;
	return selected;
}

/* TODO draw the menu in its own Window */
static void
menulist_draw(Menulist *ml, Image *dst)
{
	static Image *bc;
	Rectangle tr, er; /* title and per-entry */
	int i, width;

	if(ml->filling)
		return;

	if(bc == nil)
		bc = eallocimage(display, Rect(0,0,1,1), screen->chan, 1, DPurpleblue);

	/* draw title */
	width = stringwidth(font, ml->title);
	tr.min = subpt(ml->r.min, Pt(0,Menuborder + font->height+Vspace));
	tr.max = Pt(ml->r.max.x, ml->r.min.y - Menuborder);
	draw(dst, tr, display->black, nil, ZP);
	string(dst, addpt(tr.min, Pt(Dx(tr)/2 - width/2,0)), display->white, ZP, font, ml->title);

	/* draw content */
	border(dst, ml->r, -Menuborder, bc, ZP);
	er.min = ml->r.min;
	er.max = Pt(ml->r.max.x, er.min.y + font->height+Vspace);
	for(i = ml->off; i < ml->nentries && er.min.y < ml->r.max.y; i++){
		width = stringwidth(font, ml->entries[i].title);
		draw(dst, er, i == ml->high? display->black: display->white, nil, ZP);
		string(dst, addpt(er.min, Pt(Dx(er)/2 - width/2,0)), i == ml->high? display->white: display->black, ZP, font, ml->entries[i].title);
		er.min.y += font->height+Vspace;
		er.max.y = er.min.y + font->height+Vspace;
	}
	if(i == 0){
		width = stringwidth(font, none);
		draw(dst, er, display->white, nil, ZP);
		string(dst, addpt(er.min, Pt(Dx(er)/2 - width/2,0)), display->black, ZP, font, none);
	}

	/* draw scroll */
	if(ml->nentries > Maxvisitems){
		border(dst, ml->sr, -Menuborder, bc, ZP);
		draw(dst, ml->sr, display->black, nil, ZP);
		draw(dst, Rpt(addpt(ml->sr.min, Pt(0,ml->off*Dy(ml->sr)/ml->nentries)), Pt(ml->sr.max.x,ml->sr.min.y + (ml->off+Maxvisitems)*Dy(ml->sr)/ml->nentries)), display->white, nil, ZP);
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
