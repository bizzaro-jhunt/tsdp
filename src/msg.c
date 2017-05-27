#include <tsdp.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>

#define BYTE(x,n) (((unsigned char*)(x))[(n)])
#define WORD(x,n) ((BYTE((x),(n)) << 8) | BYTE((x),(n)+1))

#define extract_header_version(h) ((BYTE((h),0) & 0xf0) >> 4)
#define extract_header_opcode(h)   (BYTE((h),0) & 0x0f)
#define extract_header_flags(h)    (BYTE((h),1))
#define extract_header_payload(h) ((BYTE((h),2) << 8) | (BYTE((h), 3)))

#define extract_frame_final(f)     ((BYTE((f),0) & 0x80) >> 7)
#define extract_frame_type(f)      ((BYTE((f),0) & 0x70) >> 4)
#define extract_frame_length(f)   (((BYTE((f),0) & 0x0f) << 8) | (BYTE((f), 1)))

static inline uint64_t
h2n16(uint64_t u)
{
	int e = 37;
	if (*(char*)&e == 37) {
		return ((u & 0xff00) >> 8)
		     | ((u & 0x00ff) << 8);
	}
	return u;
}

static inline uint64_t
h2n32(uint64_t u)
{
	int e = 37;
	if (*(char*)&e == 37) {
		return ((u & 0xff000000) >> 24)
		     | ((u & 0x00ff0000) >>  8)
		     | ((u & 0x0000ff00) <<  8)
		     | ((u & 0x000000ff) << 24);
	}
	return u;
}

static inline uint64_t
h2n64(uint64_t u)
{
	int e = 37;
	if (*(char*)&e == 37) {
		return ((u & 0xff00000000000000) >> 56)
		     | ((u & 0x00ff000000000000) >> 40)
		     | ((u & 0x0000ff0000000000) >> 24)
		     | ((u & 0x000000ff00000000) >>  8)
		     | ((u & 0x00000000ff000000) <<  8)
		     | ((u & 0x0000000000ff0000) << 24)
		     | ((u & 0x000000000000ff00) << 40)
		     | ((u & 0x00000000000000ff) << 56);
	}
	return u;
}

#define n2h16(b) h2n16(*(uint16_t *)b)
#define n2h32(b) h2n32(*(uint32_t *)b)
#define n2h64(b) h2n64(*(uint64_t *)b)

struct tsdp_msg *
tsdp_msg_new(int version, int opcode, int flags, int payload)
{
	struct tsdp_msg *m;

	errno = TSDP_E_INVALID_VERSION;
	if (!tsdp_version_ok(version)) return NULL;

	errno = TSDP_E_INVALID_OPCODE;
	if (!tsdp_opcode_ok(opcode)) return NULL;

	errno = TSDP_E_INVALID_FLAG;
	if (!tsdp_flags_ok(flags)) return NULL;

	errno = TSDP_E_INVALID_PAYLOAD;
	if (!tsdp_payload_ok(payload)) return NULL;

	m = calloc(1, sizeof(struct tsdp_msg));
	if (!m) {
		return NULL;
	}

	m->version = version & 0xf;
	m->opcode  = opcode  & 0xf;
	m->flags   = flags   & 0xff;
	m->payload = payload & ~TSDP_PAYLOAD_RSVP;

	return m;
}

void
tsdp_msg_free(struct tsdp_msg *m)
{
	struct tsdp_frame *f, *tmp;

	if (!m) return;

	f = m->frames;
	while (f) {
		tmp = f->next;
		free(f);
		f = tmp;
	}
	free(m);
}

