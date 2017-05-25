#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tsdp.h>

int main(int argc, char **argv)
{
	struct qname *q, *copy;
	char *key, *value, *s;
	int rc;

	if (argc != 4) {
		fprintf(stderr, "incorrect usage.  try %s a=b,c=d key value\n", argv[0]);
		return 2;
	}

	q = qname_parse(argv[1]);
	if (!q) {
		fprintf(stderr, "unable to parse '%s' as a qname...\n", argv[1]);
		return 3;
	}

	key = argv[2];
	value = argv[3];
	rc = qname_set(q, key, value);
	if (rc != 0) {
		fprintf(stderr, "failed to set %s='%s'\n", key, value);
		return 4;
	}

	s = qname_string(q);
	fprintf(stdout, "%s / ", s);
	free(s);

	copy = qname_dup(q);
	if (!copy) {
		fprintf(stderr, "unable to dup the '%s'...\n", argv[1]);
		return 4;
	}
	qname_free(q); /* just to make sure */

	s = qname_string(copy);
	fprintf(stdout, "%s\n", s);

	return 0;
}
