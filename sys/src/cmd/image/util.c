#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include "fns.h"

void
initworkrects(Rectangle *wr, int nwr, Rectangle *fbr)
{
	int i, Δy;

	wr[0] = *fbr;
	Δy = Dy(wr[0])/nwr;
	wr[0].max.y = wr[0].min.y + Δy;
	for(i = 1; i < nwr; i++)
		wr[i] = rectaddpt(wr[i-1], Pt(0,Δy));
	if(wr[nwr-1].max.y < fbr->max.y)
		wr[nwr-1].max.y = fbr->max.y;
}

void *
emalloc(ulong n)
{
	void *v;

	v = malloc(n);
	if(v == nil)
		sysfatal("realloc: %r");
	setmalloctag(v, getcallerpc(&n));
	return v;
}

void *
erealloc(void *v, ulong n)
{
	void *nv;

	nv = realloc(v, n);
	if(nv == nil)
		sysfatal("realloc: %r");
	if(v == nil)
		setmalloctag(nv, getcallerpc(&v));
	else
		setrealloctag(nv, getcallerpc(&v));
	return nv;
}

Memimage *
eallocmemimage(Rectangle r, ulong chan)
{
	Memimage *i;

	i = allocmemimage(r, chan);
	if(i == nil)
		sysfatal("allocmemimage: %r");
	memfillcolor(i, DTransparent);
	setmalloctag(i, getcallerpc(&r));
	return i;
}

Memimage *
ereadmemimage(int fd)
{
	Memimage *i;

	i = readmemimage(fd);
	if(i == nil)
		sysfatal("readmemimage: %r");
	return i;
}

int
ewritememimage(int fd, Memimage *i)
{
	int rc;

	rc = writememimage(fd, i);
	if(rc < 0)
		sysfatal("writememimage: %r");
	return rc;
}