int
tsdp_msg_extend(struct tsdp_msg *m, int type, const void *v, size_t len)
{
	struct tsdp_frame *f;

	f = calloc(1, sizeof(struct tsdp_frame) + len);
	if (!f) return -1;

	f->type = type;
	f->length = len;

	errno = EINVAL;
	switch (type) {
		case TSDP_FRAME_NIL:
			if (len != 0) {
				free(f);
				return -1;
			}
			break;

		case TSDP_FRAME_UINT:
			if (len == 2) {           /* UINT/16 */
				f->payload.uint16 = *(uint16_t*)v;
			} else if (len == 4) {    /* UINT/32 */
				f->payload.uint32 = *(uint32_t*)v;
			} else if (len == 8) {    /* UINT/64 */
				f->payload.uint64 = *(uint64_t*)v;
			} else {
				free(f);
				return -1;
			}
			break;

		case TSDP_FRAME_FLOAT:
			if (len == 4) {           /* FLOAT/32 */
				f->payload.float32 = *(float*)v;
			} else if (len == 8) {    /* FLOAT/64 */
				f->payload.float64 = *(double*)v;
			} else {
				free(f);
				return -1;
			}
			break;

		case TSDP_FRAME_TSTAMP:
			if (len == 8) {           /* TSTAMP/64 */
				f->payload.tstamp = *(uint64_t*)v;
			} else {
				free(f);
				return -1;
			}
			break;

		case TSDP_FRAME_STRING:
			if (len > 0) {
				memcpy(f->data, v, len);
				f->payload.string = (char *)(f->data);
			}
			break;

		default:
			free(f);
			return -1;
	}

	m->nframes++;
	f->next = NULL;
	if (!m->last) {
		m->frames = m->last = f;
	} else {
		m->last->next = f;
		m->last = f;
	}

	return 0;
}

ssize_t
tsdp_msg_pack(void *buf, size_t len, struct tsdp_msg *m)
{
	ssize_t n = 0;
	struct tsdp_frame *f;
	uint64_t u;
#	define PACK(c) if (n<len) ((unsigned char*)buf)[n] = (unsigned char)(c) & 0xff; n++
#	define PACK2(c) do { PACK(((unsigned char*)(&c))[0]); PACK(((unsigned char*)(&c))[1]); } while (0)
#	define PACK4(c) do { PACK(((unsigned char*)(&c))[0]); PACK(((unsigned char*)(&c))[1]); \
	                     PACK(((unsigned char*)(&c))[2]); PACK(((unsigned char*)(&c))[3]); } while (0)
#	define PACK8(c) do { PACK(((unsigned char*)(&c))[0]); PACK(((unsigned char*)(&c))[1]); \
	                     PACK(((unsigned char*)(&c))[2]); PACK(((unsigned char*)(&c))[3]); \
	                     PACK(((unsigned char*)(&c))[4]); PACK(((unsigned char*)(&c))[5]); \
	                     PACK(((unsigned char*)(&c))[6]); PACK(((unsigned char*)(&c))[7]); } while (0)

	if (!buf) len = 0;

	/* HEADER */
	PACK((m->version << 4) | (m->opcode & 0x0f));
	PACK(m->flags);
	PACK(m->payload >> 8);
	PACK(m->payload);

	for (f = m->frames; f; f = f->next) {
		PACK((m->last == f ? 0x80 : 0x00) | ((f->type << 4) & 0x70) | ((f->length >> 8) & 0xf));
		PACK(f->length);
		switch (f->type) {
			case TSDP_FRAME_NIL:
				break;

			case TSDP_FRAME_UINT:
				if (f->length == 2) {
					u = h2n16(f->payload.uint16); PACK2(u);
				} else if (f->length == 4) {
					u = h2n32(f->payload.uint32); PACK4(u);
				} else if (f->length == 8) {
					u = h2n64(f->payload.uint64); PACK8(u);
				} else {
					return 0;
				}
				break;

			case TSDP_FRAME_FLOAT:
				if (f->length == 4) {
					u = h2n32(*(uint32_t*)(&f->payload.float32)); PACK4(u);
				} else if (f->length == 8) {
					u = h2n64(*(uint64_t*)(&f->payload.float64)); PACK8(u);
				} else {
					return 0;
				}
				break;

			case TSDP_FRAME_TSTAMP:
				if (f->length == 8) {
					u = h2n64(f->payload.tstamp); PACK8(u);
				} else {
					return 0;
				}
				break;

			case TSDP_FRAME_STRING:
				for (u = 0; u < f->length; u++) {
					PACK(f->payload.string[u]);
				}
				break;

			default:
				return 0;
		}
	}
