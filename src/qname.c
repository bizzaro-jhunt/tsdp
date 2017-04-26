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

#include "qname_chars.inc"
#define s_is_character(c)  (TBL_QNAME_CHARACTER[((c) & 0xff)] == 1)
#define s_is_wildcard(c)   ((c) == '*')

#define swapp(x,y) do { \
	x = (char *)((uintptr_t)(x) ^ (uintptr_t)(y)); \
	y = (char *)((uintptr_t)(y) ^ (uintptr_t)(x)); \
	x = (char *)((uintptr_t)(x) ^ (uintptr_t)(y)); \
} while (0)

#define swapu(x, y) do { x ^= y; y ^= x; x ^= y; } while (0)

static void
s_compact(struct tsdp_qname *qn)
{
	int i;

	for (i = 0; i < qn->pairs; i++) {
		if (qn->keys[i] == NULL) {
			if (i != qn->pairs - 1) {
				qn->keys[i]     = qn->keys[qn->pairs - 1];
				qn->values[i]   = qn->values[qn->pairs - 1];
				qn->alloc[i]    = qn->alloc[qn->pairs - 1];
				qn->partial[i]  = qn->partial[qn->pairs - 1];
			}
			qn->pairs--;
		}
	}
}

static void
s_sort(int num, char **keys, char **values, int *alloc, int *partial)
{
	/* dumb insertion sort algorithm */
	int i, j;

	for (i = 1; i < num; i++) {
		j = i;
		while (j > 0 && strcmp(keys[j-1], keys[j]) > 0) {
			swapp(keys[j-1], keys[j]);
			swapp(values[j-1], values[j]);
			swapu(alloc[j-1], alloc[j]);
			swapu(partial[j-1], partial[j]);
		}
	}
}

