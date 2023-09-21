#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include <geometry.h>
#include "dat.h"
#include "fns.h"


/*
 * Generous estimate of number of fields, including terminal nil pointer
 */
static int
ncmdfield(char *p, int n)
{
	int white, nwhite;
	char *ep;
	int nf;

	if(p == nil)
		return 1;

	nf = 0;
	ep = p+n;
	white = 1;	/* first text will start field */
	while(p < ep){
		nwhite = (strchr(" \t\r\n", *p++ & 0xFF) != 0);	/* UTF is irrelevant */
		if(white && !nwhite)	/* beginning of field */
			nf++;
		white = nwhite;
	}
	return nf+1;	/* +1 for nil */
}

/*
 *  parse a command written to a device
 */
Cmdbuf *
parsecmd(char *p, int n)
{
	Cmdbuf *cb;
	int nf;
	char *sp;

	nf = ncmdfield(p, n);

	/* allocate Cmdbuf plus string pointers plus copy of string including \0 */
	sp = emalloc(sizeof(*cb) + nf * sizeof(char*) + n + 1);
	cb = (Cmdbuf*)sp;
	cb->f = (char**)(&cb[1]);
	cb->buf = (char*)(&cb->f[nf]);

	memmove(cb->buf, p, n);

	/* dump new line and null terminate */
	if(n > 0 && cb->buf[n-1] == '\n')
		n--;
	cb->buf[n] = '\0';

	cb->nf = tokenize(cb->buf, cb->f, nf-1);
	cb->f[cb->nf] = nil;

	return cb;
}

/*
 * Look up entry in table
 */
Cmdtab *
lookupcmd(Cmdbuf *cb, Cmdtab *ctab, int nctab)
{
	int i;
	Cmdtab *ct;

	if(cb->nf == 0){
		werrstr("empty command");
		return nil;
	}

	for(ct = ctab, i=0; i<nctab; i++, ct++){
		if(strcmp(ct->cmd, cb->f[0]) != 0)
			continue;
		if(ct->narg != 0 && ct->narg != cb->nf){
//			werrstr("wrong #args in command");
//			return nil;
			/* allow the same cmd name for different commands */
			continue;
		}
		return ct;
	}

	werrstr("unknown command");
	return nil;
}