#	undef PACK
#	undef PACK2
#	undef PACK4
#	undef PACK8
	return n;
}

struct tsdp_msg *
tsdp_msg_unpack(const void *buf, size_t n, size_t *left)
{
	struct tsdp_msg *m;

	assert(buf);  /* need a buffer to read from... */
	assert(left); /* need a place to store unused part of buf... */

	/* we must have at least enough for a header */
	if (n < 4) {
		return NULL;
	}

	m = calloc(1, sizeof(struct tsdp_msg));
	if (!m) {
		return NULL;
	}

	/* extract header fields from (network byte-order) buf */
	m->version = extract_header_version(buf);
	m->opcode  = extract_header_opcode(buf);
	m->flags   = extract_header_flags(buf);
	m->payload = extract_header_payload(buf);

	m->complete = 0;
	*left = n - 4;
	buf += 4;

	/* extract the frames */
	if (m->opcode == TSDP_OPCODE_REPLAY) {
		/* REPLAY has no frames ... */
		m->complete = 1;
		return m;
	}

	while (*left >= 2) { /* have enough for a header */
		struct tsdp_frame *f;
		unsigned short len = extract_frame_length(buf);
		uint32_t u32;
		uint64_t u64;

		if (len > *left - 2) {
			/* not enough in buf[] to read the payload */
			return m;
		}

		f = calloc(1, sizeof(struct tsdp_frame)
		            + len); /* variable frame data */
		if (!f) {
			return NULL;
		}

		f->type   = extract_frame_type(buf);
		f->length = len;
		memmove(f->data, buf + 2, len);

		switch (f->type) {
		case TSDP_FRAME_UINT:
			switch (f->length) {
			case 2:
				f->payload.uint16 = n2h16(f->data);
				break;
			case 4:
				f->payload.uint32 = n2h32(f->data);
				break;
			case 8:
				f->payload.uint64 = n2h64(f->data);
				break;
			}
			break;

		case TSDP_FRAME_FLOAT:
			switch (f->length) {
			case 4:
				u32 = n2h32(f->data);
				f->payload.float32 = *((float *)&u32);
				break;
			case 8:
				u64 = n2h64(f->data);
				f->payload.float64 = *((double *)&u64);
				break;
			}
			break;

		case TSDP_FRAME_STRING:
			f->payload.string = (char *)(f->data);
			break;

		case TSDP_FRAME_TSTAMP:
			if (f->length == 8)
				f->payload.tstamp = n2h64(f->data);
			break;
		}

		m->nframes++;
		f->next = NULL;
		if (!m->last) {
			m->frames = m->last = f;
		} else {
			m->last->next = f;
			m->last       = f;
		}
		m->complete = extract_frame_final(buf);
		*left -= 2 + len;
		buf   += 2 + len;
	}

	return m;
}

#define s_frame_valid(f,t,l) ((f) && (f)->type == (t) && ((t) == TSDP_FRAME_STRING || (f)->length == l))

static struct tsdp_frame *
s_nth_frame(struct tsdp_msg *m, int n)
{
	struct tsdp_frame *f;

	f = m->frames;
	while (n > 0 && f) {
		f = f->next;
		n--;
	}
	return f;
}

/* hamming weight algorithm, in 8-bit */
static unsigned char
bits8(unsigned char b)
{
	b = b - ((b >> 1) & 0x55);
	b = (b & 0x33) + ((b >> 2) & 0x33);
	return ((b + (b >> 4)) & 0x0f);
}

