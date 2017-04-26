#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tsdp.h>

int main(int argc, char **argv)
{
	struct tsdp_qname *qn, *copy;
	char *key, *value, *s;
	int rc;

	if (argc != 4) {
		fprintf(stderr, "incorrect usage.  try %s a=b,c=d key value\n", argv[0]);
		return 2;
	}

	qn = tsdp_qname_parse(argv[1]);
	if (!qn) {
		fprintf(stderr, "unable to parse '%s' as a qname...\n", argv[1]);
		return 3;
	}

	key = argv[2];
	value = argv[3];
	rc = tsdp_qname_set(qn, key, value);
	if (rc != 0) {
		fprintf(stderr, "failed to set %s='%s'\n", key, value);
		return 4;
	}

	s = tsdp_qname_string(qn);
	fprintf(stdout, "%s / ", s);
	free(s);

	copy = tsdp_qname_dup(qn);
	if (!copy) {
		fprintf(stderr, "unable to dup the '%s'...\n", argv[1]);
		return 4;
	}
	tsdp_qname_free(qn); /* just to make sure */

	s = tsdp_qname_string(copy);
	fprintf(stdout, "%s\n", s);

	return 0;
}
