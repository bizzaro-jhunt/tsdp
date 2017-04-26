#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tsdp.h>

int main(int argc, char **argv)
{
	struct tsdp_qname *a, *b, *copy;
	int rc;
	char *s;

	if (argc != 3) {
		fprintf(stderr, "incorrect usage.  try %s a=b c=d\n", argv[1]);
		return 2;
	}

	a = tsdp_qname_parse(argv[1]);
	if (!a) {
		fprintf(stderr, "failed to parse qname '%s'\n", argv[1]);
		return 3;
	}
	b = tsdp_qname_parse(argv[2]);
	if (!b) {
		fprintf(stderr, "failed to parse qname '%s'\n", argv[2]);
		return 4;
	}

	rc = tsdp_qname_merge(a, b);
	if (rc != 0) {
		fprintf(stderr, "failed to merge (%s) and (%s)\n", argv[1], argv[2]);
		return 5;
	}

	s = tsdp_qname_string(a);
	fprintf(stdout, "%s / ", s);
	free(s);

	copy = tsdp_qname_dup(a);
	if (!copy) {
		fprintf(stderr, "unable to dup the '%s'...\n", argv[1]);
		return 4;
	}
	tsdp_qname_free(a); /* just to make sure */
	tsdp_qname_free(b); /* just to make sure */

	s = tsdp_qname_string(copy);
	fprintf(stdout, "%s\n", s);
	return 0;
}