int
tsdp_msg_valid(struct tsdp_msg *m)
{
	int i;
	unsigned int type, len;

	if (!m) {
		return 0;
	}

	if (!m->complete) {
		return 0;
	}

	errno = TSDP_E_INVALID_VERSION;
	if (!tsdp_version_ok(m->version)) return 0;

#define requires_single_payload() \
	do { if (bits8(m->payload) != 1) return 0; } while (0)
#define requires_empty_payload() \
	do { if (m->payload != 0) return 0; } while (0)
#define requires_one_or_more_payloads() \
	do { if (m->payload == 0) return 0; } while (0)
#define requires_payloads(p) \
	do { if (m->payload & ~(p)) return 0; } while (0)

#define requires_exact_frame_count(n) \
	do { if (m->nframes != (n)) return 0; } while (0)
#define requires_min_frame_count(n) \
	do { if (m->nframes < (n)) return 0; } while (0)
#define requires_min_max_frame_count(a,b) \
	do { if (m->nframes < (a) || m->nframes > (b)) return 0; } while (0)
#define requires_frame(n,t,l) \
	do { if (!s_frame_valid(s_nth_frame(m, (n)), (t), (l))) return 0; } while (0)

	switch (m->opcode) {
	case TSDP_OPCODE_HEARTBEAT:
		errno = TSDP_E_INVALID_PAYLOAD;
		requires_empty_payload();      /* per RFC-TSDP $4.3.1.1 */

		errno = TSDP_E_INVALID_ARITY;
		requires_exact_frame_count(2); /* per RFC-TSDP $4.3.1.2 */

		errno = TSDP_E_INVALID_FRAME;
		requires_frame(0, TSDP_FRAME_TSTAMP, 8);
		requires_frame(1, TSDP_FRAME_UINT,   8);
		return 1;

	case TSDP_OPCODE_SUBMIT:
		errno = TSDP_E_INVALID_PAYLOAD;
		requires_single_payload();
		switch (m->payload) {
		case TSDP_PAYLOAD_SAMPLE:
			errno = TSDP_E_INVALID_ARITY;
			requires_min_frame_count(3);

			errno = TSDP_E_INVALID_FRAME;
			requires_frame(0, TSDP_FRAME_STRING, 0);    /* qualified name   */
			requires_frame(1, TSDP_FRAME_TSTAMP, 8);    /* measurement time */
			for (i = 2; i < m->nframes; i++) {
				requires_frame(i, TSDP_FRAME_FLOAT, 8); /* measurement(s)   */
			}
			return 1;

		case TSDP_PAYLOAD_TALLY:
			errno = TSDP_E_INVALID_ARITY;
			requires_min_max_frame_count(2, 3);

			errno = TSDP_E_INVALID_FRAME;
			requires_frame(0, TSDP_FRAME_STRING, 0);    /* qualified name   */
			requires_frame(1, TSDP_FRAME_TSTAMP, 8);    /* measurement time */
			if (m->nframes == 3) {
				requires_frame(2, TSDP_FRAME_UINT, 8);  /* (increment)      */
			}
			return 1;

		case TSDP_PAYLOAD_DELTA:
			errno = TSDP_E_INVALID_ARITY;
			requires_exact_frame_count(3);

			errno = TSDP_E_INVALID_FRAME;
			requires_frame(0, TSDP_FRAME_STRING, 0); /* qualified name   */
			requires_frame(1, TSDP_FRAME_TSTAMP, 8); /* measurement time */
			requires_frame(2, TSDP_FRAME_FLOAT,  8); /* last value known */
			return 1;

		case TSDP_PAYLOAD_STATE:
			errno = TSDP_E_INVALID_ARITY;
			requires_min_max_frame_count(3, 4);

			errno = TSDP_E_INVALID_FRAME;
			requires_frame(0, TSDP_FRAME_STRING, 0);
			requires_frame(1, TSDP_FRAME_TSTAMP, 8);
			requires_frame(2, TSDP_FRAME_UINT,   4);
			if (m->nframes == 4) {
				requires_frame(3, TSDP_FRAME_STRING, 0);
			}
			return 1;

		case TSDP_PAYLOAD_EVENT:
			errno = TSDP_E_INVALID_ARITY;
			requires_exact_frame_count(3);

			errno = TSDP_E_INVALID_FRAME;
			requires_frame(0, TSDP_FRAME_STRING, 0);
			requires_frame(1, TSDP_FRAME_TSTAMP, 8);
			requires_frame(2, TSDP_FRAME_STRING, 0);
			return 1;

		case TSDP_PAYLOAD_FACT:
			errno = TSDP_E_INVALID_ARITY;
			requires_exact_frame_count(2);

			errno = TSDP_E_INVALID_FRAME;
			requires_frame(0, TSDP_FRAME_STRING, 0);
			requires_frame(1, TSDP_FRAME_STRING, 0);
			return 1;
		}
		return 0;

	case TSDP_OPCODE_BROADCAST:
		errno = TSDP_E_INVALID_PAYLOAD;
		requires_single_payload();

		switch (m->payload) {
		case TSDP_PAYLOAD_SAMPLE:
			errno = TSDP_E_INVALID_ARITY;
			requires_min_frame_count(4);

			errno = TSDP_E_INVALID_FRAME;
			requires_frame(0, TSDP_FRAME_STRING, 0);
			requires_frame(1, TSDP_FRAME_TSTAMP, 8);
			requires_frame(2, TSDP_FRAME_UINT,   4);
			for (i = 3; i < m->nframes; i++) {
				requires_frame(i, TSDP_FRAME_FLOAT, 8); /* measurement(s)   */
			}
			return 1;

		case TSDP_PAYLOAD_TALLY:
			errno = TSDP_E_INVALID_ARITY;
			requires_exact_frame_count(4);

			errno = TSDP_E_INVALID_FRAME;
			requires_frame(0, TSDP_FRAME_STRING, 0);
			requires_frame(1, TSDP_FRAME_TSTAMP, 8);
			requires_frame(2, TSDP_FRAME_UINT,   4);
			requires_frame(3, TSDP_FRAME_UINT,   8);
			return 1;

		case TSDP_PAYLOAD_DELTA:
			errno = TSDP_E_INVALID_ARITY;
			requires_exact_frame_count(4);

			errno = TSDP_E_INVALID_FRAME;
			requires_frame(0, TSDP_FRAME_STRING, 0);
			requires_frame(1, TSDP_FRAME_TSTAMP, 8);
			requires_frame(2, TSDP_FRAME_UINT,   4);
			requires_frame(3, TSDP_FRAME_FLOAT,  8);
			return 1;

		case TSDP_PAYLOAD_STATE:
			if (m->flags & 0x40) { /* transition */
				errno = TSDP_E_INVALID_ARITY;
				requires_exact_frame_count(6);

				errno = TSDP_E_INVALID_FRAME;
				requires_frame(0, TSDP_FRAME_STRING, 0);
				requires_frame(1, TSDP_FRAME_UINT,   4);
				requires_frame(2, TSDP_FRAME_TSTAMP, 8);
				requires_frame(3, TSDP_FRAME_STRING, 0);
				requires_frame(4, TSDP_FRAME_TSTAMP, 8);
				requires_frame(5, TSDP_FRAME_STRING, 0);

			} else { /* non-transition (no previous state) */
				errno = TSDP_E_INVALID_ARITY;
				requires_exact_frame_count(4);

				errno = TSDP_E_INVALID_FRAME;
				requires_frame(0, TSDP_FRAME_STRING, 0);
				requires_frame(1, TSDP_FRAME_UINT,   4);
				requires_frame(2, TSDP_FRAME_TSTAMP, 8);
				requires_frame(3, TSDP_FRAME_STRING, 0);
			}
			return 1;

		case TSDP_PAYLOAD_EVENT:
			errno = TSDP_E_INVALID_ARITY;
			requires_exact_frame_count(3);

			errno = TSDP_E_INVALID_FRAME;
			requires_frame(0, TSDP_FRAME_STRING, 0);
			requires_frame(1, TSDP_FRAME_TSTAMP, 8);
			requires_frame(2, TSDP_FRAME_STRING, 0);
			return 1;

		case TSDP_PAYLOAD_FACT:
			errno = TSDP_E_INVALID_ARITY;
			requires_exact_frame_count(2);

			errno = TSDP_E_INVALID_FRAME;
			requires_frame(0, TSDP_FRAME_STRING, 0);
			requires_frame(1, TSDP_FRAME_STRING, 0);
			return 1;

		}
		return 0;

	case TSDP_OPCODE_FORGET:
		errno = TSDP_E_INVALID_PAYLOAD;
		requires_payloads(TSDP_PAYLOAD_SAMPLE
		                | TSDP_PAYLOAD_TALLY
		                | TSDP_PAYLOAD_DELTA
		                | TSDP_PAYLOAD_STATE);

		errno = TSDP_E_INVALID_ARITY;
		requires_exact_frame_count(1);
		requires_frame(0, TSDP_FRAME_STRING, 0);
		return 1;

	case TSDP_OPCODE_REPLAY:
		errno = TSDP_E_INVALID_PAYLOAD;
		requires_one_or_more_payloads();

		errno = TSDP_E_INVALID_ARITY;
		requires_exact_frame_count(0);
		return 1;

	case TSDP_OPCODE_SUBSCRIBE:
		errno = TSDP_E_INVALID_PAYLOAD;
		requires_one_or_more_payloads();

		errno = TSDP_E_INVALID_ARITY;
		requires_exact_frame_count(1);
		requires_frame(0, TSDP_FRAME_STRING, 0);
		return 1;

	default:
		errno = TSDP_E_INVALID_OPCODE;
		return 0;
	}

	return 1;
}

