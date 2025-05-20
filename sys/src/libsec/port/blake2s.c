#include <u.h>
#include <libc.h>
#include <libsec.h>

extern void _blake2sblock(u32int h[16], uchar *b, u32int blen, u64int offset, u32int f);
static DigestState* blake2s_512(uchar *p, ulong len, uchar *digest, DigestState *s, int dlen);

static void
encode32(uchar *output, u32int *input, ulong len)
{
	u32int x;
	uchar *e;

	for(e = output + len; output < e;) {
		x = *input++;
		*output++ = x >> 0;
		*output++ = x >> 8;
		*output++ = x >> 16;
		*output++ = x >> 24;
	}
}

DigestState*
blake2s_256(uchar *p, ulong len, uchar *digest, DigestState *s)
{
	if(s == nil) {
		s = mallocz(sizeof(*s), 1);
		if(s == nil)
			return nil;
		s->malloced = 1;
	}
	if(s->seeded == 0){
		s->state[0] = 0x6A09E667UL;
		s->state[1] = 0xBB67AE85UL;
		s->state[2] = 0x3C6EF372UL;
		s->state[3] = 0xA54FF53AUL;
		s->state[4] = 0x510E527FUL;
		s->state[5] = 0x9B05688CUL;
		s->state[6] = 0x1F83D9ABUL;
		s->state[7] = 0x5BE0CD19UL;

		s->state[0] ^= 0x01010000 ^ B2s_256dlen;
		s->seeded = 1;
	}
	return blake2s_512(p, len, digest, s, B2s_256dlen);
}

enum{
	bb = 64,

	fcont = 0,
	flast = 0xFFFFFFFF,
};

/* 64 byte input blocks */
static DigestState*
blake2s_512(uchar *p, ulong len, uchar *digest, DigestState *s, int dlen)
{
	int i;

	if(s->blen){
		i = bb - s->blen;
		if(len < i)
			i = len;
		if(i > 0){
			memmove(s->buf + s->blen, p, i);
			len -= i;
			s->blen += i;
			p += i;
		}
		if(s->blen == bb){
			if(len == 0){
				if(digest != nil)
					goto Last;
				return s;
			}
			s->len += bb;
			_blake2sblock(s->state, s->buf, bb, s->len, fcont);
			s->blen = 0;
		}
	}

	/*
	 * the last block must always have 'flast' set,
	 * so only go up to second to (potential) last block
	 */
	i = len & ~(bb-1);
	if(i > bb){
		i -= bb;
		_blake2sblock(s->state, p, i, s->len + bb, fcont);
		s->len += i;
		len -= i;
		p += i;
	}

	/* more to come */
	if(digest == 0){
		if(len){
			memmove(s->buf, p, len);
			s->blen += len;
		}
		return s;
	}

	/* last block(s), might have > 1 still */
	if(len > bb){
		s->len += bb;
		_blake2sblock(s->state, p, bb, s->len, fcont);
		len -= bb;
		p += bb;
	}

	if(s->blen){
Last:
		len = s->blen;
	} else
		memmove(s->buf, p, len);
	p = s->buf;
	s->len += len;
	i = bb - len;
	assert(i >= 0);
	memset(p + len, 0, i);
	_blake2sblock(s->state, p, bb, s->len, flast);

	encode32(digest, s->state, dlen);
	if(s->malloced == 1)
		free(s);
	return nil;
}

DigestState*
hmac_blake2s_256(uchar *p, ulong len, uchar *key, ulong klen, uchar *digest, DigestState *s)
{
	return hmac_x(p, len, key, klen, digest, s, blake2s_256, B2s_256dlen);
}
