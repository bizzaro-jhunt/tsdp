#include <tsdp.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "debug.h"

/* constants for use in the Parser Finite State Machine
   (refer to docs/qname-fsm.dot for more details)
 */
#define TSDP_PFSM_K1 1
#define TSDP_PFSM_K2 2
#define TSDP_PFSM_V1 3
#define TSDP_PFSM_V2 4
#define TSDP_PFSM_M  5

static char *__QNAME_WILDCARD = "*";

#define qfree(x) do { if ((x) != __QNAME_WILDCARD) free((x)); } while (0)
#define qvalue(x) ((x) && (x) != __QNAME_WILDCARD)

#include "qname_chars.inc"
#define s_is_character(c)  (TBL_QNAME_CHARACTER[((c) & 0xff)] == 1)
#define s_is_wildcard(c)   ((c) == '*')

#define swap(x,y) do { \
	x = (char *)((uintptr_t)(x) ^ (uintptr_t)(y)); \
	y = (char *)((uintptr_t)(y) ^ (uintptr_t)(x)); \
	x = (char *)((uintptr_t)(x) ^ (uintptr_t)(y)); \
} while (0)

static void
s_qname_expand(struct qname *q)
{
	int i;

	if (!q->flat) return; /* already expanded */
	q->metric = strdup(q->metric);
	for (i = 0; i < q->i; i++) {
		if (qvalue(q->pairs[i].key))   q->pairs[i].key = strdup(q->pairs[i].key);
		if (qvalue(q->pairs[i].value)) q->pairs[i].value = strdup(q->pairs[i].value);
	}
	free(q->flat);
	q->flat = NULL;
}

static void
s_qname_contract(struct qname *q)
{
	int i;
	size_t len;
	char *f, *t;

	if (q->flat) return; /* already contracted */
	len = strlen(q->metric) + 1;
	for (i = 0; i < q->i; i++)
		len = len + strlen(q->pairs[i].key)   + 1
		          + strlen(q->pairs[i].value) + 1;

	q->len = len;
	q->flat = malloc(len);
	if (!q->flat) return;

	f = q->flat;
#define copy(s) do { \
	memcpy(f, (s), strlen(s)); \
	t = f + strlen(s); *t++ = '\0'; \
	qfree(s); \
	(s) = t; \
} while (0);

	copy(q->metric);
	for (i = 0; i < q->i; i++) {
		copy(q->pairs[i].key);
		copy(q->pairs[i].value);
	}

#undef copy
}

static inline int
s_qname_next(struct qname *q)
{
	q->i++;
	if (q->i >= QNAME_MAX_PAIRS) {
		debugf("exceeded QNAME_MAX_PAIRS (%d) after key '%s'\n", QNAME_MAX_PAIRS, q->pairs[q->i-1].key);
		return -1;
	}
	return 0;
}




struct qname *
qname_new()
{
	struct qname *q;

	q = calloc(1, sizeof(struct qname));
	if (!q) return NULL;

	return q;
}


/**
   Parse a qualified name from an input string,
   returning the `struct qname *` that results,
   or NULL on error, with `errno` set appropriately.

   This function allocates memory, and may fail
   if insufficient memory is available.
 **/
