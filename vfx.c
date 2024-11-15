#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include <geometry.h>
#include "dat.h"
#include "fns.h"

static void
vfx_step(Vfx *v, ulong Δt)
{
	if(v->times == 0 && v->a->curframe == 0){
		delvfx(v);
		return;
	}

	v->a->step(v->a, Δt);

	if(v->times > 0 && v->a->curframe == v->a->nframes-1)
		v->times--;
}

static void
vfx_draw(Vfx *v, Image *dst)
{
	if(v->times == 0 && v->a->curframe == 0)
		return;

	v->a->draw(v->a, dst, subpt(v->p, divpt(subpt(v->a->r.max, v->a->r.min), 2)));
}

Vfx *
newvfx(Sprite *spr, Point dp, int repeat)
{
	Vfx *v;

	v = emalloc(sizeof(Vfx));
	v->a = spr;
	v->p = dp;
	v->times = repeat;
	v->step = vfx_step;
	v->draw = vfx_draw;

	return v;
}

void
delvfx(Vfx *v)
{
	v->next->prev = v->prev;
	v->prev->next = v->next;
	delsprite(v->a);
	free(v);
}

void
addvfx(Vfx *v, Vfx *nv)
{
	nv->prev = v->prev;
	nv->next = v;
	v->prev->next = nv;
	v->prev = nv;
}

void
initvfxq(Vfx *v)
{
	v->next = v->prev = v;
}
