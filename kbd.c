#include "inc.h"

static void
_ioproc(void *arg)
{
	int m, n, nerr;
	char buf[1024], *e, *p;
	Rune r;
	RKeyboardctl *kc;

	kc = arg;
	threadsetname("kbdproc");
	n = 0;
	nerr = 0;
	if(kc->kbdfd >= 0){
		while(kc->kbdfd >= 0){
			m = read(kc->kbdfd, buf, sizeof(buf)-1);
			if(m <= 0){
				yield();	/* if error is due to exiting, we'll exit here */
				if(kc->kbdfd < 0)
					break;
				fprint(2, "keyboard: short read: %r\n");
				if(m<0 || ++nerr>10)
					threadexits("read error");
				continue;
			}
			/* one read can return multiple messages, delimited by NUL
			 * split them up for sending on the channel */
			e = buf+m;
			e[-1] = 0;
			e[0] = 0;
			for(p = buf; p < e; p += strlen(p)+1)
				chanprint(kc->c, "%s", p);
		}
	}else{
		while(kc->consfd >= 0){
			m = read(kc->consfd, buf+n, sizeof buf-n);
			if(m <= 0){
				yield();	/* if error is due to exiting, we'll exit here */
				if(kc->consfd < 0)
					break;
				fprint(2, "keyboard: short read: %r\n");
				if(m<0 || ++nerr>10)
					threadexits("read error");
				continue;
			}
			nerr = 0;
			n += m;
			while(n>0 && fullrune(buf, n)){
				m = chartorune(&r, buf);
				n -= m;
				memmove(buf, buf+m, n);
				if(chanprint(kc->c, "c%C", r) < 0)
					break;
			}
		}
	}
	chanfree(kc->c);
	free(kc->file);
	free(kc);
}

RKeyboardctl*
initkbd(char *file, char *kbdfile)
{
	RKeyboardctl *kc;
	char *t;

	if(file == nil)
		file = "/dev/cons";
	if(kbdfile == nil)
		kbdfile = "/dev/kbd";

	kc = mallocz(sizeof(RKeyboardctl), 1);
	if(kc == nil)
		return nil;
	kc->file = strdup(file);
// TODO: handle file == nil
	kc->consfd = open(file, ORDWR|OCEXEC);
	t = malloc(strlen(file)+16);
	if(kc->consfd<0 || t==nil)
		goto Error1;
	sprint(t, "%sctl", file);
	kc->ctlfd = open(t, OWRITE|OCEXEC);
	if(kc->ctlfd < 0){
		fprint(2, "initkeyboard: can't open %s: %r\n", t);
		goto Error2;
	}
	if(ctlkeyboard(kc, "rawon") < 0){
		fprint(2, "initkeyboard: can't turn on raw mode on %s: %r\n", t);
		close(kc->ctlfd);
		goto Error2;
	}
	free(t);
	kc->kbdfd = open(kbdfile, OREAD|OCEXEC);
	kc->c = chancreate(sizeof(char*), 20);
	kc->pid = proccreate(_ioproc, kc, 4096);
	return kc;

Error2:
	close(kc->consfd);
Error1:
	free(t);
	free(kc->file);
	free(kc);
	return nil;
}