struct tsdp_qname *
tsdp_qname_parse(const char *string)
{
	struct tsdp_qname *qn; /* the qualified name itself (allocated)     */
	const char *p;         /* pointer for iterating over string         */
	char *fill;            /* pointer for filling flyweight             */
	int fsm;               /* parser finite state machine state         */
	int escaped;           /* last token was backslash (1) or not (0)   */
	int i;                 /* for iterating over pairs in post-process  */
	size_t len;

	if (!string) {
		debugf("invalid input string (%p / %s)\n", string, string);
		return INVALID_QNAME;
	}

	len = strlen(string);
	if (len > TSDP_MAX_QNAME_LEN) {
		debugf("input string %p is %lu octets long (>%u)\n", string, strlen(string), TSDP_MAX_QNAME_LEN);
		return INVALID_QNAME;
	}

	qn = calloc(1, sizeof(struct tsdp_qname)
	             + len /* flyweight       */
	             + 1); /* NULL-terminator */
	if (!qn) {
		debugf("alloc(%lu + %lu + %lu) [=%lu] failed\n", sizeof(struct tsdp_qname), strlen(string), 1LU,
				sizeof(struct tsdp_qname) + len + 1LU);
		return INVALID_QNAME;
	}

	/* store the allocation size, so we can dup() later... */
	qn->allocated = sizeof(struct tsdp_qname) + len + 1;

	escaped = 0;
	fsm = TSDP_PFSM_K1;
	fill = qn->flyweight;
	for (p = string; *p; p++) {
		if (*p == '\\') {
			escaped = 1;
			continue;
		}
		if (escaped) {
			switch (fsm) {
			case TSDP_PFSM_K1:
				fsm = TSDP_PFSM_K2;
				qn->keys[qn->pairs] = fill;
			case TSDP_PFSM_K2:
				*fill++ = *p;
				qn->length++;
				break;

			case TSDP_PFSM_V1:
				fsm = TSDP_PFSM_V2;
				qn->values[qn->pairs] = fill;
			case TSDP_PFSM_V2:
				*fill++ = *p;
				qn->length++;
				break;

			default:
				free(qn);
				debugf("invalid FSM state [%d] for escape sequence\n", fsm);
				return INVALID_QNAME;
			}
			escaped = 0;
			continue;
		}

		switch (fsm) {
		case TSDP_PFSM_K1:
			if (s_is_character(*p)) {
				qn->keys[qn->pairs] = fill;
				*fill++ = *p;
				qn->length++;
				fsm = TSDP_PFSM_K2;

			} else if (s_is_wildcard(*p)) {
				qn->keys[qn->pairs] = fill;
				qn->wildcard = 1;
				*fill++ = *p;
				qn->length++;
				fsm = TSDP_PFSM_M;

			} else {
				free(qn);
				debugf("invalid token (%c / %#02x) for transition from state K1\n", *p, *p);
				return INVALID_QNAME;
			}
			break;


		case TSDP_PFSM_K2:
			if (*p == '=') {
				*fill++ = '\0';
				qn->length++;
				fsm = TSDP_PFSM_V1;

			} else if (*p == ',') {
				*fill++ = '\0';
				qn->length++;
				qn->values[qn->pairs] = NULL;
				qn->pairs++;
				if (qn->pairs >= TSDP_MAX_QNAME_PAIRS) {
					free(qn);
					debugf("exceeded TSDP_MAX_QNAME_PAIRS (%d) after key '%s'\n", TSDP_MAX_QNAME_PAIRS, qn->keys[qn->pairs-1]);
					return INVALID_QNAME;
				}
				fsm = TSDP_PFSM_K1;

			} else if (s_is_character(*p)) {
				*fill++ = *p;
				qn->length++;

			} else {
				free(qn);
				debugf("invalid token (%c / %#02x) for transition from state K2\n", *p, *p);
				return INVALID_QNAME;
			}
			break;


		case TSDP_PFSM_V1:
			if (s_is_character(*p)) {
				qn->values[qn->pairs] = fill;
				*fill++ = *p;
				qn->length++;
				fsm = TSDP_PFSM_V2;

			} else if (s_is_wildcard(*p)) {
				qn->values[qn->pairs] = fill;
				qn->partial[qn->pairs] = 1;
				*fill++ = *p;
				qn->length++;
				fsm = TSDP_PFSM_M;

			} else if (*p == ',') {
				*fill++ = '\0';
				qn->length++;
				qn->pairs++;
				if (qn->pairs >= TSDP_MAX_QNAME_PAIRS) {
					free(qn);
					debugf("exceeded TSDP_MAX_QNAME_PAIRS (%d) after key '%s'\n", TSDP_MAX_QNAME_PAIRS, qn->keys[qn->pairs-1]);
					return INVALID_QNAME;
				}
				fsm = TSDP_PFSM_K1;

			} else {
				free(qn);
				debugf("invalid token (%c / %#02x) for transition from state V1\n", *p, *p);
				return INVALID_QNAME;
			}
			break;


		case TSDP_PFSM_V2:
			if (*p == ',') {
				*fill++ = '\0';
				qn->length++;
				qn->pairs++;
				if (qn->pairs >= TSDP_MAX_QNAME_PAIRS) {
					free(qn);
					debugf("exceeded TSDP_MAX_QNAME_PAIRS (%d) after key '%s'\n", TSDP_MAX_QNAME_PAIRS, qn->keys[qn->pairs-1]);
					return INVALID_QNAME;
				}
				fsm = TSDP_PFSM_K1;

			} else if (s_is_character(*p)) {
				*fill++ = *p;
				qn->length++;

			} else {
				free(qn);
				debugf("invalid token (%c / %#02x) for transition from state V2\n", *p, *p);
				return INVALID_QNAME;
			}
			break;


		case TSDP_PFSM_M:
			if (*p == ',') {
				*fill++ = '\0';
				qn->length++;
				qn->pairs++;
				if (qn->pairs >= TSDP_MAX_QNAME_PAIRS) {
					free(qn);
					debugf("exceeded TSDP_MAX_QNAME_PAIRS (%d) after key '%s'\n", TSDP_MAX_QNAME_PAIRS, qn->keys[qn->pairs-1]);
					return INVALID_QNAME;
				}
				fsm = TSDP_PFSM_K1;

			} else {
				free(qn);
				debugf("invalid token (%c / %#02x) for transition from state M\n", *p, *p);
				return INVALID_QNAME;
			}
			break;


		defaut:
			free(qn);
			debugf("invalid FSM state [%d]\n", fsm);
			return INVALID_QNAME;
		}
	}

	/* EOF; check states that can legitimately lead to DONE */
	switch (fsm) {
	case TSDP_PFSM_K2:
	case TSDP_PFSM_V1:
	case TSDP_PFSM_V2:
	case TSDP_PFSM_M:
		if (!qn->wildcard) {
			qn->pairs++;
		}
		break;

	default:
		free(qn);
		debugf("invalid final FSM state [%d]\n", fsm);
		return INVALID_QNAME;
	}

	/* remove trailing and leading whitespace from keys
	   and values, adjusting length as necessary */
	for (i = 0; i < qn->pairs; i++) {

		fill = qn->keys[i];
		if (fill) {
			/* leading space on key */
			while (*fill == ' ') {
				fill++;
				qn->length--;
			}
			if (!*fill) {
				/* N.B.: this should never happen unless there is a bug in the
				         K1 -> K2 (on whitespace) state transition in the FSM,
				         but it doesn't hurt to be cautious. */
				free(qn);
				debugf("key %d was pure whitespace\n", i+1);
				return INVALID_QNAME;
			}
			qn->keys[i] = fill;

			/* trailing space on key */
			while (*fill)
				fill++;
			fill--;
			while (fill > qn->keys[i] && *fill == ' ') {
				*fill-- = '\0';
				qn->length--;
			}
		}

		fill = qn->values[i];
		if (fill) {
			/* leading space on value */
			while (*fill == ' ') {
				fill++;
				qn->length--;
			}
			qn->values[i] = fill;

			if (*fill) {
				/* trailing space on value */
				while (*fill)
					fill++;
				fill--;
				while (fill > qn->values[i] && *fill == ' ') {
					*fill-- = '\0';
					qn->length--;
				}
			}
		}
	}

	/* sort the key/value pairs lexcially by key, to make
	   comparison and stringification easier, later */
	s_sort(qn->pairs, qn->keys, qn->values, qn->alloc, qn->partial);
	return qn;
}