struct qname *
qname_parse(const char *s)
{
	struct qname *q;
	const char *p;
	char *fill;
	int fsm, esc, i;
	size_t len;

	if (!s) return NULL;
	if (strlen(s) > QNAME_MAX_LEN) {
		debugf("input string %p is %lu octets long (>%u)\n", string, strlen(string), TSDP_MAX_QNAME_LEN);
		return NULL;
	}

	q = qname_new();
	if (!q) return NULL;

	q->len  = strlen(s) + 1;
	q->flat = strdup(s);
	if (!q->flat) goto cleanup;

	/* skip whitespace */
	for (p = s; *p && *p == ' '; p++);

	/* metric name */
	q->metric = fill = q->flat;
	while (*p && *p != ' ') *fill++ = *p++;

	/* is metric name empty? */
	if (fill == q->metric) goto cleanup;

	/* do we have tags? */
	if (*p) { p++; *fill++ = '\0'; }

	esc = 0;
	q->i = 0;
	fsm = TSDP_PFSM_K1;
	for (; *p; p++) {
		if (*p == '\\') { esc = 1; continue; }
		if (esc) {
			switch (fsm) {
			case TSDP_PFSM_K1: q->pairs[q->i].key = fill;
			                   fsm = TSDP_PFSM_K2;
			case TSDP_PFSM_K2: *fill++ = *p;
			                   break;

			case TSDP_PFSM_V1: q->pairs[q->i].value = fill;
			                   fsm = TSDP_PFSM_V2;
			case TSDP_PFSM_V2: *fill++ = *p;
			                   break;

			default: debugf("invalid FSM state [%d] for escape sequence\n", fsm);
			         goto cleanup;
			}
			esc = 0;
			continue;
		}

		switch (fsm) {
		case TSDP_PFSM_K1:
			if (*p == ' ') break;
			if (s_is_character(*p)) {
				q->pairs[q->i].key = fill;
				*fill++ = *p;
				fsm = TSDP_PFSM_K2;

			} else if (s_is_wildcard(*p)) {
				q->pairs[q->i].key = fill;
				q->wild = 1;
				*fill++ = *p;
				fsm = TSDP_PFSM_M;

			} else {
				debugf("invalid token (%c / %#02x) for transition from state K1\n", *p, *p);
				goto cleanup;
			}
			break;


		case TSDP_PFSM_K2:
			if (*p == '=') {
				*fill++ = '\0';
				fsm = TSDP_PFSM_V1;

			} else if (*p == ',') {
				*fill++ = '\0';
				q->pairs[q->i].value = NULL;
				if (s_qname_next(q) != 0) goto cleanup;
				fsm = TSDP_PFSM_K1;

			} else if (s_is_character(*p)) {
				*fill++ = *p;

			} else {
				debugf("invalid token (%c / %#02x) for transition from state K2\n", *p, *p);
				goto cleanup;
			}
			break;


		case TSDP_PFSM_V1:
			if (s_is_character(*p)) {
				q->pairs[q->i].value = fill;
				*fill++ = *p;
				fsm = TSDP_PFSM_V2;

			} else if (s_is_wildcard(*p)) {
				q->pairs[q->i].value = __QNAME_WILDCARD;
				*fill++ = *p;
				fsm = TSDP_PFSM_M;

			} else if (*p == ',') {
				*fill++ = '\0';
				if (s_qname_next(q) != 0) goto cleanup;
				fsm = TSDP_PFSM_K1;

			} else {
				debugf("invalid token (%c / %#02x) for transition from state V1\n", *p, *p);
				goto cleanup;
			}
			break;


		case TSDP_PFSM_V2:
			if (*p == ',') {
				*fill++ = '\0';
				if (s_qname_next(q) != 0) goto cleanup;
				fsm = TSDP_PFSM_K1;

			} else if (s_is_character(*p)) {
				*fill++ = *p;

			} else {
				debugf("invalid token (%c / %#02x) for transition from state V2\n", *p, *p);
				goto cleanup;
			}
			break;


		case TSDP_PFSM_M:
			if (*p == ',') {
				*fill++ = '\0';
				if (s_qname_next(q) != 0) goto cleanup;
				fsm = TSDP_PFSM_K1;

			} else {
				debugf("invalid token (%c / %#02x) for transition from state M\n", *p, *p);
				goto cleanup;
			}
			break;


		defaut:
			debugf("invalid FSM state [%d]\n", fsm);
			goto cleanup;
		}
	}

	/* EOF; check states that can legitimately lead to DONE */
	switch (fsm) {
	case TSDP_PFSM_K2:
	case TSDP_PFSM_V1:
	case TSDP_PFSM_V2:
	case TSDP_PFSM_M:
		if (!q->wild) q->i++;
		*fill++ = '\0';
		break;

	default:
		debugf("invalid final FSM state [%d]\n", fsm);
		goto cleanup;
	}

	/* remove trailing and leading whitespace from keys
	   and values, adjusting length as necessary */
	for (i = 0; i < q->i; i++) {
		if (fill = q->pairs[i].key) {
			/* leading space on key */
			while (*fill == ' ') fill++;
			if (!*fill) {
				/* N.B.: this should never happen unless there is a bug in the
				         K1 -> K2 (on whitespace) state transition in the FSM,
				         but it doesn't hurt to be cautious. */
				debugf("key %d was pure whitespace\n", i+1);
				goto cleanup;
			}
			q->pairs[i].key = fill;

			/* trailing space on key */
			while (*fill) fill++; fill--; while (*fill == ' ') *fill-- = '\0';
		}

		if (fill = q->pairs[i].value) {
			/* leading space on value */
			while (*fill == ' ') fill++;
			q->pairs[i].value = fill;

			/* trailing space on value */
			while (*fill) fill++; fill--; while (*fill == ' ') *fill-- = '\0';
		}
	}

	/* sort the key/value pairs lexcially by key, to make
	   comparison and stringification easier, later */
	for (i = 1; i < q->i; i++) {
		int j = i;
		while (j > 0 && strcmp(q->pairs[j-1].key, q->pairs[j].key) > 0) {
			swap(q->pairs[j-1].key,   q->pairs[j].key);
			swap(q->pairs[j-1].value, q->pairs[j].value);
		}
	}
	return q;

cleanup:
	if (q) qname_free(q);
	return NULL;
}