static const char* OPCODE_NAMES[] = {
	"HEARTBEAT",
	"SUBMIT",
	"BROADCAST",
	"FORGET",
	"REPLAY",
	"SUBSCRIBE",
	NULL
};
static const char*
s_opcode_name(unsigned int opcode)
{
	if (opcode > TSDP_OPCODE_SUBSCRIBE) {
		return "<unknown>";
	}
	return OPCODE_NAMES[opcode];
}

static char binfmt[8 * (8 + 1) + 1];
static const char*
s_in_binary(unsigned long i, int size)
{
	int off = (8 * (8 + 1));

	if (size > 4) {
		off = 0;
		memcpy(binfmt, "<bad size>", 11);
		return binfmt;
	}

	binfmt[8 * (8 + 1)] = '\0';
	while (size-- > 0) {
		off -= (8 + 1);
		binfmt[off + 0] = ' ';
		binfmt[off + 8] = (i & 0x01) ? '1' : '0';
		binfmt[off + 7] = (i & 0x02) ? '1' : '0';
		binfmt[off + 6] = (i & 0x04) ? '1' : '0';
		binfmt[off + 5] = (i & 0x08) ? '1' : '0';
		binfmt[off + 4] = (i & 0x10) ? '1' : '0';
		binfmt[off + 3] = (i & 0x20) ? '1' : '0';
		binfmt[off + 2] = (i & 0x40) ? '1' : '0';
		binfmt[off + 1] = (i & 0x80) ? '1' : '0';
		i = i >> 8;
	}
	return binfmt + off + 1;
}

