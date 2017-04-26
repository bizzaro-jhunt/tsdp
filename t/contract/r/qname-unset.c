#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tsdp.h>

int main(int argc, char **argv)
{
	struct tsdp_qname *qn, *copy;
	char *key, *s;
	int rc;

	if (argc != 3) {
		fprintf(stderr, "incorrect usage.  try %s a=b,c=d c\n", argv[0]);
		return 2;
	}

	qn = tsdp_qname_parse(argv[1]);
	if (!qn) {
		fprintf(stderr, "unable to parse '%s' as a qname...\n", argv[1]);
		return 3;
	}

	key = argv[2];
	rc = tsdp_qname_unset(qn, key);
	if (rc != 0) {
		fprintf(stderr, "failed to unset %s\n", key);
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

	s = tsdp_qname_string(copy); /* just to make sure */
	fprintf(stdout, "%s\n", s);
	return 0;
}