/**
   Duplicate a qualified name into a newly allocated
   qname structure.

   This function allocates memory, and may fail
   if insufficient memory is available.
 **/
struct qname *
qname_dup(struct qname *q)
{
	struct qname *dup;
	int i;

	if (!q) return NULL;

	dup = malloc(sizeof(struct qname));
	if (!dup) return NULL;

	dup->wild = q->wild;
	dup->i    = q->i;

	if (!q->flat) { /* expanded */
		dup->flat = NULL;
		dup->metric = q->metric ? strdup(q->metric) : NULL;
		for (i = 0; i < q->i; i++) {
			dup->pairs[i].key   = strdup(q->pairs[i].key);
			dup->pairs[i].value = q->pairs[i].value;
			if (dup->pairs[i].value && dup->pairs[i].value != __QNAME_WILDCARD)
				dup->pairs[i].value = strdup(dup->pairs[i].value);
		}
	} else { /* contracted */
		dup->len = q->len;
		dup->flat = malloc(dup->len);
		if (!dup->flat) {
			qname_free(dup);
			return NULL;
		}
		memcpy(dup->flat, q->flat, dup->len);
		dup->metric = q->metric - q->flat + dup->flat;
		for (i = 0; i < q->i; i++) {
			dup->pairs[i].key   = q->pairs[i].key - q->flat + dup->flat;
			dup->pairs[i].value = q->pairs[i].value;
			if (q->pairs[i].value && q->pairs[i].value != __QNAME_WILDCARD)
				dup->pairs[i].value = dup->pairs[i].value - q->flat + dup->flat;
		}
	}

	return dup;
}


/**
  Frees the memory allocated to the qualified name.
  Handles passing n as `INVALID_QNAME`.
 **/
void
qname_free(struct qname *q)
{
	int i;
	if (!q) return;

	if (q->flat) free(q->flat);
	else {
		free(q->metric);
		for (i = 0; i < q->i; i++) {
			qfree(q->pairs[i].key);
			qfree(q->pairs[i].value);
		}
	}
	free(q);
}


/**
  Allocates a fresh null-terminated string which
  contains the canonical representation of the
  given qualified name.

  Returns the empty string for a null qname.
 **/
char *
qname_string(struct qname *q)
{
	int i;
	size_t len;
	char *f, *s;

	if (!q) return strdup("");

	len = strlen(q->metric) + 1;
	for (i = 0; i < q->i; i++) {
		len += strlen(q->pairs[i].key)   + 1;
		if (q->pairs[i].value)
			len += strlen(q->pairs[i].value) + 1;
	}
	if (q->wild) len += 2;

	s = malloc(len);
	if (!s) return NULL;

	f = s;
#define copy(s) do { \
	memcpy(f, (s), strlen(s)); \
	f += strlen(s); \
} while (0);

	copy(q->metric); *f++ = ' ';
	for (i = 0; i < q->i; i++) {
		copy(q->pairs[i].key);
		if (q->pairs[i].value) {
			*f++ = '=';
			copy(q->pairs[i].value);
		}
		*f++ = ',';
	}
	if (q->wild) *f++ = '*';
	else f--;
	*f = '\0';
	return s;
#undef copy
}


/**
  Returns non-zero if the two qualified names
  are exactly equivalent, handling wildcard as
  explicit matches (that is `key=*` is equivalent
  to `key=*`, but not `key=value`).

  Returns 0 otherwise.

  The NULL qname is never equivalent to anything,
  not even another NULL qname.
 **/
int
qname_equal(struct qname *a, struct qname *b)
{
	unsigned int i;
	/* the invalid qualified name is never equivalent to anything */
	if (!a || !b) return 0;

	/* wildcard names only match other wildcard names */
	if (a->wild != b->wild) return 0;

	/* equivalent names have the same number of key=value pairs */
	if (a->i != b->i) return 0;

	/* metric names must be present */
	if (!a->metric || !b->metric) return 0;
	/* ... and they must match */
	if (strcmp(a->metric, b->metric) != 0) return 0;

	/* pairs should be lexically ordered, so we can compare
	   them sequentially for key- and value-equality */
	for (i = 0; i < a->i; i++) {
		if (!a->pairs[i].key || !b->pairs[i].key)              return 0; /* it is an error to have NULL keys */
		if (strcmp(a->pairs[i].key, b->pairs[i].key) != 0)     return 0; /* key mismatch */
		if (!!a->pairs[i].value != !!b->pairs[i].value)        return 0; /* value not present in both */
		if (a->pairs[i].value &&
		    strcmp(a->pairs[i].value, b->pairs[i].value) != 0) return 0; /* value mismatch */
	}
	return 1;
}