static const char *
qtime(uint64_t seconds)
{
	time_t ts;
	static char qtime_buf[64];
	char *fmt, *p;

	ts = seconds;
	fmt = ctime(&ts);
	if (!fmt) return "";

	p = strchr(fmt, '\n');
	if (p && *p) *p = '\0';

	snprintf(qtime_buf, 64, "[%s]", fmt);
	return qtime_buf;
}

static const char *
qstr(const char *input, size_t len) {
	const char *from;
	char *to;
	int max, trunc;
	static const char *hex = "0123456789abcdef";
	static char qstr_buf[128];

	trunc = 0;
	max = 127 - 2; /* make room for quotes */
	if (len > 127) {
		trunc = 1;
		max -= 3; /* make room for "..." */
	}

	from = input; to = qstr_buf;
	*to++ = '"';
	while (max > 0 && from < input + len) {
		if (!*from) break;
		if (isprint(*from) || isspace(*from)) {
			*to++ = *from++;
		} else {
			if (max >= 3) {
				*to++ = '\\';
				*to++ = 'x';
				*to++ = hex[(*from & 0xf0) >> 8];
				*to++ = hex[(*from & 0x0f)];
				max -= 4;
			}
		}
	}
	*to++ = '"';
	if (trunc) {
		*to++ = '.';
		*to++ = '.';
		*to++ = '.';
	}
	*to = '\0';
	return qstr_buf;
}

