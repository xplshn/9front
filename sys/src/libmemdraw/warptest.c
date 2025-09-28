#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <geometry.h>

uvlong t0;

void
profbegin(void)
{
	char buf[128];
	int fd;

	snprint(buf, sizeof(buf), "/proc/%d/ctl", getpid());
	fd = open(buf, OWRITE);
	if(fd < 0)
		sysfatal("open: %r");
	fprint(fd, "profile\n");
	close(fd);
	t0 = nsec();
}

void
profend(void)
{
	char buf[128];
	double dt;

	dt = (nsec() - t0)/1e6;
	fprint(2, "dt: %fms\n", dt);

	snprint(buf, sizeof(buf), "%d", getpid());
	switch(fork()){
	case -1:
		abort();
	case 0:
		dup(2, 1);
		print("tprof %s\n", buf);
		execl("/bin/tprof", "tprof", buf, nil);
		abort();
	default:
		free(wait());
		break;
	}
}

#define min(a,b)	((a)<(b)?(a):(b))
#define max(a,b)	((a)>(b)?(a):(b))
Rectangle
getbbox(Rectangle *sr, Matrix m)
{
	Point2 p0, p1, p2, p3;

	p0 = Pt2(sr->min.x, sr->min.y, 1);
	p0 = xform(p0, m);
	p1 = Pt2(sr->max.x, sr->min.y, 1);
	p1 = xform(p1, m);
	p2 = Pt2(sr->min.x, sr->max.y, 1);
	p2 = xform(p2, m);
	p3 = Pt2(sr->max.x, sr->max.y, 1);
	p3 = xform(p3, m);

	p0.x = min(min(min(p0.x, p1.x), p2.x), p3.x);
	p0.y = min(min(min(p0.y, p1.y), p2.y), p3.y);
	p1.x = max(max(max(p1.x, p1.x), p2.x), p3.x);
	p1.y = max(max(max(p1.y, p1.y), p2.y), p3.y);
	return Rect(p0.x, p0.y, p1.x, p1.y);
}

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

void
usage(void)
{
	fprint(2, "usage: %s [-SRp] [-s sx sy] [-t tx ty] [-r θ]\n", argv0);
	exits("usage");
}

void
main(int argc, char *argv[])
{
	Memimage *dst, *src;
	Warp w;
	Rectangle dr, *wr;
	double sx, sy, tx, ty, θ, c, s;
	int smooth, dorepl, parallel, nproc, i;
	char *nprocs;

	sx = sy = 1;
	tx = ty = 0;
	θ = 0;
	smooth = 0;
	dorepl = 0;
	parallel = 0;
	ARGBEGIN{
	case 's':
		sx = strtod(EARGF(usage()), nil);
		sy = strtod(EARGF(usage()), nil);
		break;
	case 't':
		tx = strtod(EARGF(usage()), nil);
		ty = strtod(EARGF(usage()), nil);
		break;
	case 'r':
		θ = strtod(EARGF(usage()), nil)*DEG;
		break;
	case 'S':
		smooth++;
		break;
	case 'R':
		dorepl++;
		break;
	case 'p':
		parallel++;
		break;
	default:
		usage();
	}ARGEND;
	if(argc != 0)
		usage();

	if(memimageinit() != 0)
		sysfatal("memimageinit: %r");

	src = readmemimage(0);
	if(dorepl)
		src->flags |= Frepl;
	c = cos(θ);
	s = sin(θ);
	Matrix S = {
		sx, 0, 0,
		0, sy, 0,
		0, 0, 1,
	}, T = {
		1, 0, tx,
		0, 1, ty,
		0, 0, 1,
	}, R = {
		c, -s, 0,
		s, c, 0,
		0, 0, 1,
	};

	mulm(S, R);
	mulm(T, S);

//	dr = getbbox(&src->r, T);
	dr = src->r;
	dst = allocmemimage(dr, src->chan);
	memfillcolor(dst, DTransparent);

	mkwarp(w, T);

	profbegin();
	if(parallel){
		nprocs = getenv("NPROC");
		if(nprocs == nil || (nproc = strtoul(nprocs, nil, 10)) < 2)
			nproc = 1;
		free(nprocs);

		wr = malloc(nproc*sizeof(Rectangle));
		initworkrects(wr, nproc, &dr);

		for(i = 0; i < nproc; i++){
			switch(rfork(RFPROC|RFMEM)){
			case -1:
				sysfatal("rfork: %r");
			case 0:
				if(memaffinewarp(dst, wr[i], src, src->r.min, w, smooth) < 0)
					fprint(2, "[%d] memaffinewarp: %r\n", getpid());
				exits(nil);
			}
		}
		while(waitpid() != -1)
			;

		free(wr);
	}else{
		if(memaffinewarp(dst, dr, src, src->r.min, w, smooth) < 0)
			sysfatal("memaffinewarp: %r");

//		dr = rectaddpt(Rect(0,0,Dx(dst->r)/2,Dy(dst->r)/2), dst->r.min);
//		dst->clipr = dr;
//		if(memaffinewarp(dst, dr, src, src->r.min, w, smooth) < 0)
//			sysfatal("memaffinewarp: %r");
//		dr = rectaddpt(Rect(Dx(dst->r)/2+1,0,Dx(dst->r),Dy(dst->r)/2), dst->r.min);
//		dst->clipr = dst->r;
//		if(memaffinewarp(dst, dr, src, src->r.min, w, smooth) < 0)
//			sysfatal("memaffinewarp: %r");
//		dr = rectaddpt(Rect(0,Dy(dst->r)/2+1,Dx(dst->r)/2,Dy(dst->r)), dst->r.min);
//		if(memaffinewarp(dst, dr, src, src->r.min, w, smooth) < 0)
//			sysfatal("memaffinewarp: %r");
//		dr = rectaddpt(Rect(Dx(dst->r)/2+1,Dy(dst->r)/2+1,Dx(dst->r),Dy(dst->r)), dst->r.min);
//		dst->clipr = dr;
//		if(memaffinewarp(dst, dr, src, src->r.min, w, smooth) < 0)
//			sysfatal("memaffinewarp: %r");
	}
	profend();
	writememimage(1, dst);

	exits(nil);
}
