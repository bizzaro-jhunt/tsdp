#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tsdp.h>

int main(int argc, char **argv)
{
	struct tsdp_qname *qn, *copy;
	char *key, *s;

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
	s = tsdp_qname_get(qn, key);
	fprintf(stdout, "%s / ", s ? s : "~");

	copy = tsdp_qname_dup(qn);
	if (!copy) {
		fprintf(stderr, "unable to dup the '%s'...\n", argv[1]);
		return 4;
	}

	tsdp_qname_free(qn); /* just to make sure */
	s = tsdp_qname_get(copy, key);
	fprintf(stdout, "%s", s ? s : "~");

	return 0;
}