void
tsdp_msg_fdump(FILE *io, struct tsdp_msg *m)
{
	int i;
	struct tsdp_frame *f;

	fprintf(io, "version: %d\n",         m->version);
	fprintf(io, "opcode:  %d [%s]\n",    m->opcode, s_opcode_name(m->opcode));
	fprintf(io, "flags:   %02x (%sb)\n", m->flags,   s_in_binary(m->flags,   sizeof(m->flags)));
	fprintf(io, "payload: %04x (%sb)\n", m->payload, s_in_binary(m->payload, sizeof(m->payload)));
	if (m->payload & TSDP_PAYLOAD_SAMPLE) fprintf(io, "          - SAMPLE (%04x)\n", TSDP_PAYLOAD_SAMPLE);
	if (m->payload & TSDP_PAYLOAD_TALLY)  fprintf(io, "          - TALLY  (%04x)\n", TSDP_PAYLOAD_TALLY);
	if (m->payload & TSDP_PAYLOAD_DELTA)  fprintf(io, "          - DELTA  (%04x)\n", TSDP_PAYLOAD_DELTA);
	if (m->payload & TSDP_PAYLOAD_STATE)  fprintf(io, "          - STATE  (%04x)\n", TSDP_PAYLOAD_STATE);
	if (m->payload & TSDP_PAYLOAD_EVENT)  fprintf(io, "          - EVENT  (%04x)\n", TSDP_PAYLOAD_EVENT);
	if (m->payload & TSDP_PAYLOAD_FACT)   fprintf(io, "          - FACT   (%04x)\n", TSDP_PAYLOAD_FACT);

	fprintf(io, "frames:  %d\n", m->nframes);
	for (i = 0, f = m->frames; f; f = f->next, i++) {
		time_t ts;
		fprintf(io, "   % 2d) ", i);
		switch (f->type) {
		case TSDP_FRAME_UINT:
			switch (f->length) {
			case 2:  fprintf(io, "UINT/2   %u\n",  f->payload.uint16); break;
			case 4:  fprintf(io, "UINT/4   %u\n",  f->payload.uint32); break;
			case 8:  fprintf(io, "UINT/8   %lu\n", f->payload.uint64); break;
			default: fprintf(io, "UINT/%d\n", f->length); break;
			}
			break;

		case TSDP_FRAME_FLOAT:
			switch (f->length) {
			case 4:  fprintf(io, "FLOAT/4  %e\n", f->payload.float32); break;
			case 8:  fprintf(io, "FLOAT/8  %e\n", f->payload.float64); break;
			default: fprintf(io, "FLOAT/%d\n", f->length); break;
			}
			break;

		case TSDP_FRAME_TSTAMP:
			switch (f->length) {
			case 8:  fprintf(io, "TSTAMP/8 %s (%lu)\n", qtime(f->payload.tstamp),
			                                            f->payload.tstamp); break;
			default: fprintf(io, "TSTAMP/%d\n", f->length); break;
			}
			break;

		case TSDP_FRAME_NIL:     fprintf(io, "NIL/%d\n",    f->length); break;
		case TSDP_FRAME_STRING:  fprintf(io, "STRING/%d %s\n", f->length,
		                                     qstr(f->payload.string, f->length)); break;

		default: fprintf(io, "\?\?\?(%02x)/%d\n", f->type, f->length);
		}
	}
}