struct tsdp_qname *
tsdp_qname_dup(struct tsdp_qname *qn)
{
	int i;
	struct tsdp_qname *dup;

	if (qn == INVALID_QNAME) return INVALID_QNAME;

	dup = calloc(1, qn->allocated);
	memcpy(dup, qn, qn->allocated);

	/* adjust all key/value pointers to point to _our_ flyweight... */
	for (i = 0; i < TSDP_MAX_QNAME_PAIRS; i++) {
		if (qn->alloc[i]) {
			dup->keys[i] = strdup(qn->keys[i]);
			dup->values[i] = strdup(qn->values[i]);
		} else {
			if (dup->keys[i])   dup->keys[i]   = dup->keys[i]   - (char *)qn + (char *)dup;
			if (dup->values[i]) dup->values[i] = dup->values[i] - (char *)qn + (char *)dup;
		}
	}

	return dup;
}


void
tsdp_qname_free(struct tsdp_qname *qn)
{
	int i;
	if (qn) {
		for (i = 0; i < qn->pairs; i++) {
			if (qn->alloc[i]) {
				free(qn->keys[i]);
				free(qn->values[i]);
			}
		}
	}
	free(qn);
}


char *
tsdp_qname_string(struct tsdp_qname *qn)
{
	char *string, *fill, *tombstone;
	const char *p;
	int i;

	if (qn == INVALID_QNAME) {
		return strdup("");
	}

	s_compact(qn);
	string = calloc(qn->length + 1, sizeof(char));
	if (!string) {
		debugf("alloc(%u + %lu, %lu) [=%lu] failed\n", qn->length, 1LU, sizeof(char),
				(qn->length + 1LU) * sizeof(char));
		return NULL;
	}

	fill = string;
	tombstone = fill + qn->length;
	for (i = 0; i < qn->pairs && i < TSDP_MAX_QNAME_PAIRS; i++) {
		if (!qn->keys[i])
			continue; /* tombstone'd key */

		p = qn->keys[i];
		while (*p && fill != tombstone)
			*fill++ = *p++;
		if ( (p = qn->values[i]) != NULL) {
			*fill++ = '=';
			while (*p && fill != tombstone)
				*fill++ = *p++;
		}
		if (i + 1 != qn->pairs) {
			*fill++ = ',';
		}
	}
	if (qn->wildcard) {
		if (qn->pairs != 0) {
			*fill++ = ',';
		}
		*fill++ = '*';
	}

	return string;
}

int
tsdp_qname_equal(struct tsdp_qname *a, struct tsdp_qname *b)
{
	unsigned int i;
	/* the invalid qualified name is never equivalent to anything */
	if (a == INVALID_QNAME || b == INVALID_QNAME) {
		return 0;
	}

	/* wildcard names only match other wildcard names */
	if (a->wildcard != b->wildcard) {
		return 0;
	}

	/* equivalent names have the same number of key=value pairs */
	if (a->pairs != b->pairs) {
		return 0;
	}

	/* pairs should be lexically ordered, so we can compare
	   them sequentially for key- and value-equality */
	for (i = 0; i < a->pairs; i++) {
		if (!a->keys[i] || !b->keys[i]) {
			return 0; /* it is an error to have NULL keys */
		}
		if (strcmp(a->keys[i], b->keys[i]) != 0) {
			return 0; /* key mismatch */
		}
		if (!!a->values[i] != !!b->values[i]) {
			return 0; /* value not present in both */
		}
		if (a->values[i] &&
		    strcmp(a->values[i], b->values[i]) != 0) {
			return 0; /* value mismatch */
		}
	}
	return 1;
}

