#ifndef TSDP_H
#define TSDP_H

#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>

#define TSDP_MAX_QNAME_PAIRS    64
#define TSDP_MAX_QNAME_LEN    4095

struct tsdp_qname {
	unsigned int pairs;       /* how many key/value pairs are there?   */
	unsigned int length;      /* how long is the canonical string rep? */
	int wildcard;             /* is this a wildcard pattern match?     */

	char *keys[TSDP_MAX_QNAME_PAIRS];    /* keys (pointers into flyweight)        */
	char *values[TSDP_MAX_QNAME_PAIRS];  /* values (pointers into flyweight)      */
	int   partial[TSDP_MAX_QNAME_PAIRS]; /* key is a '*' match (1) or not (0)     */

	char flyweight[];         /* munged copy of original qn string     */
};
#define INVALID_QNAME ((struct tsdp_qname*)(0))

/**
   Parse a qualified name from an input string,
   returning the `struct tsdp_qname *` that results,
   or the value `INVALID_QNAME` on error, at which
   point `errno` is set appropriately.

   This function allocates memory, and may fail
   if insufficient memory is available.
 **/
struct tsdp_qname *
tsdp_qname_parse(const char *str);

/**
  Frees the memory allocated to the qualified name.
  Handles passing n as `INVALID_QNAME`.
 **/
void
tsdp_qname_free(struct tsdp_qname *n);

/**
  Allocates a fresh null-terminated string which
  contains the canonical representation of the
  given qualified name.

  Returns the empty string for `INVALID_QNAME`.
 **/
char *
tsdp_qname_string(struct tsdp_qname *n);

/**
  Returns non-zero if the two qualified names
  are exactly equivalent, handling wildcard as
  explicit matches (that is `key=*` is equivalent
  to `key=*`, but not `key=value`).

  Returns 0 otherwise.

  The value INVALID_QNAME is never equivalent
  to anything, even another INVALID_QNAME.
 **/
int
tsdp_qname_equal(struct tsdp_qname *a, struct tsdp_qname *b);

/**
  Returns non-zero if the first qualified name (`qn`) matches the
  second qualified name (`pattern`), honoring wildcard semantics
  in the pattern name.

  Returns 0 if `qn` does not match `pattern`.

  The value INVALID_QNAME will never match any other name,
  even another INVALID_QNAME.
 **/
int
tsdp_qname_match(struct tsdp_qname *qn, struct tsdp_qname *pattern);


#define TSDP_PROTOCOL_V1       1


#define TSDP_OPCODE_HEARTBEAT  0
#define TSDP_OPCODE_SUBMIT     1
#define TSDP_OPCODE_BROADCAST  2
#define TSDP_OPCODE_FORGET     3
#define TSDP_OPCODE_REPLAY     4
#define TSDP_OPCODE_SUBSCRIBE  5


#define TSDP_PAYLOAD_SAMPLE    0x0001
#define TSDP_PAYLOAD_TALLY     0x0002
#define TSDP_PAYLOAD_DELTA     0x0004
#define TSDP_PAYLOAD_STATE     0x0008
#define TSDP_PAYLOAD_EVENT     0x0010
#define TSDP_PAYLOAD_FACT      0x0020
// ......................      ......
#define TSDP_PAYLOAD_RSVP      0xffc0


struct tsdp_frame {
	unsigned char  type;       /* type of payload (TSDP_FRAME_*) */
	unsigned short length;     /* length of raw payload (data[]) */
	union {
		uint32_t  uint32;      /* TSDP_FRAME_UINT/32             */
		uint64_t  uint64;      /* TSDP_FRAME_UINT/64             */
		float     float32;     /* TSDP_FRAME_FLOAT/32            */
		float     float64;     /* TSDP_FRAME_FLOAT/64            */
		char     *string;      /* TSDP_FRAME_STRING              */
		uint64_t  tstamp;      /* TSDP_FRAME_TSTAMP/64           */
		/* nothing */          /* TSDP_FRAME_NIL                 */
	} payload;

	struct tsdp_frame *next;   /* next sequential frame or NULL  */
	uint8_t data[];            /* variable length data buffer    */
};

struct tsdp_msg {
	unsigned char   version;   /* TSDP protocol version (1)      */
	unsigned char   opcode;    /* what type of message is this?  */
	unsigned char   flags;     /* opcode-specific flags          */
	unsigned short  payload;   /* TSDP_PAYLOAD_* constant(s)     */

	int complete;              /* do we have all the frames?     */
	int nframes;               /* how many frames do we have?    */
	struct tsdp_frame *frames; /* constituent MSG FRAMEs         */
	struct tsdp_frame *last;   /* helper pointer to last frame   */
};

#define TSDP_FRAME_UINT        0
#define TSDP_FRAME_FLOAT       1
#define TSDP_FRAME_STRING      2
// ......................      .
#define TSDP_FRAME_TSTAMP      6
#define TSDP_FRAME_NIL         7

struct tsdp_msg *
tsdp_msg_unpack(const void *buf, size_t n, size_t *left);

int
tsdp_msg_valid(struct tsdp_msg *m);

void
tsdp_msg_fdump(FILE* io, struct tsdp_msg *m);

unsigned int
tsdp_msg_version(struct tsdp_msg *m);

unsigned int
tsdp_msg_opcode(struct tsdp_msg *m);

unsigned int
tsdp_msg_flags(struct tsdp_msg *m);

unsigned int
tsdp_msg_payload(struct tsdp_msg *m);

unsigned int
tsdp_msg_nframes(struct tsdp_msg *m);


unsigned int
tsdp_frame_type(struct tsdp_frame *f);

unsigned int
tsdp_frame_length(struct tsdp_frame *f);


#endif
