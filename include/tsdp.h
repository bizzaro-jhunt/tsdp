#ifndef TSDP_H
#define TSDP_H

#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>

#define TSDP_E_INVALID_VERSION   513
#define TSDP_E_INVALID_OPCODE    514
#define TSDP_E_INVALID_FLAG      515
#define TSDP_E_INVALID_PAYLOAD   516
#define TSDP_E_INVALID_ARITY     517
#define TSDP_E_INVALID_FRAME     518

const char *
tsdp_strerror(int num);

void
tsdp_perror(const char *prefix);

#define TSDP_MAX_QNAME_PAIRS    64
#define TSDP_MAX_QNAME_LEN    4095

struct tsdp_qname {
	size_t allocated; /* number of octets allocated to this qname */

	unsigned int pairs;       /* how many key/value pairs are there?   */
	unsigned int length;      /* how long is the canonical string rep? */
	int wildcard;             /* is this a wildcard pattern match?     */

	char *keys[TSDP_MAX_QNAME_PAIRS];    /* keys (pointers into flyweight)        */
	char *values[TSDP_MAX_QNAME_PAIRS];  /* values (pointers into flyweight)      */
	int   partial[TSDP_MAX_QNAME_PAIRS]; /* key is a '*' match (1) or not (0)     */
	int   alloc[TSDP_MAX_QNAME_PAIRS];   /* key and value are malloc'd (via _set) */

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
   Duplicate a qualified name into a newly allocated
   tsdp_qname structure.

   This function allocates memory, and may fail
   if insufficient memory is available.
 **/
struct tsdp_qname *
tsdp_qname_dup(struct tsdp_qname *n);

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

int
tsdp_qname_set(struct tsdp_qname *qn, const char *key, const char *value);

int
tsdp_qname_unset(struct tsdp_qname *qn, const char *key);

const char *
tsdp_qname_get(struct tsdp_qname *qn, const char *key);

int
tsdp_qname_merge(struct tsdp_qname *a, struct tsdp_qname *b);


#define TSDP_PROTOCOL_V1       1
#define tsdp_version_ok(v) ((v) == TSDP_PROTOCOL_V1)


#define TSDP_OPCODE_HEARTBEAT  0
#define TSDP_OPCODE_SUBMIT     1
#define TSDP_OPCODE_BROADCAST  2
#define TSDP_OPCODE_FORGET     3
#define TSDP_OPCODE_REPLAY     4
#define TSDP_OPCODE_SUBSCRIBE  5
#define tsdp_opcode_ok(o) ((o) >= TSDP_OPCODE_HEARTBEAT && (o) <= TSDP_OPCODE_SUBSCRIBE)


#define tsdp_flags_ok(o) ((o) < 256 && (o) >= 0)


#define TSDP_PAYLOAD_SAMPLE    0x0001
#define TSDP_PAYLOAD_TALLY     0x0002
#define TSDP_PAYLOAD_DELTA     0x0004
#define TSDP_PAYLOAD_STATE     0x0008
#define TSDP_PAYLOAD_EVENT     0x0010
#define TSDP_PAYLOAD_FACT      0x0020
// ......................      ......
#define TSDP_PAYLOAD_RSVP      0xffc0
#define tsdp_payload_ok(p) (((p) & TSDP_PAYLOAD_RSVP) == 0)


struct tsdp_frame {
	unsigned char  type;       /* type of payload (TSDP_FRAME_*) */
	unsigned short length;     /* length of raw payload (data[]) */
	union {
		uint16_t  uint16;      /* TSDP_FRAME_UINT/16             */
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

/**
  Allocates and returns a new, empty tsdp_msg structure with
  the version, opcode, flags and payload set.  Rudimentary
  validation will be made against passed arguments, mostly
  for boundary checking.

  Returns a pointer to a freshly allocated message structure
  on success, or NULL on failure, at which point `errno` will
  be set to one of the following:

    EINVAL   One of the arguments was out of range or otherwise
             incorrect (i.e., an unsupported version, unknown
             opcode, bad payload, etc.)

    any error that `calloc(3)` can raise.

  The returned message structure must be freed by the caller,
  via `tsdp_msg_free()`, when no longer needed.
 */
struct tsdp_msg *
tsdp_msg_new(int version, int opcode, int flags, int payload);

/**
  Free memory resources allocated to the given tsdp_msg
  structure.  After this call, the pointer passed may not
  be used for anything, and should be nullified, if possible.

  It is not an error to pass a NULL pointer.
 */
void
tsdp_msg_free(struct tsdp_msg *m);

/**
  Extend a tsdp_msg structure by appending an additional
  frame to it, of the specified type, length and data payload.

  Some minimal validation will be performed, to ensure that the
  frame itself is well-formed.  Whether that frame makes sense
  where it gets inserted, given the opcode / flags / etc. of the
  containing message is unknown (and unchecked).

  Returns 0 on success; -1 on error and sets `errno`.
 */
int
tsdp_msg_extend(struct tsdp_msg *m, int type, const void *v, size_t len);

/**
  Pack the tsdp_msg structure, in TSDP wire protocol format,
  into the first `len` bytes of `buf`.  If `buf` is not big
  enough to hold all of the binary representation, only the first
  `len` octets will be copied into `buf`.

  The number of octets required to represent the message structure
  will be returned.  If this is larger than `len`, truncation has
  occurred.

  It is valid to pass `len` as 0 and `buf` as NULL, to determine
  how big `buf` needs to be (e.g., for dynamic allocation).

  Returns 0 on failure.
 */
ssize_t
tsdp_msg_pack(void *buf, size_t len, struct tsdp_msg *m);

struct tsdp_msg *
tsdp_msg_unpack(const void *buf, size_t n, size_t *left);

/**
  Check the validity of a TSDP message, based on its opcode
  and other details of the message itself.

  Returns 1 if the message is well-formed and semantically
  valid, and 0 if not.  This is designed to allow easy use
  inside of `if` conditionals.
 */
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

int
tsdp_msg_frame_as_string(char **dst, struct tsdp_msg *m, int n);

int
tsdp_msg_frame_as_tstamp8(uint64_t *dst, struct tsdp_msg *m, int n);

int
tsdp_msg_frame_as_uint2(uint16_t *dst, struct tsdp_msg *m, int n);

int
tsdp_msg_frame_as_uint4(uint32_t *dst, struct tsdp_msg *m, int n);

int
tsdp_msg_frame_as_uint8(uint64_t *dst, struct tsdp_msg *m, int n);

int
tsdp_msg_frame_as_float8(float *dst, struct tsdp_msg *m, int n);

unsigned int
tsdp_frame_type(struct tsdp_frame *f);

unsigned int
tsdp_frame_length(struct tsdp_frame *f);


#endif
