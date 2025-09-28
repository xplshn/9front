#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

void
threadsrvforker(void (*fn)(void*), void *arg, int rflag)
{
	procrfork(fn, arg, 32*1024, rflag);
}