/**
  Returns non-zero if the first qualified name (`qn`) matches the
  second qualified name (`pattern`), honoring wildcard semantics
  in the pattern name.

  Returns 0 if `qn` does not match `pattern`.

  The NULL qname will never match any other qname,
  not even another NULL qname.
 **/
int
qname_match(struct qname *qn, struct qname *pattern)
{
	unsigned int i, j;
	int found;

	/* the invalid qualified name never matches to anything */
	if (!qn || !pattern) return 0;

	/* metric names must be present */
	if (!qn->metric || !pattern->metric) return 0;
	/* ... and must be equivalent or wildcarded */
	if (pattern->metric != __QNAME_WILDCARD
	 && strcmp(qn->metric, pattern->metric) != 0) return 0;

	/* pattern constraints must be met first */
	for (i = 0; i < pattern->i; i++) {
		if (!pattern->pairs[i].key) return 0; /* it is an error to have NULL keys */

		/* see if the key is present in the qn */
		found = 0;
		for (j = 0; j < qn->i; j++) {
			if (strcmp(pattern->pairs[i].key, qn->pairs[j].key) == 0) {
				found = 1;
				break;
			}
		}
		if (!found) return 0; /* pattern constraint not met */
		if (pattern->pairs[i].value == __QNAME_WILDCARD) continue;
		if (!!qn->pairs[j].value != !!pattern->pairs[i].value) {
			return 0; /* value present in one, but not the other */
		}
		if (qn->pairs[j].value &&
		    strcmp(qn->pairs[j].value, pattern->pairs[i].value) != 0) {
			return 0; /* value mismatch */
		}
	}

	/* if the length of the name and the pattern don't match,
	   the name must be longer, and we need to check if the
	   pattern is a wildcard or not */
	if (qn->i != pattern->i && !pattern->wild) {
		return 0;
	}
	return 1;
}



int
qname_set(struct qname *q, const char *key, const char *value)
{
	int i;

	errno = EINVAL;
	if (!q) return -1;
	s_qname_expand(q);

	for (i = 0; i < q->i; i++) {
		if (strcmp(q->pairs[i].key, key) == 0) {
			free(q->pairs[i].value);
			q->pairs[i].value = value ? strdup(value) : NULL;
			return 0;
		}
	}
	if (q->i + 1 >= QNAME_MAX_PAIRS) {
		errno = ENOBUFS;
		return -1;
	}

	q->pairs[q->i].key = strdup(key);
	q->pairs[q->i].value = strcmp(value, __QNAME_WILDCARD) == 0
	                     ? __QNAME_WILDCARD
	                     : value ? strdup(value) : NULL;
	q->i++;
	return 0;
}


int
qname_unset(struct qname *q, const char *key)
{
	int i, j;
	errno = EINVAL;
	if (!q) return -1;
	s_qname_expand(q);

	for (i = 0; i < q->i; i++) {
		if (strcmp(q->pairs[i].key, key) == 0) {
			qfree(q->pairs[i].key);
			qfree(q->pairs[i].value);

			for (j = i+1; j < q->i; j++) {
				q->pairs[j-1].key   = q->pairs[j].key;
				q->pairs[j-1].value = q->pairs[j].value;
			}
			q->i--;
			return 0;
		}
	}

	return 0;
}


const char *
qname_get(struct qname *q, const char *key)
{
	int i;
	errno = EINVAL;
	if (!q) return NULL;

	for (i = 0; i < q->i; i++)
		if (strcmp(q->pairs[i].key, key) == 0)
			return q->pairs[i].value;
	return NULL;
}


int
qname_merge(struct qname *a, struct qname *b)
{
	int i, rc;
	errno = EINVAL;
	if (!a || !b) return -1;
	s_qname_expand(a);
	s_qname_expand(b);

	for (i = 0; i < b->i; i++) {
		if (b->pairs[i].key) {
			rc = qname_set(a, b->pairs[i].key, b->pairs[i].value);
			if (rc != 0) return rc;
		}
	}
	return 0;
}