int
tsdp_qname_match(struct tsdp_qname *qn, struct tsdp_qname *pattern)
{
	unsigned int i, j;
	int found;

	/* the invalid qualified name never matches to anything */
	if (qn == INVALID_QNAME || pattern == INVALID_QNAME) {
		return 0;
	}

	/* pattern constraints must be met first */
	for (i = 0; i < pattern->pairs; i++) {
		if (!pattern->keys[i]) {
			return 0; /* it is an error to have NULL keys */
		}

		/* see if the key is present in the qn */
		found = 0;
		for (j = 0; j < qn->pairs; j++) {
			if (!qn->keys[j]) {
				continue; /* tombstone'd key */
			}
			if (strcmp(pattern->keys[i], qn->keys[j]) == 0) {
				found = 1;
				break;
			}
		}
		if (!found) {
			return 0; /* pattern constraint not met */
		}

		if (pattern->partial[i]) {
			continue;
		}
		if (!!qn->values[j] != !!pattern->values[i]) {
			return 0; /* value present in one, but not the other */
		}
		if (qn->values[j] &&
		    strcmp(qn->values[j], pattern->values[i]) != 0) {
			return 0; /* value mismatch */
		}
	}

	/* if the length of the name and the pattern don't match,
	   the name must be longer, and we need to check if the
	   pattern is a wildcard or not */
	if (qn->pairs != pattern->pairs && !pattern->wildcard) {
		return 0;
	}
	return 1;
}

int
tsdp_qname_set(struct tsdp_qname *qn, const char *key, const char *value)
{
	int i, tombstone;

	errno = EINVAL;
	if (!qn) return -1;

	tombstone = -1;
	for (i = 0; i < qn->pairs; i++) {
		if (!qn->keys[i]) {
			tombstone = i;
			continue; /* tombstone'd key */
		}
		if (strcmp(qn->keys[i], key) == 0) {
			qn->length = qn->length - strlen(value);
			if (qn->values[i]) qn->length += strlen(qn->values[i]);
			qn->keys[i]    = strdup(key);   /* not necessary, but simplifies */
			qn->values[i]  = strdup(value); /* the implementation slightly.  */
			qn->alloc[i]   = 1;
			qn->partial[i] = 0;
			return 0;
		}
	}

	errno = ENOMEM; /* FIXME */
	if (qn->pairs >= TSDP_MAX_QNAME_PAIRS) {
		if (tombstone < 0) return -1;
		/* reclaim tombstone */
		qn->keys[tombstone]    = strdup(key);
		qn->values[tombstone]  = strdup(value);
		qn->alloc[tombstone]   = 1;
		qn->partial[tombstone] = 0;
		qn->length += strlen(key) + 1 + strlen(value) + 1;
		return 0;
	}

	qn->keys[qn->pairs]    = strdup(key);
	qn->values[qn->pairs]  = strdup(value);
	qn->alloc[qn->pairs]   = 1;
	qn->partial[qn->pairs] = 0;
	qn->length += strlen(key) + 1 + strlen(value) + 1;
	qn->pairs++;
	return 0;
}

int
tsdp_qname_unset(struct tsdp_qname *qn, const char *key)
{
	int i;
	errno = EINVAL;
	if (!qn) return -1;

	for (i = 0; i < qn->pairs; i++) {
		if (strcmp(qn->keys[i], key) == 0) {
			qn->keys[i]    = NULL;
			qn->values[i]  = NULL;
			qn->alloc[i]   = 0;
			qn->partial[i] = 0;
			return 0;
		}
	}

	return 0;
}

const char *
tsdp_qname_get(struct tsdp_qname *qn, const char *key)
{
	int i;
	errno = EINVAL;
	if (!qn) return NULL;

	for (i = 0; i < qn->pairs; i++) {
		if (strcmp(qn->keys[i], key) == 0) {
			return qn->values[i];
		}
	}

	return NULL;
}

int
tsdp_qname_merge(struct tsdp_qname *a, struct tsdp_qname *b)
{
	int i, rc;

	for (i = 0; i < b->pairs; i++) {
		if (b->keys[i]) {
			rc = tsdp_qname_set(a, b->keys[i], b->values[i]);
			if (rc != 0) return rc;
		}
	}
	return 0;
}