unsigned int
tsdp_msg_version(struct tsdp_msg *m)
{
	return m->version;
}

unsigned int
tsdp_msg_opcode(struct tsdp_msg *m)
{
	return m->opcode;
}

unsigned int
tsdp_msg_flags(struct tsdp_msg *m)
{
	return m->flags;
}

unsigned int
tsdp_msg_payload(struct tsdp_msg *m)
{
	return m->payload;
}

unsigned int
tsdp_msg_nframes(struct tsdp_msg *m)
{
	return m->nframes;
}

int
tsdp_msg_frame_as_string(char **dst, struct tsdp_msg *m, int n)
{
	struct tsdp_frame *f;

	f = s_nth_frame(m, n);
	if (!f) return 1;
	if (f->type != TSDP_FRAME_STRING) return 1;

	*dst = calloc(f->length + 1, sizeof(char));
	if (!*dst) return 1;

	memcpy(*dst, f->payload.string, f->length);
	return 0;
}

int
tsdp_msg_frame_as_tstamp8(uint64_t *dst, struct tsdp_msg *m, int n)
{
	struct tsdp_frame *f;

	f = s_nth_frame(m, n);
	if (!f) return 1;
	if (f->type != TSDP_FRAME_TSTAMP) return 1;
	if (f->length == 8) {
		*dst = f->payload.tstamp;
		return 0;
	}
	return 1;
}

int
tsdp_msg_frame_as_uint2(uint16_t *dst, struct tsdp_msg *m, int n)
{
	struct tsdp_frame *f;

	f = s_nth_frame(m, n);
	if (!f) return 1;
	if (f->type != TSDP_FRAME_UINT) return 1;
	if (f->length == 2) {
		*dst = f->payload.uint16;
		return 0;
	}
	return 1;
}

int
tsdp_msg_frame_as_uint4(uint32_t *dst, struct tsdp_msg *m, int n)
{
	struct tsdp_frame *f;

	f = s_nth_frame(m, n);
	if (!f) return 1;
	if (f->type != TSDP_FRAME_UINT) return 1;
	if (f->length == 4) {
		*dst = f->payload.uint32;
		return 0;
	}
	return 1;
}

int
tsdp_msg_frame_as_uint8(uint64_t *dst, struct tsdp_msg *m, int n)
{
	struct tsdp_frame *f;

	f = s_nth_frame(m, n);
	if (!f) return 1;
	if (f->type != TSDP_FRAME_UINT) return 1;
	if (f->length == 8) {
		*dst = f->payload.uint64;
		return 0;
	}
	return 1;
}

int
tsdp_msg_frame_as_float8(double *dst, struct tsdp_msg *m, int n)
{
	struct tsdp_frame *f;

	f = s_nth_frame(m, n);
	if (!f) return 1;
	if (f->type != TSDP_FRAME_FLOAT) return 1;
	if (f->length == 8) {
		*dst = f->payload.float64;
		return 0;
	}
	return 1;
}


unsigned int
tsdp_frame_type(struct tsdp_frame *f)
{
	return f->type;
}

unsigned int
tsdp_frame_length(struct tsdp_frame *f)
{
	return f->length;
}
