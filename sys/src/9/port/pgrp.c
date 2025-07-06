#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"

enum {
	Whinesecs = 10,		/* frequency of out-of-resources printing */
};

uvlong
nextmount(void)
{
	static uvlong next = 0;
	static Lock lk;
	uvlong n;

	lock(&lk);
	n = ++next;
	unlock(&lk);
	return n;
}

Pgrp*
newpgrp(void)
{
	Pgrp *p;

	p = malloc(sizeof(Pgrp));
	if(p == nil)
		error(Enomem);
	p->ref = 1;
	return p;
}

Rgrp*
newrgrp(void)
{
	Rgrp *r;

	r = malloc(sizeof(Rgrp));
	if(r == nil)
		error(Enomem);
	r->ref = 1;
	return r;
}

void
closergrp(Rgrp *r)
{
	if(decref(r) == 0)
		free(r);
}

void
closepgrp(Pgrp *p)
{
	Mhead **h, **e, *f;
	Mount *m;

	if(decref(p))
		return;

	e = &p->mnthash[MNTHASH];
	for(h = p->mnthash; h < e; h++) {
		while((f = *h) != nil){
			*h = f->hash;
			wlock(&f->lock);
			m = f->mount;
			f->mount = nil;
			wunlock(&f->lock);
			mountfree(m);
			putmhead(f);
		}
	}
	free(p);
}

static void
pgrpinsert(Mount **order, Mount *m)
{
	Mount *f;

	m->order = nil;
	for(f = *order; f != nil; f = f->order) {
		if(m->mountid < f->mountid) {
			m->order = f;
			*order = m;
			return;
		}
		order = &f->order;
	}
	*order = m;
}

/*
 * pgrpcpy MUST preserve the mountid allocation order of the parent group
 */
void
pgrpcpy(Pgrp *to, Pgrp *from)
{
	Mount *n, *m, **link, *order;
	Mhead *f, **l, *mh;
	int i;

	wlock(&to->ns);
	rlock(&from->ns);
	if(waserror()){
		runlock(&from->ns);
		wunlock(&to->ns);
		nexterror();
	}
	order = nil;
	for(i = 0; i < MNTHASH; i++) {
		l = &to->mnthash[i];
		for(f = from->mnthash[i]; f != nil; f = f->hash) {
			rlock(&f->lock);
			if(waserror()){
				runlock(&f->lock);
				nexterror();
			}
			mh = newmhead(f->from);
			*l = mh;
			l = &mh->hash;
			link = &mh->mount;
			for(m = f->mount; m != nil; m = m->next) {
				n = malloc(sizeof(Mount)+strlen(m->spec)+1);
				if(n == nil)
					error(Enomem);
				n->mountid = m->mountid;
				n->mflag = m->mflag;
				n->to = m->to;
				incref(n->to);
				strcpy(n->spec, m->spec);
				pgrpinsert(&order, n);
				*link = n;
				link = &n->next;
			}
			runlock(&f->lock);
			poperror();
		}
	}
	/*
	 * Allocate mount ids in the same sequence as the parent group
	 */
	for(m = order; m != nil; m = m->order)
		m->mountid = nextmount();
	runlock(&from->ns);
	wunlock(&to->ns);
	poperror();
}

Fgrp*
dupfgrp(Fgrp *f)
{
	Fgrp *new;
	Chan *c;
	int i;

	new = malloc(sizeof(Fgrp));
	if(new == nil)
		error(Enomem);
	new->ref = 1;
	if(f == nil){
		new->nfd = DELTAFD;
		new->fd = malloc(DELTAFD*sizeof(new->fd[0]));
		new->flag = malloc(DELTAFD*sizeof(new->flag[0]));
		if(new->fd == nil || new->flag == nil){
			free(new->flag);
			free(new->fd);
			free(new);
			error(Enomem);
		}
		return new;
	}

	lock(f);
	/* Make new fd list shorter if possible, preserving quantization */
	new->nfd = f->maxfd+1;
	i = new->nfd%DELTAFD;
	if(i != 0)
		new->nfd += DELTAFD - i;
	new->fd = malloc(new->nfd*sizeof(new->fd[0]));
	new->flag = malloc(new->nfd*sizeof(new->flag[0]));
	if(new->fd == nil || new->flag == nil){
		unlock(f);
		free(new->flag);
		free(new->fd);
		free(new);
		error(Enomem);
	}
	new->maxfd = f->maxfd;
	for(i = 0; i <= f->maxfd; i++) {
		if((c = f->fd[i]) != nil){
			new->fd[i] = c;
			new->flag[i] = f->flag[i];
			incref(c);
		}
	}
	unlock(f);

	return new;
}

void
closefgrp(Fgrp *f)
{
	int i;
	Chan *c;

	if(f == nil || decref(f))
		return;

	/*
	 * If we get into trouble, forceclosefgrp
	 * will bail us out.
	 */
	up->closingfgrp = f;
	for(i = 0; i <= f->maxfd; i++)
		if((c = f->fd[i]) != nil){
			f->fd[i] = nil;
			cclose(c);
		}
	up->closingfgrp = nil;

	free(f->flag);
	free(f->fd);
	free(f);
}

/*
 * Called from interrupted() because up is in the middle
 * of closefgrp and just got a kill ctl message.
 * This usually means that up has wedged because
 * of some kind of deadly embrace with mntclose
 * trying to talk to itself.  To break free, hand the
 * unclosed channels to the close queue.  Once they
 * are finished, the blocked cclose that we've 
 * interrupted will finish by itself.
 */
void
forceclosefgrp(void)
{
	int i;
	Chan *c;
	Fgrp *f;

	if(up->procctl != Proc_exitme || up->closingfgrp == nil){
		print("bad forceclosefgrp call");
		return;
	}

	f = up->closingfgrp;
	for(i = 0; i <= f->maxfd; i++)
		if((c = f->fd[i]) != nil){
			f->fd[i] = nil;
			ccloseq(c);
		}
}


Mount*
newmount(Chan *to, int flag, char *spec)
{
	Mount *m;

	if(spec == nil)
		spec = "";
	m = malloc(sizeof(Mount)+strlen(spec)+1);
	if(m == nil)
		error(Enomem);
	m->to = to;
	incref(to);
	m->mountid = nextmount();
	m->mflag = flag;
	strcpy(m->spec, spec);
	setmalloctag(m, getcallerpc(&to));
	return m;
}

void
mountfree(Mount *m)
{
	Mount *f;

	while((f = m) != nil) {
		m = m->next;
		cclose(f->to);
		free(f);
	}
}

void
resrcwait(char *reason)
{
	static ulong lastwhine;
	ulong now;
	char *p;

	if(up == nil)
		panic("resrcwait: %s", reason);

	p = up->psstate;
	if(reason != nil) {
		if(waserror()){
			up->psstate = p;
			nexterror();
		}
		up->psstate = reason;
		now = seconds();
		/* don't tie up the console with complaints */
		if(now - lastwhine > Whinesecs) {
			lastwhine = now;
			print("%s\n", reason);
		}
	}
	tsleep(&up->sleep, return0, 0, 100+nrand(200));
	if(reason != nil) {
		up->psstate = p;
		poperror();
	}
}
